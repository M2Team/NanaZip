// CompressDialog.cpp

#include "StdAfx.h"

#include "../../../../C/CpuArch.h"

#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/System.h"

#include "../../Common/MethodProps.h"

#include "../FileManager/BrowseDialog.h"
#include "../FileManager/FormatUtils.h"
#include "../FileManager/SplitUtils.h"

#include "../Explorer/MyMessages.h"

#include "../Common/ZipRegistry.h"

#include "CompressDialog.h"

#ifndef _UNICODE
extern bool g_IsNT;
#endif

#ifdef LANG
#include "../FileManager/LangUtils.h"
#endif

#include "CompressDialogRes.h"
#include "ExtractRes.h"

#ifdef LANG
static const UInt32 kLangIDs[] =
{
  IDT_COMPRESS_ARCHIVE,
  IDT_COMPRESS_UPDATE_MODE,
  IDT_COMPRESS_FORMAT,
  IDT_COMPRESS_LEVEL,
  IDT_COMPRESS_METHOD,
  IDT_COMPRESS_DICTIONARY,
  IDT_COMPRESS_ORDER,
  IDT_COMPRESS_SOLID,
  IDT_COMPRESS_THREADS,
  IDT_COMPRESS_PARAMETERS,

  IDG_COMPRESS_OPTIONS,
  IDX_COMPRESS_SFX,
  IDX_COMPRESS_SHARED,
  IDX_COMPRESS_DEL,

  IDT_COMPRESS_MEMORY,
  IDT_COMPRESS_MEMORY_DE,

  IDX_COMPRESS_NT_SYM_LINKS,
  IDX_COMPRESS_NT_HARD_LINKS,
  IDX_COMPRESS_NT_ALT_STREAMS,
  IDX_COMPRESS_NT_SECUR,

  IDG_COMPRESS_ENCRYPTION,
  IDT_COMPRESS_ENCRYPTION_METHOD,
  IDX_COMPRESS_ENCRYPT_FILE_NAMES,

  IDT_PASSWORD_ENTER,
  IDT_PASSWORD_REENTER,
  IDX_PASSWORD_SHOW,

  IDT_SPLIT_TO_VOLUMES,
  IDT_COMPRESS_PATH_MODE
};
#endif

using namespace NWindows;
using namespace NFile;
using namespace NName;
using namespace NDir;

static const unsigned kHistorySize = 20;

static const UInt32 kSolidLog_NoSolid = 0;
static const UInt32 kSolidLog_FullSolid = 64;

static const UInt32 kLzmaMaxDictSize = (UInt32)15 << 28;

static LPCSTR const kExeExt = ".exe";

#define k7zFormat "7z"

static const UInt32 g_Levels[] =
{
  IDS_METHOD_STORE,
  IDS_METHOD_FASTEST,
  0,
  IDS_METHOD_FAST,
  0,
  IDS_METHOD_NORMAL,
  0,
  IDS_METHOD_MAXIMUM,
  0,
  IDS_METHOD_ULTRA
};

enum EMethodID
{
  kCopy,
  kLZMA,
  kLZMA2,
  kPPMd,
  kBZip2,
  kDeflate,
  kDeflate64,
  kPPMdZip,
  kSha256,
  kSha1,
  kCrc32,
  kCrc64,
};

static LPCSTR const kMethodsNames[] =
{
    "Copy"
  , "LZMA"
  , "LZMA2"
  , "PPMd"
  , "BZip2"
  , "Deflate"
  , "Deflate64"
  , "PPMd"
  , "SHA256"
  , "SHA1"
  , "CRC32"
  , "CRC64"
};

static const EMethodID g_7zMethods[] =
{
  kLZMA2,
  kLZMA,
  kPPMd,
  kBZip2
  , kDeflate
  , kDeflate64
  , kCopy
};

static const EMethodID g_7zSfxMethods[] =
{
  kCopy,
  kLZMA,
  kLZMA2,
  kPPMd
};

static const EMethodID g_ZipMethods[] =
{
  kDeflate,
  kDeflate64,
  kBZip2,
  kLZMA,
  kPPMdZip
};

static const EMethodID g_GZipMethods[] =
{
  kDeflate
};

static const EMethodID g_BZip2Methods[] =
{
  kBZip2
};

static const EMethodID g_XzMethods[] =
{
  kLZMA2
};

static const EMethodID g_SwfcMethods[] =
{
  kDeflate
  // kLZMA
};

static const EMethodID g_HashMethods[] =
{
    kSha256
  , kSha1
  // , kCrc32
  // , kCrc64
};

static const UInt32 kFF_Filter      = 1 << 0;
static const UInt32 kFF_Solid       = 1 << 1;
static const UInt32 kFF_MultiThread = 1 << 2;
static const UInt32 kFF_Encrypt     = 1 << 3;
static const UInt32 kFF_EncryptFileNames  = 1 << 4;
static const UInt32 kFF_MemUse      = 1 << 5;
static const UInt32 kFF_SFX         = 1 << 6;

struct CFormatInfo
{
  LPCSTR Name;
  UInt32 LevelsMask;
  unsigned NumMethods;
  const EMethodID *MethodIDs;

  UInt32 Flags;

  bool Filter_() const { return (Flags & kFF_Filter) != 0; }
  bool Solid_() const { return (Flags & kFF_Solid) != 0; }
  bool MultiThread_() const { return (Flags & kFF_MultiThread) != 0; }
  bool Encrypt_() const { return (Flags & kFF_Encrypt) != 0; }
  bool EncryptFileNames_() const { return (Flags & kFF_EncryptFileNames) != 0; }
  bool MemUse_() const { return (Flags & kFF_MemUse) != 0; }
  bool SFX_() const { return (Flags & kFF_SFX) != 0; }
};

#define METHODS_PAIR(x) ARRAY_SIZE(x), x

