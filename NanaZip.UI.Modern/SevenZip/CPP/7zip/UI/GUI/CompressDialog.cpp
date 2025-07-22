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
#include "../FileManager/PropertyName.h"
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

// #define IDS_OPTIONS 2100

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

  IDB_COMPRESS_OPTIONS, // IDS_OPTIONS

  IDG_COMPRESS_OPTIONS,
  IDX_COMPRESS_SFX,
  IDX_COMPRESS_SHARED,
  IDX_COMPRESS_DEL,

  IDT_COMPRESS_MEMORY,
  IDT_COMPRESS_MEMORY_DE,

  IDG_COMPRESS_ENCRYPTION,
  IDT_COMPRESS_ENCRYPTION_METHOD,
  IDX_COMPRESS_ENCRYPT_FILE_NAMES,

  IDT_PASSWORD_ENTER,
  IDT_PASSWORD_REENTER,
  IDX_PASSWORD_SHOW,

  IDT_SPLIT_TO_VOLUMES,
  IDT_COMPRESS_PATH_MODE,
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
  IDS_METHOD_FAST,
  IDS_METHOD_NORMAL,
  IDS_METHOD_MAXIMUM,
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
  kFLZMA2,
  kZSTD,
  kBROTLI,
  kLZ4,
  kLZ5,
  kLIZARD_M1,
  kLIZARD_M2,
  kLIZARD_M3,
  kLIZARD_M4,
  kSha256,
  kSha1,
  kCrc32,
  kCrc64,
  kGnu,
  kPosix
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
  , "FLZMA2"
  , "zstd"
  , "Brotli"
  , "LZ4"
  , "LZ5"
  , "Lizard"
  , "Lizard"
  , "Lizard"
  , "Lizard"
  , "SHA256"
  , "SHA1"
  , "CRC32"
  , "CRC64"
  , "GNU"
  , "POSIX"
};

static LPCSTR const kMethodsNamesLong[] =
{
    "Copy [std]"
  , "LZMA [std]"
  , "LZMA2 [std]"
  , "PPMd [std]"
  , "BZip2 [std]"
  , "Deflate [std]"
  , "Deflate64 [std]"
  , "PPMd [std]"
  , "LZMA2, Fast [std]"
  , "Zstandard"
  , "Brotli"
  , "LZ4"
  , "LZ5"
  , "Lizard, FastLZ4"
  , "Lizard, LIZv1"
  , "Lizard, FastLZ4 + Huffman"
  , "Lizard, LIZv1 + Huffman"
  , "SHA256"
  , "SHA1"
  , "CRC32"
  , "CRC64"
  , "GNU"
  , "POSIX"
};

static const EMethodID g_ZstdMethods[] =
{
  kZSTD
};

static const EMethodID g_BrotliMethods[] =
{
  kBROTLI
};

static const EMethodID g_LizardMethods[] =
{
  kLIZARD_M1,
  kLIZARD_M2,
  kLIZARD_M3,
  kLIZARD_M4
};

static const EMethodID g_Lz4Methods[] =
{
  kLZ4
};

static const EMethodID g_Lz5Methods[] =
{
  kLZ5
};

static const EMethodID g_7zMethods[] =
{
  kLZMA2,
  kLZMA,
  kPPMd,
  kBZip2
  , kDeflate
  , kDeflate64
  , kZSTD
  , kBROTLI
  , kLZ4
  , kLZ5
  , kLIZARD_M1
  , kLIZARD_M2
  , kLIZARD_M3
  , kLIZARD_M4
  , kFLZMA2
  , kCopy
};

static const EMethodID g_7zSfxMethods[] =
{
  kCopy,
  kLZMA,
  kLZMA2,
  kPPMd,
  kFLZMA2,
  kZSTD
};

