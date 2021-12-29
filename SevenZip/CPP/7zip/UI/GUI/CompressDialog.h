// CompressDialog.h

#ifndef __COMPRESS_DIALOG_H
#define __COMPRESS_DIALOG_H

#include "../../../Common/Wildcard.h"

#include "../../../Windows/Control/ComboBox.h"
#include "../../../Windows/Control/Edit.h"

#include "../Common/LoadCodecs.h"
#include "../Common/ZipRegistry.h"

#include "../FileManager/DialogSize.h"

#include "CompressDialogRes.h"

namespace NCompressDialog
{
  namespace NUpdateMode
  {
    enum EEnum
    {
      kAdd,
      kUpdate,
      kFresh,
      kSync
    };
  }

  struct CInfo
  {
    NUpdateMode::EEnum UpdateMode;
    NWildcard::ECensorPathMode PathMode;

    bool SolidIsSpecified;
    // bool MultiThreadIsAllowed;
    UInt64 SolidBlockSize;
    UInt32 NumThreads;

    NCompression::CMemUse MemUsage;

    CRecordVector<UInt64> VolumeSizes;

    UInt32 Level;
    UString Method;
    UInt64 Dict64;
    bool OrderMode;
    UInt32 Order;
    UString Options;

    UString EncryptionMethod;

    bool SFXMode;
    bool OpenShareForWrite;
    bool DeleteAfterCompressing;

    CBoolPair SymLinks;
    CBoolPair HardLinks;
    CBoolPair AltStreams;
    CBoolPair NtSecurity;

    UString ArcPath; // in: Relative or abs ; out: Relative or abs

    // FString CurrentDirPrefix;
    bool KeepName;

    bool GetFullPathName(UString &result) const;

    int FormatIndex;

    UString Password;
    bool EncryptHeadersIsAllowed;
    bool EncryptHeaders;

    CInfo():
        UpdateMode(NCompressDialog::NUpdateMode::kAdd),
        PathMode(NWildcard::k_RelatPath),
        SFXMode(false),
        OpenShareForWrite(false),
        DeleteAfterCompressing(false),
        FormatIndex(-1)
    {
      Level = Order = (UInt32)(Int32)-1;
      NumThreads = (UInt32)(Int32)-1;
      SolidIsSpecified = false;
      Dict64 = (UInt64)(Int64)(-1);
      OrderMode = false;
      Method.Empty();
      Options.Empty();
      EncryptionMethod.Empty();
    }
  };
}


class CCompressDialog: public NWindows::NControl::CModalDialog
{
  NWindows::NControl::CComboBox m_ArchivePath;
  NWindows::NControl::CComboBox m_Format;
  NWindows::NControl::CComboBox m_Level;
  NWindows::NControl::CComboBox m_Method;
  NWindows::NControl::CComboBox m_Dictionary;
  NWindows::NControl::CComboBox m_Order;
  NWindows::NControl::CComboBox m_Solid;
  NWindows::NControl::CComboBox m_NumThreads;
  NWindows::NControl::CComboBox m_MemUse;
  NWindows::NControl::CComboBox m_Volume;

  UStringVector _memUse_Strings;

  NWindows::NControl::CDialogChildControl m_Params;

  NWindows::NControl::CComboBox m_UpdateMode;
  NWindows::NControl::CComboBox m_PathMode;

  NWindows::NControl::CEdit _password1Control;
  NWindows::NControl::CEdit _password2Control;
  NWindows::NControl::CComboBox _encryptionMethod;

  int _auto_MethodId;
  UInt32 _auto_Dict; // (UInt32)(Int32)-1 means unknown
  UInt32 _auto_Order;
  UInt64 _auto_Solid;
  UInt32 _auto_NumThreads;

  int _default_encryptionMethod_Index;

  NCompression::CInfo m_RegistryInfo;

  int m_PrevFormat;
  UString DirPrefix;
  UString StartDirPrefix;

  bool _ramSize_Defined;
  UInt64 _ramSize;         // full RAM size avail
  UInt64 _ramSize_Reduced; // full for 64-bit and reduced for 32-bit
  UInt64 _ramUsage_Auto;

  void CheckButton_TwoBools(UINT id, const CBoolPair &b1, const CBoolPair &b2);
  void GetButton_Bools(UINT id, CBoolPair &b1, CBoolPair &b2);

  void SetArchiveName(const UString &name);
  int FindRegistryFormat(const UString &name);
  int FindRegistryFormatAlways(const UString &name);

  const CArcInfoEx &Get_ArcInfoEx()
  {
    return (*ArcFormats)[GetFormatIndex()];
  }

  NCompression::CFormatOptions &Get_FormatOptions()
  {
    const CArcInfoEx &ai = Get_ArcInfoEx();
    return m_RegistryInfo.Formats[ FindRegistryFormatAlways(ai.Name) ];
  }

  void CheckSFXNameChange();
  void SetArchiveName2(bool prevWasSFX);

  int GetStaticFormatIndex();

  void SetNearestSelectComboBox(NWindows::NControl::CComboBox &comboBox, UInt32 value);

  void SetLevel2();
  void SetLevel()
  {
    SetLevel2();
    EnableMultiCombo(IDC_COMPRESS_LEVEL);
    SetMethod();
  }