static const CFormatInfo g_Formats[] =
{
  {
    "",
    // (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    ((UInt32)1 << 10) - 1,
    // (UInt32)(Int32)-1,
    0, NULL,
    kFF_MultiThread | kFF_MemUse
  },
  {
    k7zFormat,
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    METHODS_PAIR(g_7zMethods),
    kFF_Filter | kFF_Solid | kFF_MultiThread | kFF_Encrypt |
    kFF_EncryptFileNames | kFF_MemUse | kFF_SFX
  },
  {
    "Zip",
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    METHODS_PAIR(g_ZipMethods),
    kFF_MultiThread | kFF_Encrypt | kFF_MemUse
  },
  {
    "GZip",
    (1 << 1) | (1 << 5) | (1 << 7) | (1 << 9),
    METHODS_PAIR(g_GZipMethods),
    kFF_MemUse
  },
  {
    "BZip2",
    (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    METHODS_PAIR(g_BZip2Methods),
    kFF_MultiThread | kFF_MemUse
  },
  {
    "xz",
    (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    METHODS_PAIR(g_XzMethods),
    kFF_Solid | kFF_MultiThread | kFF_MemUse
  },
  {
    "Swfc",
    (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    METHODS_PAIR(g_SwfcMethods),
    0
  },
  {
    "Tar",
    (1 << 0),
    0, NULL,
    0
  },
  {
    "wim",
    (1 << 0),
    0, NULL,
    0
  },
  {
    "Hash",
    (0 << 0),
    METHODS_PAIR(g_HashMethods),
    0
  }
};

static bool IsMethodSupportedBySfx(int methodID)
{
  for (unsigned i = 0; i < ARRAY_SIZE(g_7zSfxMethods); i++)
    if (methodID == g_7zSfxMethods[i])
      return true;
  return false;
}


static const
  // NCompressDialog::NUpdateMode::EEnum
  int
  k_UpdateMode_Vals[] =
{
  NCompressDialog::NUpdateMode::kAdd,
  NCompressDialog::NUpdateMode::kUpdate,
  NCompressDialog::NUpdateMode::kFresh,
  NCompressDialog::NUpdateMode::kSync
};

static const UInt32 k_UpdateMode_IDs[] =
{
  IDS_COMPRESS_UPDATE_MODE_ADD,
  IDS_COMPRESS_UPDATE_MODE_UPDATE,
  IDS_COMPRESS_UPDATE_MODE_FRESH,
  IDS_COMPRESS_UPDATE_MODE_SYNC
};

static const
  // NWildcard::ECensorPathMode
  int
  k_PathMode_Vals[] =
{
  NWildcard::k_RelatPath,
  NWildcard::k_FullPath,
  NWildcard::k_AbsPath,
};

static const UInt32 k_PathMode_IDs[] =
{
  IDS_PATH_MODE_RELAT,
  IDS_EXTRACT_PATHS_FULL,
  IDS_EXTRACT_PATHS_ABS
};

void AddComboItems(NControl::CComboBox &combo, const UInt32 *langIDs, unsigned numItems, const int *values, int curVal);
bool GetBoolsVal(const CBoolPair &b1, const CBoolPair &b2);

void CCompressDialog::CheckButton_TwoBools(UINT id, const CBoolPair &b1, const CBoolPair &b2)
{
  CheckButton(id, GetBoolsVal(b1, b2));
}

void CCompressDialog::GetButton_Bools(UINT id, CBoolPair &b1, CBoolPair &b2)
{
  bool val = IsButtonCheckedBool(id);
  bool oldVal = GetBoolsVal(b1, b2);
  if (val != oldVal)
    b1.Def = b2.Def = true;
  b1.Val = b2.Val = val;
}


void CCompressDialog::SetMethods(const CObjectVector<CCodecInfoUser> &userCodecs)
{
  ExternalMethods.Clear();
  {
    FOR_VECTOR (i, userCodecs)
    {
      const CCodecInfoUser &c = userCodecs[i];
      if (!c.EncoderIsAssigned
          || !c.IsFilter_Assigned
          || c.IsFilter
          || c.NumStreams != 1)
        continue;
      unsigned k;
      for (k = 0; k < ARRAY_SIZE(g_7zMethods); k++)
        if (c.Name.IsEqualTo_Ascii_NoCase(kMethodsNames[g_7zMethods[k]]))
          break;
      if (k != ARRAY_SIZE(g_7zMethods))
        continue;
      ExternalMethods.Add(c.Name);
    }
  }
}


bool CCompressDialog::OnInit()
{
  #ifdef LANG
  LangSetWindowText(*this, IDD_COMPRESS);
  LangSetDlgItems(*this, kLangIDs, ARRAY_SIZE(kLangIDs));
  #endif

  {
    UInt64 size = (UInt64)(sizeof(size_t)) << 29;
    _ramSize_Defined = NSystem::GetRamSize(size);
    // size = (UInt64)3 << 62; // for debug only;
    _ramSize = size;
    const UInt64 kMinUseSize = (1 << 26);
    if (size < kMinUseSize)
      size = kMinUseSize;

    unsigned bits = sizeof(size_t) * 8;
    if (bits == 32)
    {
      const UInt32 limit2 = (UInt32)7 << 28;
      if (size > limit2)
        size = limit2;
    }

    _ramSize_Reduced = size;

    // 80% - is auto usage limit in handlers
    _ramUsage_Auto = Calc_From_Val_Percents(size, 80);
  }

  _password1Control.Attach(GetItem(IDE_COMPRESS_PASSWORD1));
  _password2Control.Attach(GetItem(IDE_COMPRESS_PASSWORD2));
  _password1Control.SetText(Info.Password);
  _password2Control.SetText(Info.Password);
  _encryptionMethod.Attach(GetItem(IDC_COMPRESS_ENCRYPTION_METHOD));
  _default_encryptionMethod_Index = -1;

  m_ArchivePath.Attach(GetItem(IDC_COMPRESS_ARCHIVE));
  m_Format.Attach(GetItem(IDC_COMPRESS_FORMAT));
  m_Level.Attach(GetItem(IDC_COMPRESS_LEVEL));
  m_Method.Attach(GetItem(IDC_COMPRESS_METHOD));
  m_Dictionary.Attach(GetItem(IDC_COMPRESS_DICTIONARY));
  m_Order.Attach(GetItem(IDC_COMPRESS_ORDER));
  m_Solid.Attach(GetItem(IDC_COMPRESS_SOLID));
  m_NumThreads.Attach(GetItem(IDC_COMPRESS_THREADS));
  m_MemUse.Attach(GetItem(IDC_COMPRESS_MEM_USE));

  m_UpdateMode.Attach(GetItem(IDC_COMPRESS_UPDATE_MODE));
  m_PathMode.Attach(GetItem(IDC_COMPRESS_PATH_MODE));

  m_Volume.Attach(GetItem(IDC_COMPRESS_VOLUME));
  m_Params.Attach(GetItem(IDE_COMPRESS_PARAMETERS));

  AddVolumeItems(m_Volume);

  m_RegistryInfo.Load();
  CheckButton(IDX_PASSWORD_SHOW, m_RegistryInfo.ShowPassword);
  CheckButton(IDX_COMPRESS_ENCRYPT_FILE_NAMES, m_RegistryInfo.EncryptHeaders);

  CheckButton_TwoBools(IDX_COMPRESS_NT_SYM_LINKS,   Info.SymLinks,   m_RegistryInfo.SymLinks);
  CheckButton_TwoBools(IDX_COMPRESS_NT_HARD_LINKS,  Info.HardLinks,  m_RegistryInfo.HardLinks);
  CheckButton_TwoBools(IDX_COMPRESS_NT_ALT_STREAMS, Info.AltStreams, m_RegistryInfo.AltStreams);
  CheckButton_TwoBools(IDX_COMPRESS_NT_SECUR,       Info.NtSecurity, m_RegistryInfo.NtSecurity);

  UpdatePasswordControl();

  {
    bool needSetMain = (Info.FormatIndex < 0);
    FOR_VECTOR(i, ArcIndices)
    {
      unsigned arcIndex = ArcIndices[i];
      const CArcInfoEx &ai = (*ArcFormats)[arcIndex];
      int index = (int)m_Format.AddString(ai.Name);
      m_Format.SetItemData(index, arcIndex);
      if (!needSetMain)
      {
        if (Info.FormatIndex == (int)arcIndex)
          m_Format.SetCurSel(index);
        continue;
      }
      if (i == 0 || ai.Name.IsEqualTo_NoCase(m_RegistryInfo.ArcType))
      {
        m_Format.SetCurSel(index);
        Info.FormatIndex = arcIndex;
      }
    }
  }

  CheckButton(IDX_COMPRESS_SFX, Info.SFXMode);

  {
    UString fileName;
    SetArcPathFields(Info.ArcPath, fileName, true);
    StartDirPrefix = DirPrefix;
    SetArchiveName(fileName);
  }

  for (unsigned i = 0; i < m_RegistryInfo.ArcPaths.Size() && i < kHistorySize; i++)
    m_ArchivePath.AddString(m_RegistryInfo.ArcPaths[i]);

  AddComboItems(m_UpdateMode, k_UpdateMode_IDs, ARRAY_SIZE(k_UpdateMode_IDs),
      k_UpdateMode_Vals, Info.UpdateMode);

  AddComboItems(m_PathMode, k_PathMode_IDs, ARRAY_SIZE(k_PathMode_IDs),
      k_PathMode_Vals, Info.PathMode);


  TCHAR s[32] = { TEXT('/'), TEXT(' '), 0 };
  ConvertUInt32ToString(NSystem::GetNumberOfProcessors(), s + 2);
  SetItemText(IDT_COMPRESS_HARDWARE_THREADS, s);

  CheckButton(IDX_COMPRESS_SHARED, Info.OpenShareForWrite);
  CheckButton(IDX_COMPRESS_DEL, Info.DeleteAfterCompressing);

  FormatChanged();

  // OnButtonSFX();

  NormalizePosition();

  return CModalDialog::OnInit();
}

/*
namespace NCompressDialog
{
  bool CInfo::GetFullPathName(UString &result) const
  {
    #ifndef UNDER_CE
    // NDirectory::MySetCurrentDirectory(CurrentDirPrefix);
    #endif
    FString resultF;
    bool res = MyGetFullPathName(us2fs(ArchiveName), resultF);
    result = fs2us(resultF);
    return res;
  }
}
*/

void CCompressDialog::UpdatePasswordControl()
{
  bool showPassword = IsShowPasswordChecked();
  TCHAR c = showPassword ? (TCHAR)0: TEXT('*');
  _password1Control.SetPasswordChar(c);
  _password2Control.SetPasswordChar(c);
  UString password;
  _password1Control.GetText(password);
  _password1Control.SetText(password);
  _password2Control.GetText(password);
  _password2Control.SetText(password);

  ShowItem_Bool(IDT_PASSWORD_REENTER, !showPassword);
  _password2Control.Show_Bool(!showPassword);
}

bool CCompressDialog::OnButtonClicked(int buttonID, HWND buttonHWND)
{
  switch (buttonID)
  {
    case IDB_COMPRESS_SET_ARCHIVE:
    {
      OnButtonSetArchive();
      return true;
    }
    case IDX_COMPRESS_SFX:
    {
      SetMethod(GetMethodID());
      OnButtonSFX();
      SetMemoryUsage();
      return true;
    }
    case IDX_PASSWORD_SHOW:
    {
      UpdatePasswordControl();
      return true;
    }
  }
  return CModalDialog::OnButtonClicked(buttonID, buttonHWND);
}

void CCompressDialog::CheckSFXControlsEnable()
{
  const CFormatInfo &fi = g_Formats[GetStaticFormatIndex()];
  bool enable = fi.SFX_();
  if (enable)
  {
    const int methodID = GetMethodID();
    enable = (methodID == -1 || IsMethodSupportedBySfx(methodID));
  }
  if (!enable)
    CheckButton(IDX_COMPRESS_SFX, false);
  EnableItem(IDX_COMPRESS_SFX, enable);
}

/*
void CCompressDialog::CheckVolumeEnable()
{
  bool isSFX = IsSFX();
  m_Volume.Enable(!isSFX);
  if (isSFX)
    m_Volume.SetText(TEXT(""));
}
*/

void CCompressDialog::EnableMultiCombo(unsigned id)
{
  NWindows::NControl::CComboBox combo;
  combo.Attach(GetItem(id));
  const bool enable = (combo.GetCount() > 1);
  EnableItem(id, enable);
}


void CCompressDialog::FormatChanged()
{
  SetLevel();
  SetSolidBlockSize();
  SetParams();
  SetMemUseCombo();
  SetNumThreads();

  const CFormatInfo &fi = g_Formats[GetStaticFormatIndex()];
  Info.SolidIsSpecified = fi.Solid_();
  Info.EncryptHeadersIsAllowed = fi.EncryptFileNames_();

  /*
  const bool multiThreadEnable = fi.MultiThread;
  Info.MultiThreadIsAllowed = multiThreadEnable;
  EnableItem(IDC_COMPRESS_SOLID, fi.Solid);
  EnableItem(IDC_COMPRESS_THREADS, multiThreadEnable);
  const bool methodEnable = (fi.MethodIDs != NULL);
  EnableItem(IDC_COMPRESS_METHOD, methodEnable);
  EnableMultiCombo(IDC_COMPRESS_DICTIONARY, methodEnable);
  EnableItem(IDC_COMPRESS_ORDER, methodEnable);
  */

  CheckSFXControlsEnable();

  {
    const CArcInfoEx &ai = Get_ArcInfoEx();

    ShowItem_Bool(IDX_COMPRESS_NT_SYM_LINKS, ai.Flags_SymLinks());
    ShowItem_Bool(IDX_COMPRESS_NT_HARD_LINKS, ai.Flags_HardLinks());
    ShowItem_Bool(IDX_COMPRESS_NT_ALT_STREAMS, ai.Flags_AltStreams());
    ShowItem_Bool(IDX_COMPRESS_NT_SECUR, ai.Flags_NtSecure());

    ShowItem_Bool(IDG_COMPRESS_NTFS,
           ai.Flags_SymLinks()
        || ai.Flags_HardLinks()
        || ai.Flags_AltStreams()
        || ai.Flags_NtSecure());
  }
  // CheckVolumeEnable();

  const bool encrypt = fi.Encrypt_();
  EnableItem(IDG_COMPRESS_ENCRYPTION, encrypt);

  EnableItem(IDT_PASSWORD_ENTER, encrypt);
  EnableItem(IDT_PASSWORD_REENTER, encrypt);
  EnableItem(IDE_COMPRESS_PASSWORD1, encrypt);
  EnableItem(IDE_COMPRESS_PASSWORD2, encrypt);
  EnableItem(IDX_PASSWORD_SHOW, encrypt);

  EnableItem(IDT_COMPRESS_ENCRYPTION_METHOD, encrypt);
  EnableItem(IDC_COMPRESS_ENCRYPTION_METHOD, encrypt);
  EnableItem(IDX_COMPRESS_ENCRYPT_FILE_NAMES, fi.EncryptFileNames_());

  ShowItem_Bool(IDX_COMPRESS_ENCRYPT_FILE_NAMES, fi.EncryptFileNames_());

  SetEncryptionMethod();
  SetMemoryUsage();
}


bool CCompressDialog::IsSFX()
{
  CWindow sfxButton = GetItem(IDX_COMPRESS_SFX);
  return sfxButton.IsEnabled() && IsButtonCheckedBool(IDX_COMPRESS_SFX);
}

static int GetExtDotPos(const UString &s)
{
  int dotPos = s.ReverseFind_Dot();
  if (dotPos > s.ReverseFind_PathSepar() + 1)
    return dotPos;
  return -1;
}

void CCompressDialog::OnButtonSFX()
{
  UString fileName;
  m_ArchivePath.GetText(fileName);
  int dotPos = GetExtDotPos(fileName);
  if (IsSFX())
  {
    if (dotPos >= 0)
      fileName.DeleteFrom(dotPos);
    fileName += kExeExt;
    m_ArchivePath.SetText(fileName);
  }
  else
  {
    if (dotPos >= 0)
    {
      UString ext = fileName.Ptr(dotPos);
      if (ext.IsEqualTo_Ascii_NoCase(kExeExt))
      {
        fileName.DeleteFrom(dotPos);
        m_ArchivePath.SetText(fileName);
      }
    }
    SetArchiveName2(false); // it's for OnInit
  }

  // CheckVolumeEnable();
}

bool CCompressDialog::GetFinalPath_Smart(UString &resPath)
{
  UString name;
  m_ArchivePath.GetText(name);
  name.Trim();
  UString tempPath = name;
  if (!IsAbsolutePath(name))
  {
    UString newDirPrefix = DirPrefix;
    if (newDirPrefix.IsEmpty())
      newDirPrefix = StartDirPrefix;
    FString resultF;
    if (!MyGetFullPathName(us2fs(newDirPrefix + name), resultF))
      return false;
    tempPath = fs2us(resultF);
  }
  if (!SetArcPathFields(tempPath, name, false))
    return false;
  FString resultF;
  if (!MyGetFullPathName(us2fs(DirPrefix + name), resultF))
    return false;
  resPath = fs2us(resultF);
  return true;
}

bool CCompressDialog::SetArcPathFields(const UString &path, UString &name, bool always)
{
  FString resDirPrefix;
  FString resFileName;
  bool res = GetFullPathAndSplit(us2fs(path), resDirPrefix, resFileName);
  if (res)
  {
    DirPrefix = fs2us(resDirPrefix);
    name = fs2us(resFileName);
  }
  else
  {
    if (!always)
      return false;
    DirPrefix.Empty();
    name = path;
  }
  SetItemText(IDT_COMPRESS_ARCHIVE_FOLDER, DirPrefix);
  m_ArchivePath.SetText(name);
  return res;
}

static const wchar_t * const k_IncorrectPathMessage = L"Incorrect archive path";

void CCompressDialog::OnButtonSetArchive()
{
  UString path;
  if (!GetFinalPath_Smart(path))
  {
    ShowErrorMessage(*this, k_IncorrectPathMessage);
    return;
  }

  UString title = LangString(IDS_COMPRESS_SET_ARCHIVE_BROWSE);
  UString filterDescription = LangString(IDS_OPEN_TYPE_ALL_FILES);
  filterDescription += " (*.*)";
  UString resPath;
  CurrentDirWasChanged = true;
  if (!MyBrowseForFile(*this, title,
      // DirPrefix.IsEmpty() ? NULL : (const wchar_t *)DirPrefix,
      // NULL,
      path,
      filterDescription,
      NULL, // L"*.*",
      resPath))
    return;
  UString dummyName;
  SetArcPathFields(resPath, dummyName, true);
}

// in ExtractDialog.cpp
extern void AddUniqueString(UStringVector &strings, const UString &srcString);

static bool IsAsciiString(const UString &s)
{
  for (unsigned i = 0; i < s.Len(); i++)
  {
    wchar_t c = s[i];
    if (c < 0x20 || c > 0x7F)
      return false;
  }
  return true;
}


static void AddSize_MB(UString &s, UInt64 size)
{
  const UInt64 v2 = size + ((UInt32)1 << 20) - 1;
  if (size <= v2)
    size = v2;
  s.Add_UInt64(size >> 20);
  s += " MB";
}


void SetErrorMessage_MemUsage(UString &s, UInt64 reqSize, UInt64 ramSize, UInt64 ramLimit, const UString &usageString)
{
  s += "The operation was blocked by NanaZip";
  s.Add_LF();
  s += "The operation can require big amount of RAM (memory):";
  s.Add_LF();
  s.Add_LF();
  AddSize_MB(s, reqSize);

  if (!usageString.IsEmpty())
  {
    s += " : ";
    s += usageString;
  }

  s.Add_LF();
  AddSize_MB(s, ramSize);
  s += " : RAM";

  // if (ramLimit != 0)
  {
    s.Add_LF();
    AddSize_MB(s, ramLimit);
    s += " : NanaZip limit";
  }

  s.Add_LF();
  s.Add_LF();
  //s += LangString(IDS_MEM_ERROR);
  s += "Do you want to continue?";
}


void CCompressDialog::OnOK()
{
  _password1Control.GetText(Info.Password);
  if (IsZipFormat())
  {
    if (!IsAsciiString(Info.Password))
    {
      ShowErrorMessageHwndRes(*this, IDS_PASSWORD_USE_ASCII);
      return;
    }
    UString method = GetEncryptionMethodSpec();
    if (method.IsPrefixedBy_Ascii_NoCase("aes"))
    {
      if (Info.Password.Len() > 99)
      {
        ShowErrorMessageHwndRes(*this, IDS_PASSWORD_TOO_LONG);
        return;
      }
    }
  }
  if (!IsShowPasswordChecked())
  {
    UString password2;
    _password2Control.GetText(password2);
    if (password2 != Info.Password)
    {
      ShowErrorMessageHwndRes(*this, IDS_PASSWORD_NOT_MATCH);
      return;
    }
  }

  {
    UInt64 decompressMem;
    const UInt64 memUsage = GetMemoryUsage_DecompMem(decompressMem);
    if (memUsage != (UInt64)(Int64)-1)
    {
      const UInt64 limit = Get_MemUse_Bytes();
      if (memUsage > limit)
      {
        UString s;
        UString s2 = LangString(IDT_COMPRESS_MEMORY);
        if (s2.IsEmpty())
          GetItemText(IDT_COMPRESS_MEMORY, s2);
        SetErrorMessage_MemUsage(s, memUsage, _ramSize, limit, s2);
        //MessageBoxError(s);
        if (IDOK != ::MessageBoxW(
            *this,
            s,
            L"NanaZip",
            MB_ICONWARNING | MB_OKCANCEL))
        {
            return;
        }
      }
    }
  }

  SaveOptionsInMem();
  {
    UString s;
    if (!GetFinalPath_Smart(s))
    {
      ShowErrorMessage(*this, k_IncorrectPathMessage);
      return;
    }

    m_RegistryInfo.ArcPaths.Clear();
    AddUniqueString(m_RegistryInfo.ArcPaths, s);
    Info.ArcPath = s;
  }

  Info.UpdateMode = (NCompressDialog::NUpdateMode::EEnum)k_UpdateMode_Vals[m_UpdateMode.GetCurSel()];;
  Info.PathMode = (NWildcard::ECensorPathMode)k_PathMode_Vals[m_PathMode.GetCurSel()];

  Info.Level = GetLevelSpec();
  Info.Dict64 = GetDictSpec();
  Info.Order = GetOrderSpec();
  Info.OrderMode = GetOrderMode();
  Info.NumThreads = GetNumThreadsSpec();

  Info.MemUsage.Clear();
  {
    const UString mus = Get_MemUse_Spec();
    if (!mus.IsEmpty())
    {
      NCompression::CMemUse mu;
      mu.Parse(mus);
      if (mu.IsDefined)
        Info.MemUsage = mu;
    }
  }

  {
    // Info.SolidIsSpecified = g_Formats[GetStaticFormatIndex()].Solid;
    const UInt32 solidLogSize = GetBlockSizeSpec();
    Info.SolidBlockSize = 0;
    if (solidLogSize == (UInt32)(Int32)-1)
      Info.SolidIsSpecified = false;
    else if (solidLogSize > 0)
      Info.SolidBlockSize = (solidLogSize >= 64) ?
          (UInt64)(Int64)-1 :
          ((UInt64)1 << solidLogSize);
  }

  Info.Method = GetMethodSpec();
  Info.EncryptionMethod = GetEncryptionMethodSpec();
  Info.FormatIndex = GetFormatIndex();
  Info.SFXMode = IsSFX();
  Info.OpenShareForWrite = IsButtonCheckedBool(IDX_COMPRESS_SHARED);
  Info.DeleteAfterCompressing = IsButtonCheckedBool(IDX_COMPRESS_DEL);

  m_RegistryInfo.EncryptHeaders =
    Info.EncryptHeaders = IsButtonCheckedBool(IDX_COMPRESS_ENCRYPT_FILE_NAMES);


  GetButton_Bools(IDX_COMPRESS_NT_SYM_LINKS,   Info.SymLinks,   m_RegistryInfo.SymLinks);
  GetButton_Bools(IDX_COMPRESS_NT_HARD_LINKS,  Info.HardLinks,  m_RegistryInfo.HardLinks);
  GetButton_Bools(IDX_COMPRESS_NT_ALT_STREAMS, Info.AltStreams, m_RegistryInfo.AltStreams);
  GetButton_Bools(IDX_COMPRESS_NT_SECUR,       Info.NtSecurity, m_RegistryInfo.NtSecurity);

  {
    const CArcInfoEx &ai = Get_ArcInfoEx();
    if (!ai.Flags_SymLinks()) Info.SymLinks.Val = false;
    if (!ai.Flags_HardLinks()) Info.HardLinks.Val = false;
    if (!ai.Flags_AltStreams()) Info.AltStreams.Val = false;
    if (!ai.Flags_NtSecure()) Info.NtSecurity.Val = false;
  }

  m_Params.GetText(Info.Options);

  UString volumeString;
  m_Volume.GetText(volumeString);
  volumeString.Trim();
  Info.VolumeSizes.Clear();

  if (!volumeString.IsEmpty())
  {
    if (!ParseVolumeSizes(volumeString, Info.VolumeSizes))
    {
      ShowErrorMessageHwndRes(*this, IDS_INCORRECT_VOLUME_SIZE);
      return;
    }
    if (!Info.VolumeSizes.IsEmpty())
    {
      const UInt64 volumeSize = Info.VolumeSizes.Back();
      if (volumeSize < (100 << 10))
      {
        wchar_t s[32];
        ConvertUInt64ToString(volumeSize, s);
        if (::MessageBoxW(*this, MyFormatNew(IDS_SPLIT_CONFIRM, s),
            L"NanaZip", MB_YESNOCANCEL | MB_ICONQUESTION) != IDYES)
          return;
      }
    }
  }

  for (int i = 0; i < m_ArchivePath.GetCount(); i++)
  {
    UString sTemp;
    m_ArchivePath.GetLBText(i, sTemp);
    sTemp.Trim();
    AddUniqueString(m_RegistryInfo.ArcPaths, sTemp);
  }

  if (m_RegistryInfo.ArcPaths.Size() > kHistorySize)
    m_RegistryInfo.ArcPaths.DeleteBack();

  if (Info.FormatIndex >= 0)
    m_RegistryInfo.ArcType = (*ArcFormats)[Info.FormatIndex].Name;
  m_RegistryInfo.ShowPassword = IsShowPasswordChecked();

  m_RegistryInfo.Save();

  CModalDialog::OnOK();
}

bool CCompressDialog::OnCommand(int code, int itemID, LPARAM lParam)
{
  if (code == CBN_SELCHANGE)
  {
    switch (itemID)
    {
      case IDC_COMPRESS_ARCHIVE:
      {
        // we can 't change m_ArchivePath in that handler !
        DirPrefix.Empty();
        SetItemText(IDT_COMPRESS_ARCHIVE_FOLDER, DirPrefix);

        /*
        UString path;
        m_ArchivePath.GetText(path);
        m_ArchivePath.SetText(L"");
        if (IsAbsolutePath(path))
        {
          UString fileName;
          SetArcPathFields(path, fileName);
          SetArchiveName(fileName);
        }
        */
        return true;
      }

      case IDC_COMPRESS_FORMAT:
      {
        const bool isSFX = IsSFX();
        SaveOptionsInMem();
        FormatChanged();
        SetArchiveName2(isSFX);
        return true;
      }

      case IDC_COMPRESS_LEVEL:
      {
        Get_FormatOptions().ResetForLevelChange();

        SetMethod();
        SetSolidBlockSize();
        SetNumThreads();
        CheckSFXNameChange();
        SetMemoryUsage();
        return true;
      }

      case IDC_COMPRESS_METHOD:
      {
        MethodChanged();
        SetSolidBlockSize();
        SetNumThreads();
        CheckSFXNameChange();
        SetMemoryUsage();
        if (Get_ArcInfoEx().Flags_HashHandler())
          SetArchiveName2(false);
        return true;
      }

      case IDC_COMPRESS_DICTIONARY:
      {
        /* we want to change the reported threads for Auto line
           and keep selected NumThreads option
           So we save selected NumThreads option in memory */
        SaveOptionsInMem();
        const UInt32 blockSizeLog = GetBlockSizeSpec();
        if (// blockSizeLog != (UInt32)(Int32)-1 &&
               blockSizeLog != kSolidLog_NoSolid
            && blockSizeLog != kSolidLog_FullSolid)
        {
          Get_FormatOptions().Reset_BlockLogSize();
          // SetSolidBlockSize(true);
        }

        SetSolidBlockSize();
        SetNumThreads(); // we want to change the reported threads for Auto line only
        SetMemoryUsage();
        return true;
      }

      case IDC_COMPRESS_ORDER:
        return true;

      case IDC_COMPRESS_SOLID:
      {
        SetMemoryUsage();
        return true;
      }

      case IDC_COMPRESS_THREADS:
      {
        SetMemoryUsage();
        return true;
      }

      case IDC_COMPRESS_MEM_USE:
      {
        /* we want to change the reported threads for Auto line
           and keep selected NumThreads option
           So we save selected NumThreads option in memory */
        SaveOptionsInMem();

        SetNumThreads(); // we want to change the reported threads for Auto line only
        SetMemoryUsage();
        return true;
      }
    }
  }
  return CModalDialog::OnCommand(code, itemID, lParam);
}

void CCompressDialog::CheckSFXNameChange()
{
  const bool isSFX = IsSFX();
  CheckSFXControlsEnable();
  if (isSFX != IsSFX())
    SetArchiveName2(isSFX);
}

void CCompressDialog::SetArchiveName2(bool prevWasSFX)
{
  UString fileName;
  m_ArchivePath.GetText(fileName);
  const CArcInfoEx &prevArchiverInfo = (*ArcFormats)[m_PrevFormat];
  if (prevArchiverInfo.Flags_KeepName() || Info.KeepName)
  {
    UString prevExtension;
    if (prevWasSFX)
      prevExtension = kExeExt;
    else
    {
      prevExtension += '.';
      prevExtension += prevArchiverInfo.GetMainExt();
    }
    const unsigned prevExtensionLen = prevExtension.Len();
    if (fileName.Len() >= prevExtensionLen)
      if (StringsAreEqualNoCase(fileName.RightPtr(prevExtensionLen), prevExtension))
        fileName.DeleteFrom(fileName.Len() - prevExtensionLen);
  }
  SetArchiveName(fileName);
}

// if type.KeepName then use OriginalFileName
// else if !KeepName remove extension
// add new extension

void CCompressDialog::SetArchiveName(const UString &name)
{
  UString fileName = name;
  Info.FormatIndex = GetFormatIndex();
  const CArcInfoEx &ai = (*ArcFormats)[Info.FormatIndex];
  m_PrevFormat = Info.FormatIndex;
  if (ai.Flags_KeepName())
  {
    fileName = OriginalFileName;
  }
  else
  {
    if (!Info.KeepName)
    {
      int dotPos = GetExtDotPos(fileName);
      if (dotPos >= 0)
        fileName.DeleteFrom(dotPos);
    }
  }

  if (IsSFX())
    fileName += kExeExt;
  else
  {
    fileName += '.';
    UString ext = ai.GetMainExt();
    if (ai.Flags_HashHandler())
    {
      UString estimatedName;
      GetMethodSpec(estimatedName);
      if (!estimatedName.IsEmpty())
      {
        ext = estimatedName;
        ext.MakeLower_Ascii();
      }
    }
    fileName += ext;
  }
  m_ArchivePath.SetText(fileName);
}

int CCompressDialog::FindRegistryFormat(const UString &name)
{
  FOR_VECTOR (i, m_RegistryInfo.Formats)
  {
    const NCompression::CFormatOptions &fo = m_RegistryInfo.Formats[i];
    if (name.IsEqualTo_NoCase(GetUnicodeString(fo.FormatID)))
      return i;
  }
  return -1;
}

int CCompressDialog::FindRegistryFormatAlways(const UString &name)
{
  int index = FindRegistryFormat(name);
  if (index < 0)
  {
    NCompression::CFormatOptions fo;
    fo.FormatID = GetSystemString(name);
    index = m_RegistryInfo.Formats.Add(fo);
  }
  return index;
}

int CCompressDialog::GetStaticFormatIndex()
{
  const CArcInfoEx &ai = Get_ArcInfoEx();
  for (unsigned i = 0; i < ARRAY_SIZE(g_Formats); i++)
    if (ai.Name.IsEqualTo_Ascii_NoCase(g_Formats[i].Name))
      return i;
  return 0; // -1;
}

void CCompressDialog::SetNearestSelectComboBox(NControl::CComboBox &comboBox, UInt32 value)
{
  for (int i = comboBox.GetCount() - 1; i >= 0; i--)
    if ((UInt32)comboBox.GetItemData(i) <= value)
    {
      comboBox.SetCurSel(i);
      return;
    }
  if (comboBox.GetCount() > 0)
    comboBox.SetCurSel(0);
}

void CCompressDialog::SetLevel2()
{
  m_Level.ResetContent();
  const CFormatInfo &fi = g_Formats[GetStaticFormatIndex()];
  const CArcInfoEx &ai = Get_ArcInfoEx();
  UInt32 level = 5;
  {
    int index = FindRegistryFormat(ai.Name);
    if (index >= 0)
    {
      const NCompression::CFormatOptions &fo = m_RegistryInfo.Formats[index];
      if (fo.Level <= 9)
        level = fo.Level;
      else if (fo.Level == (UInt32)(Int32)-1)
        level = 5;
      else
        level = 9;
    }
  }

  for (unsigned i = 0; i < sizeof(UInt32) * 8; i++)
  {
    const UInt32 mask = (UInt32)1 << i;
    if ((fi.LevelsMask & mask) != 0)
    {
      UInt32 langID = g_Levels[i];
      UString s;
      s.Add_UInt32(i);
      if (langID)
      {
          s += " (";
          s += LangString(langID);
          s += ")";
      }
      int index = (int)m_Level.AddString(s);
      m_Level.SetItemData(index, i);
    }
    if (fi.LevelsMask <= mask)
      break;
  }
  SetNearestSelectComboBox(m_Level, level);
}


static LRESULT ComboBox_AddStringAscii(NControl::CComboBox &cb, const char *s)
{
  return cb.AddString((CSysString)s);
}

// static const char *k_Auto = "- "; // "auto : ";

static void Modify_Auto(AString &s)
{
  s.Insert(0, "*  ");
  // s += " -";
}

void CCompressDialog::SetMethod2(int keepMethodId)
{
  m_Method.ResetContent();
  _auto_MethodId = -1;
  const CFormatInfo &fi = g_Formats[GetStaticFormatIndex()];
  const CArcInfoEx &ai = Get_ArcInfoEx();
  if (GetLevel() == 0 && !ai.Flags_HashHandler())
  {
    MethodChanged();
    return;
  }
  UString defaultMethod;
  {
    const int index = FindRegistryFormat(ai.Name);
    if (index >= 0)
    {
      const NCompression::CFormatOptions &fo = m_RegistryInfo.Formats[index];
      defaultMethod = fo.Method;
    }
  }
  const bool isSfx = IsSFX();
  bool weUseSameMethod = false;

  const bool is7z = ai.Name.IsEqualTo_Ascii_NoCase("7z");

  for (unsigned m = 0;; m++)
  {
    int methodID;
    const char *method;
    if (m < fi.NumMethods)
    {
      methodID = fi.MethodIDs[m];
      method = kMethodsNames[methodID];
      if (is7z)
      if (methodID == kCopy
          || methodID == kDeflate
          || methodID == kDeflate64
          )
        continue;
    }
    else
    {
      if (!is7z)
        break;
      unsigned extIndex = m - fi.NumMethods;
      if (extIndex >= ExternalMethods.Size())
        break;
      methodID = ARRAY_SIZE(kMethodsNames) + extIndex;
      method = ExternalMethods[extIndex].Ptr();
    }
    if (isSfx)
      if (!IsMethodSupportedBySfx(methodID))
        continue;

    AString s (method);
    int writtenMethodId = methodID;
    if (m == 0)
    {
      _auto_MethodId = methodID;
      writtenMethodId = -1;
      Modify_Auto(s);
    }
    const int itemIndex = (int)ComboBox_AddStringAscii(m_Method, s);
    m_Method.SetItemData(itemIndex, writtenMethodId);
    if (keepMethodId == methodID)
    {
      m_Method.SetCurSel(itemIndex);
      weUseSameMethod = true;
      continue;
    }
    if ((defaultMethod.IsEqualTo_Ascii_NoCase(method) || m == 0) && !weUseSameMethod)
      m_Method.SetCurSel(itemIndex);
  }

  if (!weUseSameMethod)
    MethodChanged();
}



bool CCompressDialog::IsZipFormat()
{
  return Get_ArcInfoEx().Name.IsEqualTo_Ascii_NoCase("zip");
}

bool CCompressDialog::IsXzFormat()
{
  return Get_ArcInfoEx().Name.IsEqualTo_Ascii_NoCase("xz");
}

void CCompressDialog::SetEncryptionMethod()
{
  _encryptionMethod.ResetContent();
  _default_encryptionMethod_Index = -1;
  const CArcInfoEx &ai = Get_ArcInfoEx();
  if (ai.Name.IsEqualTo_Ascii_NoCase("7z"))
  {
    ComboBox_AddStringAscii(_encryptionMethod, "AES-256");
    _encryptionMethod.SetCurSel(0);
    _default_encryptionMethod_Index = 0;
  }
  else if (ai.Name.IsEqualTo_Ascii_NoCase("zip"))
  {
    int index = FindRegistryFormat(ai.Name);
    UString encryptionMethod;
    if (index >= 0)
    {
      const NCompression::CFormatOptions &fo = m_RegistryInfo.Formats[index];
      encryptionMethod = fo.EncryptionMethod;
    }
    int sel = 0;
    // if (ZipCryptoIsAllowed)
    {
      ComboBox_AddStringAscii(_encryptionMethod, "ZipCrypto");
      sel = (encryptionMethod.IsPrefixedBy_Ascii_NoCase("aes") ? 1 : 0);
      _default_encryptionMethod_Index = 0;
    }
    ComboBox_AddStringAscii(_encryptionMethod, "AES-256");
    _encryptionMethod.SetCurSel(sel);
  }
}


int CCompressDialog::GetMethodID_RAW()
{
  if (m_Method.GetCount() <= 0)
    return -1;
  return (int)(Int32)(UInt32)m_Method.GetItemData_of_CurSel();
}

int CCompressDialog::GetMethodID()
{
  int raw = GetMethodID_RAW();
  if (raw < 0)
    return _auto_MethodId;
  return raw;
}


UString CCompressDialog::GetMethodSpec(UString &estimatedName)
{
  estimatedName.Empty();
  if (m_Method.GetCount() < 1)
    return estimatedName;
  const int methodIdRaw = GetMethodID_RAW();
  int methodId = methodIdRaw;
  if (methodIdRaw < 0)
    methodId = _auto_MethodId;
  UString s;
  if (methodId >= 0)
  {
    if ((unsigned)methodId < ARRAY_SIZE(kMethodsNames))
      estimatedName = kMethodsNames[methodId];
    else
      estimatedName = ExternalMethods[methodId - ARRAY_SIZE(kMethodsNames)];
    if (methodIdRaw >= 0)
      s = estimatedName;
  }
  return s;
}


UString CCompressDialog::GetMethodSpec()
{
  UString estimatedName;
  UString s = GetMethodSpec(estimatedName);
  return s;
}

bool CCompressDialog::IsMethodEqualTo(const UString &s)
{
  UString estimatedName;
  const UString shortName = GetMethodSpec(estimatedName);
  if (s.IsEmpty())
    return shortName.IsEmpty();
  return s.IsEqualTo_NoCase(estimatedName);
}


UString CCompressDialog::GetEncryptionMethodSpec()
{
  UString s;
  if (_encryptionMethod.GetCount() > 0
      && _encryptionMethod.GetCurSel() != _default_encryptionMethod_Index)
  {
    _encryptionMethod.GetText(s);
    s.RemoveChar(L'-');
  }
  return s;
}

static const size_t k_Auto_Dict = (size_t)0 - 1;


int CCompressDialog::AddDict2(size_t sizeReal, size_t sizeShow)
{
  char c = 0;
  unsigned moveBits = 0;
       if ((sizeShow & 0xFFFFF) == 0) { moveBits = 20; c = 'M'; }
  else if ((sizeShow &   0x3FF) == 0) { moveBits = 10; c = 'K'; }
  AString s;
  s.Add_UInt64(sizeShow >> moveBits);
  s.Add_Space();
  if (moveBits != 0)
    s += c;
  s += 'B';
  if (sizeReal == k_Auto_Dict)
    Modify_Auto(s);
  const int index = (int)ComboBox_AddStringAscii(m_Dictionary, s);
  m_Dictionary.SetItemData(index, sizeReal);
  return index;
}


int CCompressDialog::AddDict(size_t size)
{
  return AddDict2(size, size);
}


void CCompressDialog::SetDictionary2()
{
  m_Dictionary.ResetContent();
  // _auto_Dict = (UInt32)1 << 24; // we can use this dictSize to calculate _auto_Solid for unknown method for 7z
  _auto_Dict = (UInt32)(Int32)-1; // for debug:

  const CArcInfoEx &ai = Get_ArcInfoEx();
  UInt32 defaultDict = (UInt32)(Int32)-1;
  {
    const int index = FindRegistryFormat(ai.Name);
    if (index >= 0)
    {
      const NCompression::CFormatOptions &fo = m_RegistryInfo.Formats[index];
      if (IsMethodEqualTo(fo.Method))
        defaultDict = fo.Dictionary;
    }
  }

  const int methodID = GetMethodID();
  const UInt32 level = GetLevel2();
  if (methodID < 0)
    return;

  switch (methodID)
  {
    case kLZMA:
    case kLZMA2:
    {
      {
        _auto_Dict =
            ( level <= 3 ? ((UInt32)1 << (level * 2 + 16)) :
            ( level <= 6 ? ((UInt32)1 << (level + 19)) :
            ( level <= 7 ? ((UInt32)1 << 25) : ((UInt32)1 << 26)
            )));
      }

      // we use threshold 3.75 GiB to switch to kLzmaMaxDictSize.
      if (defaultDict != (UInt32)(Int32)-1
          && defaultDict >= ((UInt32)15 << 28))
        defaultDict = kLzmaMaxDictSize;

      const size_t kLzmaMaxDictSize_Up = (size_t)1 << (20 + sizeof(size_t) / 4 * 6);

      int curSel = AddDict2(k_Auto_Dict, _auto_Dict);

      for (unsigned i = (16 - 1) * 2; i <= (32 - 1) * 2; i++)
      {
        if (i < (20 - 1) * 2
            && i != (16 - 1) * 2
            && i != (18 - 1) * 2)
          continue;
        if (i == (20 - 1) * 2 + 1)
          continue;
        const size_t dict_up = (size_t)(2 + (i & 1)) << (i / 2);
        size_t dict = dict_up;
        if (dict_up >= kLzmaMaxDictSize)
          dict = kLzmaMaxDictSize; // we reduce dictionary

        const int index = AddDict(dict);
        // AddDict2(dict, dict_up); // for debug : we show 4 GB

        // const UInt32 numThreads = 2;
        // const UInt64 memUsage = GetMemoryUsageComp_Threads_Dict(numThreads, dict);
        if (defaultDict != (UInt32)(Int32)-1)
          if (dict <= defaultDict || curSel <= 0)
          // if (!maxRamSize_Defined || memUsage <= maxRamSize)
            curSel = index;
        if (dict_up >= kLzmaMaxDictSize_Up)
          break;
      }

      m_Dictionary.SetCurSel(curSel);
      break;
    }

    case kPPMd:
    {
      _auto_Dict = (UInt32)1 << (level + 19);

      const UInt32 kPpmd_Default_4g = (UInt32)0 - ((UInt32)1 << 10);
      const size_t kPpmd_MaxDictSize_Up = (size_t)1 << (29 + sizeof(size_t) / 8);

      if (defaultDict != (UInt32)(Int32)-1
          && defaultDict >= ((UInt32)15 << 28)) // threshold
        defaultDict = kPpmd_Default_4g;

      int curSel = AddDict2(k_Auto_Dict, _auto_Dict);

      for (unsigned i = (20 - 1) * 2; i <= (32 - 1) * 2; i++)
      {
        if (i == (20 - 1) * 2 + 1)
          continue;

        const size_t dict_up = (size_t)(2 + (i & 1)) << (i / 2);
        size_t dict = dict_up;
        if (dict_up >= kPpmd_Default_4g)
          dict = kPpmd_Default_4g;

        const int index = AddDict2(dict, dict_up);
        // AddDict2((UInt32)((UInt32)0 - 2), dict_up); // for debug
        // AddDict(dict_up); // for debug
        // const UInt64 memUsage = GetMemoryUsageComp_Threads_Dict(1, dict);
        if (defaultDict != (UInt32)(Int32)-1)
          if (dict <= defaultDict || curSel <= 0)
            // if (!maxRamSize_Defined || memUsage <= maxRamSize)
            curSel = index;
        if (dict_up >= kPpmd_MaxDictSize_Up)
          break;
      }
      m_Dictionary.SetCurSel(curSel);
      break;
    }

    case kPPMdZip:
    {
      _auto_Dict = (UInt32)1 << (level + 19);

      int curSel = AddDict2(k_Auto_Dict, _auto_Dict);

      for (unsigned i = 20; i <= 28; i++)
      {
        const UInt32 dict = (UInt32)1 << i;
        const int index = AddDict(dict);
        // const UInt64 memUsage = GetMemoryUsageComp_Threads_Dict(1, dict);
        if (defaultDict != (UInt32)(Int32)-1)
          if (dict <= defaultDict || curSel <= 0)
            // if (!maxRamSize_Defined || memUsage <= maxRamSize)
            curSel = index;
      }
      m_Dictionary.SetCurSel(curSel);
      break;
    }

    case kDeflate:
    case kDeflate64:
    {
      const UInt32 dict = (methodID == kDeflate ? (UInt32)(1 << 15) : (UInt32)(1 << 16));
      _auto_Dict = dict;
      AddDict2(k_Auto_Dict, _auto_Dict);
      m_Dictionary.SetCurSel(0);
      // EnableItem(IDC_COMPRESS_DICTIONARY, false);
      break;
    }

    case kBZip2:
    {
      {
             if (level >= 5) _auto_Dict = (900 << 10);
        else if (level >= 3) _auto_Dict = (500 << 10);
        else                 _auto_Dict = (100 << 10);
      }

      int curSel = AddDict2(k_Auto_Dict, _auto_Dict);

      for (unsigned i = 1; i <= 9; i++)
      {
        const UInt32 dict = ((UInt32)i * 100) << 10;
        AddDict(dict);
        // AddDict2(i * 100000, dict);
        if (defaultDict != (UInt32)(Int32)-1)
          if (i <= defaultDict / 100000 || curSel <= 0)
            curSel = m_Dictionary.GetCount() - 1;
      }
      m_Dictionary.SetCurSel(curSel);
      break;
    }

    case kCopy:
    {
      _auto_Dict = 0;
      AddDict(0);
      m_Dictionary.SetCurSel(0);
      break;
    }
  }
}


UInt32 CCompressDialog::GetComboValue(NWindows::NControl::CComboBox &c, int defMax)
{
  if (c.GetCount() <= defMax)
    return (UInt32)(Int32)-1;
  return (UInt32)c.GetItemData_of_CurSel();
}


UInt64 CCompressDialog::GetComboValue_64(NWindows::NControl::CComboBox &c, int defMax)
{
  if (c.GetCount() <= defMax)
    return (UInt64)(Int64)-1;
  // LRESULT is signed. so we cast it to unsigned size_t at first:
  LRESULT val = c.GetItemData_of_CurSel();
  if (val == (LPARAM)(INT_PTR)(-1))
    return (UInt64)(Int64)-1;
  return (UInt64)(size_t)c.GetItemData_of_CurSel();
}

UInt32 CCompressDialog::GetLevel2()
{
  UInt32 level = GetLevel();
  if (level == (UInt32)(Int32)-1)
    level = 5;
  return level;
}


int CCompressDialog::AddOrder(UInt32 size)
{
  char s[32];
  ConvertUInt32ToString(size, s);
  int index = (int)ComboBox_AddStringAscii(m_Order, s);
  m_Order.SetItemData(index, size);
  return index;
}

int CCompressDialog::AddOrder_Auto()
{
  AString s;
  s.Add_UInt32(_auto_Order);
  Modify_Auto(s);
  int index = (int)ComboBox_AddStringAscii(m_Order, s);
  m_Order.SetItemData(index, (LPARAM)(INT_PTR)(-1));
  return index;
}

void CCompressDialog::SetOrder2()
{
  m_Order.ResetContent();

  _auto_Order = 1;

  const CArcInfoEx &ai = Get_ArcInfoEx();
  UInt32 defaultOrder = (UInt32)(Int32)-1;

  {
    const int index = FindRegistryFormat(ai.Name);
    if (index >= 0)
    {
      const NCompression::CFormatOptions &fo = m_RegistryInfo.Formats[index];
      if (IsMethodEqualTo(fo.Method))
        defaultOrder = fo.Order;
    }
  }

  const int methodID = GetMethodID();
  const UInt32 level = GetLevel2();
  if (methodID < 0)
    return;

  switch (methodID)
  {
    case kLZMA:
    case kLZMA2:
    {
      _auto_Order = (level < 7 ? 32 : 64);
      int curSel = AddOrder_Auto();
      for (unsigned i = 2 * 2; i < 8 * 2; i++)
      {
        UInt32 order = ((UInt32)(2 + (i & 1)) << (i / 2));
        if (order > 256)
          order = 273;
        const int index = AddOrder(order);
        if (defaultOrder != (UInt32)(Int32)-1)
          if (order <= defaultOrder || curSel <= 0)
            curSel = index;
      }
      m_Order.SetCurSel(curSel);
      break;
    }

    case kDeflate:
    case kDeflate64:
    {
      {
             if (level >= 9) _auto_Order = 128;
        else if (level >= 7) _auto_Order = 64;
        else                 _auto_Order = 32;
      }
      int curSel = AddOrder_Auto();
      for (unsigned i = 2 * 2; i < 8 * 2; i++)
      {
        UInt32 order = ((UInt32)(2 + (i & 1)) << (i / 2));
        if (order > 256)
          order = (methodID == kDeflate64 ? 257 : 258);
        const int index = AddOrder(order);
        if (defaultOrder != (UInt32)(Int32)-1)
          if (order <= defaultOrder || curSel <= 0)
            curSel = index;
      }

      m_Order.SetCurSel(curSel);
      break;
    }

    case kPPMd:
    {
      {
             if (level >= 9) _auto_Order = 32;
        else if (level >= 7) _auto_Order = 16;
        else if (level >= 5) _auto_Order = 6;
        else                 _auto_Order = 4;
      }

      int curSel = AddOrder_Auto();

      for (unsigned i = 0;; i++)
      {
        UInt32 order = i + 2;
        if (i >= 2)
          order = (4 + ((i - 2) & 3)) << ((i - 2) / 4);
        const int index = AddOrder(order);
        if (defaultOrder != (UInt32)(Int32)-1)
          if (order <= defaultOrder || curSel <= 0)
            curSel = index;
        if (order >= 32)
          break;
      }
      m_Order.SetCurSel(curSel);
      break;
    }

    case kPPMdZip:
    {
      _auto_Order = level + 3;
      int curSel = AddOrder_Auto();
      for (unsigned i = 2; i <= 16; i++)
      {
        const int index = AddOrder(i);
        if (defaultOrder != (UInt32)(Int32)-1)
          if (i <= defaultOrder || curSel <= 0)
            curSel = index;
      }
      m_Order.SetCurSel(curSel);
      break;
    }

    // case kBZip2:
    default:
      break;
  }
}

bool CCompressDialog::GetOrderMode()
{
  switch (GetMethodID())
  {
    case kPPMd:
    case kPPMdZip:
      return true;
  }
  return false;
}


static UInt64 Get_Lzma2_ChunkSize(UInt64 dict)
{
  // we use same default chunk sizes as defined in 7z encoder and lzma2 encoder
  UInt64 cs = (UInt64)dict << 2;
  const UInt32 kMinSize = (UInt32)1 << 20;
  const UInt32 kMaxSize = (UInt32)1 << 28;
  if (cs < kMinSize) cs = kMinSize;
  if (cs > kMaxSize) cs = kMaxSize;
  if (cs < dict) cs = dict;
  cs += (kMinSize - 1);
  cs &= ~(UInt64)(kMinSize - 1);
  return cs;
}


static void Add_Size(AString &s, UInt64 val)
{
  unsigned moveBits = 0;
  char c = 0;
       if ((val & 0x3FFFFFFF) == 0) { moveBits = 30; c = 'G'; }
  else if ((val &    0xFFFFF) == 0) { moveBits = 20; c = 'M'; }
  else if ((val &      0x3FF) == 0) { moveBits = 10; c = 'K'; }
  s.Add_UInt64(val >> moveBits);
  s.Add_Space();
  if (moveBits != 0)
    s += c;
  s += 'B';
}


void CCompressDialog::SetSolidBlockSize2()
{
  m_Solid.ResetContent();
  _auto_Solid = 1 << 20;

  const CFormatInfo &fi = g_Formats[GetStaticFormatIndex()];
  if (!fi.Solid_())
    return;

  const UInt32 level = GetLevel2();
  if (level == 0)
    return;

  UInt64 dict = GetDict2();
  if (dict == (UInt64)(Int64)-1)
  {
    dict = 1 << 25; // default dict for unknown methods
    // return;
  }


  UInt32 defaultBlockSize = (UInt32)(Int32)-1;

  const CArcInfoEx &ai = Get_ArcInfoEx();

  /*
  if (usePrevDictionary)
    defaultBlockSize = GetBlockSizeSpec();
  else
  */
  {
    const int index = FindRegistryFormat(ai.Name);
    if (index >= 0)
    {
      const NCompression::CFormatOptions &fo = m_RegistryInfo.Formats[index];
      if (IsMethodEqualTo(fo.Method))
        defaultBlockSize = fo.BlockLogSize;
    }
  }

  const bool is7z = ai.Name.IsEqualTo_Ascii_NoCase("7z");

  const UInt64 cs = Get_Lzma2_ChunkSize(dict);

  // Solid Block Size
  UInt64 blockSize = cs; // for xz

  if (is7z)
  {
    // we use same default block sizes as defined in 7z encoder
    UInt64 kMaxSize = (UInt64)1 << 32;
    const int methodId = GetMethodID();
    if (methodId == kLZMA2)
    {
      blockSize = cs << 6;
      kMaxSize = (UInt64)1 << 34;
    }
    else
    {
      UInt64 dict2 = dict;
      if (methodId == kBZip2)
      {
        dict2 /= 100000;
        if (dict2 < 1)
          dict2 = 1;
        dict2 *= 100000;
      }
      blockSize = dict2 << 7;
    }

    const UInt32 kMinSize = (UInt32)1 << 24;
    if (blockSize < kMinSize) blockSize = kMinSize;
    if (blockSize > kMaxSize) blockSize = kMaxSize;
  }

  _auto_Solid = blockSize;

  int curSel;
  {
    AString s;
    Add_Size(s, _auto_Solid);
    Modify_Auto(s);
    int index = (int)ComboBox_AddStringAscii(m_Solid, s);
    m_Solid.SetItemData(index, (UInt32)(Int32)-1);
    curSel = index;
  }

  if (is7z)
  {
    UString s ('-');
    // kSolidLog_NoSolid = 0 for xz means default blockSize
    if (is7z)
      LangString(IDS_COMPRESS_NON_SOLID, s);
    const int index = (int)m_Solid.AddString(s);
    m_Solid.SetItemData(index, (UInt32)kSolidLog_NoSolid);
    if (defaultBlockSize == kSolidLog_NoSolid)
      curSel = index;
  }

  for (unsigned i = 20; i <= 36; i++)
  {
    AString s;
    Add_Size(s, (UInt64)1 << i);
    const int index = (int)ComboBox_AddStringAscii(m_Solid, s);
    m_Solid.SetItemData(index, (UInt32)i);
    if (defaultBlockSize != (UInt32)(Int32)-1)
      if (i <= defaultBlockSize || index <= 1)
        curSel = index;
  }

  {
    const int index = (int)m_Solid.AddString(LangString(IDS_COMPRESS_SOLID));
    m_Solid.SetItemData(index, kSolidLog_FullSolid);
    if (defaultBlockSize == kSolidLog_FullSolid)
      curSel = index;
  }

  m_Solid.SetCurSel(curSel);
}


void CCompressDialog::SetNumThreads2()
{
  _auto_NumThreads = 1;

  m_NumThreads.ResetContent();
  const CFormatInfo &fi = g_Formats[GetStaticFormatIndex()];
  if (!fi.MultiThread_())
    return;

  const UInt32 numHardwareThreads = NSystem::GetNumberOfProcessors();
    // 64; // for debug:

  UInt32 defaultValue = numHardwareThreads;
  bool useAutoThreads = true;

  {
    const CArcInfoEx &ai = Get_ArcInfoEx();
    int index = FindRegistryFormat(ai.Name);
    if (index >= 0)
    {
      const NCompression::CFormatOptions &fo = m_RegistryInfo.Formats[index];
      if (IsMethodEqualTo(fo.Method) && fo.NumThreads != (UInt32)(Int32)-1)
      {
        defaultValue = fo.NumThreads;
        useAutoThreads = false;
      }
    }
  }

  UInt32 numAlgoThreadsMax = numHardwareThreads * 2;
  const int methodID = GetMethodID();
  switch (methodID)
  {
    case kLZMA: numAlgoThreadsMax = 2; break;
    case kLZMA2: numAlgoThreadsMax = 256; break;
    case kBZip2: numAlgoThreadsMax = 32; break;
    case kCopy:
    case kPPMd:
    case kDeflate:
    case kDeflate64:
    case kPPMdZip:
      numAlgoThreadsMax = 1;
  }
  const bool isZip = IsZipFormat();
  if (isZip)
  {
    numAlgoThreadsMax =
      #ifdef _WIN32
        64; // _WIN32 supports only 64 threads in one group. So no need for more threads here
      #else
        128;
      #endif
  }

  UInt32 autoThreads = numHardwareThreads;
  if (autoThreads > numAlgoThreadsMax)
    autoThreads = numAlgoThreadsMax;

  const UInt64 memUse_Limit = Get_MemUse_Bytes();

  if (autoThreads > 1 && _ramSize_Defined)
  {
    if (isZip)
    {
      for (; autoThreads > 1; autoThreads--)
      {
        const UInt64 dict64 = GetDict2();
        UInt64 decompressMemory;
        const UInt64 usage = GetMemoryUsage_Threads_Dict_DecompMem(autoThreads, dict64, decompressMemory);
        if (usage <= memUse_Limit)
          break;
      }
    }
    else if (methodID == kLZMA2)
    {
      const UInt64 dict64 = GetDict2();
      const UInt32 numThreads1 = (GetLevel2() >= 5 ? 2 : 1);
      UInt32 numBlockThreads = autoThreads / numThreads1;
      for (; numBlockThreads > 1; numBlockThreads--)
      {
        autoThreads = numBlockThreads * numThreads1;
        UInt64 decompressMemory;
        const UInt64 usage = GetMemoryUsage_Threads_Dict_DecompMem(autoThreads, dict64, decompressMemory);
        if (usage <= memUse_Limit)
          break;
      }
      autoThreads = numBlockThreads * numThreads1;
    }
  }

  _auto_NumThreads = autoThreads;

  int curSel = -1;
  {
    AString s;
    s.Add_UInt32(autoThreads);
    Modify_Auto(s);
    int index = (int)ComboBox_AddStringAscii(m_NumThreads, s);
    m_NumThreads.SetItemData(index, (LPARAM)(INT_PTR)(-1));
    // m_NumThreads.SetItemData(index, autoThreads);
    if (useAutoThreads)
      curSel = index;
  }

  if (numAlgoThreadsMax != autoThreads || autoThreads != 1)
  for (UInt32 i = 1; i <= numHardwareThreads * 2 && i <= numAlgoThreadsMax; i++)
  {
    char s[32];
    ConvertUInt32ToString(i, s);
    int index = (int)ComboBox_AddStringAscii(m_NumThreads, s);
    m_NumThreads.SetItemData(index, (UInt32)i);
    if (!useAutoThreads && i == defaultValue)
      curSel = index;
  }

  m_NumThreads.SetCurSel(curSel);
}


static void AddMemSize(UString &res, UInt64 size)
{
  char c;
  unsigned moveBits = 0;
  if (size >= ((UInt64)1 << 31) && (size & 0x3FFFFFFF) == 0)
    { moveBits = 30; c = 'G'; }
  else // if (size >= ((UInt32)1 << 21) && (size & 0xFFFFF) == 0)
    { moveBits = 20; c = 'M'; }
  // else { moveBits = 10; c = 'K'; }
  res.Add_UInt64(size >> moveBits);
  res.Add_Space();
  if (moveBits != 0)
    res += c;
  res += 'B';
}


int CCompressDialog::AddMemComboItem(UInt64 val, bool isPercent, bool isDefault)
{
  UString sUser;
  UString sRegistry;
  if (isPercent)
  {
    UString s;
    s.Add_UInt64(val);
    s += '%';
    if (isDefault)
      sUser = "* ";
    else
      sRegistry = s;
    sUser += s;
  }
  else
  {
    AddMemSize(sUser, val);
    sRegistry = sUser;
    for (;;)
    {
      int pos = sRegistry.Find(L' ');
      if (pos < 0)
        break;
      sRegistry.Delete(pos);
    }
    if (!sRegistry.IsEmpty())
      if (sRegistry.Back() == 'B')
        sRegistry.DeleteBack();
  }
  const unsigned dataIndex = _memUse_Strings.Add(sRegistry);
  const int index = (int)m_MemUse.AddString(sUser);
  m_MemUse.SetItemData(index, dataIndex);
  return index;
}



void CCompressDialog::SetMemUseCombo()
{
  _memUse_Strings.Clear();
  m_MemUse.ResetContent();
  const CFormatInfo &fi = g_Formats[GetStaticFormatIndex()];

  {
    const bool enable = fi.MemUse_();
    ShowItem_Bool(IDT_COMPRESS_MEMORY, enable);
    ShowItem_Bool(IDT_COMPRESS_MEMORY_VALUE, enable);
    ShowItem_Bool(IDT_COMPRESS_MEMORY_DE, enable);
    ShowItem_Bool(IDT_COMPRESS_MEMORY_DE_VALUE, enable);
    ShowItem_Bool(IDC_COMPRESS_MEM_USE, enable);
    EnableItem(IDC_COMPRESS_MEM_USE, enable);
    if (!enable)
      return;
  }

  UInt64 curMem_Bytes = 0;
  UInt64 curMem_Percents = 0;
  bool needSetCur_Bytes = false;
  bool needSetCur_Percents = false;
  {
    const NCompression::CFormatOptions &fo = Get_FormatOptions();
    if (!fo.MemUse.IsEmpty())
    {
      NCompression::CMemUse mu;
      mu.Parse(fo.MemUse);
      if (mu.IsDefined)
      {
        if (mu.IsPercent)
        {
          curMem_Percents = mu.Val;
          needSetCur_Percents = true;
        }
        else
        {
          curMem_Bytes = mu.GetBytes(_ramSize_Reduced);
          needSetCur_Bytes = true;
        }
      }
    }
  }


  // 80% - is auto usage limit in handlers
  AddMemComboItem(80, true, true);
  m_MemUse.SetCurSel(0);

  {
    for (unsigned i = 10;; i += 10)
    {
      UInt64 size = i;
      if (i > 100)
        size = (UInt64)(Int64)-1;
      if (needSetCur_Percents && size >= curMem_Percents)
      {
        const int index = AddMemComboItem(curMem_Percents, true);
        m_MemUse.SetCurSel(index);
        needSetCur_Percents = false;
        if (size == curMem_Percents)
          continue;
      }
      if (size == (UInt64)(Int64)-1)
        break;
      AddMemComboItem(size, true);
    }
  }
  {
    for (unsigned i = (27) * 2;; i++)
    {
      UInt64 size = (UInt64)(2 + (i & 1)) << (i / 2);
      if (i > (20 + sizeof(size_t) * 3 - 1) * 2)
        size = (UInt64)(Int64)-1;
      if (needSetCur_Bytes && size >= curMem_Bytes)
      {
        const int index = AddMemComboItem(curMem_Bytes);
        m_MemUse.SetCurSel(index);
        needSetCur_Bytes = false;
        if (size == curMem_Bytes)
          continue;
      }
      if (size == (UInt64)(Int64)-1)
        break;
      AddMemComboItem(size);
    }
  }
}


UString CCompressDialog::Get_MemUse_Spec()
{
  if (m_MemUse.GetCount() < 1)
    return UString();
  return _memUse_Strings[(unsigned)m_MemUse.GetItemData_of_CurSel()];
}


UInt64 CCompressDialog::Get_MemUse_Bytes()
{
  const UString mus = Get_MemUse_Spec();
  NCompression::CMemUse mu;
  if (!mus.IsEmpty())
  {
    mu.Parse(mus);
    if (mu.IsDefined)
      return mu.GetBytes(_ramSize_Reduced);
  }
  return _ramUsage_Auto; // _ramSize_Reduced; // _ramSize;;
}



UInt64 CCompressDialog::GetMemoryUsage_DecompMem(UInt64 &decompressMemory)
{
  return GetMemoryUsage_Dict_DecompMem(GetDict2(), decompressMemory);
}

UInt64 CCompressDialog::GetMemoryUsageComp_Threads_Dict(UInt32 numThreads, UInt64 dict64)
{
  UInt64 decompressMemory;
  return GetMemoryUsage_Threads_Dict_DecompMem(numThreads, dict64, decompressMemory);
}

UInt64 CCompressDialog::GetMemoryUsage_Dict_DecompMem(UInt64 dict64, UInt64 &decompressMemory)
{
  return GetMemoryUsage_Threads_Dict_DecompMem(GetNumThreads2(), dict64, decompressMemory);
}

UInt64 CCompressDialog::GetMemoryUsage_Threads_Dict_DecompMem(UInt32 numThreads, UInt64 dict64, UInt64 &decompressMemory)
{
  decompressMemory = (UInt64)(Int64)-1;
  if (dict64 == (UInt64)(Int64)-1)
    return (UInt64)(Int64)-1;

  UInt32 level = GetLevel2();
  if (level == 0)
  {
    decompressMemory = (1 << 20);
    return decompressMemory;
  }
  UInt64 size = 0;

  const CFormatInfo &fi = g_Formats[GetStaticFormatIndex()];
  if (fi.Filter_() && level >= 9)
    size += (12 << 20) * 2 + (5 << 20);
  // UInt32 numThreads = GetNumThreads2();

  UInt32 numMainZipThreads = 1;

  if (IsZipFormat())
  {
    UInt32 numSubThreads = 1;
    if (GetMethodID() == kLZMA && numThreads > 1 && level >= 5)
      numSubThreads = 2;
    numMainZipThreads = numThreads / numSubThreads;
    if (numMainZipThreads > 1)
      size += (UInt64)numMainZipThreads * ((size_t)sizeof(size_t) << 23);
    else
      numMainZipThreads = 1;
  }

  const int methodId = GetMethodID();

  switch (methodId)
  {
    case kLZMA:
    case kLZMA2:
    {
      const UInt32 dict = (dict64 >= kLzmaMaxDictSize ? kLzmaMaxDictSize : (UInt32)dict64);
      UInt32 hs = dict - 1;
      hs |= (hs >> 1);
      hs |= (hs >> 2);
      hs |= (hs >> 4);
      hs |= (hs >> 8);
      hs >>= 1;
      if (hs >= (1 << 24))
        hs >>= 1;
      hs |= (1 << 16) - 1;
      // if (numHashBytes >= 5)
      if (level < 5)
        hs |= (256 << 10) - 1;
      hs++;
      UInt64 size1 = (UInt64)hs * 4;
      size1 += (UInt64)dict * 4;
      if (level >= 5)
        size1 += (UInt64)dict * 4;
      size1 += (2 << 20);

      UInt32 numThreads1 = 1;
      if (numThreads > 1 && level >= 5)
      {
        size1 += (2 << 20) + (4 << 20);
        numThreads1 = 2;
      }

      UInt32 numBlockThreads = numThreads / numThreads1;

      UInt64 chunkSize = 0; // it's solid chunk

      if (methodId != kLZMA && numBlockThreads != 1)
      {
        chunkSize = Get_Lzma2_ChunkSize(dict);

        if (IsXzFormat())
        {
          UInt32 blockSizeLog = GetBlockSizeSpec();
          if (blockSizeLog != (UInt32)(Int32)-1)
          {
            if (blockSizeLog == kSolidLog_FullSolid)
            {
              numBlockThreads = 1;
              chunkSize = 0;
            }
            else if (blockSizeLog != kSolidLog_NoSolid)
              chunkSize = (UInt64)1 << blockSizeLog;
          }
        }
      }

      if (chunkSize == 0)
      {
        const UInt32 kBlockSizeMax = (UInt32)0 - (UInt32)(1 << 16);
        UInt64 blockSize = (UInt64)dict + (1 << 16)
          + (numThreads1 > 1 ? (1 << 20) : 0);
        blockSize += (blockSize >> (blockSize < ((UInt32)1 << 30) ? 1 : 2));
        if (blockSize >= kBlockSizeMax)
          blockSize = kBlockSizeMax;
        size += numBlockThreads * (size1 + blockSize);
      }
      else
      {
        size += numBlockThreads * (size1 + chunkSize);
        UInt32 numPackChunks = numBlockThreads + (numBlockThreads / 8) + 1;
        if (chunkSize < ((UInt32)1 << 26)) numBlockThreads++;
        if (chunkSize < ((UInt32)1 << 24)) numBlockThreads++;
        if (chunkSize < ((UInt32)1 << 22)) numBlockThreads++;
        size += numPackChunks * chunkSize;
      }

      decompressMemory = dict + (2 << 20);
      return size;
    }

    case kPPMd:
    {
      decompressMemory = dict64 + (2 << 20);
      return size + decompressMemory;
    }

    case kDeflate:
    case kDeflate64:
    {
      UInt64 size1 = 3 << 20;
      // if (level >= 7)
        size1 += (1 << 20);
      size += size1 * numMainZipThreads;
      decompressMemory = (2 << 20);
      return size;
    }

    case kBZip2:
    {
      decompressMemory = (7 << 20);
      UInt64 memForOneThread = (10 << 20);
      return size + memForOneThread * numThreads;
    }

    case kPPMdZip:
    {
      decompressMemory = dict64 + (2 << 20);
      return size + (UInt64)decompressMemory * numThreads;
    }
  }

  return (UInt64)(Int64)-1;
}



static void AddMemUsage(UString &s, UInt64 v)
{
  const char *post;
  if (v <= ((UInt64)16 << 30))
  {
    v = (v + (1 << 20) - 1) >> 20;
    post = "MB";
  }
  else if (v <= ((UInt64)64 << 40))
  {
    v = (v + (1 << 30) - 1) >> 30;
    post = "GB";
  }
  else
  {
    const UInt64 v2 = v + ((UInt64)1 << 40) - 1;
    if (v <= v2)
      v = v2;
    v >>= 40;
    post = "TB";
  }
  s.Add_UInt64(v);
  s.Add_Space();
  s += post;
}


void CCompressDialog::PrintMemUsage(UINT res, UInt64 value)
{
  if (value == (UInt64)(Int64)-1)
  {
    SetItemText(res, TEXT("?"));
    return;
  }
  UString s;
  AddMemUsage(s, value);
  if (res == IDT_COMPRESS_MEMORY_VALUE)
  {
    const UString mus = Get_MemUse_Spec();
    NCompression::CMemUse mu;
    if (!mus.IsEmpty())
      mu.Parse(mus);
    if (mu.IsDefined)
    {
      s += " / ";
      AddMemUsage(s, mu.GetBytes(_ramSize_Reduced));
    }
    else if (_ramSize_Defined)
    {
      s += " / ";
      AddMemUsage(s, _ramUsage_Auto);
    }

    if (_ramSize_Defined)
    {
      s += " / ";
      AddMemUsage(s, _ramSize);
    }
  }
  SetItemText(res, s);
}


void CCompressDialog::SetMemoryUsage()
{
  UInt64 decompressMem;
  const UInt64 memUsage = GetMemoryUsage_DecompMem(decompressMem);
  PrintMemUsage(IDT_COMPRESS_MEMORY_VALUE, memUsage);
  PrintMemUsage(IDT_COMPRESS_MEMORY_DE_VALUE, decompressMem);
}

void CCompressDialog::SetParams()
{
  const CArcInfoEx &ai = Get_ArcInfoEx();
  m_Params.SetText(TEXT(""));
  const int index = FindRegistryFormat(ai.Name);
  if (index >= 0)
  {
    const NCompression::CFormatOptions &fo = m_RegistryInfo.Formats[index];
    m_Params.SetText(fo.Options);
  }
}

void CCompressDialog::SaveOptionsInMem()
{
  m_Params.GetText(Info.Options);
  Info.Options.Trim();

  const CArcInfoEx &ai = (*ArcFormats)[Info.FormatIndex];
  const int index = FindRegistryFormatAlways(ai.Name);
  NCompression::CFormatOptions &fo = m_RegistryInfo.Formats[index];
  fo.Options = Info.Options;
  fo.Level = GetLevelSpec();
  {
    const UInt64 dict64 = GetDictSpec();
    UInt32 dict32;
    if (dict64 == (UInt64)(Int64)-1)
      dict32 = (UInt32)(Int32)-1;
    else
    {
      dict32 = (UInt32)dict64;
      if (dict64 != dict32)
      {
        /* here we must write 32-bit value for registry that indicates big_value
           (UInt32)(Int32)-1  : is used as marker for default size
           (UInt32)(Int32)-2  : it can be used to indicate big value (4 GiB)
           the value must be larger than threshold
        */
        dict32 = (UInt32)(Int32)-2;
        // dict32 = kLzmaMaxDictSize; // it must be larger than threshold
      }
    }
    fo.Dictionary = dict32;
  }

  fo.Order = GetOrderSpec();
  fo.Method = GetMethodSpec();
  fo.EncryptionMethod = GetEncryptionMethodSpec();
  fo.NumThreads = GetNumThreadsSpec();
  fo.BlockLogSize = GetBlockSizeSpec();
  fo.MemUse = Get_MemUse_Spec();
}

unsigned CCompressDialog::GetFormatIndex()
{
  return (unsigned)m_Format.GetItemData_of_CurSel();
}