static const EMethodID g_ZipMethods[] =
{
  kDeflate,
  kDeflate64,
  kBZip2,
  kLZMA,
  kPPMdZip,
  kZSTD,
  kCopy
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

static const EMethodID g_TarMethods[] =
{
  kGnu,
  kPosix
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

/*
static const UInt32 kFF_Time_Win  = 1 << 10;
static const UInt32 kFF_Time_Unix = 1 << 11;
static const UInt32 kFF_Time_DOS  = 1 << 12;
static const UInt32 kFF_Time_1ns  = 1 << 13;
*/

struct CFormatInfo
{
  LPCSTR Name;

  // **************** NanaZip Modification Start ****************

  // List of levels supported by the format.
  // Format:
  // - Values are represented by left shifts of 1.
  //   (i.e. 1 << 9 means Level 9)
  // - Multiple values are represented by bitwise OR.
  //   (i.e. (1 << 6) | (1 << 9) means Level 6 and Level 9)
  UInt32 Levels;

  // List of levels that needs a label.
  // Labels (starting from smallest to largest):
  //   Store, Fastest, Fast, Normal, Maximum, Ultra
  // Typically, Store is Level 0.
  // Format is the same as the Levels field.
  UInt32 LevelsMask;

  // **************** NanaZip Modification End ****************

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

// **************** NanaZip Modification Start ****************
static const CFormatInfo g_Formats[] =
{
#if 0 // ******** 7-Zip Mainline Source Code snippet Start ********
  {
    "",
    // (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    ((UInt32)1 << 10) - 1,
    // (UInt32)(Int32)-1,
    0, NULL,
    kFF_MultiThread | kFF_MemUse
  },
  {
    "7z",
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    METHODS_PAIR(g_7zMethods),
    kFF_Filter | kFF_Solid | kFF_MultiThread | kFF_Encrypt |
    kFF_EncryptFileNames | kFF_MemUse | kFF_SFX
    // | kFF_Time_Win
  },
  {
    "Zip",
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    METHODS_PAIR(g_ZipMethods),
    kFF_MultiThread | kFF_Encrypt | kFF_MemUse
    // | kFF_Time_Win | kFF_Time_Unix | kFF_Time_DOS
  },
  {
    "GZip",
    (1 << 1) | (1 << 5) | (1 << 7) | (1 << 9),
    METHODS_PAIR(g_GZipMethods),
    kFF_MemUse
    // | kFF_Time_Unix
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
    "zstd",
    (1 << 1) | (1 << 3) | (1 << 5) | (1 << 11) | (1 << 17) | (1 << 22),
    METHODS_PAIR(g_ZstdMethods),
    kFF_MultiThread
  },
  {
    "Brotli",
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 6) | (1 << 9) | (1 << 11),
    METHODS_PAIR(g_BrotliMethods),
    kFF_MultiThread
  },
  {
    "Lizard",
    (1 << 10) | (1 << 11) | (1 << 13) | (1 << 15) | (1 << 17) | (1 << 19),
    METHODS_PAIR(g_LizardMethods),
    kFF_MultiThread
  },
  {
    "LZ4",
    (1 << 1) | (1 << 3) | (1 << 6) | (1 << 9) | (1 << 12),
    METHODS_PAIR(g_Lz4Methods),
    kFF_MultiThread
  },
  {
    "LZ5",
    (1 << 1) | (1 << 3) | (1 << 7) | (1 << 11) | (1 << 15),
    METHODS_PAIR(g_Lz5Methods),
    kFF_MultiThread
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
    METHODS_PAIR(g_TarMethods),
    0
    // kFF_Time_Unix | kFF_Time_Win // | kFF_Time_1ns
  },
  {
    "wim",
    (1 << 0),
    0, NULL,
    0
    // | kFF_Time_Win
  },
  {
    "Hash",
    (0 << 0),
    METHODS_PAIR(g_HashMethods),
    0
  }
#endif // ******** 7-Zip Mainline Source Code snippet End ********
  {
    "",
        // (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
        ((UInt32)1 << 10) - 1,
        ((UInt32)1 << 10) - 1,
        // (UInt32)(Int32)-1,
        0, NULL,
        kFF_MultiThread | kFF_MemUse
      },
      {
        "7z",
        ((UInt32)1 << 10) - 1,
        (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
        METHODS_PAIR(g_7zMethods),
        kFF_Filter | kFF_Solid | kFF_MultiThread | kFF_Encrypt |
        kFF_EncryptFileNames | kFF_MemUse | kFF_SFX
    // | kFF_Time_Win
  },
  {
    "Zip",
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    METHODS_PAIR(g_ZipMethods),
    kFF_MultiThread | kFF_Encrypt | kFF_MemUse
    // | kFF_Time_Win | kFF_Time_Unix | kFF_Time_DOS
  },
  {
    "GZip",
    (1 << 1) | (1 << 5) | (1 << 7) | (1 << 9),
    (1 << 1) | (1 << 5) | (1 << 7) | (1 << 9),
    METHODS_PAIR(g_GZipMethods),
    kFF_MemUse
    // | kFF_Time_Unix
  },
  {
    "BZip2",
    (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    METHODS_PAIR(g_BZip2Methods),
    kFF_MultiThread | kFF_MemUse
  },
  {
    "xz",
    (UInt32)((1 << 10) - 2),
    (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    METHODS_PAIR(g_XzMethods),
    kFF_Solid | kFF_MultiThread | kFF_MemUse
  },
  {
    "zstd",
    (UInt32)((1U << 23) - 2),
    (1 << 1) | (1 << 3) | (1 << 5) | (1 << 11) | (1 << 17) | (1 << 22),
    METHODS_PAIR(g_ZstdMethods),
    kFF_MultiThread
  },
  {
    "Brotli",
    (UInt32)((1 << 12) - 1),
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 6) | (1 << 9) | (1 << 11),
    METHODS_PAIR(g_BrotliMethods),
    kFF_MultiThread
  },
  {
    "Lizard",
    ((1U << 20) - (1U << 10)),
    (1 << 10) | (1 << 11) | (1 << 13) | (1 << 15) | (1 << 17) | (1 << 19),
    METHODS_PAIR(g_LizardMethods),
    kFF_MultiThread
  },
  {
    "LZ4",
    (1U << 13) - 2,
    (1 << 1) | (1 << 3) | (1 << 6) | (1 << 9) | (1 << 12),
    METHODS_PAIR(g_Lz4Methods),
    kFF_MultiThread
  },
  {
    "LZ5",
    (1U << 16) - 2,
    (1 << 1) | (1 << 3) | (1 << 7) | (1 << 11) | (1 << 15),
    METHODS_PAIR(g_Lz5Methods),
    kFF_MultiThread
  },
  {
    "Swfc",
    (1U << 10) - 2,
    (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    METHODS_PAIR(g_SwfcMethods),
    0
  },
  {
    "Tar",
    1 << 0,
    (1 << 0),
    METHODS_PAIR(g_TarMethods),
    0
    // kFF_Time_Unix | kFF_Time_Win // | kFF_Time_1ns
  },
  {
    "wim",
    1 << 0,
    (1 << 0),
    0, NULL,
    0
    // | kFF_Time_Win
  },
  {
    "Hash",
    0 << 0,
    (0 << 0),
    METHODS_PAIR(g_HashMethods),
    0
  }
};
// **************** NanaZip Modification End ****************

static const signed char g_LevelRanges[][2] = {
  { 1, 22 }, // zstd
  { 0, 11 }, // brotli
  { 1, 12 }, // lz4
  { 1, 15 }, // lz5
  { 10, 19 }, // lizard m1
  { 20, 29 }, // lizard m2
  { 30, 39 }, // lizard m3
  { 40, 49 }, // lizard m4
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
  // LangSetDlgItemText(*this, IDB_COMPRESS_OPTIONS, IDS_OPTIONS); // IDG_COMPRESS_OPTIONS
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

  FormatChanged(false); // isChanged

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
    case IDB_COMPRESS_OPTIONS:
    {
      COptionsDialog dialog(this);
      if (dialog.Create(*this) == IDOK)
        ShowOptionsString();
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

static LRESULT ComboBox_AddStringAscii(NControl::CComboBox &cb, const char *s);

static void Combine_Two_BoolPairs(const CBoolPair &b1, const CBoolPair &b2, CBool1 &res)
{
  if (!b1.Def && b2.Def)
    res.Val = b2.Val;
  else
    res.Val = b1.Val;
}

#define SET_GUI_BOOL(name) \
      Combine_Two_BoolPairs(Info. name, m_RegistryInfo. name, name)


static void Set_Final_BoolPairs(
    const CBool1 &gui,
    CBoolPair &cmd,
    CBoolPair &reg)
{
  if (!cmd.Def)
  {
    reg.Val = gui.Val;
    reg.Def = gui.Val;
  }
  if (gui.Supported)
  {
    cmd.Val = gui.Val;
    cmd.Def = gui.Val;
  }
  else
    cmd.Init();
}

#define SET_FINAL_BOOL_PAIRS(name) \
    Set_Final_BoolPairs(name, Info. name, m_RegistryInfo. name)

void CCompressDialog::FormatChanged(bool isChanged)
{
  SetMethod();
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
    if (!isChanged)
    {
      SET_GUI_BOOL (SymLinks);
      SET_GUI_BOOL (HardLinks);
      SET_GUI_BOOL (AltStreams);
      SET_GUI_BOOL (NtSecurity);
      SET_GUI_BOOL (PreserveATime);
    }

    PreserveATime.Supported = true;

    {
      const CArcInfoEx &ai = Get_ArcInfoEx();
      SymLinks.Supported   = ai.Flags_SymLinks();
      HardLinks.Supported  = ai.Flags_HardLinks();
      AltStreams.Supported = ai.Flags_AltStreams();
      NtSecurity.Supported = ai.Flags_NtSecurity();
    }

    ShowOptionsString();
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


  /* (Info) is for saving to registry:
     (CBoolPair::Val) will be set as (false), if it was (false)
       in registry at dialog creation, and user didn't click checkbox.
     in another case (CBoolPair::Val) will be set as (true) */

  {
    /* Info properties could be for another archive types.
       so we disable unsupported properties in Info */
    // const CArcInfoEx &ai = Get_ArcInfoEx();

    SET_FINAL_BOOL_PAIRS (SymLinks);
    SET_FINAL_BOOL_PAIRS (HardLinks);
    SET_FINAL_BOOL_PAIRS (AltStreams);
    SET_FINAL_BOOL_PAIRS (NtSecurity);

    SET_FINAL_BOOL_PAIRS (PreserveATime);
  }

  {
    const NCompression::CFormatOptions &fo = Get_FormatOptions();

    Info.TimePrec = fo.TimePrec;
    Info.MTime = fo.MTime;
    Info.CTime = fo.CTime;
    Info.ATime = fo.ATime;
    Info.SetArcMTime = fo.SetArcMTime;
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
            L"7-Zip", MB_YESNOCANCEL | MB_ICONQUESTION) != IDYES)
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
        FormatChanged(true); // isChanged
        SetArchiveName2(isSFX);
        return true;
      }

      case IDC_COMPRESS_LEVEL:
      {
        Get_FormatOptions().ResetForLevelChange();

        SetMethod();
        MethodChanged();
        SetSolidBlockSize();
        SetNumThreads();
        CheckSFXNameChange();
        SetMemoryUsage();
        return true;
      }

      case IDC_COMPRESS_METHOD:
      {
        MethodChanged();
        SetLevel2();
        EnableMultiCombo(IDC_COMPRESS_LEVEL);
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


unsigned CCompressDialog::FindRegistryFormat_Always(const UString &name)
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


NCompression::CFormatOptions &CCompressDialog::Get_FormatOptions()
{
  const CArcInfoEx &ai = Get_ArcInfoEx();
  return m_RegistryInfo.Formats[FindRegistryFormat_Always(ai.Name)];
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
  const CFormatInfo &fi = g_Formats[GetStaticFormatIndex()];
  const CArcInfoEx &ai = Get_ArcInfoEx();
  UInt32 LevelsMask = fi.LevelsMask;
  // **************** NanaZip Modification Start ****************
  UInt32 LevelsList = fi.Levels;
  // **************** NanaZip Modification End ****************
  UInt32 LevelsStart = 0;
  UInt32 LevelsEnd = 9;
  if (ai.LevelsMask != 0xFFFFFFFF)
    LevelsMask = ai.LevelsMask;
  else
  {
    int id = GetMethodID();
    if (id == kCopy) {
      LevelsStart = 0;
      LevelsEnd = 0;
      LevelsMask = 0;
    } else if (id >= kZSTD && id <= kLIZARD_M4) {
      auto& r = g_LevelRanges[id - kZSTD];
      LevelsStart = r[0];
      LevelsEnd = r[1];
      if (id == kZSTD)
        LevelsMask = g_Formats[6].LevelsMask;
      else if (id == kBROTLI)
        LevelsMask = g_Formats[7].LevelsMask;
      else if (id >= kLIZARD_M1 && id <= kLIZARD_M4)
        LevelsMask = g_Formats[8].LevelsMask;
      else if (id == kLZ4)
        LevelsMask = g_Formats[9].LevelsMask;
      else if (id == kLZ5)
        LevelsMask = g_Formats[10].LevelsMask;
    }
  }
  UInt32 level = m_Level.GetCount() > 0 ? (UInt32)m_Level.GetItemData_of_CurSel() : (LevelsEnd - LevelsStart + 1) / 2;
  m_Level.ResetContent();
  {
    int index = FindRegistryFormat(ai.Name);
    if (index >= 0)
    {
      const NCompression::CFormatOptions &fo = m_RegistryInfo.Formats[index];
      if (fo.Level <= LevelsEnd)
        level = fo.Level;
      else if (fo.Level == (UInt32)(Int32)-1)
        level = (LevelsEnd - LevelsStart + 1) / 2;
      else
        level = LevelsEnd;
    }
  }

  const WCHAR t[] = L"Level ";
  for (UInt32 i = LevelsStart, ir, langID = 0; i <= LevelsEnd; i++)
  {

    // lizard needs extra handling
    if (GetMethodID() >= kLIZARD_M1 && GetMethodID() <= kLIZARD_M4) {
      ir = i;
      if (ir % 10 == 0) langID = 0;
      while (ir > 19) { ir -= 10; }
    } else {
      ir = i;
    }

    // **************** NanaZip Modification Start ****************

    // If the level is not supported, ignore it.
    if ((LevelsList & (1 << ir)) == 0)
        continue;

    // max reached
    if (LevelsMask < (UInt32)(1 << ir))
      break;

    // As mentioned above, Store is typically Level 0.
    // However, lizard and zstd use different numbers for Store.
    //
    // Hence, if the selected format is lizard or zstd,
    // we let the LevelsMask of them determine the level value for Store.
    // 
    // Otherwise, hide the Store label if there's no Level 0.
    if (langID == 0 && ((LevelsMask & (1 << 0)) == 0) &&
        !((GetMethodID() >= kLIZARD_M1 && GetMethodID() <= kLIZARD_M4) ||
            GetMethodID() == kZSTD))
    {
        langID = 1;
    }

    // **************** NanaZip Modification End ****************

    if ((LevelsMask & (1 << ir)) != 0)
    {
      UString s = t;
      s.Add_UInt32(i);
      s += L" (";
      s += LangString(g_Levels[langID]);
      s += L")";
      int index = (int)m_Level.AddString(s);
      m_Level.SetItemData(index, i);
      langID++;
    } else {
      UString s = t;
      s.Add_UInt32(i);
      int index = (int)m_Level.AddString(s);
      m_Level.SetItemData(index, i);
    }
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
  int defaultLevel = 5;
  {
    const int index = FindRegistryFormat(ai.Name);
    if (index >= 0)
    {
      const NCompression::CFormatOptions &fo = m_RegistryInfo.Formats[index];
      defaultMethod = fo.Method;
      defaultLevel = fo.Level;
    }
  }
  const bool isSfx = IsSFX();
  bool weUseSameMethod = false;

  const bool is7z = ai.Is_7z();

  for (unsigned m = 0;; m++)
  {
    int methodID;
    const char *method, *methodLong;
    if (m < fi.NumMethods)
    {
      methodID = fi.MethodIDs[m];
      method = kMethodsNames[methodID];
      methodLong = kMethodsNamesLong[methodID];
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
      methodLong = method;
    }
    if (isSfx)
      if (!IsMethodSupportedBySfx(methodID))
        continue;

    AString s (methodLong);
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

    // Lizard :/
    if (defaultMethod.IsEqualTo_Ascii_NoCase("lizard") && keepMethodId == -1) {
      if (defaultLevel >= 10 && defaultLevel <= 19) m_Method.SetCurSel(kLIZARD_M1 - 1);
      if (defaultLevel >= 20 && defaultLevel <= 29) m_Method.SetCurSel(kLIZARD_M2 - 1);
      if (defaultLevel >= 30 && defaultLevel <= 39) m_Method.SetCurSel(kLIZARD_M3 - 1);
      if (defaultLevel >= 40 && defaultLevel <= 49) m_Method.SetCurSel(kLIZARD_M4 - 1);
    }

    if ((defaultMethod.IsEqualTo_Ascii_NoCase(method) || m == 0) && !weUseSameMethod)
      m_Method.SetCurSel(itemIndex);
  }

  if (!weUseSameMethod)
    MethodChanged();
}



bool CCompressDialog::IsZipFormat()
{
  return Get_ArcInfoEx().Is_Zip();
}

bool CCompressDialog::IsXzFormat()
{
  return Get_ArcInfoEx().Is_Xz();
}

void CCompressDialog::SetEncryptionMethod()
{
  _encryptionMethod.ResetContent();
  _default_encryptionMethod_Index = -1;
  const CArcInfoEx &ai = Get_ArcInfoEx();
  if (ai.Is_7z())
  {
    ComboBox_AddStringAscii(_encryptionMethod, "AES-256");
    _encryptionMethod.SetCurSel(0);
    _default_encryptionMethod_Index = 0;
  }
  else if (ai.Is_Zip())
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

#define FL2_MAX_7Z_CLEVEL 9
#define MATCH_BUFFER_SHIFT 8
#define MATCH_BUFFER_ELBOW_BITS 17
#define MATCH_BUFFER_ELBOW (1UL << MATCH_BUFFER_ELBOW_BITS)
#define RMF_BUILDER_SIZE (8 * 0x40100U)

#define MB *(1U<<20)

struct FL2_compressionParameters
{
  UInt32   dictionarySize;   /* largest match distance : larger == more compression, more memory needed during decompression; > 64Mb == more memory per byte, slower */
  unsigned chainLog;         /* HC3 sliding window : larger == more compression, slower; hybrid mode only (ultra) */
  unsigned fastLength;       /* acceptable match size for parser : larger == more compression, slower; fast bytes parameter from 7-Zip */
  bool isUltra;
};

static const FL2_compressionParameters FL2_7zCParameters[FL2_MAX_7Z_CLEVEL + 1] = {
    { 0,       0,   0, false },
    { 1 MB,    7,  32, false },
    { 2 MB,    7,  32, false },
    { 2 MB,    7,  32, false },
    { 4 MB,    7,  32, false },
    { 16 MB,   9,  48, true },
    { 32 MB,  10,  64, true },
    { 64 MB,  11,  96, true },
    { 64 MB,  12, 273, true },
    { 128 MB, 14, 273, true },
};

#undef MB


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
  UInt32 level = GetLevel2();
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

    case kFLZMA2:
    {
      static const UInt32 kMinDicSize = (1 << 20);
      level += !level;
      if (level > FL2_MAX_7Z_CLEVEL)
        level = FL2_MAX_7Z_CLEVEL;
      if (defaultDict == (UInt32)(Int32)-1)
        defaultDict = FL2_7zCParameters[level].dictionarySize;

      m_Dictionary.SetCurSel(0);

      for (unsigned i = 20; i <= 31; i++) {
        UInt32 dict = (UInt32)1 << i;

        if (dict >
          #ifdef MY_CPU_64BIT
            (1 << 30)
          #else
            (1 << 27)
          #endif
          )
          continue;

        AddDict(dict);
        //const UInt64 memUsage = GetMemoryUsageComp_Threads_Dict(dict);
        if (dict <= defaultDict /*&& (!maxRamSize_Defined || memUsage <= maxRamSize)*/)
          m_Dictionary.SetCurSel(m_Dictionary.GetCount() - 1);
      }

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
    case kFLZMA2:
    {
      if (methodID == kFLZMA2)
        _auto_Order = FL2_7zCParameters[level].fastLength;
      else
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

  const bool is7z = ai.Is_7z();

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
    case kZSTD: numAlgoThreadsMax = 128; break;
    case kBROTLI: numAlgoThreadsMax = 128; break;
    case kLZ4: numAlgoThreadsMax = 128; break;
    case kLZ5: numAlgoThreadsMax = 128; break;
    case kLIZARD_M1: numAlgoThreadsMax = 128; break;
    case kLIZARD_M2: numAlgoThreadsMax = 128; break;
    case kLIZARD_M3: numAlgoThreadsMax = 128; break;
    case kLIZARD_M4: numAlgoThreadsMax = 128; break;
    case kFLZMA2: numAlgoThreadsMax = 128; break;
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

    case kFLZMA2:
    {
      const UInt32 dict = (dict64 >= kLzmaMaxDictSize ? kLzmaMaxDictSize : (UInt32)dict64);
      if (level > FL2_MAX_7Z_CLEVEL)
        level = FL2_MAX_7Z_CLEVEL;
      /* dual buffer is enabled in Lzma2Encoder.cpp so size is dict * 6 */
      size += dict * 6 + (1UL << 18) * numThreads;
      UInt32 bufSize = dict >> MATCH_BUFFER_SHIFT;
      if (bufSize > MATCH_BUFFER_ELBOW) {
        UInt32 extra = 0;
        unsigned n = MATCH_BUFFER_ELBOW_BITS - 1;
        for (; (4UL << n) <= bufSize; ++n)
          extra += MATCH_BUFFER_ELBOW >> 4;
        if ((3UL << n) <= bufSize)
          extra += MATCH_BUFFER_ELBOW >> 5;
        bufSize = MATCH_BUFFER_ELBOW + extra;
      }
      size += (bufSize * 12 + RMF_BUILDER_SIZE) * numThreads;
      if (dict > (UInt32(1) << 26))
        size += dict;
      if (FL2_7zCParameters[level].isUltra)
        size += (UInt32(4) << 14) + (UInt32(4) << FL2_7zCParameters[level].chainLog);
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
  /* these options are for (Info.FormatIndex).
     If it's called just after format changing,
     then it's format that was selected before format changing
     So we store previous format properties */

  m_Params.GetText(Info.Options);
  Info.Options.Trim();

  const CArcInfoEx &ai = (*ArcFormats)[Info.FormatIndex];
  const unsigned index = FindRegistryFormat_Always(ai.Name);
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



static void AddText_from_BoolPair(AString &s, const char *name, const CBoolPair &bp)
{
  if (bp.Def)
  {
    s.Add_OptSpaced(name);
    if (!bp.Val)
      s += "-";
  }
  /*
  else if (bp.Val)
  {
    s.Add_OptSpaced("[");
    s += name;
    s += "]";
  }
  */
}


static void AddText_from_Bool1(AString &s, const char *name, const CBool1 &b)
{
  if (b.Supported && b.Val)
    s.Add_OptSpaced(name);
}


void CCompressDialog::ShowOptionsString()
{
  NCompression::CFormatOptions &fo = Get_FormatOptions();

  AString s;
  if (fo.IsSet_TimePrec())
  {
    s.Add_OptSpaced("tp");
    s.Add_UInt32(fo.TimePrec);
  }
  AddText_from_BoolPair(s, "tm", fo.MTime);
  AddText_from_BoolPair(s, "tc", fo.CTime);
  AddText_from_BoolPair(s, "ta", fo.ATime);
  AddText_from_BoolPair(s, "-stl", fo.SetArcMTime);

  // const CArcInfoEx &ai = Get_ArcInfoEx();
  AddText_from_Bool1(s, "SL",  SymLinks);
  AddText_from_Bool1(s, "HL",  HardLinks);
  AddText_from_Bool1(s, "AS",  AltStreams);
  AddText_from_Bool1(s, "Sec", NtSecurity);

  // AddText_from_Bool1(s, "Preserve", PreserveATime);

  SetItemText(IDT_COMPRESS_OPTIONS, GetUnicodeString(s));
}





// ---------- OPTIONS ----------


void COptionsDialog::CheckButton_Bool1(UINT id, const CBool1 &b1)
{
  CheckButton(id, b1.Val);
}

void COptionsDialog::GetButton_Bool1(UINT id, CBool1 &b1)
{
  b1.Val = IsButtonCheckedBool(id);
}


void COptionsDialog::CheckButton_BoolBox(
    bool supported, const CBoolPair &b2, CBoolBox &bb)
{
  const bool isSet = b2.Def;
  const bool val = isSet ? b2.Val : bb.DefaultVal;

  bb.IsSupported = supported;

  CheckButton (bb.Set_Id, isSet);
  ShowItem_Bool (bb.Set_Id, supported);
  CheckButton (bb.Id, val);
  EnableItem (bb.Id, isSet);
  ShowItem_Bool (bb.Id, supported);
}

void COptionsDialog::GetButton_BoolBox(CBoolBox &bb)
{
  // we save value for invisible buttons too
  bb.BoolPair.Val = IsButtonCheckedBool (bb.Id);
  bb.BoolPair.Def = IsButtonCheckedBool (bb.Set_Id);
}


void COptionsDialog::Store_TimeBoxes()
{
  TimePrec = GetPrecSpec();
  GetButton_BoolBox (MTime);
  GetButton_BoolBox (CTime);
  GetButton_BoolBox (ATime);
  GetButton_BoolBox (ZTime);
}


UInt32 COptionsDialog::GetComboValue(NWindows::NControl::CComboBox &c, int defMax)
{
  if (c.GetCount() <= defMax)
    return (UInt32)(Int32)-1;
  return (UInt32)c.GetItemData_of_CurSel();
}

static const unsigned kTimePrec_Win  = 0;
static const unsigned kTimePrec_Unix = 1;
static const unsigned kTimePrec_DOS  = 2;
static const unsigned kTimePrec_1ns  = 3;

static void AddTimeOption(UString &s, UInt32 val, const UString &unit, const char *sys = NULL)
{
  // s += " : ";
  {
    AString s2;
    s2.Add_UInt32(val);
    s += s2;
  }
  s.Add_Space();
  s += unit;
  if (sys)
  {
    s += " : ";
    s += sys;
  }
}

int COptionsDialog::AddPrec(unsigned prec, bool isDefault)
{
  UString s;
  UInt32 writePrec = prec;
  if (isDefault)
  {
    // s += "* ";
    // writePrec = (UInt32)(Int32)-1;
  }
       if (prec == kTimePrec_Win)  AddTimeOption(s, 100, NsString, "Windows");
  else if (prec == kTimePrec_Unix) AddTimeOption(s, 1, SecString, "Unix");
  else if (prec == kTimePrec_DOS)  AddTimeOption(s, 2, SecString, "DOS");
  else if (prec == kTimePrec_1ns)  AddTimeOption(s, 1, NsString, "Linux");
  else if (prec == k_PropVar_TimePrec_Base) AddTimeOption(s, 1, SecString);
  else if (prec >= k_PropVar_TimePrec_Base)
  {
    UInt32 d = 1;
    for (unsigned i = prec; i < k_PropVar_TimePrec_Base + 9; i++)
      d *= 10;
    AddTimeOption(s, d, NsString);
  }
  else
    s.Add_UInt32(prec);
  const int index = (int)m_Prec.AddString(s);
  m_Prec.SetItemData(index, writePrec);
  return index;
}


void COptionsDialog::SetPrec()
{
  // const CFormatInfo &fi = g_Formats[cd->GetStaticFormatIndex()];
  const CArcInfoEx &ai = cd->Get_ArcInfoEx();

  // UInt32 flags = fi.Flags;

  UInt32 flags = ai.Get_TimePrecFlags();
  UInt32 defaultPrec = ai.Get_DefaultTimePrec();
  if (defaultPrec != 0)
    flags |= ((UInt32)1 << defaultPrec);

  // const NCompression::CFormatOptions &fo = cd->Get_FormatOptions();

  // unsigned defaultPrec = kTimePrec_Win;

  if (ai.Is_GZip())
    defaultPrec = kTimePrec_Unix;

  {
    UString s;
    s += GetNameOfProperty(kpidType, L"type");
    s += ": ";
    s += ai.Name;
    if (ai.Is_Tar())
    {
      const int methodID = cd->GetMethodID();

      // for debug
      // defaultPrec = kTimePrec_Unix;
      // flags = (UInt32)1 << kTimePrec_Unix;

      s += ":";
      if (methodID >= 0 && (unsigned)methodID < ARRAY_SIZE(kMethodsNames))
        s += kMethodsNames[methodID];
      if (methodID == kPosix)
      {
        // for debug
        // flags |= (UInt32)1 << kTimePrec_Win;
        // flags |= (UInt32)1 << kTimePrec_1ns;
      }
    }
    else
    {
      // if (is_for_MethodChanging) return;
    }

    SetItemText(IDT_COMPRESS_TIME_INFO, s);
  }

  m_Prec.ResetContent();
  _auto_Prec = defaultPrec;

  unsigned selectedPrec = defaultPrec;
  {
    // if (TimePrec >= kTimePrec_Win && TimePrec <= kTimePrec_DOS)
    if ((Int32)TimePrec >= 0)
      selectedPrec = TimePrec;
  }

  int curSel = -1;
  int defaultPrecIndex = -1;
  for (unsigned prec = 0;
      // prec <= k_PropVar_TimePrec_HighPrec;
      prec <= k_PropVar_TimePrec_1ns;
      prec++)
  {
    if (((flags >> prec) & 1) == 0)
      continue;
    const bool isDefault = (defaultPrec == prec);
    const int index = AddPrec(prec, isDefault);
    if (isDefault)
      defaultPrecIndex = index;
    if (selectedPrec == prec)
      curSel = index;
  }

  if (curSel < 0 && selectedPrec > kTimePrec_DOS)
    curSel = AddPrec(selectedPrec, false); // isDefault
  if (curSel < 0)
    curSel = defaultPrecIndex;
  if (curSel >= 0)
    m_Prec.SetCurSel(curSel);

  {
    const bool isSet = IsSet_TimePrec();
    const int count = m_Prec.GetCount();
    const bool showPrec = (count != 0);
    ShowItem_Bool(IDC_COMPRESS_TIME_PREC, showPrec);
    ShowItem_Bool(IDT_COMPRESS_TIME_PREC, showPrec);
    EnableItem(IDC_COMPRESS_TIME_PREC, isSet && (count > 1));

    CheckButton(IDX_COMPRESS_PREC_SET, isSet);
    const bool setIsSupported = isSet || (count > 1);
    EnableItem(IDX_COMPRESS_PREC_SET, setIsSupported);
    ShowItem_Bool(IDX_COMPRESS_PREC_SET, setIsSupported);
  }

  SetTimeMAC();
}


void COptionsDialog::SetTimeMAC()
{
  const CArcInfoEx &ai = cd->Get_ArcInfoEx();

  const
  bool m_allow = ai.Flags_MTime();
  bool c_allow = ai.Flags_CTime();
  bool a_allow = ai.Flags_ATime();

  if (ai.Is_Tar())
  {
    const int methodID = cd->GetMethodID();
    c_allow = false;
    a_allow = false;
    if (methodID == kPosix)
    {
      // c_allow = true; // do we need it as change time ?
      a_allow = true;
    }
  }

  if (ai.Is_Zip())
  {
    // const int methodID = GetMethodID();
    UInt32 prec = GetPrec();
    if (prec == (UInt32)(Int32)-1)
      prec = _auto_Prec;
    if (prec != kTimePrec_Win)
    {
      c_allow = false;
      a_allow = false;
    }
  }


  /*
  MTime.DefaultVal = true;
  CTime.DefaultVal = false;
  ATime.DefaultVal = false;
  */

  MTime.DefaultVal = ai.Flags_MTime_Default();
  CTime.DefaultVal = ai.Flags_CTime_Default();
  ATime.DefaultVal = ai.Flags_ATime_Default();

  ZTime.DefaultVal = false;

  const NCompression::CFormatOptions &fo = cd->Get_FormatOptions();

  CheckButton_BoolBox (m_allow, fo.MTime, MTime );
  CheckButton_BoolBox (c_allow, fo.CTime, CTime );
  CheckButton_BoolBox (a_allow, fo.ATime, ATime );
  CheckButton_BoolBox (true, fo.SetArcMTime, ZTime);

  if (m_allow && !fo.MTime.Def)
  {
    const bool isSingleFile = ai.Flags_KeepName();
    if (!isSingleFile)
    {
      // we can hide changing checkboxes for MTime here:
      ShowItem_Bool (MTime.Set_Id, false);
      EnableItem (MTime.Id, false);
    }
  }
  // On_CheckBoxSet_Prec_Clicked();
  // const bool isSingleFile = ai.Flags_KeepName();
  // mtime for Gz can be
}



void COptionsDialog::On_CheckBoxSet_Prec_Clicked()
{
  const bool isSet = IsButtonCheckedBool(IDX_COMPRESS_PREC_SET);
  if (!isSet)
  {
    // We save current MAC boxes to memory before SetPrec()
    Store_TimeBoxes();
    Reset_TimePrec();
    SetPrec();
  }
  EnableItem(IDC_COMPRESS_TIME_PREC, isSet);
}

void COptionsDialog::On_CheckBoxSet_Clicked(const CBoolBox &bb)
{
  const bool isSet = IsButtonCheckedBool(bb.Set_Id);
  if (!isSet)
    CheckButton(bb.Id, bb.DefaultVal);
  EnableItem(bb.Id, isSet);
}




#ifdef LANG
static const UInt32 kLangIDs_Options[] =
{
  IDX_COMPRESS_NT_SYM_LINKS,
  IDX_COMPRESS_NT_HARD_LINKS,
  IDX_COMPRESS_NT_ALT_STREAMS,
  IDX_COMPRESS_NT_SECUR,

  IDG_COMPRESS_TIME,
  IDT_COMPRESS_TIME_PREC,
  IDX_COMPRESS_MTIME,
  IDX_COMPRESS_CTIME,
  IDX_COMPRESS_ATIME,
  IDX_COMPRESS_ZTIME,
  IDX_COMPRESS_PRESERVE_ATIME
};
#endif


bool COptionsDialog::OnInit()
{
  #ifdef LANG
  LangSetWindowText(*this, IDB_COMPRESS_OPTIONS); // IDS_OPTIONS
  LangSetDlgItems(*this, kLangIDs_Options, ARRAY_SIZE(kLangIDs_Options));
  // LangSetDlgItemText(*this, IDB_COMPRESS_TIME_DEFAULT, IDB_COMPRESS_TIME_DEFAULT);
  // LangSetDlgItemText(*this, IDX_COMPRESS_TIME_DEFAULT, IDX_COMPRESS_TIME_DEFAULT);
  #endif

  LangString(IDS_COMPRESS_SEC, SecString);
  if (SecString.IsEmpty())
    SecString = "sec";
  LangString(IDS_COMPRESS_NS, NsString);
  if (NsString.IsEmpty())
    NsString = "ns";

  {
    // const CArcInfoEx &ai = cd->Get_ArcInfoEx();

    ShowItem_Bool ( IDX_COMPRESS_NT_SYM_LINKS,    cd->SymLinks.Supported);
    ShowItem_Bool ( IDX_COMPRESS_NT_HARD_LINKS,   cd->HardLinks.Supported);
    ShowItem_Bool ( IDX_COMPRESS_NT_ALT_STREAMS,  cd->AltStreams.Supported);
    ShowItem_Bool ( IDX_COMPRESS_NT_SECUR,        cd->NtSecurity.Supported);

    ShowItem_Bool ( IDG_COMPRESS_NTFS,
           cd->SymLinks.Supported
        || cd->HardLinks.Supported
        || cd->AltStreams.Supported
        || cd->NtSecurity.Supported);
  }

   /* we read property from two sources:
       1) command line  : (Info)
       2) registry      : (m_RegistryInfo)
     (Info) has priority, if both are no defined */

  CheckButton_Bool1 ( IDX_COMPRESS_NT_SYM_LINKS,   cd->SymLinks);
  CheckButton_Bool1 ( IDX_COMPRESS_NT_HARD_LINKS,  cd->HardLinks);
  CheckButton_Bool1 ( IDX_COMPRESS_NT_ALT_STREAMS, cd->AltStreams);
  CheckButton_Bool1 ( IDX_COMPRESS_NT_SECUR,       cd->NtSecurity);

  CheckButton_Bool1 (IDX_COMPRESS_PRESERVE_ATIME, cd->PreserveATime);

  m_Prec.Attach (GetItem(IDC_COMPRESS_TIME_PREC));

  MTime.SetIDs ( IDX_COMPRESS_MTIME, IDX_COMPRESS_MTIME_SET);
  CTime.SetIDs ( IDX_COMPRESS_CTIME, IDX_COMPRESS_CTIME_SET);
  ATime.SetIDs ( IDX_COMPRESS_ATIME, IDX_COMPRESS_ATIME_SET);
  ZTime.SetIDs ( IDX_COMPRESS_ZTIME, IDX_COMPRESS_ZTIME_SET);

  {
    const NCompression::CFormatOptions &fo = cd->Get_FormatOptions();
    TimePrec = fo.TimePrec;
    MTime.BoolPair = fo.MTime;
    CTime.BoolPair = fo.CTime;
    ATime.BoolPair = fo.ATime;
    ZTime.BoolPair = fo.SetArcMTime;
  }

  SetPrec();

  NormalizePosition();

  return CModalDialog::OnInit();
}


bool COptionsDialog::OnCommand(int code, int itemID, LPARAM lParam)
{
  if (code == CBN_SELCHANGE)
  {
    switch (itemID)
    {
      case IDC_COMPRESS_TIME_PREC:
      {
        Store_TimeBoxes();
        SetTimeMAC(); // for zip/tar
        return true;
      }
    }
  }
  return CModalDialog::OnCommand(code, itemID, lParam);
}


bool COptionsDialog::OnButtonClicked(int buttonID, HWND buttonHWND)
{
  switch (buttonID)
  {
    case IDX_COMPRESS_PREC_SET:  { On_CheckBoxSet_Prec_Clicked(); return true; }
    case IDX_COMPRESS_MTIME_SET: { On_CheckBoxSet_Clicked (MTime); return true; }
    case IDX_COMPRESS_CTIME_SET: { On_CheckBoxSet_Clicked (CTime); return true; }
    case IDX_COMPRESS_ATIME_SET: { On_CheckBoxSet_Clicked (ATime); return true; }
    case IDX_COMPRESS_ZTIME_SET: { On_CheckBoxSet_Clicked (ZTime); return true; }
  }
  return CModalDialog::OnButtonClicked(buttonID, buttonHWND);
}


void COptionsDialog::OnOK()
{
  GetButton_Bool1 (IDX_COMPRESS_NT_SYM_LINKS,   cd->SymLinks);
  GetButton_Bool1 (IDX_COMPRESS_NT_HARD_LINKS,  cd->HardLinks);
  GetButton_Bool1 (IDX_COMPRESS_NT_ALT_STREAMS, cd->AltStreams);
  GetButton_Bool1 (IDX_COMPRESS_NT_SECUR,       cd->NtSecurity);
  GetButton_Bool1 (IDX_COMPRESS_PRESERVE_ATIME, cd->PreserveATime);

  Store_TimeBoxes();
  {
    NCompression::CFormatOptions &fo = cd->Get_FormatOptions();
    fo.TimePrec = TimePrec;
    fo.MTime = MTime.BoolPair;
    fo.CTime = CTime.BoolPair;
    fo.ATime = ATime.BoolPair;
    fo.SetArcMTime = ZTime.BoolPair;
  }

  CModalDialog::OnOK();
}
