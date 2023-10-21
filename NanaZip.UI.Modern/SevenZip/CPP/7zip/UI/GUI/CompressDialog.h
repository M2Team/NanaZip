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

    CBoolPair PreserveATime;

    UInt32 TimePrec;
    CBoolPair MTime;
    CBoolPair CTime;
    CBoolPair ATime;
    CBoolPair SetArcMTime;

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
      TimePrec = (UInt32)(Int32)(-1);
    }
  };
}


struct CBool1
{
  bool Val;
  bool Supported;

  CBool1(): Val(false), Supported(false) {}

  void Init()
  {
    Val = false;
    Supported = false;
  }

  void SetTrueTrue()
  {
    Val = true;
    Supported = true;
  }

  void SetVal_as_Supported(bool val)
  {
    Val = val;
    Supported = true;
  }

  /*
  bool IsVal_True_and_Defined() const
  {
    return Def && Val;
  }
  */
};


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

  int m_PrevFormat;
  UString DirPrefix;
  UString StartDirPrefix;

  bool _ramSize_Defined;
  UInt64 _ramSize;         // full RAM size avail
  UInt64 _ramSize_Reduced; // full for 64-bit and reduced for 32-bit
  UInt64 _ramUsage_Auto;

public:
  NCompression::CInfo m_RegistryInfo;

  CBool1 SymLinks;
  CBool1 HardLinks;
  CBool1 AltStreams;
  CBool1 NtSecurity;
  CBool1 PreserveATime;

  void SetArchiveName(const UString &name);
  int FindRegistryFormat(const UString &name);
  unsigned FindRegistryFormat_Always(const UString &name);

  const CArcInfoEx &Get_ArcInfoEx()
  {
    return (*ArcFormats)[GetFormatIndex()];
  }

  NCompression::CFormatOptions &Get_FormatOptions();

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

  /*
  UInt32 GetPrecSpec() { return GetComboValue(m_Prec, 1); }
  UInt32 GetPrec() { return GetComboValue(m_Prec, 0); }
  */


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
  void FormatChanged(bool isChanged);

  void OnButtonSetArchive();
  bool IsSFX();
  void OnButtonSFX();

  virtual bool OnInit();
  virtual bool OnCommand(int code, int itemID, LPARAM lParam);
  virtual bool OnButtonClicked(int buttonID, HWND buttonHWND);
  virtual void OnOK();

  void ShowOptionsString();

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
    BIG_DIALOG_SIZE(400, 320);
    return CModalDialog::Create(SIZED_DIALOG(IDD_COMPRESS), wndParent);
  }

  CCompressDialog(): CurrentDirWasChanged(false) {};
};




class COptionsDialog: public NWindows::NControl::CModalDialog
{
  struct CBoolBox
  {
    bool IsSupported;
    bool DefaultVal;
    CBoolPair BoolPair;

    int Id;
    int Set_Id;

    void SetIDs(int id, int set_Id)
    {
      Id = id;
      Set_Id = set_Id;
    }

    CBoolBox():
        IsSupported(false),
        DefaultVal(false)
        {}
  };

  CCompressDialog *cd;

  NWindows::NControl::CComboBox m_Prec;

  UInt32 _auto_Prec;
  UInt32 TimePrec;

  void Reset_TimePrec() { TimePrec = (UInt32)(Int32)-1; }
  bool IsSet_TimePrec() const { return TimePrec != (UInt32)(Int32)-1; }

  CBoolBox MTime;
  CBoolBox CTime;
  CBoolBox ATime;
  CBoolBox ZTime;

  UString SecString;
  UString NsString;


  void CheckButton_Bool1(UINT id, const CBool1 &b1);
  void GetButton_Bool1(UINT id, CBool1 &b1);
  void CheckButton_BoolBox(bool supported, const CBoolPair &b2, CBoolBox &bb);
  void GetButton_BoolBox(CBoolBox &bb);

  void Store_TimeBoxes();

  UInt32 GetComboValue(NWindows::NControl::CComboBox &c, int defMax = 0);
  UInt32 GetPrecSpec()
  {
    UInt32 prec = GetComboValue(m_Prec, 1);
    if (prec == _auto_Prec)
      prec = (UInt32)(Int32)-1;
    return prec;
  }
  UInt32 GetPrec() { return GetComboValue(m_Prec, 0); }

  // void OnButton_TimeDefault();
  int AddPrec(unsigned prec, bool isDefault);
  void SetPrec();
  void SetTimeMAC();

  void On_CheckBoxSet_Prec_Clicked();
  void On_CheckBoxSet_Clicked(const CBoolBox &bb);

  virtual bool OnInit();
  virtual bool OnCommand(int code, int itemID, LPARAM lParam);
  virtual bool OnButtonClicked(int buttonID, HWND buttonHWND);
  virtual void OnOK();

public:

  INT_PTR Create(HWND wndParent = 0)
  {
    BIG_DIALOG_SIZE(240, 232);
    return CModalDialog::Create(SIZED_DIALOG(IDD_COMPRESS_OPTIONS), wndParent);
  }

  COptionsDialog(CCompressDialog *cdLoc):
      cd(cdLoc)
      // , TimePrec(0)
      {
        Reset_TimePrec();
      };
};

#endif
