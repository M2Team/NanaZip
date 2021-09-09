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
    bool MultiThreadIsAllowed;
    UInt64 SolidBlockSize;
    UInt32 NumThreads;

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

  NWindows::NControl::CComboBox m_UpdateMode;
  NWindows::NControl::CComboBox m_PathMode;
  
  NWindows::NControl::CComboBox m_Volume;
  NWindows::NControl::CDialogChildControl m_Params;

  NWindows::NControl::CEdit _password1Control;
  NWindows::NControl::CEdit _password2Control;
  NWindows::NControl::CComboBox _encryptionMethod;
  int _default_encryptionMethod_Index;

  NCompression::CInfo m_RegistryInfo;

  int m_PrevFormat;
  UString DirPrefix;
  UString StartDirPrefix;

  void CheckButton_TwoBools(UINT id, const CBoolPair &b1, const CBoolPair &b2);
  void GetButton_Bools(UINT id, CBoolPair &b1, CBoolPair &b2);

  void SetArchiveName(const UString &name);
  int FindRegistryFormat(const UString &name);
  int FindRegistryFormatAlways(const UString &name);
  
  void CheckSFXNameChange();
  void SetArchiveName2(bool prevWasSFX);
  
  int GetStaticFormatIndex();

  void SetNearestSelectComboBox(NWindows::NControl::CComboBox &comboBox, UInt32 value);

  void SetLevel();
  
  void SetMethod(int keepMethodId = -1);
  int GetMethodID();
  UString GetMethodSpec();
  UString GetEncryptionMethodSpec();

  bool IsZipFormat();
  bool IsXzFormat();

  void SetEncryptionMethod();

  void AddDict2(size_t sizeReal, size_t sizeShow);
  void AddDict(size_t size);
  
  void SetDictionary();

  UInt32 GetComboValue(NWindows::NControl::CComboBox &c, int defMax = 0);
  UInt64 GetComboValue_64(NWindows::NControl::CComboBox &c, int defMax = 0);

  UInt32 GetLevel()  { return GetComboValue(m_Level); }
  UInt32 GetLevelSpec()  { return GetComboValue(m_Level, 1); }
  UInt32 GetLevel2();
  UInt64 GetDict() { return GetComboValue_64(m_Dictionary); }
  UInt64 GetDictSpec() { return GetComboValue_64(m_Dictionary, 1); }
  UInt32 GetOrder() { return GetComboValue(m_Order); }
  UInt32 GetOrderSpec() { return GetComboValue(m_Order, 1); }
  UInt32 GetNumThreadsSpec() { return GetComboValue(m_NumThreads, 1); }
  UInt32 GetNumThreads2() { UInt32 num = GetNumThreadsSpec(); if (num == UInt32(-1)) num = 1; return num; }
  UInt32 GetBlockSizeSpec() { return GetComboValue(m_Solid, 1); }

  int AddOrder(UInt32 size);
  void SetOrder();
  bool GetOrderMode();

  void SetSolidBlockSize(bool useDictionary = false);
  void SetNumThreads();

  UInt64 GetMemoryUsage_Dict_DecompMem(UInt64 dict, UInt64 &decompressMemory);
  UInt64 GetMemoryUsage_DecompMem(UInt64 &decompressMemory);
  UInt64 GetMemoryUsageComp_Dict(UInt64 dict64);

  void PrintMemUsage(UINT res, UInt64 value);
  void SetMemoryUsage();
  void SetParams();
  void SaveOptionsInMem();

  void UpdatePasswordControl();
  bool IsShowPasswordChecked() const { return IsButtonCheckedBool(IDX_PASSWORD_SHOW); }

  unsigned GetFormatIndex();
  bool SetArcPathFields(const UString &path, UString &name, bool always);
  bool GetFinalPath_Smart(UString &resPath);

public:
  CObjectVector<CArcInfoEx> *ArcFormats;
  CUIntVector ArcIndices; // can not be empty, must contain Info.FormatIndex, if Info.FormatIndex >= 0

  NCompressDialog::CInfo Info;
  UString OriginalFileName; // for bzip2, gzip2
  bool CurrentDirWasChanged;

  INT_PTR Create(HWND wndParent = 0)
  {
    BIG_DIALOG_SIZE(400, 304);
    return CModalDialog::Create(SIZED_DIALOG(IDD_COMPRESS), wndParent);
  }

  CCompressDialog(): CurrentDirWasChanged(false) {};

  void MessageBoxError(LPCWSTR message)
  {
    MessageBoxW(*this, message, L"7-Zip", MB_ICONERROR);
  }

protected:

  void CheckSFXControlsEnable();
  // void CheckVolumeEnable();
  void CheckControlsEnable();

  void OnButtonSetArchive();
  bool IsSFX();
  void OnButtonSFX();

  virtual bool OnInit();
  virtual bool OnCommand(int code, int itemID, LPARAM lParam);
  virtual bool OnButtonClicked(int buttonID, HWND buttonHWND);
  virtual void OnOK();
  virtual void OnHelp();

};

#endif
