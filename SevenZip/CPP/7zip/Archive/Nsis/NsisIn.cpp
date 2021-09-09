// NsisIn.cpp

#include "StdAfx.h"

#include "../../../Common/IntToString.h"
#include "../../../Common/StringToInt.h"

#include "../../Common/LimitedStreams.h"
#include "../../Common/StreamUtils.h"

#include "NsisIn.h"

#define Get16(p) GetUi16(p)
#define Get32(p) GetUi32(p)

// #define NUM_SPEED_TESTS 1000

namespace NArchive {
namespace NNsis {

static const size_t kInputBufSize = 1 << 20;

const Byte kSignature[kSignatureSize] = NSIS_SIGNATURE;
static const UInt32 kMask_IsCompressed = (UInt32)1 << 31;

static const unsigned kNumCommandParams = 6;
static const unsigned kCmdSize = 4 + kNumCommandParams * 4;

#ifdef NSIS_SCRIPT
#define CR_LF "\x0D\x0A"
#endif

static const char * const kErrorStr = "$_ERROR_STR_";

#define RINOZ(x) { int __tt = (x); if (__tt != 0) return __tt; }


/* There are several versions of NSIS:
   1) Original NSIS:
        NSIS-2 ANSI
        NSIS-3 ANSI
        NSIS-3 Unicode
   2) NSIS from Jim Park that extends old NSIS-2 to Unicode support:
        NSIS-Park-(1,2,3) ANSI
        NSIS-Park-(1,2,3) Unicode

   The command IDs layout is slightly different for different versions.
   Also there are additional "log" versions of NSIS that support EW_LOG.
   We use the layout of "NSIS-3 Unicode" without "log" as main layout.
   And we transfer the command IDs to main layout, if another layout is detected. */


enum
{
  EW_INVALID_OPCODE,
  EW_RET,               // Return
  EW_NOP,               // Nop, Goto
  EW_ABORT,             // Abort
  EW_QUIT,              // Quit
  EW_CALL,              // Call, InitPluginsDir
  EW_UPDATETEXT,        // DetailPrint
  EW_SLEEP,             // Sleep
  EW_BRINGTOFRONT,      // BringToFront
  EW_CHDETAILSVIEW,     // SetDetailsView
  EW_SETFILEATTRIBUTES, // SetFileAttributes
  EW_CREATEDIR,         // CreateDirectory, SetOutPath
  EW_IFFILEEXISTS,      // IfFileExists
  EW_SETFLAG,           // SetRebootFlag, ...
  EW_IFFLAG,            // IfAbort, IfSilent, IfErrors, IfRebootFlag
  EW_GETFLAG,           // GetInstDirError, GetErrorLevel
  EW_RENAME,            // Rename
  EW_GETFULLPATHNAME,   // GetFullPathName
  EW_SEARCHPATH,        // SearchPath
  EW_GETTEMPFILENAME,   // GetTempFileName
  EW_EXTRACTFILE,       // File
  EW_DELETEFILE,        // Delete
  EW_MESSAGEBOX,        // MessageBox
  EW_RMDIR,             // RMDir
  EW_STRLEN,            // StrLen
  EW_ASSIGNVAR,         // StrCpy
  EW_STRCMP,            // StrCmp
  EW_READENVSTR,        // ReadEnvStr, ExpandEnvStrings
  EW_INTCMP,            // IntCmp, IntCmpU
  EW_INTOP,             // IntOp
  EW_INTFMT,            // IntFmt
  EW_PUSHPOP,           // Push/Pop/Exchange
  EW_FINDWINDOW,        // FindWindow
  EW_SENDMESSAGE,       // SendMessage
  EW_ISWINDOW,          // IsWindow
  EW_GETDLGITEM,        // GetDlgItem
  EW_SETCTLCOLORS,      // SetCtlColors
  EW_SETBRANDINGIMAGE,  // SetBrandingImage
  EW_CREATEFONT,        // CreateFont
  EW_SHOWWINDOW,        // ShowWindow, EnableWindow, HideWindow
  EW_SHELLEXEC,         // ExecShell
  EW_EXECUTE,           // Exec, ExecWait
  EW_GETFILETIME,       // GetFileTime
  EW_GETDLLVERSION,     // GetDLLVersion

  // EW_GETFONTVERSION, // Park : 2.46.2
  // EW_GETFONTNAME,    // Park : 2.46.3
 
  EW_REGISTERDLL,       // RegDLL, UnRegDLL, CallInstDLL
  EW_CREATESHORTCUT,    // CreateShortCut
  EW_COPYFILES,         // CopyFiles
  EW_REBOOT,            // Reboot
  EW_WRITEINI,          // WriteINIStr, DeleteINISec, DeleteINIStr, FlushINI
  EW_READINISTR,        // ReadINIStr
  EW_DELREG,            // DeleteRegValue, DeleteRegKey
  EW_WRITEREG,          // WriteRegStr, WriteRegExpandStr, WriteRegBin, WriteRegDWORD
  EW_READREGSTR,        // ReadRegStr, ReadRegDWORD
  EW_REGENUM,           // EnumRegKey, EnumRegValue
  EW_FCLOSE,            // FileClose
  EW_FOPEN,             // FileOpen
  EW_FPUTS,             // FileWrite, FileWriteByte
  EW_FGETS,             // FileRead, FileReadByte

  // Park
  // EW_FPUTWS,            // FileWriteUTF16LE, FileWriteWord
  // EW_FGETWS,            // FileReadUTF16LE, FileReadWord
  
  EW_FSEEK,             // FileSeek
  EW_FINDCLOSE,         // FindClose
  EW_FINDNEXT,          // FindNext
  EW_FINDFIRST,         // FindFirst
  EW_WRITEUNINSTALLER,  // WriteUninstaller
  
  // Park : since 2.46.3 the log is enabled in main Park version
  // EW_LOG,               // LogSet, LogText

  EW_SECTIONSET,        // Get*, Set*
  EW_INSTTYPESET,       // InstTypeSetText, InstTypeGetText, SetCurInstType, GetCurInstType

  // instructions not actually implemented in exehead, but used in compiler.
  EW_GETLABELADDR,      // both of these get converted to EW_ASSIGNVAR
  EW_GETFUNCTIONADDR,
  
  EW_LOCKWINDOW,        // LockWindow
  
  // 2 unicode commands available only in Unicode archive
  EW_FPUTWS,            // FileWriteUTF16LE, FileWriteWord
  EW_FGETWS,            // FileReadUTF16LE, FileReadWord

  // The following IDs are not IDs in real order.
  // We just need some IDs to translate eny extended layout to main layout.

  EW_LOG,               // LogSet, LogText

  // Park
  EW_FINDPROC,          // FindProc
  
  EW_GETFONTVERSION,    // GetFontVersion
  EW_GETFONTNAME,       // GetFontName