  void SetMethod2(int keepMethodId);
  void SetMethod(int keepMethodId = -1)
  {
    SetMethod2(keepMethodId);
    EnableMultiCombo(IDC_COMPRESS_METHOD);
  }

  void MethodChanged()
  {
    SetDictionary2();
    EnableMultiCombo(IDC_COMPRESS_DICTIONARY);
    SetOrder2();
    EnableMultiCombo(IDC_COMPRESS_ORDER);
  }

  int GetMethodID_RAW();
  int GetMethodID();

  UString GetMethodSpec(UString &estimatedName);
  UString GetMethodSpec();
  bool IsMethodEqualTo(const UString &s);
  UString GetEncryptionMethodSpec();

  bool IsZipFormat();
  bool IsXzFormat();

  void SetEncryptionMethod();

  int AddDict2(size_t sizeReal, size_t sizeShow);
  int AddDict(size_t size);

  void SetDictionary2();

  UInt32 GetComboValue(NWindows::NControl::CComboBox &c, int defMax = 0);
  UInt64 GetComboValue_64(NWindows::NControl::CComboBox &c, int defMax = 0);

  UInt32 GetLevel()  { return GetComboValue(m_Level); }
  UInt32 GetLevelSpec()  { return GetComboValue(m_Level, 1); }
  UInt32 GetLevel2();

  UInt64 GetDictSpec() { return GetComboValue_64(m_Dictionary, 1); }
  UInt64 GetDict2()
  {
    UInt64 num = GetDictSpec();
    if (num == (UInt64)(Int64)-1)
    {
      if (_auto_Dict == (UInt32)(Int32)-1)
        return (UInt64)(Int64)-1; // unknown
      num = _auto_Dict;
    }
    return num;
  }

  // UInt32 GetOrder() { return GetComboValue(m_Order); }
  UInt32 GetOrderSpec() { return GetComboValue(m_Order, 1); }
  UInt32 GetNumThreadsSpec() { return GetComboValue(m_NumThreads, 1); }

  UInt32 GetNumThreads2()
  {
    UInt32 num = GetNumThreadsSpec();
    if (num == (UInt32)(Int32)-1)
      num = _auto_NumThreads;
    return num;
  }

  UInt32 GetBlockSizeSpec() { return GetComboValue(m_Solid, 1); }

  int AddOrder(UInt32 size);
  int AddOrder_Auto();

  void SetOrder2();

  bool GetOrderMode();

  void SetSolidBlockSize2();
  void SetSolidBlockSize(/* bool useDictionary = false */)
  {
    SetSolidBlockSize2();
    EnableMultiCombo(IDC_COMPRESS_SOLID);
  }

  void SetNumThreads2();
  void SetNumThreads()
  {
    SetNumThreads2();
    EnableMultiCombo(IDC_COMPRESS_THREADS);
  }

  int AddMemComboItem(UInt64 val, bool isPercent = false, bool isDefault = false);
  void SetMemUseCombo();
  UString Get_MemUse_Spec();
  UInt64 Get_MemUse_Bytes();

  UInt64 GetMemoryUsage_Dict_DecompMem(UInt64 dict, UInt64 &decompressMemory);
  UInt64 GetMemoryUsage_Threads_Dict_DecompMem(UInt32 numThreads, UInt64 dict, UInt64 &decompressMemory);
  UInt64 GetMemoryUsage_DecompMem(UInt64 &decompressMemory);
  UInt64 GetMemoryUsageComp_Threads_Dict(UInt32 numThreads, UInt64 dict64);

  void PrintMemUsage(UINT res, UInt64 value);
  void SetMemoryUsage();
  void SetParams();
  void SaveOptionsInMem();

  void UpdatePasswordControl();
  bool IsShowPasswordChecked() const { return IsButtonCheckedBool(IDX_PASSWORD_SHOW); }

  unsigned GetFormatIndex();
  bool SetArcPathFields(const UString &path, UString &name, bool always);
  bool GetFinalPath_Smart(UString &resPath);

  void CheckSFXControlsEnable();
  // void CheckVolumeEnable();
  void EnableMultiCombo(unsigned id);
  void FormatChanged();

  void OnButtonSetArchive();
  bool IsSFX();
  void OnButtonSFX();

  virtual bool OnInit();
  virtual bool OnCommand(int code, int itemID, LPARAM lParam);
  virtual bool OnButtonClicked(int buttonID, HWND buttonHWND);
  virtual void OnOK();

//   void MessageBoxError(LPCWSTR message)
//   {
//     MessageBoxW(*this, message, L"NanaZip", MB_ICONERROR);
//   }

public:
  const CObjectVector<CArcInfoEx> *ArcFormats;
  CUIntVector ArcIndices; // can not be empty, must contain Info.FormatIndex, if Info.FormatIndex >= 0
  AStringVector ExternalMethods;

  void SetMethods(const CObjectVector<CCodecInfoUser> &userCodecs);

  NCompressDialog::CInfo Info;
  UString OriginalFileName; // for bzip2, gzip2
  bool CurrentDirWasChanged;

  INT_PTR Create(HWND wndParent = 0)
  {
    BIG_DIALOG_SIZE(400, 304);
    return CModalDialog::Create(SIZED_DIALOG(IDD_COMPRESS), wndParent);
  }

  CCompressDialog(): CurrentDirWasChanged(false) {};
};

#endif