  kNumCmds
};



struct CCommandInfo
{
  Byte NumParams;
};

static const CCommandInfo k_Commands[kNumCmds] =
{
  { 0 }, // "Invalid" },
  { 0 }, // Return
  { 1 }, // Nop, Goto
  { 1 }, // "Abort" },
  { 0 }, // "Quit" },
  { 2 }, // Call
  { 6 }, // "DetailPrint" }, // 1 param in new versions, 6 in old NSIS versions
  { 1 }, // "Sleep" },
  { 0 }, // "BringToFront" },
  { 2 }, // "SetDetailsView" },
  { 2 }, // "SetFileAttributes" },
  { 3 }, // CreateDirectory, SetOutPath
  { 3 }, // "IfFileExists" },
  { 3 }, // SetRebootFlag, ...
  { 4 }, // "If" }, // IfAbort, IfSilent, IfErrors, IfRebootFlag
  { 2 }, // "Get" }, // GetInstDirError, GetErrorLevel
  { 4 }, // "Rename" },
  { 3 }, // "GetFullPathName" },
  { 2 }, // "SearchPath" },
  { 2 }, // "GetTempFileName" },
  { 6 }, // "File"
  { 2 }, // "Delete" },
  { 6 }, // "MessageBox" },
  { 2 }, // "RMDir" },
  { 2 }, // "StrLen" },
  { 4 }, // StrCpy, GetCurrentAddress
  { 5 }, // "StrCmp" },
  { 3 }, // ReadEnvStr, ExpandEnvStrings
  { 6 }, // "IntCmp" },
  { 4 }, // "IntOp" },
  { 3 }, // "IntFmt" },
  { 6 }, // Push, Pop, Exch // it must be 3 params. But some multi-command write garbage.
  { 5 }, // "FindWindow" },
  { 6 }, // "SendMessage" },
  { 3 }, // "IsWindow" },
  { 3 }, // "GetDlgItem" },
  { 2 }, // "SetCtlColors" },
  { 3 }, // "SetBrandingImage" },
  { 5 }, // "CreateFont" },
  { 4 }, // ShowWindow, EnableWindow, HideWindow
  { 6 }, // "ExecShell" },
  { 3 }, // "Exec" }, // Exec, ExecWait
  { 3 }, // "GetFileTime" },
  { 3 }, // "GetDLLVersion" },
  { 6 }, // RegDLL, UnRegDLL, CallInstDLL // it must be 5 params. But some multi-command write garbage.
  { 6 }, // "CreateShortCut" },
  { 4 }, // "CopyFiles" },
  { 1 }, // "Reboot" },
  { 5 }, // WriteINIStr, DeleteINISec, DeleteINIStr, FlushINI
  { 4 }, // "ReadINIStr" },
  { 5 }, // "DeleteReg" }, // DeleteRegKey, DeleteRegValue
  { 6 }, // "WriteReg" },  // WriteRegStr, WriteRegExpandStr, WriteRegBin, WriteRegDWORD
  { 5 }, // "ReadReg" }, // ReadRegStr, ReadRegDWORD
  { 5 }, // "EnumReg" }, // EnumRegKey, EnumRegValue
  { 1 }, // "FileClose" },
  { 4 }, // "FileOpen" },
  { 3 }, // "FileWrite" }, // FileWrite, FileWriteByte
  { 4 }, // "FileRead" }, // FileRead, FileReadByte
  { 4 }, // "FileSeek" },
  { 1 }, // "FindClose" },
  { 2 }, // "FindNext" },
  { 3 }, // "FindFirst" },
  { 4 }, // "WriteUninstaller" },
  { 5 }, // "Section" },  // ***
  { 4 }, // InstTypeSetText, InstTypeGetText, SetCurInstType, GetCurInstType
  { 6 }, // "GetLabelAddr" },
  { 2 }, // "GetFunctionAddress" },
  { 1 }, // "LockWindow" },
  { 3 }, // "FileWrite" }, // FileWriteUTF16LE, FileWriteWord
  { 4 }, // "FileRead" }, // FileReadUTF16LE, FileReadWord
  
  { 2 }, // "Log" }, // LogSet, LogText
  // Park
  { 2 }, // "FindProc" },
  { 2 }, // "GetFontVersion" },
  { 2 }, // "GetFontName" }
};

#ifdef NSIS_SCRIPT

static const char * const k_CommandNames[kNumCmds] =
{
    "Invalid"
  , NULL // Return
  , NULL // Nop, Goto
  , "Abort"
  , "Quit"
  , NULL // Call
  , "DetailPrint" // 1 param in new versions, 6 in old NSIS versions
  , "Sleep"
  , "BringToFront"
  , "SetDetailsView"
  , "SetFileAttributes"
  , NULL // CreateDirectory, SetOutPath
  , "IfFileExists"
  , NULL // SetRebootFlag, ...
  , "If" // IfAbort, IfSilent, IfErrors, IfRebootFlag
  , "Get" // GetInstDirError, GetErrorLevel
  , "Rename"
  , "GetFullPathName"
  , "SearchPath"
  , "GetTempFileName"
  , NULL // File
  , "Delete"
  , "MessageBox"
  , "RMDir"
  , "StrLen"
  , NULL // StrCpy, GetCurrentAddress
  , "StrCmp"
  , NULL // ReadEnvStr, ExpandEnvStrings
  , "IntCmp"
  , "IntOp"
  , "IntFmt"
  , NULL // Push, Pop, Exch // it must be 3 params. But some multi-command write garbage.
  , "FindWindow"
  , "SendMessage"
  , "IsWindow"
  , "GetDlgItem"
  , "SetCtlColors"
  , "SetBrandingImage"
  , "CreateFont"
  , NULL // ShowWindow, EnableWindow, HideWindow
  , "ExecShell"
  , "Exec" // Exec, ExecWait
  , "GetFileTime"
  , "GetDLLVersion"
  , NULL // RegDLL, UnRegDLL, CallInstDLL // it must be 5 params. But some multi-command write garbage.
  , "CreateShortCut"
  , "CopyFiles"
  , "Reboot"
  , NULL // WriteINIStr, DeleteINISec, DeleteINIStr, FlushINI
  , "ReadINIStr"
  , "DeleteReg" // DeleteRegKey, DeleteRegValue
  , "WriteReg"  // WriteRegStr, WriteRegExpandStr, WriteRegBin, WriteRegDWORD
  , "ReadReg" // ReadRegStr, ReadRegDWORD
  , "EnumReg" // EnumRegKey, EnumRegValue
  , "FileClose"
  , "FileOpen"
  , "FileWrite" // FileWrite, FileWriteByte
  , "FileRead" // FileRead, FileReadByte
  , "FileSeek"
  , "FindClose"
  , "FindNext"
  , "FindFirst"
  , "WriteUninstaller"
  , "Section"  // ***
  , NULL // InstTypeSetText, InstTypeGetText, SetCurInstType, GetCurInstType
  , "GetLabelAddr"
  , "GetFunctionAddress"
  , "LockWindow"
  , "FileWrite" // FileWriteUTF16LE, FileWriteWord
  , "FileRead" // FileReadUTF16LE, FileReadWord
  
  , "Log" // LogSet, LogText

  // Park
  , "FindProc"
  , "GetFontVersion"
  , "GetFontName"
};

#endif

/* NSIS can use one name for two CSIDL_*** and CSIDL_COMMON_*** items (CurrentUser / AllUsers)
   Some NSIS shell names are not identical to WIN32 CSIDL_* names.
   NSIS doesn't use some CSIDL_* values. But we add name for all CSIDL_ (marked with '+'). */

static const char * const kShellStrings[] =
{
    "DESKTOP"     // +
  , "INTERNET"    // +
  , "SMPROGRAMS"  // CSIDL_PROGRAMS
  , "CONTROLS"    // +
  , "PRINTERS"    // +
  , "DOCUMENTS"   // CSIDL_PERSONAL
  , "FAVORITES"   // CSIDL_FAVORITES
  , "SMSTARTUP"   // CSIDL_STARTUP
  , "RECENT"      // CSIDL_RECENT
  , "SENDTO"      // CSIDL_SENDTO
  , "BITBUCKET"   // +
  , "STARTMENU"
  , NULL          // CSIDL_MYDOCUMENTS = CSIDL_PERSONAL
  , "MUSIC"       // CSIDL_MYMUSIC
  , "VIDEOS"      // CSIDL_MYVIDEO
  , NULL
  , "DESKTOP"     // CSIDL_DESKTOPDIRECTORY
  , "DRIVES"      // +
  , "NETWORK"     // +
  , "NETHOOD"
  , "FONTS"
  , "TEMPLATES"
  , "STARTMENU"   // CSIDL_COMMON_STARTMENU
  , "SMPROGRAMS"  // CSIDL_COMMON_PROGRAMS
  , "SMSTARTUP"   // CSIDL_COMMON_STARTUP
  , "DESKTOP"     // CSIDL_COMMON_DESKTOPDIRECTORY
  , "APPDATA"     // CSIDL_APPDATA         !!! "QUICKLAUNCH"
  , "PRINTHOOD"
  , "LOCALAPPDATA"
  , "ALTSTARTUP"
  , "ALTSTARTUP"  // CSIDL_COMMON_ALTSTARTUP
  , "FAVORITES"   // CSIDL_COMMON_FAVORITES
  , "INTERNET_CACHE"
  , "COOKIES"
  , "HISTORY"
  , "APPDATA"     // CSIDL_COMMON_APPDATA
  , "WINDIR"
  , "SYSDIR"
  , "PROGRAM_FILES" // +
  , "PICTURES"    // CSIDL_MYPICTURES
  , "PROFILE"
  , "SYSTEMX86" // +
  , "PROGRAM_FILESX86" // +
  , "PROGRAM_FILES_COMMON" // +
  , "PROGRAM_FILES_COMMONX8" // +  CSIDL_PROGRAM_FILES_COMMONX86
  , "TEMPLATES"   // CSIDL_COMMON_TEMPLATES
  , "DOCUMENTS"   // CSIDL_COMMON_DOCUMENTS
  , "ADMINTOOLS"  // CSIDL_COMMON_ADMINTOOLS
  , "ADMINTOOLS"  // CSIDL_ADMINTOOLS
  , "CONNECTIONS" // +
  , NULL
  , NULL
  , NULL
  , "MUSIC"       // CSIDL_COMMON_MUSIC
  , "PICTURES"    // CSIDL_COMMON_PICTURES
  , "VIDEOS"      // CSIDL_COMMON_VIDEO
  , "RESOURCES"
  , "RESOURCES_LOCALIZED"
  , "COMMON_OEM_LINKS" // +
  , "CDBURN_AREA"
  , NULL // unused
  , "COMPUTERSNEARME" // +
};


static inline void UIntToString(AString &s, UInt32 v)
{
  s.Add_UInt32(v);
}

#ifdef NSIS_SCRIPT

void CInArchive::Add_UInt(UInt32 v)
{
  char sz[16];
  ConvertUInt32ToString(v, sz);
  Script += sz;
}

static void Add_SignedInt(CDynLimBuf &s, Int32 v)
{
  char sz[32];
  ConvertInt64ToString(v, sz);
  s += sz;
}

static void Add_Hex(CDynLimBuf &s, UInt32 v)
{
  char sz[16];
  sz[0] = '0';
  sz[1] = 'x';
  ConvertUInt32ToHex(v, sz + 2);
  s += sz;
}

static UInt32 GetUi16Str_Len(const Byte *p)
{
  const Byte *pp = p;
  for (; *pp != 0 || *(pp + 1) != 0; pp += 2);
  return (UInt32)((pp - p) >> 1);
}

void CInArchive::AddLicense(UInt32 param, Int32 langID)
{
  Space();
  if (param >= NumStringChars ||
      param + 1 >= NumStringChars)
  {
    Script += kErrorStr;
    return;
  }
  strUsed[param] = 1;

  UInt32 start = _stringsPos + (IsUnicode ? param * 2 : param);
  UInt32 offset = start + (IsUnicode ? 2 : 1);
  {
    FOR_VECTOR (i, LicenseFiles)
    {
      const CLicenseFile &lic = LicenseFiles[i];
      if (offset == lic.Offset)
      {
        Script += lic.Name;
        return;
      }
    }
  }
  AString fileName ("[LICENSE]");
  if (langID >= 0)
  {
    fileName += "\\license-";
    // LangId_To_String(fileName, langID);
    UIntToString(fileName, langID);
  }
  else if (++_numRootLicenses > 1)
  {
    fileName += '-';
    UIntToString(fileName, _numRootLicenses);
  }
  const Byte *sz = (_data + start);
  unsigned marker = IsUnicode ? Get16(sz) : *sz;
  bool isRTF = (marker == 2);
  fileName += isRTF ? ".rtf" : ".txt"; // if (*sz == 1) it's text;
  Script += fileName;

  CLicenseFile &lic = LicenseFiles.AddNew();
  lic.Name = fileName;
  lic.Offset = offset;
  if (!IsUnicode)
    lic.Size = (UInt32)strlen((const char *)sz + 1);
  else
  {
    sz += 2;
    UInt32 len = GetUi16Str_Len(sz);
    lic.Size = len * 2;
    if (isRTF)
    {
      lic.Text.Alloc((size_t)len);
      for (UInt32 i = 0; i < len; i++, sz += 2)
      {
        unsigned c = Get16(sz);
        if (c >= 256)
          c = '?';
        lic.Text[i] = (Byte)(c);
      }
      lic.Size = len;
      lic.Offset = 0;
    }
  }
}

#endif


// #define kVar_CMDLINE    20
#define kVar_INSTDIR    21
#define kVar_OUTDIR     22
#define kVar_EXEDIR     23
// #define kVar_LANGUAGE   24
#define kVar_TEMP       25
#define kVar_PLUGINSDIR 26
#define kVar_EXEPATH    27  // NSIS 2.26+
// #define kVar_EXEFILE    28  // NSIS 2.26+

#define kVar_HWNDPARENT_225 27
#ifdef NSIS_SCRIPT
#define kVar_HWNDPARENT     29
#endif

// #define kVar__CLICK 30
#define kVar_Spec_OUTDIR_225  29  // NSIS 2.04 - 2.25
#define kVar_Spec_OUTDIR      31  // NSIS 2.26+


static const char * const kVarStrings[] =
{
    "CMDLINE"
  , "INSTDIR"
  , "OUTDIR"
  , "EXEDIR"
  , "LANGUAGE"
  , "TEMP"
  , "PLUGINSDIR"
  , "EXEPATH"   // NSIS 2.26+
  , "EXEFILE"   // NSIS 2.26+
  , "HWNDPARENT"
  , "_CLICK"    // is set from page->clicknext
  , "_OUTDIR"   // NSIS 2.04+
};

static const unsigned kNumInternalVars = 20 + ARRAY_SIZE(kVarStrings);

#define GET_NUM_INTERNAL_VARS (IsNsis200 ? kNumInternalVars - 3 : IsNsis225 ? kNumInternalVars - 2 : kNumInternalVars);

void CInArchive::GetVar2(AString &res, UInt32 index)
{
  if (index < 20)
  {
    if (index >= 10)
    {
      res += 'R';
      index -= 10;
    }
    UIntToString(res, index);
  }
  else
  {
    unsigned numInternalVars = GET_NUM_INTERNAL_VARS;
    if (index < numInternalVars)
    {
      if (IsNsis225 && index >= kVar_EXEPATH)
        index += 2;
      res += kVarStrings[index - 20];
    }
    else
    {
      res += '_';
      UIntToString(res, index - numInternalVars);
      res += '_';
    }
  }
}

void CInArchive::GetVar(AString &res, UInt32 index)
{
  res += '$';
  GetVar2(res, index);
}

#ifdef NSIS_SCRIPT

void CInArchive::Add_Var(UInt32 index)
{
  _tempString_for_GetVar.Empty();
  GetVar(_tempString_for_GetVar, index);
  Script += _tempString_for_GetVar;
}

void CInArchive::AddParam_Var(UInt32 index)
{
  Space();
  Add_Var(index);
}

void CInArchive::AddParam_UInt(UInt32 value)
{
  Space();
  Add_UInt(value);
}

#endif


#define NS_CODE_SKIP    252
#define NS_CODE_VAR     253
#define NS_CODE_SHELL   254
// #define NS_CODE_LANG    255

// #define NS_3_CODE_LANG  1
#define NS_3_CODE_SHELL 2
#define NS_3_CODE_VAR   3
#define NS_3_CODE_SKIP  4

#define PARK_CODE_SKIP  0xE000
#define PARK_CODE_VAR   0xE001
#define PARK_CODE_SHELL 0xE002
#define PARK_CODE_LANG  0xE003

#define IS_NS_SPEC_CHAR(c) ((c) >= NS_CODE_SKIP)
#define IS_PARK_SPEC_CHAR(c) ((c) >= PARK_CODE_SKIP && (c) <= PARK_CODE_LANG)

#define DECODE_NUMBER_FROM_2_CHARS(c0, c1) (((c0) & 0x7F) | (((unsigned)((c1) & 0x7F)) << 7))
#define CONVERT_NUMBER_NS_3_UNICODE(n) n = ((n & 0x7F) | (((n >> 8) & 0x7F) << 7))
#define CONVERT_NUMBER_PARK(n) n &= 0x7FFF


static bool AreStringsEqual_16and8(const Byte *p16, const char *p8)
{
  for (;;)
  {
    unsigned c16 = Get16(p16); p16 += 2;
    unsigned c = (Byte)(*p8++);
    if (c16 != c)
      return false;
    if (c == 0)
      return true;
  }
}

void CInArchive::GetShellString(AString &s, unsigned index1, unsigned index2)
{
  // zeros are not allowed here.
  // if (index1 == 0 || index2 == 0) throw 333;

  if ((index1 & 0x80) != 0)
  {
    unsigned offset = (index1 & 0x3F);

    /* NSIS reads registry string:
         keyName   = HKLM Software\\Microsoft\\Windows\\CurrentVersion
         mask      = KEY_WOW64_64KEY, If 64-bit flag in index1 is set
         valueName = string(offset)
       If registry reading is failed, NSIS uses second parameter (index2)
       to read string. The recursion is possible in that case in NSIS.
       We don't parse index2 string. We only set strUsed status for that
       string (but without recursion). */

    if (offset >= NumStringChars)
    {
      s += kErrorStr;
      return;
    }
    
    #ifdef NSIS_SCRIPT
    strUsed[offset] = 1;
    if (index2 < NumStringChars)
      strUsed[index2] = 1;
    #endif

    const Byte *p = (const Byte *)(_data + _stringsPos);
    int id = -1;
    if (IsUnicode)
    {
      p += offset * 2;
      if (AreStringsEqual_16and8(p, "ProgramFilesDir"))
        id = 0;
      else if (AreStringsEqual_16and8(p, "CommonFilesDir"))
        id = 1;
    }
    else
    {
      p += offset;
      if (strcmp((const char *)p, "ProgramFilesDir") == 0)
        id = 0;
      else if (strcmp((const char *)p, "CommonFilesDir") == 0)
        id = 1;
    }

    s += ((id >= 0) ? (id == 0 ? "$PROGRAMFILES" : "$COMMONFILES") :
      "$_ERROR_UNSUPPORTED_VALUE_REGISTRY_");
    // s += ((index1 & 0x40) != 0) ? "64" : "32";
    if ((index1 & 0x40) != 0)
      s += "64";

    if (id < 0)
    {
      s += '(';
      if (IsUnicode)
      {
        for (unsigned i = 0; i < 256; i++)
        {
          wchar_t c = Get16(p + i * 2);
          if (c == 0)
            break;
          if (c < 0x80)
            s += (char)c;
        }
      }
      else
        s += (const char *)p;
      s += ')';
    }
    return;
  }

  s += '$';
  if (index1 < ARRAY_SIZE(kShellStrings))
  {
    const char *sz = kShellStrings[index1];
    if (sz)
    {
      s += sz;
      return;
    }
  }
  if (index2 < ARRAY_SIZE(kShellStrings))
  {
    const char *sz = kShellStrings[index2];
    if (sz)
    {
      s += sz;
      return;
    }
  }
  s += "_ERROR_UNSUPPORTED_SHELL_";
  s += '[';
  UIntToString(s, index1);
  s += ',';
  UIntToString(s, index2);
  s += ']';
}

#ifdef NSIS_SCRIPT

void CInArchive::Add_LangStr_Simple(UInt32 id)
{
  Script += "LSTR_";
  Add_UInt(id);
}

#endif

void CInArchive::Add_LangStr(AString &res, UInt32 id)
{
  #ifdef NSIS_SCRIPT
  langStrIDs.Add(id);
  #endif
  res += "$(LSTR_";
  UIntToString(res, id);
  res += ')';
}

void CInArchive::GetNsisString_Raw(const Byte *s)
{
  Raw_AString.Empty();

  if (NsisType != k_NsisType_Nsis3)
  {
    for (;;)
    {
      Byte c = *s++;
      if (c == 0)
        return;
      if (IS_NS_SPEC_CHAR(c))
      {
        Byte c0 = *s++;
        if (c0 == 0)
          return;
        if (c != NS_CODE_SKIP)
        {
          Byte c1 = *s++;
          if (c1 == 0)
            return;
          
          if (c == NS_CODE_SHELL)
            GetShellString(Raw_AString, c0, c1);
          else
          {
            unsigned n = DECODE_NUMBER_FROM_2_CHARS(c0, c1);
            if (c == NS_CODE_VAR)
              GetVar(Raw_AString, n);
            else //  if (c == NS_CODE_LANG)
              Add_LangStr(Raw_AString, n);
          }
          continue;
        }
        c = c0;
      }
      Raw_AString += (char)c;
    }
  }

  // NSIS-3 ANSI
  for (;;)
  {
    Byte c = *s++;
    if (c <= NS_3_CODE_SKIP)
    {
      if (c == 0)
        return;
      Byte c0 = *s++;
      if (c0 == 0)
        return;
      if (c != NS_3_CODE_SKIP)
      {
        Byte c1 = *s++;
        if (c1 == 0)
          return;
        
        if (c == NS_3_CODE_SHELL)
          GetShellString(Raw_AString, c0, c1);
        else
        {
          unsigned n = DECODE_NUMBER_FROM_2_CHARS(c0, c1);
          if (c == NS_3_CODE_VAR)
            GetVar(Raw_AString, n);
          else // if (c == NS_3_CODE_LANG)
            Add_LangStr(Raw_AString, n);
        }
        continue;
      }
      c = c0;
    }
    Raw_AString += (char)c;
  }
}

#ifdef NSIS_SCRIPT

void CInArchive::GetNsisString(AString &res, const Byte *s)
{
  for (;;)
  {
    Byte c = *s++;
    if (c == 0)
      return;
    if (NsisType != k_NsisType_Nsis3)
    {
      if (IS_NS_SPEC_CHAR(c))
      {
        Byte c0 = *s++;
        if (c0 == 0)
          return;
        if (c != NS_CODE_SKIP)
        {
          Byte c1 = *s++;
          if (c1 == 0)
            return;
          if (c == NS_CODE_SHELL)
            GetShellString(res, c0, c1);
          else
          {
            unsigned n = DECODE_NUMBER_FROM_2_CHARS(c0, c1);
            if (c == NS_CODE_VAR)
              GetVar(res, n);
            else // if (c == NS_CODE_LANG)
              Add_LangStr(res, n);
          }
          continue;
        }
        c = c0;
      }
    }
    else
    {
      // NSIS-3 ANSI
      if (c <= NS_3_CODE_SKIP)
      {
        Byte c0 = *s++;
        if (c0 == 0)
          return;
        if (c0 == 0)
          break;
        if (c != NS_3_CODE_SKIP)
        {
          Byte c1 = *s++;
          if (c1 == 0)
            return;
          if (c == NS_3_CODE_SHELL)
            GetShellString(res, c0, c1);
          else
          {
            unsigned n = DECODE_NUMBER_FROM_2_CHARS(c0, c1);
            if (c == NS_3_CODE_VAR)
              GetVar(res, n);
            else // if (c == NS_3_CODE_LANG)
              Add_LangStr(res, n);
          }
          continue;
        }
        c = c0;
      }
    }

    {
      const char *e;
           if (c ==   9) e = "$\\t";
      else if (c ==  10) e = "$\\n";
      else if (c ==  13) e = "$\\r";
      else if (c == '"') e = "$\\\"";
      else if (c == '$') e = "$$";
      else
      {
        res += (char)c;
        continue;
      }
      res += e;
      continue;
    }
  }
}

#endif

void CInArchive::GetNsisString_Unicode_Raw(const Byte *p)
{
  Raw_UString.Empty();

  if (IsPark())
  {
    for (;;)
    {
      unsigned c = Get16(p);
      p += 2;
      if (c == 0)
        break;
      if (c < 0x80)
      {
        Raw_UString += (char)c;
        continue;
      }
      
      if (IS_PARK_SPEC_CHAR(c))
      {
        unsigned n = Get16(p);
        p += 2;
        if (n == 0)
          break;
        if (c != PARK_CODE_SKIP)
        {
          Raw_AString.Empty();
          if (c == PARK_CODE_SHELL)
            GetShellString(Raw_AString, n & 0xFF, n >> 8);
          else
          {
            CONVERT_NUMBER_PARK(n);
            if (c == PARK_CODE_VAR)
              GetVar(Raw_AString, n);
            else // if (c == PARK_CODE_LANG)
              Add_LangStr(Raw_AString, n);
          }
          Raw_UString += Raw_AString.Ptr(); // check it !
          continue;
        }
        c = n;
      }
      
      Raw_UString += (wchar_t)c;
    }
    
    return;
  }

  // NSIS-3 Unicode
  for (;;)
  {
    unsigned c = Get16(p);
    p += 2;
    if (c > NS_3_CODE_SKIP)
    {
      Raw_UString += (wchar_t)c;
      continue;
    }
    if (c == 0)
      break;

    unsigned n = Get16(p);
    p += 2;
    if (n == 0)
      break;
    if (c == NS_3_CODE_SKIP)
    {
      Raw_UString += (wchar_t)n;
      continue;
    }

    Raw_AString.Empty();
    if (c == NS_3_CODE_SHELL)
      GetShellString(Raw_AString, n & 0xFF, n >> 8);
    else
    {
      CONVERT_NUMBER_NS_3_UNICODE(n);
      if (c == NS_3_CODE_VAR)
        GetVar(Raw_AString, n);
      else // if (c == NS_3_CODE_LANG)
        Add_LangStr(Raw_AString, n);
    }
    Raw_UString += Raw_AString.Ptr();
  }
}

#ifdef NSIS_SCRIPT

static const Byte kUtf8Limits[5] = { 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

void CInArchive::GetNsisString_Unicode(AString &res, const Byte *p)
{
  for (;;)
  {
    unsigned c = Get16(p);
    p += 2;
    if (c == 0)
      break;
    if (IsPark())
    {
      if (IS_PARK_SPEC_CHAR(c))
      {
        unsigned n = Get16(p);
        p += 2;
        if (n == 0)
          break;
        if (c != PARK_CODE_SKIP)
        {
          if (c == PARK_CODE_SHELL)
            GetShellString(res, n & 0xFF, n >> 8);
          else
          {
            CONVERT_NUMBER_PARK(n);
            if (c == PARK_CODE_VAR)
              GetVar(res, n);
            else // if (c == PARK_CODE_LANG)
              Add_LangStr(res, n);
          }
          continue;
        }
        c = n;
      }
    }
    else
    {
      // NSIS-3 Unicode
      if (c <= NS_3_CODE_SKIP)
      {
        unsigned n = Get16(p);
        p += 2;
        if (n == 0)
          break;
        if (c != NS_3_CODE_SKIP)
        {
          if (c == NS_3_CODE_SHELL)
            GetShellString(res, n & 0xFF, n >> 8);
          else
          {
            CONVERT_NUMBER_NS_3_UNICODE(n);
            if (c == NS_3_CODE_VAR)
              GetVar(res, n);
            else // if (c == NS_3_CODE_LANG)
              Add_LangStr(res, n);
          }
          continue;
        }
        c = n;
      }
    }

    if (c < 0x80)
    {
      const char *e;
           if (c ==   9) e = "$\\t";
      else if (c ==  10) e = "$\\n";
      else if (c ==  13) e = "$\\r";
      else if (c == '"') e = "$\\\"";
      else if (c == '$') e = "$$";
      else
      {
        res += (char)c;
        continue;
      }
      res += e;
      continue;
    }

    UInt32 value = c;
    /*
    if (value >= 0xD800 && value < 0xE000)
    {
      UInt32 c2;
      if (value >= 0xDC00 || srcPos == srcLen)
        break;
      c2 = src[srcPos++];
      if (c2 < 0xDC00 || c2 >= 0xE000)
        break;
      value = (((value - 0xD800) << 10) | (c2 - 0xDC00)) + 0x10000;
    }
    */
    unsigned numAdds;
    for (numAdds = 1; numAdds < 5; numAdds++)
      if (value < (((UInt32)1) << (numAdds * 5 + 6)))
        break;
    res += (char)(kUtf8Limits[numAdds - 1] + (value >> (6 * numAdds)));
    do
    {
      numAdds--;
      res += (char)(0x80 + ((value >> (6 * numAdds)) & 0x3F));
      // destPos++;
    }
    while (numAdds != 0);

    // AddToUtf8(res, c);
  }
}

#endif

void CInArchive::ReadString2_Raw(UInt32 pos)
{
  Raw_AString.Empty();
  Raw_UString.Empty();
  if ((Int32)pos < 0)
    Add_LangStr(Raw_AString, -((Int32)pos + 1));
  else if (pos >= NumStringChars)
  {
    Raw_AString += kErrorStr;
    // UIntToString(Raw_AString, pos);
  }
  else
  {
    if (IsUnicode)
      GetNsisString_Unicode_Raw(_data + _stringsPos + pos * 2);
    else
      GetNsisString_Raw(_data + _stringsPos + pos);
    return;
  }
  Raw_UString = Raw_AString.Ptr();
}

bool CInArchive::IsGoodString(UInt32 param) const
{
  if (param >= NumStringChars)
    return false;
  if (param == 0)
    return true;
  const Byte *p = _data + _stringsPos;
  unsigned c;
  if (IsUnicode)
    c = Get16(p + param * 2 - 2);
  else
    c = p[param - 1];
  // some files have '\\' character before string?
  return (c == 0 || c == '\\');
}

bool CInArchive::AreTwoParamStringsEqual(UInt32 param1, UInt32 param2) const
{
  if (param1 == param2)
    return true;

  /* NSIS-3.0a1 probably contains bug, so it can use 2 different strings
     with same content. So we check real string also.
     Also it's possible to check identical postfix parts of strings. */

  if (param1 >= NumStringChars ||
      param2 >= NumStringChars)
    return false;

  const Byte *p = _data + _stringsPos;

  if (IsUnicode)
  {
    const Byte *p1 = p + param1 * 2;
    const Byte *p2 = p + param2 * 2;
    for (;;)
    {
      UInt16 c = Get16(p1);
      if (c != Get16(p2))
        return false;
      if (c == 0)
        return true;
      p1 += 2;
      p2 += 2;
    }
  }
  else
  {
    const Byte *p1 = p + param1;
    const Byte *p2 = p + param2;
    for (;;)
    {
      Byte c = *p1++;
      if (c != *p2++)
        return false;
      if (c == 0)
        return true;
    }
  }
}

#ifdef NSIS_SCRIPT

UInt32 CInArchive::GetNumUsedVars() const
{
  UInt32 numUsedVars = 0;
  const Byte *data = (const Byte *)_data + _stringsPos;
  unsigned npi = 0;
  for (UInt32 i = 0; i < NumStringChars;)
  {
    bool process = true;
    if (npi < noParseStringIndexes.Size() && noParseStringIndexes[npi] == i)
    {
      process = false;
      npi++;
    }
    
    if (IsUnicode)
    {
      if (IsPark())
      {
        for (;;)
        {
          unsigned c = Get16(data + i * 2);
          i++;
          if (c == 0)
            break;
          if (IS_PARK_SPEC_CHAR(c))
          {
            UInt32 n = Get16(data + i * 2);
            i++;
            if (n == 0)
              break;
            if (process && c == PARK_CODE_VAR)
            {
              CONVERT_NUMBER_PARK(n);
              n++;
              if (numUsedVars < n)
                numUsedVars = n;
            }
          }
        }
      }
      else // NSIS-3 Unicode
      {
        for (;;)
        {
          unsigned c = Get16(data + i * 2);
          i++;
          if (c == 0)
            break;
          if (c > NS_3_CODE_SKIP)
            continue;
          UInt32 n = Get16(data + i * 2);
          i++;
          if (n == 0)
            break;
          if (process && c == NS_3_CODE_VAR)
          {
            CONVERT_NUMBER_NS_3_UNICODE(n);
            n++;
            if (numUsedVars < n)
              numUsedVars = n;
          }
        }
      }
    }
    else // not Unicode (ANSI)
    {
      if (NsisType != k_NsisType_Nsis3)
      {
        for (;;)
        {
          Byte c = data[i++];
          if (c == 0)
            break;
          if (IS_NS_SPEC_CHAR(c))
          {
            Byte c0 = data[i++];
            if (c0 == 0)
              break;
            if (c == NS_CODE_SKIP)
              continue;
            Byte c1 = data[i++];
            if (c1 == 0)
              break;
            if (process && c == NS_CODE_VAR)
            {
              UInt32 n = DECODE_NUMBER_FROM_2_CHARS(c0, c1);
              n++;
              if (numUsedVars < n)
                numUsedVars = n;
            }
          }
        }
      }
      else
      {
        // NSIS-3 ANSI
        for (;;)
        {
          Byte c = data[i++];
          if (c == 0)
            break;
          if (c > NS_3_CODE_SKIP)
            continue;

          Byte c0 = data[i++];
          if (c0 == 0)
            break;
          if (c == NS_3_CODE_SKIP)
            continue;
          Byte c1 = data[i++];
          if (c1 == 0)
            break;
          if (process && c == NS_3_CODE_VAR)
          {
            UInt32 n = DECODE_NUMBER_FROM_2_CHARS(c0, c1);
            n++;
            if (numUsedVars < n)
              numUsedVars = n;
          }
        }
      }
    }
  }
  return numUsedVars;
}

void CInArchive::ReadString2(AString &s, UInt32 pos)
{
  if ((Int32)pos < 0)
  {
    Add_LangStr(s, -((Int32)pos + 1));
    return;
  }

  if (pos >= NumStringChars)
  {
    s += kErrorStr;
    // UIntToString(s, pos);
    return;
  }

  #ifdef NSIS_SCRIPT
  strUsed[pos] = 1;
  #endif

  if (IsUnicode)
    GetNsisString_Unicode(s, _data + _stringsPos + pos * 2);
  else
    GetNsisString(s, _data + _stringsPos + pos);
}

#endif

#ifdef NSIS_SCRIPT

// #define DEL_DIR     1
#define DEL_RECURSE 2
#define DEL_REBOOT  4
// #define DEL_SIMPLE  8

void CInArchive::AddRegRoot(UInt32 val)
{
  Space();
  const char *s;
  switch (val)
  {
    case 0:  s = "SHCTX"; break;
    case 0x80000000:  s = "HKCR"; break;
    case 0x80000001:  s = "HKCU"; break;
    case 0x80000002:  s = "HKLM"; break;
    case 0x80000003:  s = "HKU";  break;
    case 0x80000004:  s = "HKPD"; break;
    case 0x80000005:  s = "HKCC"; break;
    case 0x80000006:  s = "HKDD"; break;
    case 0x80000050:  s = "HKPT"; break;
    case 0x80000060:  s = "HKPN"; break;
    default:
      // Script += " RRRRR ";
      // throw 1;
      Add_Hex(Script, val); return;
  }
  Script += s;
}

static const char * const g_WinAttrib[] =
{
    "READONLY"
  , "HIDDEN"
  , "SYSTEM"
  , NULL
  , "DIRECTORY"
  , "ARCHIVE"
  , "DEVICE"
  , "NORMAL"
  , "TEMPORARY"
  , "SPARSE_FILE"
  , "REPARSE_POINT"
  , "COMPRESSED"
  , "OFFLINE"
  , "NOT_CONTENT_INDEXED"
  , "ENCRYPTED"
  , NULL
  , "VIRTUAL"
};

#define FLAGS_DELIMITER '|'

static void FlagsToString2(CDynLimBuf &s, const char * const *table, unsigned num, UInt32 flags)
{
  bool filled = false;
  for (unsigned i = 0; i < num; i++)
  {
    UInt32 f = (UInt32)1 << i;
    if ((flags & f) != 0)
    {
      const char *name = table[i];
      if (name)
      {
        if (filled)
          s += FLAGS_DELIMITER;
        filled = true;
        s += name;
        flags &= ~f;
      }
    }
  }
  if (flags != 0)
  {
    if (filled)
      s += FLAGS_DELIMITER;
    Add_Hex(s, flags);
  }
}

static bool DoesNeedQuotes(const char *s)
{
  {
    char c = s[0];
    if (c == 0 || c == '#' || c == ';' || (c == '/' && s[1] == '*'))
      return true;
  }
  for (;;)
  {
    char c = *s++;
    if (c == 0)
      return false;
    if (c == ' ')
      return true;
  }
}

void CInArchive::Add_QuStr(const AString &s)
{
  bool needQuotes = DoesNeedQuotes(s);
  if (needQuotes)
    Script += '\"';
  Script += s;
  if (needQuotes)
    Script += '\"';
}

void CInArchive::SpaceQuStr(const AString &s)
{
  Space();
  Add_QuStr(s);
}

void CInArchive::AddParam(UInt32 pos)
{
  _tempString.Empty();
  ReadString2(_tempString, pos);
  SpaceQuStr(_tempString);
}

void CInArchive::AddParams(const UInt32 *params, unsigned num)
{
  for (unsigned i = 0; i < num; i++)
    AddParam(params[i]);
}

void CInArchive::AddOptionalParam(UInt32 pos)
{
  if (pos != 0)
    AddParam(pos);
}

static unsigned GetNumParams(const UInt32 *params, unsigned num)
{
  for (; num > 0 && params[num - 1] == 0; num--);
  return num;
}
 
void CInArchive::AddOptionalParams(const UInt32 *params, unsigned num)
{
  AddParams(params, GetNumParams(params, num));
}


static const UInt32 CMD_REF_Goto    = (1 << 0);
static const UInt32 CMD_REF_Call    = (1 << 1);
static const UInt32 CMD_REF_Pre     = (1 << 2);
static const UInt32 CMD_REF_Show    = (1 << 3);
static const UInt32 CMD_REF_Leave   = (1 << 4);
static const UInt32 CMD_REF_OnFunc  = (1 << 5);
static const UInt32 CMD_REF_Section = (1 << 6);
static const UInt32 CMD_REF_InitPluginDir = (1 << 7);
// static const UInt32 CMD_REF_Creator = (1 << 5); // _Pre is used instead
static const unsigned CMD_REF_OnFunc_NumShifts = 28; // it uses for onFunc too
static const unsigned CMD_REF_Page_NumShifts = 16; // it uses for onFunc too
static const UInt32 CMD_REF_Page_Mask   = 0x0FFF0000;
static const UInt32 CMD_REF_OnFunc_Mask = 0xF0000000;

inline bool IsPageFunc(UInt32 flag)
{
  return (flag & (CMD_REF_Pre | CMD_REF_Show | CMD_REF_Leave)) != 0;
}

inline bool IsFunc(UInt32 flag)
{
  // return (flag & (CMD_REF_Pre | CMD_REF_Show | CMD_REF_Leave | CMD_REF_OnFunc)) != 0;
  return (flag & (CMD_REF_Call | CMD_REF_Pre | CMD_REF_Show | CMD_REF_Leave | CMD_REF_OnFunc)) != 0;
}

inline bool IsProbablyEndOfFunc(UInt32 flag)
{
  return (flag != 0 && flag != CMD_REF_Goto);
}

static const char * const kOnFunc[] =
{
    "Init"
  , "InstSuccess"
  , "InstFailed"
  , "UserAbort"
  , "GUIInit"
  , "GUIEnd"
  , "MouseOverSection"
  , "VerifyInstDir"
  , "SelChange"
  , "RebootFailed"
};

void CInArchive::Add_FuncName(const UInt32 *labels, UInt32 index)
{
  UInt32 mask = labels[index];
  if (mask & CMD_REF_OnFunc)
  {
    Script += ".on";
    Script += kOnFunc[labels[index] >> CMD_REF_OnFunc_NumShifts];
  }
  else if (mask & CMD_REF_InitPluginDir)
  {
    /*
    if (!IsInstaller)
      Script += "un."
    */
    Script += "Initialize_____Plugins";
  }
  else
  {
    Script += "func_";
    Add_UInt(index);
  }
}

void CInArchive::AddParam_Func(const UInt32 *labels, UInt32 index)
{
  Space();
  if ((Int32)index >= 0)
    Add_FuncName(labels, index);
  else
    AddQuotes();
}


void CInArchive::Add_LabelName(UInt32 index)
{
  Script += "label_";
  Add_UInt(index);
}

// param != 0
void CInArchive::Add_GotoVar(UInt32 param)
{
  Space();
  if ((Int32)param < 0)
    Add_Var(-((Int32)param + 1));
  else
    Add_LabelName(param - 1);
}

void CInArchive::Add_GotoVar1(UInt32 param)
{
  if (param == 0)
    Script += " 0";
  else
    Add_GotoVar(param);
}

void CInArchive::Add_GotoVars2(const UInt32 *params)
{
  Add_GotoVar1(params[0]);
  if (params[1] != 0)
    Add_GotoVar(params[1]);
}

static bool NoLabels(const UInt32 *labels, UInt32 num)
{
  for (UInt32 i = 0; i < num; i++)
    if (labels[i] != 0)
      return false;
  return true;
}

static const char * const k_REBOOTOK = " /REBOOTOK";

#define MY__MB_ABORTRETRYIGNORE 2
#define MY__MB_RETRYCANCEL      5

static const char * const k_MB_Buttons[] =
{
    "OK"
  , "OKCANCEL"
  , "ABORTRETRYIGNORE"
  , "YESNOCANCEL"
  , "YESNO"
  , "RETRYCANCEL"
  , "CANCELTRYCONTINUE"
};

#define MY__MB_ICONSTOP   (1 << 4)

static const char * const k_MB_Icons[] =
{
    NULL
  , "ICONSTOP"
  , "ICONQUESTION"
  , "ICONEXCLAMATION"
  , "ICONINFORMATION"
};

static const char * const k_MB_Flags[] =
{
    "HELP"
  , "NOFOCUS"
  , "SETFOREGROUND"
  , "DEFAULT_DESKTOP_ONLY"
  , "TOPMOST"
  , "RIGHT"
  , "RTLREADING"
  // , "SERVICE_NOTIFICATION" // unsupported. That bit is used for NSIS purposes
};

#define MY__IDCANCEL 2
#define MY__IDIGNORE 5

static const char * const k_Button_IDs[] =
{
    "0"
  , "IDOK"
  , "IDCANCEL"
  , "IDABORT"
  , "IDRETRY"
  , "IDIGNORE"
  , "IDYES"
  , "IDNO"
  , "IDCLOSE"
  , "IDHELP"
  , "IDTRYAGAIN"
  , "IDCONTINUE"
};

void CInArchive::Add_ButtonID(UInt32 buttonID)
{
  Space();
  if (buttonID < ARRAY_SIZE(k_Button_IDs))
    Script += k_Button_IDs[buttonID];
  else
  {
    Script += "Button_";
    Add_UInt(buttonID);
  }
}

bool CInArchive::IsDirectString_Equal(UInt32 offset, const char *s) const
{
  if (offset >= NumStringChars)
    return false;
  if (IsUnicode)
    return AreStringsEqual_16and8(_data + _stringsPos + offset * 2, s);
  else
    return strcmp((const char *)(const Byte *)_data + _stringsPos + offset, s) == 0;
}

static bool StringToUInt32(const char *s, UInt32 &res)
{
  const char *end;
  if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
    res = ConvertHexStringToUInt32(s + 2, &end);
  else
    res = ConvertStringToUInt32(s, &end);
  return (*end == 0);
}

static const unsigned k_CtlColors_Size = 24;

struct CNsis_CtlColors
{
  UInt32 text; // COLORREF
  UInt32 bkc;  // COLORREF
  UInt32 lbStyle;
  UInt32 bkb; // HBRUSH
  Int32 bkmode;
  Int32 flags;

  void Parse(const Byte *p);
};

void CNsis_CtlColors::Parse(const Byte *p)
{
  text = Get32(p);
  bkc = Get32(p + 4);
  lbStyle = Get32(p + 8);
  bkb = Get32(p + 12);
  bkmode = (Int32)Get32(p + 16);
  flags = (Int32)Get32(p + 20);
}

// Win32 constants
#define MY__TRANSPARENT 1
// #define MY__OPAQUE      2

#define MY__GENERIC_READ    ((UInt32)1 << 31)
#define MY__GENERIC_WRITE   ((UInt32)1 << 30)
#define MY__GENERIC_EXECUTE ((UInt32)1 << 29)
#define MY__GENERIC_ALL     ((UInt32)1 << 28)

#define MY__CREATE_NEW        1
#define MY__CREATE_ALWAYS     2
#define MY__OPEN_EXISTING     3
#define MY__OPEN_ALWAYS       4
#define MY__TRUNCATE_EXISTING 5

// text/bg colors
#define kColorsFlags_TEXT     1
#define kColorsFlags_TEXT_SYS 2
#define kColorsFlags_BK       4
#define kColorsFlags_BK_SYS   8
#define kColorsFlags_BKB     16

void CInArchive::Add_Color2(UInt32 v)
{
  v = ((v & 0xFF) << 16) | (v & 0xFF00) | ((v >> 16) & 0xFF);
  char sz[32];
  for (int i = 5; i >= 0; i--)
  {
    unsigned t = v & 0xF;
    v >>= 4;
    sz[i] = (char)(((t < 10) ? ('0' + t) : ('A' + (t - 10))));
  }
  sz[6] = 0;
  Script += sz;
}

void CInArchive::Add_ColorParam(UInt32 v)
{
  Space();
  Add_Color2(v);
}

void CInArchive::Add_Color(UInt32 v)
{
  Script += "0x";
  Add_Color2(v);
}

#define MY__SW_HIDE 0
#define MY__SW_SHOWNORMAL 1

#define MY__SW_SHOWMINIMIZED 2
#define MY__SW_SHOWMINNOACTIVE 7
#define MY__SW_SHOWNA 8

static const char * const kShowWindow_Commands[] =
{
    "HIDE"
  , "SHOWNORMAL"     // "NORMAL"
  , "SHOWMINIMIZED"
  , "SHOWMAXIMIZED"  // "MAXIMIZE"
  , "SHOWNOACTIVATE"
  , "SHOW"
  , "MINIMIZE"
  , "SHOWMINNOACTIVE"
  , "SHOWNA"
  , "RESTORE"
  , "SHOWDEFAULT"
  , "FORCEMINIMIZE"  // "MAX"
};

static void Add_ShowWindow_Cmd_2(AString &s, UInt32 cmd)
{
  if (cmd < ARRAY_SIZE(kShowWindow_Commands))
  {
    s += "SW_";
    s += kShowWindow_Commands[cmd];
  }
  else
    UIntToString(s, cmd);
}

void CInArchive::Add_ShowWindow_Cmd(UInt32 cmd)
{
  if (cmd < ARRAY_SIZE(kShowWindow_Commands))
  {
    Script += "SW_";
    Script += kShowWindow_Commands[cmd];
  }
  else
    Add_UInt(cmd);
}

void CInArchive::Add_TypeFromList(const char * const *table, unsigned tableSize, UInt32 type)
{
  if (type < tableSize)
    Script += table[type];
  else
  {
    Script += '_';
    Add_UInt(type);
  }
}

#define ADD_TYPE_FROM_LIST(table, type) Add_TypeFromList(table, ARRAY_SIZE(table), type)

enum
{
  k_ExecFlags_AutoClose,
  k_ExecFlags_ShellVarContext,
  k_ExecFlags_Errors,
  k_ExecFlags_Abort,
  k_ExecFlags_RebootFlag,
  k_ExecFlags_reboot_called,
  k_ExecFlags_cur_insttype,
  k_ExecFlags_plugin_api_version,
  k_ExecFlags_Silent,
  k_ExecFlags_InstDirError,
  k_ExecFlags_rtl,
  k_ExecFlags_ErrorLevel,
  k_ExecFlags_RegView,
  k_ExecFlags_DetailsPrint = 13,
};

// Names for NSIS exec_flags_t structure vars
static const char * const kExecFlags_VarsNames[] =
{
    "AutoClose" // autoclose;
  , "ShellVarContext" // all_user_var;
  , "Errors" // exec_error;
  , "Abort" // abort;
  , "RebootFlag" // exec_reboot; // NSIS_SUPPORT_REBOOT
  , "reboot_called" // reboot_called; // NSIS_SUPPORT_REBOOT
  , "cur_insttype" // XXX_cur_insttype; // depreacted
  , "plugin_api_version" // plugin_api_version; // see NSISPIAPIVER_CURR
                          // used to be XXX_insttype_changed
  , "Silent" // silent; // NSIS_CONFIG_SILENT_SUPPORT
  , "InstDirError" // instdir_error;
  , "rtl" // rtl;
  , "ErrorLevel" // errlvl;
  , "RegView" // alter_reg_view;
  , "DetailsPrint" // status_update;
};

void CInArchive::Add_ExecFlags(UInt32 flagsType)
{
  ADD_TYPE_FROM_LIST(kExecFlags_VarsNames, flagsType);
}


// ---------- Page ----------

// page flags
#define PF_CANCEL_ENABLE 4
#define PF_LICENSE_FORCE_SELECTION 32
#define PF_LICENSE_NO_FORCE_SELECTION 64
#define PF_PAGE_EX 512
#define PF_DIR_NO_BTN_DISABLE 1024
/*
#define PF_LICENSE_SELECTED 1
#define PF_NEXT_ENABLE 2
#define PF_BACK_SHOW 8
#define PF_LICENSE_STREAM 16
#define PF_NO_NEXT_FOCUS 128
#define PF_BACK_ENABLE 256
*/

// page window proc
enum
{
  PWP_LICENSE,
  PWP_SELCOM,
  PWP_DIR,
  PWP_INSTFILES,
  PWP_UNINST,
  PWP_COMPLETED,
  PWP_CUSTOM
};

static const char * const kPageTypes[] =
{
    "license"
  , "components"
  , "directory"
  , "instfiles"
  , "uninstConfirm"
  , "COMPLETED"
  , "custom"
};

#define SET_FUNC_REF(x, flag) if ((Int32)(x) >= 0 && (x) < bh.Num) \
  { labels[x] = (labels[x] & ~CMD_REF_Page_Mask) | ((flag) | (pageIndex << CMD_REF_Page_NumShifts)); }

// #define IDD_LICENSE  102
#define IDD_LICENSE_FSRB 108
#define IDD_LICENSE_FSCB 109

void CInArchive::AddPageOption1(UInt32 param, const char *name)
{
  if (param == 0)
    return;
  TabString(name);
  AddParam(param);
  NewLine();
}

void CInArchive::AddPageOption(const UInt32 *params, unsigned num, const char *name)
{
  num = GetNumParams(params, num);
  if (num == 0)
    return;
  TabString(name);
  AddParams(params, num);
  NewLine();
}

void CInArchive::Separator()
{
  AddLF();
  AddCommentAndString("--------------------");
  AddLF();
}

void CInArchive::Space()
{
  Script += ' ';
}

void CInArchive::Tab()
{
  Script += "  ";
}

void CInArchive::Tab(bool commented)
{
  Script += commented ? "    ; " : "  ";
}

void CInArchive::BigSpaceComment()
{
  Script += "    ; ";
}

void CInArchive::SmallSpaceComment()
{
  Script += " ; ";
}

void CInArchive::AddCommentAndString(const char *s)
{
  Script += "; ";
  Script += s;
}

void CInArchive::AddError(const char *s)
{
  BigSpaceComment();
  Script += "!!! ERROR: ";
  Script += s;
}

void CInArchive::AddErrorLF(const char *s)
{
  AddError(s);
  AddLF();
}

void CInArchive::CommentOpen()
{
  AddStringLF("/*");
}

void CInArchive::CommentClose()
{
  AddStringLF("*/");
}

void CInArchive::AddLF()
{
  Script += CR_LF;
}

void CInArchive::AddQuotes()
{
  Script += "\"\"";
}

void CInArchive::TabString(const char *s)
{
  Tab();
  Script += s;
}

void CInArchive::AddStringLF(const char *s)
{
  Script += s;
  AddLF();
}

// ---------- Section ----------

static const char * const kSection_VarsNames[] =
{
    "Text"
  , "InstTypes"
  , "Flags"
  , "Code"
  , "CodeSize"
  , "Size" // size in KB
};

void CInArchive::Add_SectOp(UInt32 opType)
{
  ADD_TYPE_FROM_LIST(kSection_VarsNames, opType);
}

void CSection::Parse(const Byte *p)
{
  Name = Get32(p);
  InstallTypes = Get32(p + 4);
  Flags = Get32(p + 8);
  StartCmdIndex = Get32(p + 12);
  NumCommands = Get32(p + 16);
  SizeKB = Get32(p + 20);
};

// used for section->flags
#define SF_SELECTED   (1 << 0)
#define SF_SECGRP     (1 << 1)
#define SF_SECGRPEND  (1 << 2)
#define SF_BOLD       (1 << 3)
#define SF_RO         (1 << 4)
#define SF_EXPAND     (1 << 5)
/*
#define SF_PSELECTED  (1 << 6)
#define SF_TOGGLED    (1 << 7)
#define SF_NAMECHG    (1 << 8)
*/

bool CInArchive::PrintSectionBegin(const CSection &sect, unsigned index)
{
  AString name;
  if (sect.Flags & SF_BOLD)
    name += '!';
  AString s2;
  ReadString2(s2, sect.Name);
  if (!IsInstaller)
  {
    if (!StringsAreEqualNoCase_Ascii(s2, "uninstall"))
      name += "un.";
  }
  name += s2;
  
  if (sect.Flags & SF_SECGRPEND)
  {
    AddStringLF("SectionGroupEnd");
    return true;
  }

  if (sect.Flags & SF_SECGRP)
  {
    Script += "SectionGroup";
    if (sect.Flags & SF_EXPAND)
      Script += " /e";
    SpaceQuStr(name);
    Script += "    ; Section";
    AddParam_UInt(index);
    NewLine();
    return true;
  }

  Script += "Section";
  if ((sect.Flags & SF_SELECTED) == 0)
    Script += " /o";
  if (!name.IsEmpty())
    SpaceQuStr(name);
  
  /*
  if (!name.IsEmpty())
    Script += ' ';
  else
  */
  SmallSpaceComment();
  Script += "Section_";
  Add_UInt(index);

  /*
  Script += " ; flags = ";
  Add_Hex(Script, sect.Flags);
  */

  NewLine();

  if (sect.SizeKB != 0)
  {
    // probably we must show AddSize, only if there is additional size.
    Tab();
    AddCommentAndString("AddSize");
    AddParam_UInt(sect.SizeKB);
    AddLF();
  }

  bool needSectionIn =
      (sect.Name != 0 && sect.InstallTypes != 0) ||
      (sect.Name == 0 && sect.InstallTypes != 0xFFFFFFFF);
  if (needSectionIn || (sect.Flags & SF_RO) != 0)
  {
    TabString("SectionIn");
    UInt32 instTypes = sect.InstallTypes;
    for (int i = 0; i < 32; i++, instTypes >>= 1)
      if ((instTypes & 1) != 0)
      {
        AddParam_UInt(i + 1);
      }
    if ((sect.Flags & SF_RO) != 0)
      Script += " RO";
    AddLF();
  }
  return false;
}

void CInArchive::PrintSectionEnd()
{
  AddStringLF("SectionEnd");
  AddLF();
}

// static const unsigned kOnFuncShift = 4;

void CInArchive::ClearLangComment()
{
  langStrIDs.Clear();
}

void CInArchive::PrintNumComment(const char *name, UInt32 value)
{
  // size_t len = Script.Len();
  AddCommentAndString(name);
  Script += ": ";
  Add_UInt(value);
  AddLF();
  /*
  len = Script.Len() - len;
  char sz[16];
  ConvertUInt32ToString(value, sz);
  len += MyStringLen(sz);
  for (; len < 20; len++)
    Space();
  AddStringLF(sz);
  */
}


void CInArchive::NewLine()
{
  if (!langStrIDs.IsEmpty())
  {
    BigSpaceComment();
    for (unsigned i = 0; i < langStrIDs.Size() && i < 20; i++)
    {
      /*
      if (i != 0)
        Script += ' ';
      */
      UInt32 langStr = langStrIDs[i];
      if (langStr >= _numLangStrings)
      {
        AddError("langStr");
        break;
      }
      UInt32 param = Get32(_mainLang + langStr * 4);
      if (param != 0)
        AddParam(param);
    }
    ClearLangComment();
  }
  AddLF();
}

static const UInt32 kPageSize = 16 * 4;

static const char * const k_SetOverwrite_Modes[] =
{
    "on"
  , "off"
  , "try"
  , "ifnewer"
  , "ifdiff"
  // "lastused"
};


void CInArchive::MessageBox_MB_Part(UInt32 param)
{
  {
    UInt32 v = param & 0xF;
    Script += " MB_";
    if (v < ARRAY_SIZE(k_MB_Buttons))
      Script += k_MB_Buttons[v];
    else
    {
      Script += "Buttons_";
      Add_UInt(v);
    }
  }
  {
    UInt32 icon = (param >> 4) & 0x7;
    if (icon != 0)
    {
      Script += "|MB_";
      if (icon < ARRAY_SIZE(k_MB_Icons) && k_MB_Icons[icon] != 0)
        Script += k_MB_Icons[icon];
      else
      {
        Script += "Icon_";
        Add_UInt(icon);
      }
    }
  }
  if ((param & 0x80) != 0)
    Script += "|MB_USERICON";
  {
    UInt32 defButton = (param >> 8) & 0xF;
    if (defButton != 0)
    {
      Script += "|MB_DEFBUTTON";
      Add_UInt(defButton + 1);
    }
  }
  {
    UInt32 modal = (param >> 12) & 0x3;
    if (modal == 1) Script += "|MB_SYSTEMMODAL";
    else if (modal == 2) Script += "|MB_TASKMODAL";
    else if (modal == 3) Script += "|0x3000";
    UInt32 flags = (param >> 14);
    for (unsigned i = 0; i < ARRAY_SIZE(k_MB_Flags); i++)
      if ((flags & (1 << i)) != 0)
      {
        Script += "|MB_";
        Script += k_MB_Flags[i];
      }
  }
}

#define GET_CMD_PARAM(ppp, index) Get32((ppp) + 4 + (index) * 4)

static const Byte k_InitPluginDir_Commands[] =
  { 13, 26, 31, 13, 19, 21, 11, 14, 25, 31, 1, 22, 4, 1 };

bool CInArchive::CompareCommands(const Byte *rawCmds, const Byte *sequence, size_t numCommands)
{
  for (UInt32 kkk = 0; kkk < numCommands; kkk++, rawCmds += kCmdSize)
    if (GetCmd(Get32(rawCmds)) != sequence[kkk])
      return false;
  return true;
}


static const UInt32 kSectionSize_base = 6 * 4;
// static const UInt32 kSectionSize_8bit = kSectionSize_base + 1024;
// static const UInt32 kSectionSize_16bit = kSectionSize_base + 1024 * 2;
// static const UInt32 kSectionSize_16bit_Big = kSectionSize_base + 8196 * 2;
// 8196 is default string length in NSIS-Unicode since 2.37.3

#endif


static void AddString(AString &dest, const char *src)
{
  dest.Add_Space_if_NotEmpty();
  dest += src;
}

AString CInArchive::GetFormatDescription() const
{
  AString s ("NSIS-");
  char c;
  if (IsPark())
  {
    s += "Park-";
    c = '1';
         if (NsisType == k_NsisType_Park2) c = '2';
    else if (NsisType == k_NsisType_Park3) c = '3';
  }
  else
  {
    c = '2';
    if (NsisType == k_NsisType_Nsis3)
      c = '3';
  }
  s += c;
  if (IsNsis200)
    s += ".00";
  else if (IsNsis225)
    s += ".25";

  if (IsUnicode)
    AddString(s, "Unicode");
  
  if (Is64Bit)
    AddString(s, "64-bit");

  if (LogCmdIsEnabled)
    AddString(s, "log");

  if (BadCmd >= 0)
  {
    AddString(s, "BadCmd=");
    UIntToString(s, BadCmd);
  }
  return s;
}

#ifdef NSIS_SCRIPT

static const unsigned kNumAdditionalParkCmds = 3;

unsigned CInArchive::GetNumSupportedCommands() const
{
  unsigned numCmds = IsPark() ? (unsigned)kNumCmds : (unsigned)(kNumCmds) - kNumAdditionalParkCmds;
  if (!LogCmdIsEnabled)
    numCmds--;
  if (!IsUnicode)
    numCmds -= 2;
  return numCmds;
}

#endif

UInt32 CInArchive::GetCmd(UInt32 a)
{
  if (!IsPark())
  {
    if (!LogCmdIsEnabled)
      return a;
    if (a < EW_SECTIONSET)
      return a;
    if (a == EW_SECTIONSET)
      return EW_LOG;
    return a - 1;
  }

  if (a < EW_REGISTERDLL)
    return a;
  if (NsisType >= k_NsisType_Park2)
  {
    if (a == EW_REGISTERDLL) return EW_GETFONTVERSION;
    a--;
  }
  if (NsisType >= k_NsisType_Park3)
  {
    if (a == EW_REGISTERDLL) return EW_GETFONTNAME;
    a--;
  }
  if (a >= EW_FSEEK)
  {
    if (IsUnicode)
    {
      if (a == EW_FSEEK) return EW_FPUTWS;
      if (a == EW_FSEEK + 1) return EW_FPUTWS + 1;
      a -= 2;
    }
    
    if (a >= EW_SECTIONSET && LogCmdIsEnabled)
    {
      if (a == EW_SECTIONSET)
        return EW_LOG;
      return a - 1;
    }
    if (a == EW_FPUTWS)
      return EW_FINDPROC;
    // if (a > EW_FPUTWS) return 0;
  }
  return a;
}

void CInArchive::FindBadCmd(const CBlockHeader &bh, const Byte *p)
{
  BadCmd = -1;
  
  for (UInt32 kkk = 0; kkk < bh.Num; kkk++, p += kCmdSize)
  {
    UInt32 id = GetCmd(Get32(p));
    if (id >= kNumCmds)
      continue;
    if (BadCmd >= 0 && id >= (unsigned)BadCmd)
      continue;
    unsigned i;
    if (id == EW_GETLABELADDR ||
        id == EW_GETFUNCTIONADDR)
    {
      BadCmd = id;
      continue;
    }
    for (i = 6; i != 0; i--)
    {
      UInt32 param = Get32(p + i * 4);
      if (param != 0)
        break;
    }
    if (id == EW_FINDPROC && i == 0)
    {
      BadCmd = id;
      continue;
    }
    if (k_Commands[id].NumParams < i)
      BadCmd = id;
  }
}

/* We calculate the number of parameters in commands to detect
   layout of commands. It's not very good way.
   If you know simpler and more robust way to detect Version and layout,
   please write to 7-Zip forum */

void CInArchive::DetectNsisType(const CBlockHeader &bh, const Byte *p)
{
  bool strongPark = false;
  bool strongNsis = false;

  if (NumStringChars > 2)
  {
    const Byte *strData = _data + _stringsPos;
    if (IsUnicode)
    {
      UInt32 num = NumStringChars - 2;
      for (UInt32 i = 0; i < num; i++)
      {
        if (Get16(strData + i * 2) == 0)
        {
          unsigned c2 = Get16(strData + 2 + i * 2);
          // it can be TXT/RTF with marker char (1 or 2). so we must check next char
          // if (c2 <= NS_3_CODE_SKIP && c2 != NS_3_CODE_SHELL)
          if (c2 == NS_3_CODE_VAR)
          {
            // 18.06: fixed: is it correct ?
            // if ((Get16(strData + 3 + i * 2) & 0x8000) != 0)
            if ((Get16(strData + 4 + i * 2) & 0x8080) == 0x8080)
            {
              NsisType = k_NsisType_Nsis3;
              strongNsis = true;
              break;
            }
          }
        }
      }
      if (!strongNsis)
      {
        NsisType = k_NsisType_Park1;
        strongPark = true;
      }
    }
    else
    {
      UInt32 num = NumStringChars - 2;
      for (UInt32 i = 0; i < num; i++)
      {
        if (strData[i] == 0)
        {
          Byte c2 = strData[i + 1];
          // it can be TXT/RTF with marker char (1 or 2). so we must check next char
          // for marker=1 (txt)
          if (c2 == NS_3_CODE_VAR)
            // if (c2 <= NS_3_CODE_SKIP && c2 != NS_3_CODE_SHELL && c2 != 1)
          {
            if ((strData[i + 2] & 0x80) != 0)
            {
              // const char *p2 = (const char *)(strData + i + 1);
              // p2 = p2;
              NsisType = k_NsisType_Nsis3;
              strongNsis = true;
              break;
            }
          }
        }
      }
    }
  }

  if (NsisType == k_NsisType_Nsis2 && !IsUnicode)
  {
    const Byte *p2 = p;

    for (UInt32 kkk = 0; kkk < bh.Num; kkk++, p2 += kCmdSize)
    {
      UInt32 cmd = GetCmd(Get32(p2));
      if (cmd != EW_GETDLGITEM &&
          cmd != EW_ASSIGNVAR)
        continue;
      
      UInt32 params[kNumCommandParams];
      for (unsigned i = 0; i < kNumCommandParams; i++)
        params[i] = Get32(p2 + 4 + 4 * i);

      if (cmd == EW_GETDLGITEM)
      {
        // we can use also EW_SETCTLCOLORS
        if (IsVarStr(params[1], kVar_HWNDPARENT_225))
        {
          IsNsis225 = true;
          if (params[0] == kVar_Spec_OUTDIR_225)
          {
            IsNsis200 = true;
            break;
          }
        }
      }
      else // if (cmd == EW_ASSIGNVAR)
      {
        if (params[0] == kVar_Spec_OUTDIR_225 &&
            params[2] == 0 &&
            params[3] == 0 &&
            IsVarStr(params[1], kVar_OUTDIR))
          IsNsis225 = true;
      }
    }
  }

  bool parkVer_WasDetected = false;

  if (!strongNsis && !IsNsis225 && !IsNsis200)
  {
    // it must be before FindBadCmd(bh, p);
    unsigned mask = 0;

    unsigned numInsertMax = IsUnicode ? 4 : 2;

    const Byte *p2 = p;
      
    for (UInt32 kkk = 0; kkk < bh.Num; kkk++, p2 += kCmdSize)
    {
      UInt32 cmd = Get32(p2); // we use original (not converted) command

      if (cmd < EW_WRITEUNINSTALLER ||
          cmd > EW_WRITEUNINSTALLER + numInsertMax)
        continue;

      UInt32 params[kNumCommandParams];
      for (unsigned i = 0; i < kNumCommandParams; i++)
        params[i] = Get32(p2 + 4 + 4 * i);

      if (params[4] != 0 ||
          params[5] != 0 ||
          params[0] <= 1 ||
          params[3] <= 1)
        continue;

      UInt32 altParam = params[3];
      if (!IsGoodString(params[0]) ||
          !IsGoodString(altParam))
        continue;

      UInt32 additional = 0;
      if (GetVarIndexFinished(altParam, '\\', additional) != kVar_INSTDIR)
        continue;
      if (AreTwoParamStringsEqual(altParam + additional, params[0]))
      {
        unsigned numInserts = cmd - EW_WRITEUNINSTALLER;
        mask |= (1 << numInserts);
      }
    }

    if (mask == 1)
    {
      parkVer_WasDetected = true; // it can be original NSIS or Park-1
    }
    else if (mask != 0)
    {
      ENsisType newType = NsisType;
      if (IsUnicode)
        switch (mask)
        {
          case (1 << 3): newType = k_NsisType_Park2; break;
          case (1 << 4): newType = k_NsisType_Park3; break;
        }
      else
        switch (mask)
        {
          case (1 << 1): newType = k_NsisType_Park2; break;
          case (1 << 2): newType = k_NsisType_Park3; break;
        }
      if (newType != NsisType)
      {
        parkVer_WasDetected = true;
        NsisType = newType;
      }
    }
  }

  FindBadCmd(bh, p);

  /*
  if (strongNsis)
    return;
  */

  if (BadCmd < EW_REGISTERDLL)
    return;

  /*
  // in ANSI archive we don't check Park and log version
  if (!IsUnicode)
    return;
  */
  
  // We can support Park-ANSI archives, if we remove if (strongPark) check
  if (strongPark && !parkVer_WasDetected)
  {
    if (BadCmd < EW_SECTIONSET)
    {
      NsisType = k_NsisType_Park3;
      LogCmdIsEnabled = true; // version 3 is provided with log enabled
      FindBadCmd(bh, p);
      if (BadCmd > 0 && BadCmd < EW_SECTIONSET)
      {
        NsisType = k_NsisType_Park2;
        LogCmdIsEnabled = false;
        FindBadCmd(bh, p);
        if (BadCmd > 0 && BadCmd < EW_SECTIONSET)
        {
          NsisType = k_NsisType_Park1;
          FindBadCmd(bh, p);
        }
      }
    }
  }

  if (BadCmd >= EW_SECTIONSET)
  {
    LogCmdIsEnabled = !LogCmdIsEnabled;
    FindBadCmd(bh, p);
    if (BadCmd >= EW_SECTIONSET && LogCmdIsEnabled)
    {
      LogCmdIsEnabled = false;
      FindBadCmd(bh, p);
    }
  }
}

Int32 CInArchive::GetVarIndex(UInt32 strPos) const
{
  if (strPos >= NumStringChars)
    return -1;
  
  if (IsUnicode)
  {
    if (NumStringChars - strPos < 3 * 2)
      return -1;
    const Byte *p = _data + _stringsPos + strPos * 2;
    unsigned code = Get16(p);
    if (IsPark())
    {
      if (code != PARK_CODE_VAR)
        return -1;
      UInt32 n = Get16(p + 2);
      if (n == 0)
        return -1;
      CONVERT_NUMBER_PARK(n);
      return (Int32)n;
    }
    
    // NSIS-3
    {
      if (code != NS_3_CODE_VAR)
        return -1;
      UInt32 n = Get16(p + 2);
      if (n == 0)
        return -1;
      CONVERT_NUMBER_NS_3_UNICODE(n);
      return (Int32)n;
    }
  }
  
  if (NumStringChars - strPos < 4)
    return -1;
  
  const Byte *p = _data + _stringsPos + strPos;
  unsigned c = *p;
  if (NsisType == k_NsisType_Nsis3)
  {
    if (c != NS_3_CODE_VAR)
      return -1;
  }
  else if (c != NS_CODE_VAR)
    return -1;

  unsigned c0 = p[1];
  if (c0 == 0)
    return -1;
  unsigned c1 = p[2];
  if (c1 == 0)
    return -1;
  return DECODE_NUMBER_FROM_2_CHARS(c0, c1);
}

Int32 CInArchive::GetVarIndex(UInt32 strPos, UInt32 &resOffset) const
{
  resOffset = 0;
  Int32 varIndex = GetVarIndex(strPos);
  if (varIndex < 0)
    return varIndex;
  if (IsUnicode)
  {
    if (NumStringChars - strPos < 2 * 2)
      return -1;
    resOffset = 2;
  }
  else
  {
    if (NumStringChars - strPos < 3)
      return -1;
    resOffset = 3;
  }
  return varIndex;
}

Int32 CInArchive::GetVarIndexFinished(UInt32 strPos, Byte endChar, UInt32 &resOffset) const
{
  resOffset = 0;
  Int32 varIndex = GetVarIndex(strPos);
  if (varIndex < 0)
    return varIndex;
  if (IsUnicode)
  {
    if (NumStringChars - strPos < 3 * 2)
      return -1;
    const Byte *p = _data + _stringsPos + strPos * 2;
    if (Get16(p + 4) != endChar)
      return -1;
    resOffset = 3;
  }
  else
  {
    if (NumStringChars - strPos < 4)
      return -1;
    const Byte *p = _data + _stringsPos + strPos;
    if (p[3] != endChar)
      return -1;
    resOffset = 4;
  }
  return varIndex;
}

bool CInArchive::IsVarStr(UInt32 strPos, UInt32 varIndex) const
{
  if (varIndex > (UInt32)0x7FFF)
    return false;
  UInt32 resOffset;
  return GetVarIndexFinished(strPos, 0, resOffset) == (Int32)varIndex;
}

bool CInArchive::IsAbsolutePathVar(UInt32 strPos) const
{
  Int32 varIndex = GetVarIndex(strPos);
  if (varIndex < 0)
    return false;
  switch (varIndex)
  {
    case kVar_INSTDIR:
    case kVar_EXEDIR:
    case kVar_TEMP:
    case kVar_PLUGINSDIR:
      return true;
  }
  return false;
}

#define IS_LETTER_CHAR(c) (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))

// We use same check as in NSIS decoder
static bool IsDrivePath(const wchar_t *s) { return IS_LETTER_CHAR(s[0]) && s[1] == ':' /* && s[2] == '\\' */ ; }
static bool IsDrivePath(const char *s)    { return IS_LETTER_CHAR(s[0]) && s[1] == ':' /* && s[2] == '\\' */ ; }

static bool IsAbsolutePath(const wchar_t *s)
{
  return (s[0] == WCHAR_PATH_SEPARATOR && s[1] == WCHAR_PATH_SEPARATOR) || IsDrivePath(s);
}

static bool IsAbsolutePath(const char *s)
{
  return (s[0] == CHAR_PATH_SEPARATOR && s[1] == CHAR_PATH_SEPARATOR) || IsDrivePath(s);
}

void CInArchive::SetItemName(CItem &item, UInt32 strPos)
{
  ReadString2_Raw(strPos);
  bool isAbs = IsAbsolutePathVar(strPos);
  if (IsUnicode)
  {
    item.NameU = Raw_UString;
    if (!isAbs && !IsAbsolutePath(Raw_UString))
      item.Prefix = UPrefixes.Size() - 1;
  }
  else
  {
    item.NameA = Raw_AString;
    if (!isAbs && !IsAbsolutePath(Raw_AString))
      item.Prefix = APrefixes.Size() - 1;
  }
}

HRESULT CInArchive::ReadEntries(const CBlockHeader &bh)
{
  #ifdef NSIS_SCRIPT
  CDynLimBuf &s = Script;

  CObjArray<UInt32> labels;
  labels.Alloc(bh.Num);
  memset(labels, 0, bh.Num * sizeof(UInt32));

  {
    const Byte *p = _data;
    UInt32 i;
    for (i = 0; i < numOnFunc; i++)
    {
      UInt32 func = Get32(p + onFuncOffset + 4 * i);
      if (func < bh.Num)
        labels[func] = (labels[func] & ~CMD_REF_OnFunc_Mask) | (CMD_REF_OnFunc | (i << CMD_REF_OnFunc_NumShifts));
    }
  }

  /*
  {
    for (int i = 0; i < OnFuncs.Size(); i++)
    {
      UInt32 address = OnFuncs[i] >> kOnFuncShift;
      if (address < bh.Num)
    }
  }
  */

  if (bhPages.Num != 0)
  {
    Separator();
    PrintNumComment("PAGES", bhPages.Num);

    if (bhPages.Num > (1 << 12)
        || bhPages.Offset > _size
        || bhPages.Num * kPageSize > _size - bhPages.Offset)
    {
      AddErrorLF("Pages error");
    }
    else
    {

    AddLF();
    const Byte *p = _data + bhPages.Offset;
    
    for (UInt32 pageIndex = 0; pageIndex < bhPages.Num; pageIndex++, p += kPageSize)
    {
      UInt32 dlgID = Get32(p);
      UInt32 wndProcID = Get32(p + 4);
      UInt32 preFunc = Get32(p + 8);
      UInt32 showFunc = Get32(p + 12);
      UInt32 leaveFunc = Get32(p + 16);
      UInt32 flags = Get32(p + 20);
      UInt32 caption = Get32(p + 24);
      // UInt32 back = Get32(p + 28);
      UInt32 next = Get32(p + 32);
      // UInt32 clickNext = Get32(p + 36);
      // UInt32 cancel = Get32(p + 40);
      UInt32 params[5];
      for (int i = 0; i < 5; i++)
        params[i] = Get32(p + 44 + 4 * i);

      SET_FUNC_REF(preFunc, CMD_REF_Pre);
      SET_FUNC_REF(showFunc, CMD_REF_Show);
      SET_FUNC_REF(leaveFunc, CMD_REF_Leave);

      if (wndProcID == PWP_COMPLETED)
        CommentOpen();

      AddCommentAndString("Page ");
      Add_UInt(pageIndex);
      AddLF();

      if (flags & PF_PAGE_EX)
      {
        s += "PageEx ";
        if (!IsInstaller)
          s += "un.";
      }
      else
        s += IsInstaller ? "Page " : "UninstPage ";

      if (wndProcID < ARRAY_SIZE(kPageTypes))
        s += kPageTypes[wndProcID];
      else
        Add_UInt(wndProcID);


      bool needCallbacks = (
          (Int32)preFunc >= 0 ||
          (Int32)showFunc >= 0 ||
          (Int32)leaveFunc >= 0);

      if (flags & PF_PAGE_EX)
      {
        AddLF();
        if (needCallbacks)
          TabString("PageCallbacks");
      }

      if (needCallbacks)
      {
        AddParam_Func(labels, preFunc); // it's creator_function for PWP_CUSTOM
        if (wndProcID != PWP_CUSTOM)
        {
          AddParam_Func(labels, showFunc);
        }
        AddParam_Func(labels, leaveFunc);
      }

      if ((flags & PF_PAGE_EX) == 0)
      {
        // AddOptionalParam(caption);
        if (flags & PF_CANCEL_ENABLE)
          s += " /ENABLECANCEL";
        AddLF();
      }
      else
      {
        AddLF();
        AddPageOption1(caption, "Caption");
      }

        if (wndProcID == PWP_LICENSE)
        {
          if ((flags & PF_LICENSE_NO_FORCE_SELECTION) != 0 ||
              (flags & PF_LICENSE_FORCE_SELECTION) != 0)
          {
            TabString("LicenseForceSelection ");
            if (flags & PF_LICENSE_NO_FORCE_SELECTION)
              s += "off";
            else
            {
              if (dlgID == IDD_LICENSE_FSCB)
                s += "checkbox";
              else if (dlgID == IDD_LICENSE_FSRB)
                s += "radiobuttons";
              else
                Add_UInt(dlgID);
              AddOptionalParams(params + 2, 2);
            }
            NewLine();
          }

          if (params[0] != 0 || next != 0)
          {
            TabString("LicenseText");
            AddParam(params[0]);
            AddOptionalParam(next);
            NewLine();
          }
          if (params[1] != 0)
          {
            TabString("LicenseData");
            if ((Int32)params[1] < 0)
              AddParam(params[1]);
            else
              AddLicense(params[1], -1);
            ClearLangComment();
            NewLine();
          }
        }
        else if (wndProcID == PWP_SELCOM)
          AddPageOption(params, 3, "ComponentsText");
        else if (wndProcID == PWP_DIR)
        {
          AddPageOption(params, 4, "DirText");
          if (params[4] != 0)
          {
            TabString("DirVar");
            AddParam_Var(params[4] - 1);
            AddLF();
          }
          if (flags & PF_DIR_NO_BTN_DISABLE)
          {
            TabString("DirVerify leave");
            AddLF();
          }

        }
        else if (wndProcID == PWP_INSTFILES)
        {
          AddPageOption1(params[2], "CompletedText");
          AddPageOption1(params[1], "DetailsButtonText");
        }
        else if (wndProcID == PWP_UNINST)
        {
          if (params[4] != 0)
          {
            TabString("DirVar");
            AddParam_Var(params[4] - 1);
            AddLF();
          }
          AddPageOption(params, 2, "UninstallText");
        }

      if (flags & PF_PAGE_EX)
      {
        s += "PageExEnd";
        NewLine();
      }
      if (wndProcID == PWP_COMPLETED)
        CommentClose();
      NewLine();
    }
    }
  }

  CObjArray<CSection> Sections;

  {
    Separator();
    PrintNumComment("SECTIONS", bhSections.Num);
    PrintNumComment("COMMANDS", bh.Num);
    AddLF();

    if (bhSections.Num > (1 << 15)
        // || bhSections.Offset > _size
        // || (bhSections.Num * SectionSize > _size - bhSections.Offset)
      )
    {
      AddErrorLF("Sections error");
    }
    else if (bhSections.Num != 0)
    {
      Sections.Alloc((unsigned)bhSections.Num);
      const Byte *p = _data + bhSections.Offset;
      for (UInt32 i = 0; i < bhSections.Num; i++, p += SectionSize)
      {
        CSection &section = Sections[i];
        section.Parse(p);
        if (section.StartCmdIndex < bh.Num)
          labels[section.StartCmdIndex] |= CMD_REF_Section;
      }
    }
  }

  #endif

  const Byte *p;
  UInt32 kkk;

  #ifdef NSIS_SCRIPT

  p = _data + bh.Offset;

  for (kkk = 0; kkk < bh.Num; kkk++, p += kCmdSize)
  {
    UInt32 commandId = GetCmd(Get32(p));
    UInt32 mask;
    switch (commandId)
    {
      case EW_NOP:          mask = 1 << 0; break;
      case EW_IFFILEEXISTS: mask = 3 << 1; break;
      case EW_IFFLAG:       mask = 3 << 0; break;
      case EW_MESSAGEBOX:   mask = 5 << 3; break;
      case EW_STRCMP:       mask = 3 << 2; break;
      case EW_INTCMP:       mask = 7 << 2; break;
      case EW_ISWINDOW:     mask = 3 << 1; break;
      case EW_CALL:
      {
        if (Get32(p + 4 + 4) == 1) // it's Call :Label
        {
          mask = 1 << 0;
          break;
        }
        UInt32 param0 = Get32(p + 4);
        if ((Int32)param0 > 0)
          labels[param0 - 1] |= CMD_REF_Call;
        continue;
      }
      default: continue;
    }
    for (unsigned i = 0; mask != 0; i++, mask >>= 1)
      if (mask & 1)
      {
        UInt32 param = Get32(p + 4 + 4 * i);
        if ((Int32)param > 0 && (Int32)param <= (Int32)bh.Num)
          labels[param - 1] |= CMD_REF_Goto;
      }
  }

  int InitPluginsDir_Start = -1;
  int InitPluginsDir_End = -1;
  p = _data + bh.Offset;
  for (kkk = 0; kkk < bh.Num; kkk++, p += kCmdSize)
  {
    UInt32 flg = labels[kkk];
    /*
    if (IsFunc(flg))
    {
      AddLF();
      for (int i = 0; i < 14; i++)
      {
        UInt32 commandId = GetCmd(Get32(p + kCmdSize * i));
        s += ", ";
        UIntToString(s, commandId);
      }
      AddLF();
    }
    */
    if (IsFunc(flg)
        && bh.Num - kkk >= ARRAY_SIZE(k_InitPluginDir_Commands)
        && CompareCommands(p, k_InitPluginDir_Commands, ARRAY_SIZE(k_InitPluginDir_Commands)))
    {
      InitPluginsDir_Start = kkk;
      InitPluginsDir_End = kkk + ARRAY_SIZE(k_InitPluginDir_Commands);
      labels[kkk] |= CMD_REF_InitPluginDir;
      break;
    }
  }

  #endif

  // AString prefixA_Temp;
  // UString prefixU_Temp;


  // const UInt32 kFindS = 158;

  #ifdef NSIS_SCRIPT

  UInt32 curSectionIndex = 0;
  // UInt32 lastSectionEndCmd = 0xFFFFFFFF;
  bool sectionIsOpen = false;
  // int curOnFunc = 0;
  bool onFuncIsOpen = false;

  /*
  for (unsigned yyy = 0; yyy + 3 < _data.Size(); yyy++)
  {
    UInt32 val = Get32(_data + yyy);
    if (val == kFindS)
      val = val;
  }
  */

  UInt32 overwrite_State = 0; // "SetOverwrite on"
  Int32 allowSkipFiles_State = -1; // -1: on, -2: off, >=0 : RAW value
  UInt32 endCommentIndex = 0;

  unsigned numSupportedCommands = GetNumSupportedCommands();

  #endif

  p = _data + bh.Offset;
  
  UString spec_outdir_U;
  AString spec_outdir_A;

  UPrefixes.Add(UString("$INSTDIR"));
  APrefixes.Add(AString("$INSTDIR"));

  p = _data + bh.Offset;

  unsigned spec_outdir_VarIndex = IsNsis225 ?
      kVar_Spec_OUTDIR_225 :
      kVar_Spec_OUTDIR;

  for (kkk = 0; kkk < bh.Num; kkk++, p += kCmdSize)
  {
    UInt32 commandId;
    UInt32 params[kNumCommandParams];
    commandId = GetCmd(Get32(p));
    {
      for (unsigned i = 0; i < kNumCommandParams; i++)
      {
        params[i] = Get32(p + 4 + 4 * i);
        /*
        if (params[i] == kFindS)
          i = i;
        */
      }
    }

    #ifdef NSIS_SCRIPT

    bool IsSectionGroup = false;
    while (curSectionIndex < bhSections.Num)
    {
      const CSection &sect = Sections[curSectionIndex];
      if (sectionIsOpen)
      {
        if (sect.StartCmdIndex + sect.NumCommands + 1 != kkk)
          break;
        PrintSectionEnd();
        sectionIsOpen = false;
        // lastSectionEndCmd = kkk;
        curSectionIndex++;
        continue;
      }
      if (sect.StartCmdIndex != kkk)
        break;
      if (PrintSectionBegin(sect, curSectionIndex))
      {
        IsSectionGroup = true;
        curSectionIndex++;
        // do we need to flush prefixes in new section?
        // FlushOutPathPrefixes();
      }
      else
        sectionIsOpen = true;
    }

    /*
    if (curOnFunc < OnFuncs.Size())
    {
      if ((OnFuncs[curOnFunc] >> kOnFuncShift) == kkk)
      {
        s += "Function .on";
        s += kOnFunc[OnFuncs[curOnFunc++] & ((1 << kOnFuncShift) - 1)];
        AddLF();
        onFuncIsOpen = true;
      }
    }
    */

    if (labels[kkk] != 0 && labels[kkk] != CMD_REF_Section)
    {
      UInt32 flg = labels[kkk];
      if (IsFunc(flg))
      {
        if ((int)kkk == InitPluginsDir_Start)
          CommentOpen();

        onFuncIsOpen = true;
        s += "Function ";
        Add_FuncName(labels, kkk);
        if (IsPageFunc(flg))
        {
          BigSpaceComment();
          s += "Page ";
          Add_UInt((flg & CMD_REF_Page_Mask) >> CMD_REF_Page_NumShifts);
          // if (flg & CMD_REF_Creator) s += ", Creator";
          if (flg & CMD_REF_Leave) s += ", Leave";
          if (flg & CMD_REF_Pre) s += ", Pre";
          if (flg & CMD_REF_Show) s += ", Show";
        }
        AddLF();
      }
      if (flg & CMD_REF_Goto)
      {
        Add_LabelName(kkk);
        s += ':';
        AddLF();
      }
    }

    if (commandId != EW_RET)
    {
      Tab(kkk < endCommentIndex);
    }
      
    /*
    UInt32 originalCmd = Get32(p);
    if (originalCmd >= EW_REGISTERDLL)
    {
      UIntToString(s, originalCmd);
      s += ' ';
      if (originalCmd != commandId)
      {
        UIntToString(s, commandId);
        s += ' ';
      }
    }
    */

    unsigned numSkipParams = 0;

    if (commandId < ARRAY_SIZE(k_Commands) && commandId < numSupportedCommands)
    {
      numSkipParams = k_Commands[commandId].NumParams;
      const char *sz = k_CommandNames[commandId];
      if (sz)
        s += sz;
    }
    else
    {
      s += "Command";
      Add_UInt(commandId);
      /* We don't show wrong commands that use overlapped ids.
         So we change commandId to big value */
      if (commandId < (1 << 12))
        commandId += (1 << 12);
    }

    #endif

    switch (commandId)
    {
      case EW_CREATEDIR:
      {
        bool isSetOutPath = (params[1] != 0);

        if (isSetOutPath)
        {
          UInt32 par0 = params[0];
          
          UInt32 resOffset;
          Int32 idx = GetVarIndex(par0, resOffset);
          if (idx == (Int32)spec_outdir_VarIndex ||
              idx == kVar_OUTDIR)
            par0 += resOffset;

          ReadString2_Raw(par0);

          if (IsUnicode)
          {
            if (idx == (Int32)spec_outdir_VarIndex)
              Raw_UString.Insert(0, spec_outdir_U);
            else if (idx == kVar_OUTDIR)
              Raw_UString.Insert(0, UPrefixes.Back());
            UPrefixes.Add(Raw_UString);
          }
          else
          {
            if (idx == (Int32)spec_outdir_VarIndex)
              Raw_AString.Insert(0, spec_outdir_A);
            else if (idx == kVar_OUTDIR)
              Raw_AString.Insert(0, APrefixes.Back());
            APrefixes.Add(Raw_AString);
          }
        }
        
        #ifdef NSIS_SCRIPT
        s += isSetOutPath ? "SetOutPath" : "CreateDirectory";
        AddParam(params[0]);
        if (params[2] != 0)
        {
          SmallSpaceComment();
          s += "CreateRestrictedDirectory";
        }
        #endif
        
        break;
      }


      case EW_ASSIGNVAR:
      {
        if (params[0] == spec_outdir_VarIndex)
        {
          spec_outdir_U.Empty();
          spec_outdir_A.Empty();
          if (IsVarStr(params[1], kVar_OUTDIR) &&
              params[2] == 0 &&
              params[3] == 0)
          {
            spec_outdir_U = UPrefixes.Back(); // outdir_U;
            spec_outdir_A = APrefixes.Back(); // outdir_A;
          }
        }

        #ifdef NSIS_SCRIPT
        
        if (params[2] == 0 &&
            params[3] == 0 &&
            params[4] == 0 &&
            params[5] == 0 &&
            params[1] != 0 &&
            params[1] < NumStringChars)
        {
          char sz[16];
          ConvertUInt32ToString(kkk + 1, sz);
          if (IsDirectString_Equal(params[1], sz))
          {
            // we suppose that it's GetCurrentAddress command
            // but there is probability that it's StrCpy command
            s += "GetCurrentAddress";
            AddParam_Var(params[0]);
            SmallSpaceComment();
          }
        }
        s += "StrCpy";
        AddParam_Var(params[0]);
        AddParam(params[1]);

        AddOptionalParams(params + 2, 2);

        #endif

        break;
      }

      case EW_EXTRACTFILE:
      {
        CItem &item = Items.AddNew();

        UInt32 par1 = params[1];

        SetItemName(item, par1);
          
        item.Pos = params[2];
        item.MTime.dwLowDateTime = params[3];
        item.MTime.dwHighDateTime = params[4];
        
        #ifdef NSIS_SCRIPT
        
        {
          UInt32 overwrite = params[0] & 0x7;
          if (overwrite != overwrite_State)
          {
            s += "SetOverwrite ";
            ADD_TYPE_FROM_LIST(k_SetOverwrite_Modes, overwrite);
            overwrite_State = overwrite;
            AddLF();
            Tab(kkk < endCommentIndex);
          }
        }

        {
          UInt32 nsisMB = params[0] >> 3;
          if ((Int32)nsisMB != allowSkipFiles_State)
          {
            UInt32 mb = nsisMB & ((1 << 20) - 1);  // old/new NSIS
            UInt32 b1 = nsisMB >> 21;  // NSIS 2.06+
            UInt32 b2 = nsisMB >> 20;  // NSIS old
            Int32 asf = (Int32)nsisMB;
            if (mb == (MY__MB_ABORTRETRYIGNORE | MY__MB_ICONSTOP) && (b1 == MY__IDIGNORE || b2 == MY__IDIGNORE))
              asf = -1;
            else if (mb == (MY__MB_RETRYCANCEL | MY__MB_ICONSTOP) && (b1 == MY__IDCANCEL || b2 == MY__IDCANCEL))
              asf = -2;
            else
            {
              AddCommentAndString("AllowSkipFiles [Overwrite]: ");
              MessageBox_MB_Part(mb);
              if (b1 != 0)
              {
                s += " /SD";
                Add_ButtonID(b1);
              }
            }
            if (asf != allowSkipFiles_State)
            {
              if (asf < 0)
              {
                s += "AllowSkipFiles ";
                s += (asf == -1) ? "on" : "off";
              }
              AddLF();
              Tab(kkk < endCommentIndex);
            }
            allowSkipFiles_State = (Int32)nsisMB;
          }
        }
          
        s += "File";
        AddParam(params[1]);

        /* params[5] contains link to LangString (negative value)
           with NLF_FILE_ERROR or NLF_FILE_ERROR_NOIGNORE message for MessageBox.
           We don't need to print it. */
        
        #endif
        
        if (IsVarStr(par1, 10)) // is $R0
        {
          // we parse InstallLib macro in 7-Zip installers
          unsigned kBackOffset = 28;
          if (kkk > 1)
          {
            // detect old version of InstallLib macro
            if (Get32(p - 1 * kCmdSize) == EW_NOP) // goto command
              kBackOffset -= 2;
          }

          if (kkk > kBackOffset)
          {
            const Byte *p2 = p - kBackOffset * kCmdSize;
            UInt32 cmd = Get32(p2);
            if (cmd == EW_ASSIGNVAR)
            {
              UInt32 pars[6];
              for (int i = 0; i < 6; i++)
                pars[i] = Get32(p2 + i * 4 + 4);
              if (pars[0] == 10 + 4 && pars[2] == 0 && pars[3] == 0) // 10 + 4 means $R4
              {
                item.Prefix = -1;
                item.NameA.Empty();
                item.NameU.Empty();
                SetItemName(item, pars[1]);
                // maybe here we can restore original item name, if new name is empty
              }
            }
          }
        }
        /* UInt32 allowIgnore = params[5]; */
        break;
      }

      case EW_SETFILEATTRIBUTES:
      {
        if (kkk > 0 && Get32(p - kCmdSize) == EW_EXTRACTFILE)
        {
          if (params[0] == Get32(p - kCmdSize + 4 + 4 * 1)) // compare with PrevCmd.Params[1]
          {
            CItem &item = Items.Back();
            item.Attrib_Defined = true;
            item.Attrib = params[1];
          }
        }
        #ifdef NSIS_SCRIPT
        AddParam(params[0]);
        Space();
        FlagsToString2(s, g_WinAttrib, ARRAY_SIZE(g_WinAttrib), params[1]);
        #endif
        break;
      }

      case EW_WRITEUNINSTALLER:
      {
        /* NSIS 2.29+ writes alternative path to params[3]
             "$INSTDIR\\" + Str(params[0])
           NSIS installer uses alternative path, if main path
           from params[0] is not absolute path */

        bool pathOk = (params[0] > 0) && IsGoodString(params[0]);

        if (!pathOk)
        {
          #ifdef NSIS_SCRIPT
          AddError("bad path");
          #endif
          break;
        }

        bool altPathOk = true;

        UInt32 altParam = params[3];
        if (altParam != 0)
        {
          altPathOk = false;
          UInt32 additional = 0;
          if (GetVarIndexFinished(altParam, '\\', additional) == kVar_INSTDIR)
            altPathOk = AreTwoParamStringsEqual(altParam + additional, params[0]);
        }


        #ifdef NSIS_SCRIPT

        AddParam(params[0]);

        /*
        for (int i = 1; i < 3; i++)
          AddParam_UInt(params[i]);
        */

        if (params[3] != 0)
        {
          SmallSpaceComment();
          AddParam(params[3]);
        }
        
        #endif

        if (!altPathOk)
        {
          #ifdef NSIS_SCRIPT
          AddError("alt path error");
          #endif
        }

        if (BadCmd >= 0 && BadCmd <= EW_WRITEUNINSTALLER)
        {
          /* We don't cases with incorrect installer commands.
             Such bad installer item can break unpacking for other items. */
          #ifdef NSIS_SCRIPT
          AddError("SKIP possible BadCmd");
          #endif
          break;
        }

        CItem &item = Items.AddNew();;

        SetItemName(item, params[0]);

        item.Pos = params[1];
        item.PatchSize = params[2];
        item.IsUninstaller = true;
        
        /*
        // we can add second time to test the code
        CItem item2 = item;
        item2.NameU += L'2';
        item2.NameA += '2';
        Items.Add(item2);
        */

        break;
      }

      #ifdef NSIS_SCRIPT
      
      case EW_RET:
      {
        // bool needComment = false;
        if (onFuncIsOpen)
        {
          if (kkk == bh.Num - 1 || IsProbablyEndOfFunc(labels[kkk + 1]))
          {
            AddStringLF("FunctionEnd");

            if ((int)kkk + 1 == InitPluginsDir_End)
              CommentClose();
            AddLF();
            onFuncIsOpen = false;
            // needComment = true;
            break;
          }
        }
        // if (!needComment)
            if (IsSectionGroup)
              break;
          if (sectionIsOpen)
          {
            const CSection &sect = Sections[curSectionIndex];
            if (sect.StartCmdIndex + sect.NumCommands == kkk)
            {
              PrintSectionEnd();
              sectionIsOpen = false;
              curSectionIndex++;
              break;
            }

            // needComment = true;
            // break;
          }

        /*
        if (needComment)
          s += "  ;";
        */
        TabString("Return");
        AddLF();
        break;
      }

      case EW_NOP:
      {
        if (params[0] == 0)
          s += "Nop";
        else
        {
          s += "Goto";
          Add_GotoVar(params[0]);
        }
        break;
      }

      case EW_ABORT:
      {
        AddOptionalParam(params[0]);
        break;
      }

      case EW_CALL:
      {
        if (kkk + 1 < bh.Num && GetCmd(Get32(p + kCmdSize)) == EW_EXTRACTFILE)
        {
          UInt32 par1 = GET_CMD_PARAM(p + kCmdSize, 1);

          UInt32 pluginPar = 0;

          if (GetVarIndexFinished(par1, '\\', pluginPar) == kVar_PLUGINSDIR)
          {
            pluginPar += par1;
            UInt32 commandId2 = GetCmd(Get32(p + kCmdSize * 2));
            if (commandId2 == EW_SETFLAG || commandId2 == EW_UPDATETEXT)
            {
              UInt32 i;
              for (i = kkk + 3; i < bh.Num; i++)
              {
                const Byte *pCmd = p + kCmdSize * (i - kkk);
                UInt32 commandId3 = GetCmd(Get32(pCmd));
                if (commandId3 != EW_PUSHPOP
                    || GET_CMD_PARAM(pCmd, 1) != 0
                    || GET_CMD_PARAM(pCmd, 2) != 0)
                  break;
              }
              if (i < bh.Num)
              {
                const Byte *pCmd = p + kCmdSize * (i - kkk);

                // UInt32 callDll_Param = GET_CMD_PARAM(pCmd, 0);
                // UInt32 file_Param = GET_CMD_PARAM(p + kCmdSize, 1);

                if (GetCmd(Get32(pCmd)) == EW_REGISTERDLL &&
                    AreTwoParamStringsEqual(
                      GET_CMD_PARAM(pCmd, 0),
                      GET_CMD_PARAM(p + kCmdSize, 1)))
                {
                  // params[4] = 1 means GetModuleHandle attempt before default LoadLibraryEx;
                  /// new versions of NSIS use params[4] = 1 for Plugin command
                  if (GET_CMD_PARAM(pCmd, 2) == 0
                    // && GET_CMD_PARAM(pCmd, 4) != 0
                    )
                  {
                    {
                      AString s2;
                      ReadString2(s2, pluginPar);
                      if (s2.Len() >= 4 &&
                          StringsAreEqualNoCase_Ascii(s2.RightPtr(4), ".dll"))
                        s2.DeleteFrom(s2.Len() - 4);
                      s2 += "::";
                      AString func;
                      ReadString2(func, GET_CMD_PARAM(pCmd, 1));
                      s2 += func;
                      Add_QuStr(s2);

                      if (GET_CMD_PARAM(pCmd, 3) == 1)
                        s += " /NOUNLOAD";

                      for (UInt32 j = i - 1; j >= kkk + 3; j--)
                      {
                        const Byte *pCmd2 = p + kCmdSize * (j - kkk);
                        AddParam(GET_CMD_PARAM(pCmd2, 0));
                      }
                      NewLine();
                      Tab(true);
                      endCommentIndex = i + 1;
                    }
                  }
                }
              }
            }
          }
        }
        {
          const Byte *nextCmd = p + kCmdSize;
          UInt32 commandId2 = GetCmd(Get32(nextCmd));
          if (commandId2 == EW_SETFLAG
              && GET_CMD_PARAM(nextCmd, 0) == k_ExecFlags_DetailsPrint
              && GET_CMD_PARAM(nextCmd, 2) != 0) // is "lastused"
            // || commandId2 == EW_UPDATETEXT)
          {
            if ((Int32)params[0] > 0 && labels[params[0] - 1] & CMD_REF_InitPluginDir)
            {
              s += "InitPluginsDir";
              AddLF();
              Tab(true);
              endCommentIndex = kkk + 2;
            }
          }
        }
        
        s += "Call ";
        if ((Int32)params[0] < 0)
          Add_Var(-((Int32)params[0] + 1));
        else if (params[0] == 0)
          s += '0';
        else
        {
          UInt32 val = params[0] - 1;
          if (params[1] == 1) // it's Call :Label
          {
            s += ':';
            Add_LabelName(val);
          }
          else // if (params[1] == 0) // it's Call Func
            Add_FuncName(labels, val);
        }
        break;
      }

      case EW_UPDATETEXT:
      case EW_SLEEP:
      {
        AddParam(params[0]);
        break;
      }
      
      case EW_CHDETAILSVIEW:
      {
             if (params[0] == MY__SW_SHOWNA && params[1] == MY__SW_HIDE) s += " show";
        else if (params[1] == MY__SW_SHOWNA && params[0] == MY__SW_HIDE) s += " hide";
        else
          for (int i = 0; i < 2; i++)
          {
            Space();
            Add_ShowWindow_Cmd(params[i]);
          }
        break;
      }

      case EW_IFFILEEXISTS:
      {
        AddParam(params[0]);
        Add_GotoVars2(&params[1]);
        break;
      }

      case EW_SETFLAG:
      {
        AString temp;
        ReadString2(temp, params[1]);
        if (params[0] == k_ExecFlags_Errors && params[2] == 0)
        {
          s += (temp.Len() == 1 && temp[0] == '0') ? "ClearErrors" : "SetErrors";
          break;
        }
        s += "Set";
        Add_ExecFlags(params[0]);

        if (params[2] != 0)
        {
          s += " lastused";
          break;
        }
        UInt32 v;
        if (StringToUInt32(temp, v))
        {
          const char *s2 = NULL;
          switch (params[0])
          {
            case k_ExecFlags_AutoClose:
            case k_ExecFlags_RebootFlag:
              if (v < 2) { s2 = (v == 0) ? "false" : "true"; }  break;
            case k_ExecFlags_ShellVarContext:
              if (v < 2) { s2 = (v == 0) ? "current" : "all"; }  break;
            case k_ExecFlags_Silent:
              if (v < 2) { s2 = (v == 0) ? "normal" : "silent"; }  break;
            case k_ExecFlags_RegView:
                   if (v ==   0) s2 = "32";
              else if (v == 256) s2 = "64";
              break;
            case k_ExecFlags_DetailsPrint:
                   if (v == 0) s2 = "both";
              else if (v == 2) s2 = "textonly";
              else if (v == 4) s2 = "listonly";
              else if (v == 6) s2 = "none";
              break;
          }
          if (s2)
          {
            s += ' ';
            s += s2;
            break;
          }
        }
        SpaceQuStr(temp);
        break;
      }

      case EW_IFFLAG:
      {
        Add_ExecFlags(params[2]);
        Add_GotoVars2(&params[0]);
        /*
        static const unsigned kIfErrors = 2;
        if (params[2] != kIfErrors && params[3] != 0xFFFFFFFF ||
            params[2] == kIfErrors && params[3] != 0)
        {
          s += " # FLAG &= ";
          AddParam_UInt(params[3]);
        }
        */
        break;
      }

      case EW_GETFLAG:
      {
        Add_ExecFlags(params[1]);
        AddParam_Var(params[0]);
        break;
      }

      case EW_RENAME:
      {
        if (params[2] != 0)
          s += k_REBOOTOK;
        AddParams(params, 2);
        if (params[3] != 0)
        {
          SmallSpaceComment();
          AddParam(params[3]); // rename comment for log file
        }
        break;
      }
      
      case EW_GETFULLPATHNAME:
      {
        if (params[2] == 0)
          s += " /SHORT";
        AddParam_Var(params[1]);
        AddParam(params[0]);
        break;
      }

      case EW_SEARCHPATH:
      case EW_STRLEN:
      {
        AddParam_Var(params[0]);
        AddParam(params[1]);
        break;
      }

      case EW_GETTEMPFILENAME:
      {
        AddParam_Var(params[0]);
        AString temp;
        ReadString2(temp, params[1]);
        if (temp != "$TEMP")
          SpaceQuStr(temp);
        break;
      }

      case EW_DELETEFILE:
      {
        UInt32 flag = params[1];
        if ((flag & DEL_REBOOT) != 0)
          s += k_REBOOTOK;
        AddParam(params[0]);
        break;
      }

      case EW_MESSAGEBOX:
      {
        MessageBox_MB_Part(params[0]);
        AddParam(params[1]);
        {
          UInt32 buttonID = (params[0] >> 21); // NSIS 2.06+
          if (buttonID != 0)
          {
            s += " /SD";
            Add_ButtonID(buttonID);
          }
        }
        for (int i = 2; i < 6; i += 2)
          if (params[i] != 0)
          {
            Add_ButtonID(params[i]);
            Add_GotoVar1(params[i + 1]);
          }
        break;
      }

      case EW_RMDIR:
      {
        UInt32 flag = params[1];
        if ((flag & DEL_RECURSE) != 0)
          s += " /r";
        if ((flag & DEL_REBOOT) != 0)
          s += k_REBOOTOK;
        AddParam(params[0]);
        break;
      }

      case EW_STRCMP:
      {
        if (params[4] != 0)
          s += 'S';
        AddParams(params, 2);
        Add_GotoVars2(&params[2]);
        break;
      }

      case EW_READENVSTR:
      {
        s += (params[2] != 0) ?
          "ReadEnvStr" :
          "ExpandEnvStrings";
        AddParam_Var(params[0]);
        AString temp;
        ReadString2(temp, params[1]);
        if (params[2] != 0 &&temp.Len() >= 2 && temp[0] == '%' && temp.Back() == '%')
        {
          temp.DeleteBack();
          temp.Delete(0);
        }
        SpaceQuStr(temp);
        break;
      }

      case EW_INTCMP:
      {
        if (params[5] != 0)
          s += 'U';
        AddParams(params, 2);
        Add_GotoVar1(params[2]);
        if (params[3] != 0 || params[4] != 0)
          Add_GotoVars2(params + 3);
        break;
      }

      case EW_INTOP:
      {
        AddParam_Var(params[0]);
        const char * const kOps = "+-*/|&^!|&%<>"; // NSIS 2.01+
                        // "+-*/|&^!|&%";   // NSIS 2.0b4+
                        // "+-*/|&^~!|&%";  // NSIS old
        UInt32 opIndex = params[3];
        char c = (opIndex < 13) ? kOps[opIndex] : '?';
        char c2 = (opIndex < 8 || opIndex == 10) ? (char)0 : c;
        int numOps = (opIndex == 7) ? 1 : 2;
        AddParam(params[1]);
        if (numOps == 2 && c == '^' && IsDirectString_Equal(params[2], "0xFFFFFFFF"))
          s += " ~    ;";
        Space();
        s += c;
        if (numOps != 1)
        {
          if (c2 != 0)
            s += c2;
          AddParam(params[2]);
        }
        break;
      }

      case EW_INTFMT:
      {
        AddParam_Var(params[0]);
        AddParams(params + 1, 2);
        break;
      }

      case EW_PUSHPOP:
      {
        if (params[2] != 0)
        {
          s += "Exch";
          if (params[2] != 1)
            AddParam_UInt(params[2]);
        }
        else if (params[1] != 0)
        {
          s += "Pop";
          AddParam_Var(params[0]);
        }
        else
        {
          if (NoLabels(labels + kkk + 1, 2)
              && Get32(p + kCmdSize) == EW_PUSHPOP // Exch"
              && GET_CMD_PARAM(p + kCmdSize, 2) == 1
              && Get32(p + kCmdSize * 2) == EW_PUSHPOP // Pop $VAR
              && GET_CMD_PARAM(p + kCmdSize * 2, 1) != 0)
          {
            if (IsVarStr(params[0], GET_CMD_PARAM(p + kCmdSize * 2, 0)))
            {
              s += "Exch";
              AddParam(params[0]);
              NewLine();
              Tab(true);
              endCommentIndex = kkk + 3;
            }
          }
          s += "Push";
          AddParam(params[0]);
        }
        break;
      }

      case EW_FINDWINDOW:
      {
        AddParam_Var(params[0]);
        AddParam(params[1]);
        AddOptionalParams(params + 2, 3);
        break;
      }

      case EW_SENDMESSAGE:
      {
        // SendMessage: 6 [output, hwnd, msg, wparam, lparam, [wparamstring?1:0 | lparamstring?2:0 | timeout<<2]
        AddParam(params[1]);

        const char *w = NULL;
        AString t;
        ReadString2(t, params[2]);
        UInt32 wm;
        if (StringToUInt32(t, wm))
        {
          switch (wm)
          {
            case 0x0C: w = "SETTEXT"; break;
            case 0x10: w = "CLOSE"; break;
            case 0x30: w = "SETFONT"; break;
          }
        }
        if (w)
        {
          s += " ${WM_";
          s += w;
          s += '}';
        }
        else
          SpaceQuStr(t);
        
        UInt32 spec = params[5];
        for (unsigned i = 0; i < 2; i++)
        {
          AString s2;
          if (spec & ((UInt32)1 << i))
            s2 += "STR:";
          ReadString2(s2, params[3 + i]);
          SpaceQuStr(s2);
        }

        if ((Int32)params[0] >= 0)
          AddParam_Var(params[0]);

        spec >>= 2;
        if (spec != 0)
        {
          s += " /TIMEOUT=";
          Add_UInt(spec);
        }
        break;
      }

      case EW_ISWINDOW:
      {
        AddParam(params[0]);
        Add_GotoVars2(&params[1]);
        break;
      }

      case EW_GETDLGITEM:
      {
        AddParam_Var(params[0]);
        AddParams(params + 1, 2);
        break;
      }
      
      case EW_SETCTLCOLORS:
      {
        AddParam(params[0]);

        UInt32 offset = params[1];
        
        if (_size < bhCtlColors.Offset
           || _size - bhCtlColors.Offset < offset
           || _size - bhCtlColors.Offset - offset < k_CtlColors_Size)
        {
          AddError("bad offset");
          break;
        }

        const Byte *p2 = _data + bhCtlColors.Offset + offset;
        CNsis_CtlColors colors;
        colors.Parse(p2);

        if ((colors.flags & kColorsFlags_BK_SYS) != 0 ||
            (colors.flags & kColorsFlags_TEXT_SYS) != 0)
          s += " /BRANDING";
        
        AString bk;
        bool bkc = false;
        if (colors.bkmode == MY__TRANSPARENT)
          bk += " transparent";
        else if (colors.flags & kColorsFlags_BKB)
        {
          if ((colors.flags & kColorsFlags_BK_SYS) == 0 &&
              (colors.flags & kColorsFlags_BK) != 0)
            bkc = true;
        }
        if ((colors.flags & kColorsFlags_TEXT) != 0 || !bk.IsEmpty() || bkc)
        {
          Space();
          if ((colors.flags & kColorsFlags_TEXT_SYS) != 0 || (colors.flags & kColorsFlags_TEXT) == 0)
            AddQuotes();
          else
            Add_Color(colors.text);
        }
        s += bk;
        if (bkc)
        {
          Space();
          Add_Color(colors.bkc);
        }

        break;
      }

      case EW_SETBRANDINGIMAGE:
      {
        s += " /IMGID=";
        Add_UInt(params[1]);
        if (params[2] == 1)
          s += " /RESIZETOFIT";
        AddParam(params[0]);
        break;
      }

      case EW_CREATEFONT:
      {
        AddParam_Var(params[0]);
        AddParam(params[1]);
        AddOptionalParams(params + 2, 2);
        if (params[4] & 1) s += " /ITALIC";
        if (params[4] & 2) s += " /UNDERLINE";
        if (params[4] & 4) s += " /STRIKE";
        break;
      }

      case EW_SHOWWINDOW:
      {
        AString hw, sw;
        ReadString2(hw, params[0]);
        ReadString2(sw, params[1]);
        if (params[3] != 0)
          s += "EnableWindow";
        else
        {
          UInt32 val;
          bool valDefined = false;
          if (StringToUInt32(sw, val))
          {
            if (val < ARRAY_SIZE(kShowWindow_Commands))
            {
              sw.Empty();
              sw += "${";
              Add_ShowWindow_Cmd_2(sw, val);
              sw += '}';
              valDefined = true;
            }
          }
          bool isHwndParent = IsVarStr(params[0], IsNsis225 ? kVar_HWNDPARENT_225 : kVar_HWNDPARENT);
          if (params[2] != 0)
          {
            if (valDefined && val == 0 && isHwndParent)
            {
              s += "HideWindow";
              break;
            }
          }
          if (valDefined && val == 5 && isHwndParent &&
              kkk + 1 < bh.Num && GetCmd(Get32(p + kCmdSize)) == EW_BRINGTOFRONT)
          {
            s += "  ; ";
          }
          s += "ShowWindow";
        }
        SpaceQuStr(hw);
        SpaceQuStr(sw);
        break;
      }

      case EW_SHELLEXEC:
      {
        AddParams(params, 2);
        if (params[2] != 0 || params[3] != MY__SW_SHOWNORMAL)
        {
          AddParam(params[2]);
          if (params[3] != MY__SW_SHOWNORMAL)
          {
            Space();
            Add_ShowWindow_Cmd(params[3]);
          }
        }
        if (params[5] != 0)
        {
          s += "    ;";
          AddParam(params[5]); // it's tatus text update
        }
        break;
      }

      case EW_EXECUTE:
      {
        if (params[2] != 0)
          s += "Wait";
        AddParam(params[0]);
        if (params[2] != 0)
          if ((Int32)params[1] >= 0)
            AddParam_Var(params[1]);
        break;
      }
      
      case EW_GETFILETIME:
      case EW_GETDLLVERSION:
      {
        AddParam(params[2]);
        AddParam_Var(params[0]);
        AddParam_Var(params[1]);
        break;
      }

      case EW_REGISTERDLL:
      {
        AString func;
        ReadString2(func, params[1]);
        bool printFunc = true;
        // params[4] = 1; for plugin command
        if (params[2] == 0)
        {
          s += "CallInstDLL";
          AddParam(params[0]);
          if (params[3] == 1)
            s += " /NOUNLOAD";
        }
        else
        {
          if (func == "DllUnregisterServer")
          {
            s += "UnRegDLL";
            printFunc = false;
          }
          else
          {
            s += "RegDLL";
            if (func == "DllRegisterServer")
              printFunc = false;
          }
          AddParam(params[0]);
        }
        if (printFunc)
          SpaceQuStr(func);
        break;
      }

      case EW_CREATESHORTCUT:
      {
        unsigned numParams;
        for (numParams = 6; numParams > 2; numParams--)
          if (params[numParams - 1] != 0)
            break;

        UInt32 spec = params[4];
        if (spec & 0x8000) // NSIS 3.0b0
          s += " /NoWorkingDir";

        AddParams(params, numParams > 4 ? 4 : numParams);
        if (numParams <= 4)
          break;

        UInt32 icon = (spec & 0xFF);
        Space();
        if (icon != 0)
          Add_UInt(icon);
        else
          AddQuotes();

        if ((spec >> 8) == 0 && numParams < 6)
          break;
        UInt32 sw = (spec >> 8) & 0x7F;
        Space();
        // NSIS encoder replaces these names:
        if (sw == MY__SW_SHOWMINNOACTIVE)
          sw = MY__SW_SHOWMINIMIZED;
        if (sw == 0)
          AddQuotes();
        else
          Add_ShowWindow_Cmd(sw);
        
        UInt32 modKey = spec >> 24;
        UInt32 key = (spec >> 16) & 0xFF;

        if (modKey == 0 && key == 0)
        {
          if (numParams < 6)
            break;
          Space();
          AddQuotes();
        }
        else
        {
          Space();
          if (modKey & 1) s += "SHIFT|"; // HOTKEYF_SHIFT
          if (modKey & 2) s += "CONTROL|";
          if (modKey & 4) s += "ALT|";
          if (modKey & 8) s += "EXT|";
          
          static const unsigned kMy_VK_F1 = 0x70;
          if (key >= kMy_VK_F1 && key <= kMy_VK_F1 + 23)
          {
            s += 'F';
            Add_UInt(key - kMy_VK_F1 + 1);
          }
          else if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9'))
            s += (char)key;
          else
          {
            s += "Char_";
            Add_UInt(key);
          }
        }
        AddOptionalParam(params[5]); // description
        break;
      }

      case EW_COPYFILES:
      {
        if (params[2] & 0x04) s += " /SILENT"; // FOF_SILENT
        if (params[2] & 0x80) s += " /FILESONLY"; // FOF_FILESONLY
        AddParams(params, 2);
        if (params[3] != 0)
        {
          s += "    ;";
          AddParam(params[3]); // status text update
        }
        break;
      }

      case EW_REBOOT:
      {
        if (params[0] != 0xbadf00d)
          s += " ; Corrupted ???";
        else if (kkk + 1 < bh.Num && GetCmd(Get32(p + kCmdSize)) == EW_QUIT)
          endCommentIndex = kkk + 2;
        break;
      }

      case EW_WRITEINI:
      {
        unsigned numAlwaysParams = 0;
        if (params[0] == 0)  // Section
          s += "FlushINI";
        else if (params[4] != 0)
        {
          s += "WriteINIStr";
          numAlwaysParams = 3;
        }
        else
        {
          s += "DeleteINI";
          s += (params[1] == 0) ? "Sec" : "Str";
          numAlwaysParams = 1;
        }
        AddParam(params[3]); // filename
        // Section, EntryName, Value
        AddParams(params, numAlwaysParams);
        AddOptionalParams(params + numAlwaysParams, 3 - numAlwaysParams);
        break;
      }

      case EW_READINISTR:
      {
        AddParam_Var(params[0]);
        AddParam(params[3]); // FileName
        AddParams(params +1, 2); // Section, EntryName
        break;
      }

      case EW_DELREG:
      {
        // NSIS 2.00 used another scheme!
        
        if (params[4] == 0)
          s += "Value";
        else
        {
          s += "Key";
          if (params[4] & 2)
            s += " /ifempty";
        }
        AddRegRoot(params[1]);
        AddParam(params[2]);
        AddOptionalParam(params[3]);
        break;
      }

      case EW_WRITEREG:
      {
        const char *s2 = 0;
        switch (params[4])
        {
          case 1: s2 = "Str"; break;
          case 2: s2 = "ExpandStr"; break; // maybe unused
          case 3: s2 = "Bin"; break;
          case 4: s2 = "DWORD"; break;
          default:
            s += '?';
            Add_UInt(params[4]);
        }
        if (params[4] == 1 && params[5] == 2)
          s2 = "ExpandStr";
        if (s2)
          s += s2;
        AddRegRoot(params[0]);
        AddParams(params + 1, 2); // keyName, valueName
        if (params[4] != 3)
          AddParam(params[3]); // value
        else
        {
          // Binary data.
          Space();
          UInt32 offset = params[3];
          bool isSupported = false;
          if (AfterHeaderSize >= 4
              && bhData.Offset <= AfterHeaderSize - 4
              && offset <= AfterHeaderSize - 4 - bhData.Offset)
          {
            // we support it for solid archives.
            const Byte *p2 = _afterHeader + bhData.Offset + offset;
            UInt32 size = Get32(p2);
            if (size <= AfterHeaderSize - 4 - bhData.Offset - offset)
            {
              for (UInt32 i = 0; i < size; i++)
              {
                Byte b = (p2 + 4)[i];
                unsigned t;
                t = (b >> 4); s += (char)(((t < 10) ? ('0' + t) : ('A' + (t - 10))));
                t = (b & 15); s += (char)(((t < 10) ? ('0' + t) : ('A' + (t - 10))));
              }
              isSupported = true;
            }
          }
          if (!isSupported)
          {
            // we must read from file here;
            s += "data[";
            Add_UInt(offset);
            s += " ... ]";
            s += "  ; !!! Unsupported";
          }
        }
        break;
      }

      case EW_READREGSTR:
      {
        s += (params[4] == 1) ? "DWORD" : "Str";
        AddParam_Var(params[0]);
        AddRegRoot(params[1]);
        AddParams(params + 2, 2);
        break;
      }

      case EW_REGENUM:
      {
        s += (params[4] != 0) ? "Key" : "Value";
        AddParam_Var(params[0]);
        AddRegRoot(params[1]);
        AddParams(params + 2, 2);
        break;
      }

      case EW_FCLOSE:
      case EW_FINDCLOSE:
      {
        AddParam_Var(params[0]);
        break;
      }

      case EW_FOPEN:
      {
        AddParam_Var(params[0]);
        AddParam(params[3]);
        UInt32 acc = params[1]; // dwDesiredAccess
        UInt32 creat = params[2]; // dwCreationDisposition
        if (acc == 0 && creat == 0)
          break;
        char cc = 0;
        if (acc == MY__GENERIC_READ && creat == OPEN_EXISTING)
          cc = 'r';
        else if (creat == CREATE_ALWAYS && acc == MY__GENERIC_WRITE)
          cc = 'w';
        else if (creat == OPEN_ALWAYS && (acc == (MY__GENERIC_WRITE | MY__GENERIC_READ)))
          cc = 'a';
        // cc = 0;
        if (cc != 0)
        {
          Space();
          s += cc;
          break;
        }

        if (acc & MY__GENERIC_READ)     s += " GENERIC_READ";
        if (acc & MY__GENERIC_WRITE)    s += " GENERIC_WRITE";
        if (acc & MY__GENERIC_EXECUTE)  s += " GENERIC_EXECUTE";
        if (acc & MY__GENERIC_ALL)      s += " GENERIC_ALL";
        
        const char *s2 = NULL;
        switch (creat)
        {
          case MY__CREATE_NEW:        s2 = "CREATE_NEW"; break;
          case MY__CREATE_ALWAYS:     s2 = "CREATE_ALWAYS"; break;
          case MY__OPEN_EXISTING:     s2 = "OPEN_EXISTING"; break;
          case MY__OPEN_ALWAYS:       s2 = "OPEN_ALWAYS"; break;
          case MY__TRUNCATE_EXISTING: s2 = "TRUNCATE_EXISTING"; break;
        }
        Space();
        if (s2)
          s += s2;
        else
          Add_UInt(creat);
        break;
      }

      case EW_FPUTS:
      case EW_FPUTWS:
      {
        if (commandId == EW_FPUTWS)
          s += (params[2] == 0) ? "UTF16LE" : "Word";
        else if (params[2] != 0)
          s += "Byte";
        AddParam_Var(params[0]);
        AddParam(params[1]);
        break;
      }

      case EW_FGETS:
      case EW_FGETWS:
      {
        if (commandId == EW_FPUTWS)
          s += (params[3] == 0) ? "UTF16LE" : "Word";
        if (params[3] != 0)
          s += "Byte";
        AddParam_Var(params[0]);
        AddParam_Var(params[1]);
        AString maxLenStr;
        ReadString2(maxLenStr, params[2]);
        UInt32 maxLen;
        if (StringToUInt32(maxLenStr, maxLen))
        {
          if (maxLen == 1 && params[3] != 0)
            break;
          if (maxLen == 1023 && params[3] == 0) // NSIS_MAX_STRLEN - 1; can be other value!!
            break;
        }
        SpaceQuStr(maxLenStr);
        break;
      }

      case EW_FSEEK:
      {
        AddParam_Var(params[0]);
        AddParam(params[2]);
        if (params[3] == 1) s += " CUR"; // FILE_CURRENT
        if (params[3] == 2) s += " END"; // FILE_END
        if ((Int32)params[1] >= 0)
        {
          if (params[3] == 0) s += " SET"; // FILE_BEGIN
          AddParam_Var(params[1]);
        }
        break;
      }

      case EW_FINDNEXT:
      {
        AddParam_Var(params[1]);
        AddParam_Var(params[0]);
        break;
      }

      case EW_FINDFIRST:
      {
        AddParam_Var(params[1]);
        AddParam_Var(params[0]);
        AddParam(params[2]);
        break;
      }

      case EW_LOG:
      {
        if (params[0] != 0)
        {
          s += "Set ";
          s += (params[1] == 0) ? "off" : "on";
        }
        else
        {
          s += "Text";
          AddParam(params[1]);
        }
        break;
      }

      case EW_SECTIONSET:
      {
        if ((Int32)params[2] >= 0)
        {
          s += "Get";
          Add_SectOp(params[2]);
          AddParam(params[0]);
          AddParam_Var(params[1]);
        }
        else
        {
          s += "Set";
          UInt32 t = -(Int32)params[2] - 1;
          Add_SectOp(t);
          AddParam(params[0]);
          AddParam(params[t == 0 ? 4 : 1]);

          // params[3] != 0 means call SectionFlagsChanged in installer
          // used by SECTIONSETFLAGS command
        }
        break;
      }

      case EW_INSTTYPESET:
      {
        int numQwParams = 0;
        const char *s2;
        if (params[3] == 0)
        {
          if (params[2] == 0)
          {
            s2 = "InstTypeGetText";
            numQwParams = 1;
          }
          else
          {
            s2 = "InstTypeSetText";
            numQwParams = 2;
          }
        }
        else
        {
          if (params[2] == 0)
            s2 = "GetCurInstType";
          else
          {
            s2 = "SetCurInstType";
            numQwParams = 1;
          }
        }
        s += s2;
        AddParams(params, numQwParams);
        if (params[2] == 0)
          AddParam_Var(params[1]);
        break;
      }
      
      case EW_LOCKWINDOW:
      {
        s += (params[0] == 0) ? " on" : " off";
        break;
      }

      case EW_FINDPROC:
      {
        AddParam_Var(params[0]);
        AddParam(params[1]);
        break;
      }

      default:
      {
        numSkipParams = 0;
      }
      #endif
    }
    
    #ifdef NSIS_SCRIPT

    unsigned numParams = kNumCommandParams;

    for (; numParams > 0; numParams--)
      if (params[numParams - 1] != 0)
        break;

    if (numParams > numSkipParams)
    {
      s += " ; !!!! Unknown Params: ";
      unsigned i;
      for (i = 0; i < numParams; i++)
        AddParam(params[i]);

      s += "   ;";
      
      for (i = 0; i < numParams; i++)
      {
        Space();
        UInt32 v = params[i];
        if (v > 0xFFF00000)
          Add_SignedInt(s, (Int32)v);
        else
          Add_UInt(v);
      }
    }

    NewLine();
    
    #endif
  }

  #ifdef NSIS_SCRIPT

  if (sectionIsOpen)
  {
    if (curSectionIndex < bhSections.Num)
    {
      const CSection &sect = Sections[curSectionIndex];
      if (sect.StartCmdIndex + sect.NumCommands + 1 == kkk)
      {
        PrintSectionEnd();
        sectionIsOpen = false;
        // lastSectionEndCmd = kkk;
        curSectionIndex++;
      }
    }
  }
  
  while (curSectionIndex < bhSections.Num)
  {
    const CSection &sect = Sections[curSectionIndex];
    if (sectionIsOpen)
    {
      if (sect.StartCmdIndex + sect.NumCommands != kkk)
        AddErrorLF("SECTION ERROR");
      PrintSectionEnd();
      sectionIsOpen = false;
      curSectionIndex++;
    }
    else
    {
      if (PrintSectionBegin(sect, curSectionIndex))
        curSectionIndex++;
      else
        sectionIsOpen = true;
    }
  }
  
  #endif

  return S_OK;
}

static int CompareItems(void *const *p1, void *const *p2, void *param)
{
  const CItem &i1 = **(const CItem *const *)p1;
  const CItem &i2 = **(const CItem *const *)p2;
  RINOZ(MyCompare(i1.Pos, i2.Pos));
  const CInArchive *inArchive = (const CInArchive *)param;
  if (inArchive->IsUnicode)
  {
    if (i1.Prefix != i2.Prefix)
    {
      if (i1.Prefix < 0) return -1;
      if (i2.Prefix < 0) return 1;
      RINOZ(
          inArchive->UPrefixes[i1.Prefix].Compare(
          inArchive->UPrefixes[i2.Prefix]));
    }
    RINOZ(i1.NameU.Compare(i2.NameU));
  }
  else
  {
    if (i1.Prefix != i2.Prefix)
    {
      if (i1.Prefix < 0) return -1;
      if (i2.Prefix < 0) return 1;
      RINOZ(strcmp(
          inArchive->APrefixes[i1.Prefix],
          inArchive->APrefixes[i2.Prefix]));
    }
    RINOZ(strcmp(i1.NameA, i2.NameA));
  }
  return 0;
}

HRESULT CInArchive::SortItems()
{
  {
    Items.Sort(CompareItems, (void *)this);
    unsigned i;

    for (i = 0; i + 1 < Items.Size(); i++)
    {
      const CItem &i1 = Items[i];
      const CItem &i2 = Items[i + 1];
      if (i1.Pos != i2.Pos)
        continue;

      if (IsUnicode)
      {
        if (i1.NameU != i2.NameU) continue;
        if (i1.Prefix != i2.Prefix)
        {
          if (i1.Prefix < 0 || i2.Prefix < 0) continue;
          if (UPrefixes[i1.Prefix] != UPrefixes[i2.Prefix]) continue;
        }
      }
      else
      {
        if (i1.NameA != i2.NameA) continue;
        if (i1.Prefix != i2.Prefix)
        {
          if (i1.Prefix < 0 || i2.Prefix < 0) continue;
          if (APrefixes[i1.Prefix] != APrefixes[i2.Prefix]) continue;
        }
      }
      Items.Delete(i + 1);
      i--;
    }
    
    for (i = 0; i < Items.Size(); i++)
    {
      CItem &item = Items[i];
      UInt32 curPos = item.Pos + 4;
      for (unsigned nextIndex = i + 1; nextIndex < Items.Size(); nextIndex++)
      {
        UInt32 nextPos = Items[nextIndex].Pos;
        if (curPos <= nextPos)
        {
          item.EstimatedSize_Defined = true;
          item.EstimatedSize = nextPos - curPos;
          break;
        }
      }
    }
    
    if (!IsSolid)
    {
      for (i = 0; i < Items.Size(); i++)
      {
        CItem &item = Items[i];
        RINOK(SeekToNonSolidItem(i));
        const UInt32 kSigSize = 4 + 1 + 1 + 4; // size,[flag],prop,dict
        BYTE sig[kSigSize];
        size_t processedSize = kSigSize;
        RINOK(ReadStream(_stream, sig, &processedSize));
        if (processedSize < 4)
          return S_FALSE;
        UInt32 size = Get32(sig);
        if ((size & kMask_IsCompressed) != 0)
        {
          item.IsCompressed = true;
          size &= ~kMask_IsCompressed;
          if (Method == NMethodType::kLZMA)
          {
            if (processedSize < 9)
              return S_FALSE;
            /*
            if (FilterFlag)
              item.UseFilter = (sig[4] != 0);
            */
            item.DictionarySize = Get32(sig + 4 + 1 + (FilterFlag ? 1 : 0));
          }
        }
        else
        {
          item.IsCompressed = false;
          item.Size = size;
          item.Size_Defined = true;
        }
        item.CompressedSize = size;
        item.CompressedSize_Defined = true;
      }
    }
  }
  return S_OK;
}

#ifdef NSIS_SCRIPT
// Flags for common_header.flags
// #define CH_FLAGS_DETAILS_SHOWDETAILS 1
// #define CH_FLAGS_DETAILS_NEVERSHOW 2
#define CH_FLAGS_PROGRESS_COLORED 4
#define CH_FLAGS_SILENT 8
#define CH_FLAGS_SILENT_LOG 16
#define CH_FLAGS_AUTO_CLOSE 32
// #define CH_FLAGS_DIR_NO_SHOW 64  // unused now
#define CH_FLAGS_NO_ROOT_DIR 128
#define CH_FLAGS_COMP_ONLY_ON_CUSTOM 256
#define CH_FLAGS_NO_CUSTOM 512

static const char * const k_PostStrings[] =
{
    "install_directory_auto_append"
  , "uninstchild"     // NSIS 2.25+, used by uninstaller:
  , "uninstcmd"       // NSIS 2.25+, used by uninstaller:
  , "wininit"         // NSIS 2.25+, used by move file on reboot
};
#endif


void CBlockHeader::Parse(const Byte *p, unsigned bhoSize)
{
  if (bhoSize == 12)
  {
    // UInt64 a = GetUi64(p);
    if (GetUi32(p + 4) != 0)
      throw 1;
  }
  Offset = GetUi32(p);
  Num = GetUi32(p + bhoSize - 4);
}

#define PARSE_BH(k, bh) bh.Parse (p1 + 4 + bhoSize * k, bhoSize)


HRESULT CInArchive::Parse()
{
  // UInt32 offset = ReadUInt32();
  // ???? offset == FirstHeader.HeaderSize
  const Byte * const p1 = _data;

  if (_size < 4 + 12 * 8)
    Is64Bit = false;
  else
  {
    Is64Bit = true;
    // here we test high 32-bit of possible UInt64 CBlockHeader::Offset field
    for (int k = 0; k < 8; k++)
      if (GetUi32(p1 + 4 + 12 * k + 4) != 0)
        Is64Bit = false;
  }

  const unsigned bhoSize = Is64Bit ? 12 : 8;
  if (_size < 4 + bhoSize * 8)
    return S_FALSE;

  CBlockHeader bhEntries, bhStrings, bhLangTables;

  PARSE_BH (2, bhEntries);
  PARSE_BH (3, bhStrings);
  PARSE_BH (4, bhLangTables);

  #ifdef NSIS_SCRIPT

  CBlockHeader bhFont;
  PARSE_BH (0, bhPages);
  PARSE_BH (1, bhSections);
  PARSE_BH (5, bhCtlColors);
  PARSE_BH (6, bhFont);
  PARSE_BH (7, bhData);

  #endif

  _stringsPos = bhStrings.Offset;
  if (_stringsPos > _size
      || bhLangTables.Offset > _size
      || bhEntries.Offset > _size)
    return S_FALSE;
  {
    if (bhLangTables.Offset < bhStrings.Offset)
      return S_FALSE;
    const UInt32 stringTableSize = bhLangTables.Offset - bhStrings.Offset;
    if (stringTableSize < 2)
      return S_FALSE;
    const Byte *strData = _data + _stringsPos;
    if (strData[stringTableSize - 1] != 0)
      return S_FALSE;
    IsUnicode = (Get16(strData) == 0);
    NumStringChars = stringTableSize;
    if (IsUnicode)
    {
      if ((stringTableSize & 1) != 0)
        return S_FALSE;
      NumStringChars >>= 1;
      if (strData[stringTableSize - 2] != 0)
        return S_FALSE;
    }
  }

  if (bhEntries.Num > (1 << 25))
    return S_FALSE;
  if (bhEntries.Num * kCmdSize > _size - bhEntries.Offset)
    return S_FALSE;

  DetectNsisType(bhEntries, _data + bhEntries.Offset);

  Decoder.IsNsisDeflate = (NsisType != k_NsisType_Nsis3);
  
  // some NSIS files (that are not detected as k_NsisType_Nsis3)
  // use original (non-NSIS) Deflate
  // How to detect these cases?

  // Decoder.IsNsisDeflate = false;


  #ifdef NSIS_SCRIPT

  {
    AddCommentAndString("NSIS script");
    if (IsUnicode)
      Script += " (UTF-8)";
    Space();
    Script += GetFormatDescription();
    AddLF();
  }
  {
    AddCommentAndString(IsInstaller ? "Install" : "Uninstall");
    AddLF();
  }

  AddLF();
  if (IsUnicode)
    AddStringLF("Unicode true");

  if (Method != NMethodType::kCopy)
  {
    const char *m = NULL;
    switch (Method)
    {
      case NMethodType::kDeflate: m = "zlib"; break;
      case NMethodType::kBZip2: m = "bzip2"; break;
      case NMethodType::kLZMA: m = "lzma"; break;
      default: break;
    }
    Script += "SetCompressor";
    if (IsSolid)
      Script += " /SOLID";
    if (m)
    {
      Space();
      Script += m;
    }
    AddLF();
  }
  if (Method == NMethodType::kLZMA)
  {
    // if (DictionarySize != (8 << 20))
    {
      Script += "SetCompressorDictSize";
      AddParam_UInt(DictionarySize >> 20);
      AddLF();
    }
  }

  Separator();
  PrintNumComment("HEADER SIZE", FirstHeader.HeaderSize);
  // if (bhPages.Offset != 300 && bhPages.Offset != 288)
  if (bhPages.Offset != 0)
  {
    PrintNumComment("START HEADER SIZE", bhPages.Offset);
  }

  if (bhSections.Num > 0)
  {
    if (bhEntries.Offset < bhSections.Offset)
      return S_FALSE;
    SectionSize = (bhEntries.Offset - bhSections.Offset) / bhSections.Num;
    if (bhSections.Offset + bhSections.Num * SectionSize != bhEntries.Offset)
      return S_FALSE;
    if (SectionSize < kSectionSize_base)
      return S_FALSE;
    UInt32 maxStringLen = SectionSize - kSectionSize_base;
    if (IsUnicode)
    {
      if ((maxStringLen & 1) != 0)
        return S_FALSE;
      maxStringLen >>= 1;
    }
    // if (maxStringLen != 1024)
    {
      if (maxStringLen == 0)
        PrintNumComment("SECTION SIZE", SectionSize);
      else
        PrintNumComment("MAX STRING LENGTH", maxStringLen);
    }
  }
  
  PrintNumComment("STRING CHARS", NumStringChars);
  // PrintNumComment("LANG TABLE SIZE", bhCtlColors.Offset - bhLangTables.Offset);
  
  if (bhCtlColors.Offset > _size)
    AddErrorLF("Bad COLORS TABLE");
  // PrintNumComment("COLORS TABLE SIZE", bhFont.Offset - bhCtlColors.Offset);
  if (bhCtlColors.Num != 0)
    PrintNumComment("COLORS Num", bhCtlColors.Num);

  // bhData uses offset in _afterHeader (not in _data)
  // PrintNumComment("FONT TABLE SIZE", bhData.Offset - bhFont.Offset);
  if (bhFont.Num != 0)
    PrintNumComment("FONTS Num", bhFont.Num);

  // PrintNumComment("DATA SIZE", FirstHeader.HeaderSize - bhData.Offset);
  if (bhData.Num != 0)
    PrintNumComment("DATA NUM", bhData.Num);

  AddLF();

  AddStringLF("OutFile [NSIS].exe");
  AddStringLF("!include WinMessages.nsh");

  AddLF();

  strUsed.Alloc(NumStringChars);
  memset(strUsed, 0, NumStringChars);

  {
    UInt32 ehFlags = Get32(p1);
    UInt32 showDetails = ehFlags & 3;// CH_FLAGS_DETAILS_SHOWDETAILS & CH_FLAGS_DETAILS_NEVERSHOW;
    if (showDetails >= 1 && showDetails <= 2)
    {
      Script += IsInstaller ? "ShowInstDetails" : "ShowUninstDetails";
      Script += (showDetails == 1) ? " show" : " nevershow";
      AddLF();
    }
    if (ehFlags & CH_FLAGS_PROGRESS_COLORED) AddStringLF("InstProgressFlags colored" );
    if ((ehFlags & (CH_FLAGS_SILENT | CH_FLAGS_SILENT_LOG)) != 0)
    {
      Script += IsInstaller ? "SilentInstall " : "SilentUnInstall ";
      Script += (ehFlags & CH_FLAGS_SILENT_LOG) ? "silentlog" : "silent";
      AddLF();
    }
    if (ehFlags & CH_FLAGS_AUTO_CLOSE) AddStringLF("AutoCloseWindow true");
    if ((ehFlags & CH_FLAGS_NO_ROOT_DIR) == 0) AddStringLF("AllowRootDirInstall true");
    if (ehFlags & CH_FLAGS_NO_CUSTOM) AddStringLF("InstType /NOCUSTOM");
    if (ehFlags & CH_FLAGS_COMP_ONLY_ON_CUSTOM) AddStringLF("InstType /COMPONENTSONLYONCUSTOM");
  }

  // Separator();
  // AddLF();

  Int32 licenseLangIndex = -1;
  {
    const Byte *pp = _data + bhPages.Offset;

    for (UInt32 pageIndex = 0; pageIndex < bhPages.Num; pageIndex++, pp += kPageSize)
    {
      UInt32 wndProcID = Get32(pp + 4);
      UInt32 param1 = Get32(pp + 44 + 4 * 1);
      if (wndProcID != PWP_LICENSE || param1 == 0)
        continue;
      if ((Int32)param1 < 0)
        licenseLangIndex = - ((Int32)param1 + 1);
      else
        noParseStringIndexes.AddToUniqueSorted(param1);
    }
  }

  unsigned paramsOffset;
  {
    unsigned numBhs = 8;
    // probably its for old NSIS?
    if (bhoSize == 8 && bhPages.Offset == 276)
      numBhs = 7;
    paramsOffset = 4 + bhoSize * numBhs;
  }

  const Byte *p2 = p1 + paramsOffset;

  {
    UInt32 rootKey = Get32(p2); // (rootKey = -1) in uninstaller by default (the bug in NSIS)
    UInt32 subKey = Get32(p2 + 4);
    UInt32 value = Get32(p2 + 8);
    if ((rootKey != 0 && rootKey != (UInt32)(Int32)-1) || subKey != 0 || value != 0)
    {
      Script += "InstallDirRegKey";
      AddRegRoot(rootKey);
      AddParam(subKey);
      AddParam(value);
      AddLF();
    }
  }


  {
    UInt32 bg_color1 = Get32(p2 + 12);
    UInt32 bg_color2 = Get32(p2 + 16);
    UInt32 bg_textcolor = Get32(p2 + 20);
    if (bg_color1 != (UInt32)(Int32)-1 || bg_color2 != (UInt32)(Int32)-1 || bg_textcolor != (UInt32)(Int32)-1)
    {
      Script += "BGGradient";
      if (bg_color1 != 0 || bg_color2 != 0xFF0000 || bg_textcolor != (UInt32)(Int32)-1)
      {
        Add_ColorParam(bg_color1);
        Add_ColorParam(bg_color2);
        if (bg_textcolor != (UInt32)(Int32)-1)
          Add_ColorParam(bg_textcolor);
      }
      AddLF();
    }
  }

  {
    UInt32 lb_bg = Get32(p2 + 24);
    UInt32 lb_fg = Get32(p2 + 28);
    if ((lb_bg != (UInt32)(Int32)-1 || lb_fg != (UInt32)(Int32)-1) &&
      (lb_bg != 0 || lb_fg != 0xFF00))
    {
      Script += "InstallColors";
      Add_ColorParam(lb_fg);
      Add_ColorParam(lb_bg);
      AddLF();
    }
  }

  UInt32 license_bg = Get32(p2 + 36);
  if (license_bg != (UInt32)(Int32)-1 &&
      license_bg != (UInt32)(Int32)-15) // COLOR_BTNFACE
  {
    Script += "LicenseBkColor";
    if ((Int32)license_bg == -5)  // COLOR_WINDOW
      Script += " /windows";
    /*
    else if ((Int32)license_bg == -15)
      Script += " /grey";
    */
    else
      Add_ColorParam(license_bg);
    AddLF();
  }

  if (bhLangTables.Num > 0)
  {
    const UInt32 langtable_size = Get32(p2 + 32);

    if (langtable_size == (UInt32)(Int32)-1)
      return E_NOTIMPL; // maybe it's old NSIS archive()

    if (langtable_size < 10)
      return S_FALSE;
    if (bhLangTables.Num > (_size - bhLangTables.Offset) / langtable_size)
      return S_FALSE;

    const UInt32 numStrings = (langtable_size - 10) / 4;
    _numLangStrings = numStrings;
    AddLF();
    Separator();
    PrintNumComment("LANG TABLES", bhLangTables.Num);
    PrintNumComment("LANG STRINGS", numStrings);
    AddLF();

    if (licenseLangIndex >= 0 && (unsigned)licenseLangIndex < numStrings)
    {
      for (UInt32 i = 0; i < bhLangTables.Num; i++)
      {
        const Byte * const p = _data + bhLangTables.Offset + langtable_size * i;
        const UInt16 langID = Get16(p);
        UInt32 val = Get32(p + 10 + (UInt32)licenseLangIndex * 4);
        if (val != 0)
        {
          Script += "LicenseLangString ";
          Add_LangStr_Simple(licenseLangIndex);
          AddParam_UInt(langID);
          AddLicense(val, langID);
          noParseStringIndexes.AddToUniqueSorted(val);
          NewLine();
        }
      }
      AddLF();
    }
    
    UInt32 names[3] = { 0 };

    UInt32 i;
    for (i = 0; i < bhLangTables.Num; i++)
    {
      const Byte * const p = _data + bhLangTables.Offset + langtable_size * i;
      const UInt16 langID = Get16(p);
      if (i == 0 || langID == 1033)
        _mainLang = p + 10;
      for (unsigned k = 0; k < ARRAY_SIZE(names) && k < numStrings; k++)
      {
        UInt32 v = Get32(p + 10 + k * 4);
        if (v != 0 && (langID == 1033 || names[k] == 0))
          names[k] = v;
      }
    }

    const UInt32 name = names[2];
    if (name != 0)
    {
      Script += "Name";
      AddParam(name);
      NewLine();

      ReadString2(Name, name);
    }
    
    /*
    const UInt32 caption = names[1];
    if (caption != 0)
    {
      Script += "Caption";
      AddParam(caption);
      NewLine();
    }
    */
    
    const UInt32 brandingText = names[0];
    if (brandingText != 0)
    {
      Script += "BrandingText";
      AddParam(brandingText);
      NewLine();

      ReadString2(BrandingText, brandingText);
    }

    for (i = 0; i < bhLangTables.Num; i++)
    {
      const Byte * const p = _data + bhLangTables.Offset + langtable_size * i;
      const UInt16 langID = Get16(p);
      
      AddLF();
      AddCommentAndString("LANG:");
      AddParam_UInt(langID);
      /*
      Script += " (";
      LangId_To_String(Script, langID);
      Script += ')';
      */
      AddLF();
      // UInt32 dlg_offset = Get32(p + 2);
      // UInt32 g_exec_flags_rtl = Get32(p + 6);
      
      
      for (UInt32 j = 0; j < numStrings; j++)
      {
        UInt32 val = Get32(p + 10 + j * 4);
        if (val != 0)
        {
          if ((Int32)j != licenseLangIndex)
          {
            Script += "LangString ";
            Add_LangStr_Simple(j);
            AddParam_UInt(langID);
            AddParam(val);
            AddLF();
          }
        }
      }
      AddLF();
    }
    ClearLangComment();
  }

  {
    unsigned numInternalVars = GET_NUM_INTERNAL_VARS;
    UInt32 numUsedVars = GetNumUsedVars();
    if (numUsedVars > numInternalVars)
    {
      Separator();
      PrintNumComment("VARIABLES", numUsedVars - numInternalVars);
      AddLF();
      AString temp;
      for (UInt32 i = numInternalVars; i < numUsedVars; i++)
      {
        Script += "Var ";
        temp.Empty();
        GetVar2(temp, i);
        AddStringLF(temp);
      }
      AddLF();
    }
  }

  onFuncOffset = paramsOffset + 40;
  numOnFunc = ARRAY_SIZE(kOnFunc);
  if (bhPages.Offset == 276)
    numOnFunc--;
  p2 += 40 + numOnFunc * 4;
  
  #define NSIS_MAX_INST_TYPES 32

  AddLF();

  UInt32 i;
  for (i = 0; i < NSIS_MAX_INST_TYPES + 1; i++, p2 += 4)
  {
    UInt32 instType = Get32(p2);
    if (instType != 0)
    {
      Script += "InstType";
      AString s2;
      if (!IsInstaller)
        s2 += "un.";
      ReadString2(s2, instType);
      SpaceQuStr(s2);
      NewLine();
    }
  }
  
  {
    UInt32 installDir = Get32(p2);
    p2 += 4;
    if (installDir != 0)
    {
      Script += "InstallDir";
      AddParam(installDir);
      NewLine();
    }
  }

  if (bhPages.Offset >= 288)
    for (i = 0; i < 4; i++)
    {
      if (i != 0 && bhPages.Offset < 300)
        break;
      UInt32 param = Get32(p2 + 4 * i);
      if (param == 0 || param == (UInt32)(Int32)-1)
        continue;
      
      /*
      uninstaller:
      UInt32 uninstChild = Get32(p2 + 8); // "$TEMP\\$1u_.exe"
      UInt32 uninstCmd = Get32(p2 + 12); // "\"$TEMP\\$1u_.exe\" $0 _?=$INSTDIR\\"
      int str_wininit = Get32(p2 + 16); // "$WINDIR\\wininit.ini"
      */
      
      AddCommentAndString(k_PostStrings[i]);
      Script += " =";
      AddParam(param);
      NewLine();
    }

  AddLF();

  #endif

  RINOK(ReadEntries(bhEntries));

  #ifdef NSIS_SCRIPT

  Separator();
  AddCommentAndString("UNREFERENCED STRINGS:");
  AddLF();
  AddLF();
  CommentOpen();

  for (i = 0; i < NumStringChars;)
  {
    if (!strUsed[i] && i != 0)
    // Script += "!!! ";
    {
      Add_UInt(i);
      AddParam(i);
      NewLine();
    }
    if (IsUnicode)
      i += GetUi16Str_Len((const Byte *)_data + _stringsPos + i * 2);
    else
      i += (UInt32)strlen((const char *)(const Byte *)_data + _stringsPos + i);
    i++;
  }
  CommentClose();
  #endif

  return SortItems();
}

static bool IsLZMA(const Byte *p, UInt32 &dictionary)
{
  dictionary = Get32(p + 1);
  return (p[0] == 0x5D &&
      p[1] == 0x00 && p[2] == 0x00 &&
      p[5] == 0x00 && (p[6] & 0x80) == 0x00);
}

static bool IsLZMA(const Byte *p, UInt32 &dictionary, bool &thereIsFlag)
{
  if (IsLZMA(p, dictionary))
  {
    thereIsFlag = false;
    return true;
  }
  if (p[0] <= 1 && IsLZMA(p + 1, dictionary))
  {
    thereIsFlag = true;
    return true;
  }
  return false;
}

static bool IsBZip2(const Byte *p)
{
  return (p[0] == 0x31 && p[1] < 14);
}

HRESULT CInArchive::Open2(const Byte *sig, size_t size)
{
  const UInt32 kSigSize = 4 + 1 + 5 + 2; // size, flag, 5 - lzma props, 2 - lzma first bytes
  if (size < kSigSize)
    return S_FALSE;

  _headerIsCompressed = true;
  IsSolid = true;
  FilterFlag = false;
  UseFilter = false;
  DictionarySize = 1;

  #ifdef NSIS_SCRIPT
  AfterHeaderSize = 0;
  #endif

  UInt32 compressedHeaderSize = Get32(sig);
  

  /*
    XX XX XX XX             XX XX XX XX == FirstHeader.HeaderSize, nonsolid, uncompressed
    5D 00 00 dd dd 00       solid LZMA
    00 5D 00 00 dd dd 00    solid LZMA, empty filter (there are no such archives)
    01 5D 00 00 dd dd 00    solid LZMA, BCJ filter   (only 7-Zip installer used that format)
    
    SS SS SS 80 00 5D 00 00 dd dd 00     non-solid LZMA, empty filter
    SS SS SS 80 01 5D 00 00 dd dd 00     non-solid LZMA, BCJ filte
    SS SS SS 80 01 tt         non-solid BZip (tt < 14
    SS SS SS 80               non-solid  deflate

    01 tt         solid BZip (tt < 14
    other         solid Deflate
  */

  if (compressedHeaderSize == FirstHeader.HeaderSize)
  {
    _headerIsCompressed = false;
    IsSolid = false;
    Method = NMethodType::kCopy;
  }
  else if (IsLZMA(sig, DictionarySize, FilterFlag))
    Method = NMethodType::kLZMA;
  else if (sig[3] == 0x80)
  {
    IsSolid = false;
    if (IsLZMA(sig + 4, DictionarySize, FilterFlag) && sig[3] == 0x80)
      Method = NMethodType::kLZMA;
    else if (IsBZip2(sig + 4))
      Method = NMethodType::kBZip2;
    else
      Method = NMethodType::kDeflate;
  }
  else if (IsBZip2(sig))
    Method = NMethodType::kBZip2;
  else
    Method = NMethodType::kDeflate;

  if (IsSolid)
  {
    RINOK(SeekTo_DataStreamOffset());
  }
  else
  {
    _headerIsCompressed = ((compressedHeaderSize & kMask_IsCompressed) != 0);
    compressedHeaderSize &= ~kMask_IsCompressed;
    _nonSolidStartOffset = compressedHeaderSize;
    RINOK(SeekTo(DataStreamOffset + 4));
  }

  if (FirstHeader.HeaderSize == 0)
    return S_FALSE;

  _data.Alloc(FirstHeader.HeaderSize);
  _size = (size_t)FirstHeader.HeaderSize;

  Decoder.Method = Method;
  Decoder.FilterFlag = FilterFlag;
  Decoder.Solid = IsSolid;
  
  Decoder.IsNsisDeflate = true; // we need some smart check that NSIS is not NSIS3 here.
  
  Decoder.InputStream = _stream;
  Decoder.Buffer.Alloc(kInputBufSize);
  Decoder.StreamPos = 0;

  if (_headerIsCompressed)
  {
    RINOK(Decoder.Init(_stream, UseFilter));
    if (IsSolid)
    {
      size_t processedSize = 4;
      Byte buf[4];
      RINOK(Decoder.Read(buf, &processedSize));
      if (processedSize != 4)
        return S_FALSE;
      if (Get32((const Byte *)buf) != FirstHeader.HeaderSize)
        return S_FALSE;
    }
    {
      size_t processedSize = FirstHeader.HeaderSize;
      RINOK(Decoder.Read(_data, &processedSize));
      if (processedSize != FirstHeader.HeaderSize)
        return S_FALSE;
    }
    
    #ifdef NSIS_SCRIPT
    if (IsSolid)
    {
      /* we need additional bytes for data for WriteRegBin */
      AfterHeaderSize = (1 << 12);
      _afterHeader.Alloc(AfterHeaderSize);
      size_t processedSize = AfterHeaderSize;
      RINOK(Decoder.Read(_afterHeader, &processedSize));
      AfterHeaderSize = (UInt32)processedSize;
    }
    #endif
  }
  else
  {
    size_t processedSize = FirstHeader.HeaderSize;
    RINOK(ReadStream(_stream, (Byte *)_data, &processedSize));
    if (processedSize < FirstHeader.HeaderSize)
      return S_FALSE;
  }

  #ifdef NUM_SPEED_TESTS
  for (unsigned i = 0; i < NUM_SPEED_TESTS; i++)
  {
    RINOK(Parse());
    Clear2();
  }
  #endif

  return Parse();
}

/*
NsisExe =
{
  ExeStub
  Archive  // must start from 512 * N
  #ifndef NSIS_CONFIG_CRC_ANAL
  {
    Some additional data
  }
}

Archive
{
  FirstHeader
  Data
  #ifdef NSIS_CONFIG_CRC_SUPPORT && FirstHeader.ThereIsCrc()
  {
    CRC
  }
}

FirstHeader
{
  UInt32 Flags;
  Byte Signature[16];
  // points to the header+sections+entries+stringtable in the datablock
  UInt32 HeaderSize;
  UInt32 ArcSize;
}
*/


// ---------- PE (EXE) parsing ----------

static const unsigned k_PE_StartSize = 0x40;
static const unsigned k_PE_HeaderSize = 4 + 20;
static const unsigned k_PE_OptHeader32_Size_MIN = 96;

static inline bool CheckPeOffset(UInt32 pe)
{
  return (pe >= 0x40 && pe <= 0x1000 && (pe & 7) == 0);
}


static bool IsArc_Pe(const Byte *p, size_t size)
{
  if (size < 2)
    return false;
  if (p[0] != 'M' || p[1] != 'Z')
    return false;
  if (size < k_PE_StartSize)
    return false; // k_IsArc_Res_NEED_MORE;
  UInt32 pe = Get32(p + 0x3C);
  if (!CheckPeOffset(pe))
    return false;
  if (pe + k_PE_HeaderSize > size)
    return false; // k_IsArc_Res_NEED_MORE;

  p += pe;
  if (Get32(p) != 0x00004550)
    return false;
  return Get16(p + 4 + 16) >= k_PE_OptHeader32_Size_MIN;
}

HRESULT CInArchive::Open(IInStream *inStream, const UInt64 *maxCheckStartPosition)
{
  Clear();
  
  RINOK(inStream->Seek(0, STREAM_SEEK_CUR, &StartOffset));
  
  const UInt32 kStartHeaderSize = 4 * 7;
  const unsigned kStep = 512; // nsis start is aligned for 512
  Byte buf[kStep];
  UInt64 pos = StartOffset;
  size_t bufSize = 0;
  UInt64 pePos = (UInt64)(Int64)-1;
  
  for (;;)
  {
    bufSize = kStep;
    RINOK(ReadStream(inStream, buf, &bufSize));
    if (bufSize < kStartHeaderSize)
      return S_FALSE;
    if (memcmp(buf + 4, kSignature, kSignatureSize) == 0)
      break;
    if (IsArc_Pe(buf, bufSize))
      pePos = pos;
    pos += kStep;
    UInt64 proc = pos - StartOffset;
    if (maxCheckStartPosition && proc > *maxCheckStartPosition)
    {
      if (pePos == 0)
      {
        if (proc > (1 << 20))
          return S_FALSE;
      }
      else
        return S_FALSE;
    }
  }
  
  if (pePos == (UInt64)(Int64)-1)
  {
    UInt64 posCur = StartOffset;
    for (;;)
    {
      if (posCur < kStep)
        break;
      posCur -= kStep;
      if (pos - posCur > (1 << 20))
        break;
      bufSize = kStep;
      RINOK(inStream->Seek(posCur, STREAM_SEEK_SET, NULL));
      RINOK(ReadStream(inStream, buf, &bufSize));
      if (bufSize < kStep)
        break;
      if (IsArc_Pe(buf, bufSize))
      {
        pePos = posCur;
        break;
      }
    }

    // restore buf to nsis header
    bufSize = kStep;
    RINOK(inStream->Seek(pos, STREAM_SEEK_SET, NULL));
    RINOK(ReadStream(inStream, buf, &bufSize));
    if (bufSize < kStartHeaderSize)
      return S_FALSE;
  }

  StartOffset = pos;
  UInt32 peSize = 0;
  
  if (pePos != (UInt64)(Int64)-1)
  {
    UInt64 peSize64 = (pos - pePos);
    if (peSize64 < (1 << 20))
    {
      peSize = (UInt32)peSize64;
      StartOffset = pePos;
    }
  }

  DataStreamOffset = pos + kStartHeaderSize;
  FirstHeader.Flags = Get32(buf);
  if ((FirstHeader.Flags & (~kFlagsMask)) != 0)
  {
    // return E_NOTIMPL;
    return S_FALSE;
  }
  IsInstaller = (FirstHeader.Flags & NFlags::kUninstall) == 0;

  FirstHeader.HeaderSize = Get32(buf + kSignatureSize + 4);
  FirstHeader.ArcSize = Get32(buf + kSignatureSize + 8);
  if (FirstHeader.ArcSize <= kStartHeaderSize)
    return S_FALSE;

  /*
  if ((FirstHeader.Flags & NFlags::k_BI_ExternalFileSupport) != 0)
  {
    UInt32 datablock_low = Get32(buf + kSignatureSize + 12);
    UInt32 datablock_high = Get32(buf + kSignatureSize + 16);
  }
  */
  
  RINOK(inStream->Seek(0, STREAM_SEEK_END, &_fileSize));

  IsArc = true;

  if (peSize != 0)
  {
    ExeStub.Alloc(peSize);
    RINOK(inStream->Seek(pePos, STREAM_SEEK_SET, NULL));
    RINOK(ReadStream_FALSE(inStream, ExeStub, peSize));
  }

  HRESULT res = S_FALSE;
  try
  {
    CLimitedInStream *_limitedStreamSpec = new CLimitedInStream;
    _stream = _limitedStreamSpec;
    _limitedStreamSpec->SetStream(inStream);
    _limitedStreamSpec->InitAndSeek(pos, FirstHeader.ArcSize);
    DataStreamOffset -= pos;
    res = Open2(buf + kStartHeaderSize, bufSize - kStartHeaderSize);
  }
  catch(...)
  {
    _stream.Release();
    throw;
    // res = S_FALSE;
  }
  if (res != S_OK)
  {
    _stream.Release();
    // Clear();
  }
  return res;
}

UString CInArchive::ConvertToUnicode(const AString &s) const
{
  if (IsUnicode)
  {
    UString res;
    // if (
      ConvertUTF8ToUnicode(s, res);
      return res;
  }
  return MultiByteToUnicodeString(s);
}

void CInArchive::Clear2()
{
  IsUnicode = false;
  NsisType = k_NsisType_Nsis2;
  IsNsis225 = false;
  IsNsis200 = false;
  LogCmdIsEnabled = false;
  BadCmd = -1;
  Is64Bit = false;

  #ifdef NSIS_SCRIPT
  Name.Empty();
  BrandingText.Empty();
  Script.Empty();
  LicenseFiles.Clear();
  _numRootLicenses = 0;
  _numLangStrings = 0;
  langStrIDs.Clear();
  LangComment.Empty();
  noParseStringIndexes.Clear();
  #endif

  APrefixes.Clear();
  UPrefixes.Clear();
  Items.Clear();
  IsUnicode = false;
  ExeStub.Free();
}

void CInArchive::Clear()
{
  Clear2();
  IsArc = false;
  _stream.Release();
}

}}
