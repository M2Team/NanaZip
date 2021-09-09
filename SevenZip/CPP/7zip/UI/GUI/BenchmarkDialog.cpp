// BenchmarkDialog.cpp

#include "StdAfx.h"

#include "../../../../C/CpuArch.h"

#include "../../../Common/Defs.h"
#include "../../../Common/IntToString.h"
#include "../../../Common/MyException.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/StringToInt.h"

#include "../../../Windows/Synchronization.h"
#include "../../../Windows/System.h"
#include "../../../Windows/Thread.h"
#include "../../../Windows/SystemInfo.h"

#include "../../../Windows/Control/ComboBox.h"
#include "../../../Windows/Control/Edit.h"

#include "../../Common/MethodProps.h"

#include "../FileManager/DialogSize.h"
#include "../FileManager/HelpUtils.h"
#ifdef LANG
#include "../FileManager/LangUtils.h"
#endif

#include "../../MyVersion.h"

#include "../Common/Bench.h"

#include "BenchmarkDialogRes.h"
#include "BenchmarkDialog.h"

using namespace NWindows;

#define kHelpTopic "fm/benchmark.htm"

static const UINT_PTR kTimerID = 4;
static const UINT kTimerElapse = 1000; // 1000

// use PRINT_ITER_TIME to show time of each iteration in log box
// #define PRINT_ITER_TIME

static const unsigned kRatingVector_NumBundlesMax = 20;

enum MyBenchMessages
{
  k_Message_Finished = WM_APP + 1
};

enum My_Message_WPARAM
{
  k_Msg_WPARM_Thread_Finished = 0,
  k_Msg_WPARM_Iter_Finished,
  k_Msg_WPARM_Enc1_Finished
};


struct CBenchPassResult
{
  CTotalBenchRes Enc;
  CTotalBenchRes Dec;
  #ifdef PRINT_ITER_TIME
  DWORD Ticks;
  #endif
  // CBenchInfo EncInfo; // for debug
  // CBenchPassResult() {};
};


struct CTotalBenchRes2: public CTotalBenchRes
{
  UInt64 UnpackSize;

  void Init()
  {
    CTotalBenchRes::Init();
    UnpackSize = 0;
  }

  void SetFrom_BenchInfo(const CBenchInfo &info)
  {
    NumIterations2 = 1;
    Generate_From_BenchInfo(info);
    UnpackSize = info.Get_UnpackSize_Full();
  }

  void Update_With_Res2(const CTotalBenchRes2 &r)
  {
    Update_With_Res(r);
    UnpackSize += r.UnpackSize;
  }
};

  
struct CSyncData
{
  UInt32 NumPasses_Finished;

  // UInt64 NumEncProgress; // for debug
  // UInt64 NumDecProgress; // for debug
  // CBenchInfo EncInfo; // for debug

  CTotalBenchRes2 Enc_BenchRes_1;
  CTotalBenchRes2 Enc_BenchRes;

  CTotalBenchRes2 Dec_BenchRes_1;
  CTotalBenchRes2 Dec_BenchRes;

  #ifdef PRINT_ITER_TIME
  DWORD TotalTicks;
  #endif

  int RatingVector_DeletedIndex;
  // UInt64 RatingVector_NumDeleted;

  bool BenchWasFinished; // all passes were finished
  bool NeedPrint_Freq;
  bool NeedPrint_RatingVector;
  bool NeedPrint_Enc_1;
  bool NeedPrint_Enc;
  bool NeedPrint_Dec_1;
  bool NeedPrint_Dec;
  bool NeedPrint_Tot; // intermediate Total was updated after current pass

  void Init();
};


void CSyncData::Init()
{
  NumPasses_Finished = 0;
  
  // NumEncProgress = 0;
  // NumDecProgress = 0;
  
  Enc_BenchRes.Init();
  Enc_BenchRes_1.Init();
  Dec_BenchRes.Init();
  Dec_BenchRes_1.Init();
  
  #ifdef PRINT_ITER_TIME
  TotalTicks = 0;
  #endif
  
  RatingVector_DeletedIndex = -1;
  // RatingVector_NumDeleted = 0;
  
  BenchWasFinished =
    NeedPrint_Freq =
    NeedPrint_RatingVector =
    NeedPrint_Enc_1 =
    NeedPrint_Enc   =
    NeedPrint_Dec_1 =
    NeedPrint_Dec   =
    NeedPrint_Tot   = false;
};


struct CBenchProgressSync
{
  bool Exit; // GUI asks BenchThread to Exit, and BenchThread reads that variable
  UInt32 NumThreads;
  UInt64 DictSize;
  UInt32 NumPasses_Limit;
  int Level;
  
  // must be written by benchmark thread, read by GUI thread */
  CSyncData sd;
  CRecordVector<CBenchPassResult> RatingVector;

  NWindows::NSynchronization::CCriticalSection CS;

  AString Text;
  bool TextWasChanged;

  /* BenchFinish_Task_HRESULT    - for result from benchmark code
     BenchFinish_Thread_HRESULT  - for Exceptions and service errors
             these arreos must be shown even if user escapes benchmark */

  HRESULT BenchFinish_Task_HRESULT;
  HRESULT BenchFinish_Thread_HRESULT;

  UInt32 NumFreqThreadsPrev;
  UString FreqString_Sync;
  UString FreqString_GUI;

  CBenchProgressSync()
  {
    NumPasses_Limit = 1;
  }

  void Init();
  
  void SendExit()
  {
    NWindows::NSynchronization::CCriticalSectionLock lock(CS);
    Exit = true;
  }
};


void CBenchProgressSync::Init()
{
  Exit = false;
  
  BenchFinish_Task_HRESULT = S_OK;
  BenchFinish_Thread_HRESULT = S_OK;
  
  sd.Init();
  RatingVector.Clear();
  
  NumFreqThreadsPrev = 0;
  FreqString_Sync.Empty();
  FreqString_GUI.Empty();
  
  Text.Empty();
  TextWasChanged = true;
}



struct CMyFont
{
  HFONT _font;
  CMyFont(): _font(NULL) {}
  ~CMyFont()
  {
    if (_font)
      DeleteObject(_font);
  }
  void Create(const LOGFONT *lplf)
  {
    _font = CreateFontIndirect(lplf);
  }
};


class CBenchmarkDialog;

struct CThreadBenchmark
{
  CBenchmarkDialog *BenchmarkDialog;
  DECL_EXTERNAL_CODECS_LOC_VARS2;
  // HRESULT Result;

  HRESULT Process();
  static THREAD_FUNC_DECL MyThreadFunction(void *param)
  {
    /* ((CThreadBenchmark *)param)->Result = */
    ((CThreadBenchmark *)param)->Process();
    return 0;
  }
};


class CBenchmarkDialog:
  public NWindows::NControl::CModalDialog
{
  NWindows::NControl::CComboBox m_Dictionary;
  NWindows::NControl::CComboBox m_NumThreads;
  NWindows::NControl::CComboBox m_NumPasses;
  NWindows::NControl::CEdit _consoleEdit;
  UINT_PTR _timer;

  UInt32 _startTime;
  UInt32 _finishTime;
  bool _finishTime_WasSet;
  
  bool WasStopped_in_GUI;
  bool ExitWasAsked_in_GUI;
  bool NeedRestart;

  CMyFont _font;

  UInt64 RamSize;
  UInt64 RamSize_Limit;
  bool RamSize_Defined;

  UInt32 NumPasses_Finished_Prev;

  UString ElapsedSec_Prev;

  void InitSyncNew()
  {
    NumPasses_Finished_Prev = (UInt32)(Int32)-1;
    ElapsedSec_Prev.Empty();
    Sync.Init();
  }

  virtual bool OnInit();
  virtual bool OnDestroy();
  virtual bool OnSize(WPARAM /* wParam */, int xSize, int ySize);
  virtual bool OnMessage(UINT message, WPARAM wParam, LPARAM lParam);
  virtual bool OnCommand(int code, int itemID, LPARAM lParam);
  virtual void OnHelp();
  virtual void OnCancel();
  virtual bool OnTimer(WPARAM timerID, LPARAM callback);
  virtual bool OnButtonClicked(int buttonID, HWND buttonHWND);

  void Disable_Stop_Button();
  void OnStopButton();
  void RestartBenchmark();
  void StartBenchmark();

  void UpdateGui();

  void PrintTime();
  void PrintRating(UInt64 rating, UINT controlID);
  void PrintUsage(UInt64 usage, UINT controlID);
  void PrintBenchRes(const CTotalBenchRes2 &info, const UINT ids[]);

  UInt32 GetNumberOfThreads();
  size_t OnChangeDictionary();

  void SetItemText_Number(int itemID, UInt64 val, LPCTSTR post = NULL);
  void Print_MemUsage(UString &s, UInt64 memUsage) const;
  bool IsMemoryUsageOK(UInt64 memUsage) const
    { return memUsage + (1 << 20) <= RamSize_Limit; }

  void MyKillTimer();

  void SendExit_Status(const wchar_t *message)
  {
    SetItemText(IDT_BENCH_ERROR_MESSAGE, message);
    Sync.SendExit();
  }

public:
  CBenchProgressSync Sync;

  bool TotalMode;
  CObjectVector<CProperty> Props;

  CSysString Bench2Text;

  NWindows::CThread _thread;
  CThreadBenchmark _threadBenchmark;

  CBenchmarkDialog():
      _timer(0),
      TotalMode(false),
      WasStopped_in_GUI(false),
      ExitWasAsked_in_GUI(false),
      NeedRestart(false)
      {}

  ~CBenchmarkDialog();

  bool PostMsg_Finish(LPARAM param)
  {
    if ((HWND)*this)
      return PostMsg(k_Message_Finished, param);
    // the (HWND)*this is NULL only for some internal code failure
    return true;
  }

  INT_PTR Create(HWND wndParent = 0)
  {
    BIG_DIALOG_SIZE(332, 228);
    return CModalDialog::Create(TotalMode ? IDD_BENCH_TOTAL : SIZED_DIALOG(IDD_BENCH), wndParent);
  }
  void MessageBoxError(LPCWSTR message)
  {
    MessageBoxW(*this, message, L"7-Zip", MB_ICONERROR);
  }
  void MessageBoxError_Status(LPCWSTR message)
  {
    UString s ("ERROR: ");
    s += message;
    MessageBoxError(s);
    SetItemText(IDT_BENCH_ERROR_MESSAGE, s);
  }
};









UString HResultToMessage(HRESULT errorCode);

#ifdef LANG
static const UInt32 kLangIDs[] =
{
  IDT_BENCH_DICTIONARY,
  IDT_BENCH_MEMORY,
  IDT_BENCH_NUM_THREADS,
  IDT_BENCH_SPEED,
  IDT_BENCH_RATING_LABEL,
  IDT_BENCH_USAGE_LABEL,
  IDT_BENCH_RPU_LABEL,
  IDG_BENCH_COMPRESSING,
  IDG_BENCH_DECOMPRESSING,
  IDG_BENCH_TOTAL_RATING,
  IDT_BENCH_CURRENT,
  IDT_BENCH_RESULTING,
  IDT_BENCH_ELAPSED,
  IDT_BENCH_PASSES,
  IDB_STOP,
  IDB_RESTART
};

static const UInt32 kLangIDs_Colon[] =
{
  IDT_BENCH_SIZE
};

#endif

static LPCTSTR const kProcessingString = TEXT("...");
static LPCTSTR const kGB = TEXT(" GB");
static LPCTSTR const kMB = TEXT(" MB");
static LPCTSTR const kKB = TEXT(" KB");
// static LPCTSTR const kMIPS = TEXT(" MIPS");
static LPCTSTR const kKBs = TEXT(" KB/s");

static const unsigned kMinDicLogSize = 18;

static const UInt32 kMinDicSize = (UInt32)1 << kMinDicLogSize;
static const size_t kMaxDicSize = (size_t)1 << (22 + sizeof(size_t) / 4 * 5);
// static const size_t kMaxDicSize = (size_t)1 << 16;
    /*
    #ifdef MY_CPU_64BIT
      (UInt32)(Int32)-1; // we can use it, if we want 4 GB buffer
      // (UInt32)15 << 28;
    #else
      (UInt32)1 << 27;
    #endif
    */


static int ComboBox_Add_UInt32(NWindows::NControl::CComboBox &cb, UInt32 v)
{
  TCHAR s[16];
  ConvertUInt32ToString(v, s);
  int index = (int)cb.AddString(s);
  cb.SetItemData(index, v);
  return index;
}


bool CBenchmarkDialog::OnInit()
{
  #ifdef LANG
  LangSetWindowText(*this, IDD_BENCH);
  LangSetDlgItems(*this, kLangIDs, ARRAY_SIZE(kLangIDs));
  // LangSetDlgItems_Colon(*this, kLangIDs_Colon, ARRAY_SIZE(kLangIDs_Colon));
  LangSetDlgItemText(*this, IDT_BENCH_CURRENT2, IDT_BENCH_CURRENT);
  LangSetDlgItemText(*this, IDT_BENCH_RESULTING2, IDT_BENCH_RESULTING);
  #endif

  InitSyncNew();

  if (TotalMode)
  {
    _consoleEdit.Attach(GetItem(IDE_BENCH2_EDIT));
    LOGFONT f;
    memset(&f, 0, sizeof(f));
    f.lfHeight = 14;
    f.lfWidth = 0;
    f.lfWeight = FW_DONTCARE;
    f.lfCharSet = DEFAULT_CHARSET;
    f.lfOutPrecision = OUT_DEFAULT_PRECIS;
    f.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    f.lfQuality = DEFAULT_QUALITY;

    f.lfPitchAndFamily = FIXED_PITCH;
    // MyStringCopy(f.lfFaceName, TEXT(""));
    // f.lfFaceName[0] = 0;
    _font.Create(&f);
    if (_font._font)
      _consoleEdit.SendMsg(WM_SETFONT, (WPARAM)_font._font, TRUE);
  }

  UInt32 numCPUs = 1;

  {
    AString s ("/ ");
  
    NSystem::CProcessAffinity threadsInfo;
    threadsInfo.InitST();

    #ifndef _7ZIP_ST
    if (threadsInfo.Get() && threadsInfo.processAffinityMask != 0)
      numCPUs = threadsInfo.GetNumProcessThreads();
    else
      numCPUs = NSystem::GetNumberOfProcessors();
    #endif

    s.Add_UInt32(numCPUs);
    s += GetProcessThreadsInfo(threadsInfo);
    SetItemTextA(IDT_BENCH_HARDWARE_THREADS, s);
  
    {
      AString s2;
      GetSysInfo(s, s2);
      SetItemTextA(IDT_BENCH_SYS1, s);
      if (s != s2 && !s2.IsEmpty())
        SetItemTextA(IDT_BENCH_SYS2, s2);
    }
    {
      GetCpuName_MultiLine(s);
      SetItemTextA(IDT_BENCH_CPU, s);
    }
    {
      GetOsInfoText(s);
      s += " : ";
      AddCpuFeatures(s);
      SetItemTextA(IDT_BENCH_CPU_FEATURE, s);
    }

    s = "7-Zip " MY_VERSION_CPU;
    SetItemTextA(IDT_BENCH_VER, s);
  }


  // ----- Num Threads ----------

  if (numCPUs < 1)
    numCPUs = 1;
  numCPUs = MyMin(numCPUs, (UInt32)(1 << 6)); // it's WIN32 limit

  UInt32 numThreads = Sync.NumThreads;

  if (numThreads == (UInt32)(Int32)-1)
    numThreads = numCPUs;
  if (numThreads > 1)
    numThreads &= ~1;
  const UInt32 kNumThreadsMax = (1 << 12);
  if (numThreads > kNumThreadsMax)
    numThreads = kNumThreadsMax;

  m_NumThreads.Attach(GetItem(IDC_BENCH_NUM_THREADS));
  const UInt32 numTheads_Combo = numCPUs * 2;
  UInt32 v = 1;
  int cur = 0;
  for (; v <= numTheads_Combo;)
  {
    int index = ComboBox_Add_UInt32(m_NumThreads, v);
    const UInt32 vNext = v + (v < 2 ? 1 : 2);
    if (v <= numThreads)
    if (numThreads < vNext || vNext > numTheads_Combo)
    {
      if (v != numThreads)
        index = ComboBox_Add_UInt32(m_NumThreads, numThreads);
      cur = index;
    }
    v = vNext;
  }
  m_NumThreads.SetCurSel(cur);
  Sync.NumThreads = GetNumberOfThreads();


  // ----- Dictionary ----------

  m_Dictionary.Attach(GetItem(IDC_BENCH_DICTIONARY));
  
  RamSize = (UInt64)(sizeof(size_t)) << 29;
  RamSize_Defined = NSystem::GetRamSize(RamSize);

  
  #ifdef UNDER_CE
  const UInt32 kNormalizedCeSize = (16 << 20);
  if (RamSize > kNormalizedCeSize && RamSize < (33 << 20))
    RamSize = kNormalizedCeSize;
  #endif
  RamSize_Limit = RamSize / 16 * 15;

  if (Sync.DictSize == (UInt64)(Int64)-1)
  {
    unsigned dicSizeLog = 25;
    #ifdef UNDER_CE
    dicSizeLog = 20;
    #endif
    if (RamSize_Defined)
    for (; dicSizeLog > kBenchMinDicLogSize; dicSizeLog--)
      if (IsMemoryUsageOK(GetBenchMemoryUsage(
          Sync.NumThreads, Sync.Level, (UInt64)1 << dicSizeLog, TotalMode)))
        break;
    Sync.DictSize = (UInt64)1 << dicSizeLog;
  }
  
  if (Sync.DictSize < kMinDicSize) Sync.DictSize = kMinDicSize;
  if (Sync.DictSize > kMaxDicSize) Sync.DictSize = kMaxDicSize;

  cur = 0;
  for (unsigned i = (kMinDicLogSize - 1) * 2; i <= (32 - 1) * 2; i++)
   {
      const size_t dict = (size_t)(2 + (i & 1)) << (i / 2);
      // if (i == (32 - 1) * 2) dict = kMaxDicSize;
      TCHAR s[32];
      const TCHAR *post;
      UInt32 d;
           if (dict >= ((UInt32)1 << 31)) { d = (UInt32)(dict >> 30); post = kGB; }
      else if (dict >= ((UInt32)1 << 21)) { d = (UInt32)(dict >> 20); post = kMB; }
      else                                { d = (UInt32)(dict >> 10); post = kKB; }
      ConvertUInt32ToString(d, s);
      lstrcat(s, post);
      const int index = (int)m_Dictionary.AddString(s);
      m_Dictionary.SetItemData(index, dict);
      if (dict <= Sync.DictSize)
        cur = index;
      if (dict >= kMaxDicSize)
        break;
    }
  m_Dictionary.SetCurSel(cur);


  // ----- Num Passes ----------

  m_NumPasses.Attach(GetItem(IDC_BENCH_NUM_PASSES));
  cur = 0;
  v = 1;
  for (;;)
  {
    int index = ComboBox_Add_UInt32(m_NumPasses, v);
    const bool isLast = (v >= 10000000);
    UInt32 vNext = v * 10;
         if (v < 2) vNext = 2;
    else if (v < 5) vNext = 5;
    else if (v < 10) vNext = 10;

    if (v <= Sync.NumPasses_Limit)
    if (isLast || Sync.NumPasses_Limit < vNext)
    {
      if (v != Sync.NumPasses_Limit)
        index = ComboBox_Add_UInt32(m_NumPasses, Sync.NumPasses_Limit);
      cur = index;
    }
    v = vNext;
    if (isLast)
      break;
  }
  m_NumPasses.SetCurSel(cur);

  if (TotalMode)
    NormalizeSize(true);
  else
    NormalizePosition();

  RestartBenchmark();

  return CModalDialog::OnInit();
}


bool CBenchmarkDialog::OnSize(WPARAM /* wParam */, int xSize, int ySize)
{
  int mx, my;
  GetMargins(8, mx, my);

  if (!TotalMode)
  {
    RECT rect;
    GetClientRectOfItem(IDT_BENCH_LOG, rect);
    int x = xSize - rect.left - mx;
    int y = ySize - rect.top - my;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    MoveItem(IDT_BENCH_LOG, rect.left, rect.top, x, y, true);
    return false;
  }

  int bx1, bx2, by;

  GetItemSizes(IDCANCEL, bx1, by);
  GetItemSizes(IDHELP, bx2, by);

  {
    int y = ySize - my - by;
    int x = xSize - mx - bx1;
    
    InvalidateRect(NULL);
    
    MoveItem(IDCANCEL, x, y, bx1, by);
    MoveItem(IDHELP, x - mx - bx2, y, bx2, by);
  }

  if (_consoleEdit)
  {
    int yPos = ySize - my - by;
    RECT rect;
    GetClientRectOfItem(IDE_BENCH2_EDIT, rect);
    int y = rect.top;
    int ySize2 = yPos - my - y;
    const int kMinYSize = 20;
    int xx = xSize - mx * 2;
    if (ySize2 < kMinYSize)
    {
      ySize2 = kMinYSize;
    }
    _consoleEdit.Move(mx, y, xx, ySize2);
  }
  return false;
}


UInt32 CBenchmarkDialog::GetNumberOfThreads()
{
  return (UInt32)m_NumThreads.GetItemData_of_CurSel();
}


#define UINT_TO_STR_3(s, val) { \
  s[0] = (wchar_t)('0' + (val) / 100); \
  s[1] = (wchar_t)('0' + (val) % 100 / 10); \
  s[2] = (wchar_t)('0' + (val) % 10); \
  s[3] = 0; }

static void NumberToDot3(UInt64 val, WCHAR *s)
{
  ConvertUInt64ToString(val / 1000, s);
  const UInt32 rem = (UInt32)(val % 1000);
  s += MyStringLen(s);
  *s++ = '.';
  UINT_TO_STR_3(s, rem);
}

void CBenchmarkDialog::SetItemText_Number(int itemID, UInt64 val, LPCTSTR post)
{
  TCHAR s[64];
  ConvertUInt64ToString(val, s);
  if (post)
    lstrcat(s, post);
  SetItemText(itemID, s);
}

static void AddSize_MB(UString &s, UInt64 size)
{
  char temp[32];
  ConvertUInt64ToString((size + (1 << 20) - 1) >> 20, temp);
  s += temp;
  s += kMB;
}

void CBenchmarkDialog::Print_MemUsage(UString &s, UInt64 memUsage) const
{
  AddSize_MB(s, memUsage);
  if (RamSize_Defined)
  {
    s += " / ";
    AddSize_MB(s, RamSize);
  }
}

size_t CBenchmarkDialog::OnChangeDictionary()
{
  const size_t dict = (size_t)m_Dictionary.GetItemData_of_CurSel();
  const UInt64 memUsage = GetBenchMemoryUsage(GetNumberOfThreads(),
      Sync.Level,
      dict,
      false); // totalBench mode

  UString s;
  Print_MemUsage(s, memUsage);

  #ifdef _7ZIP_LARGE_PAGES
  {
    AString s2;
    Add_LargePages_String(s2);
    if (!s2.IsEmpty())
    {
      s.Add_Space();
      s += s2;
    }
  }
  #endif

  SetItemText(IDT_BENCH_MEMORY_VAL, s);

  return dict;
}


static const UInt32 g_IDs[] =
{
  IDT_BENCH_COMPRESS_SIZE1,
  IDT_BENCH_COMPRESS_SIZE2,
  IDT_BENCH_COMPRESS_USAGE1,
  IDT_BENCH_COMPRESS_USAGE2,
  IDT_BENCH_COMPRESS_SPEED1,
  IDT_BENCH_COMPRESS_SPEED2,
  IDT_BENCH_COMPRESS_RATING1,
  IDT_BENCH_COMPRESS_RATING2,
  IDT_BENCH_COMPRESS_RPU1,
  IDT_BENCH_COMPRESS_RPU2,
  
  IDT_BENCH_DECOMPR_SIZE1,
  IDT_BENCH_DECOMPR_SIZE2,
  IDT_BENCH_DECOMPR_SPEED1,
  IDT_BENCH_DECOMPR_SPEED2,
  IDT_BENCH_DECOMPR_RATING1,
  IDT_BENCH_DECOMPR_RATING2,
  IDT_BENCH_DECOMPR_USAGE1,
  IDT_BENCH_DECOMPR_USAGE2,
  IDT_BENCH_DECOMPR_RPU1,
  IDT_BENCH_DECOMPR_RPU2,
  
  IDT_BENCH_TOTAL_USAGE_VAL,
  IDT_BENCH_TOTAL_RATING_VAL,
  IDT_BENCH_TOTAL_RPU_VAL
};
  

static const unsigned k_Ids_Enc_1[] = {
  IDT_BENCH_COMPRESS_USAGE1,
  IDT_BENCH_COMPRESS_SPEED1,
  IDT_BENCH_COMPRESS_RPU1,
  IDT_BENCH_COMPRESS_RATING1,
  IDT_BENCH_COMPRESS_SIZE1 };

static const unsigned k_Ids_Enc[] = {
  IDT_BENCH_COMPRESS_USAGE2,
  IDT_BENCH_COMPRESS_SPEED2,
  IDT_BENCH_COMPRESS_RPU2,
  IDT_BENCH_COMPRESS_RATING2,
  IDT_BENCH_COMPRESS_SIZE2 };

static const unsigned k_Ids_Dec_1[] = {
  IDT_BENCH_DECOMPR_USAGE1,
  IDT_BENCH_DECOMPR_SPEED1,
  IDT_BENCH_DECOMPR_RPU1,
  IDT_BENCH_DECOMPR_RATING1,
  IDT_BENCH_DECOMPR_SIZE1 };

static const unsigned k_Ids_Dec[] = {
  IDT_BENCH_DECOMPR_USAGE2,
  IDT_BENCH_DECOMPR_SPEED2,
  IDT_BENCH_DECOMPR_RPU2,
  IDT_BENCH_DECOMPR_RATING2,
  IDT_BENCH_DECOMPR_SIZE2 };

static const unsigned k_Ids_Tot[] = {
  IDT_BENCH_TOTAL_USAGE_VAL,
  0,
  IDT_BENCH_TOTAL_RPU_VAL,
  IDT_BENCH_TOTAL_RATING_VAL,
  0 };


void CBenchmarkDialog::MyKillTimer()
{
  if (_timer != 0)
  {
    KillTimer(kTimerID);
    _timer = 0;
  }
}


bool CBenchmarkDialog::OnDestroy()
{
  /* actually timer was removed before.
     also the timer must be removed by Windows, when window  will be removed. */
  MyKillTimer(); // it's optional code
  return false; // we return (false) to perform default dialog operation
}

void SetErrorMessage_MemUsage(UString &s, UInt64 reqSize, UInt64 ramSize, UInt64 ramLimit, const UString &usageString);

void CBenchmarkDialog::StartBenchmark()
{
  NeedRestart = false;
  WasStopped_in_GUI = false;

  SetItemText_Empty(IDT_BENCH_ERROR_MESSAGE);
  
  MyKillTimer(); // optional code. timer was killed before

  const size_t dict = OnChangeDictionary();
  const UInt32 numThreads = GetNumberOfThreads();
  const UInt32 numPasses = (UInt32)m_NumPasses.GetItemData_of_CurSel();

  for (unsigned i = 0; i < ARRAY_SIZE(g_IDs); i++)
    SetItemText(g_IDs[i], kProcessingString);

  SetItemText_Empty(IDT_BENCH_LOG);
  SetItemText_Empty(IDT_BENCH_ELAPSED_VAL);
  SetItemText_Empty(IDT_BENCH_ERROR_MESSAGE);
  
  const UInt64 memUsage = GetBenchMemoryUsage(numThreads, Sync.Level, dict,
      false); // totalBench
  if (!IsMemoryUsageOK(memUsage))
  {
    UString s2 = LangString(IDT_BENCH_MEMORY);
    if (s2.IsEmpty())
      GetItemText(IDT_BENCH_MEMORY, s2);
    UString s;
    SetErrorMessage_MemUsage(s, memUsage, RamSize, RamSize_Limit, s2);
    MessageBoxError_Status(s);
    return;
  }

  EnableItem(IDB_STOP, true);

  _startTime = GetTickCount();
  _finishTime = _startTime;
  _finishTime_WasSet = false;

  {
    NWindows::NSynchronization::CCriticalSectionLock lock(Sync.CS);
    InitSyncNew();
    Sync.DictSize = dict;
    Sync.NumThreads = numThreads;
    Sync.NumPasses_Limit = numPasses;
  }

  PrintTime();

  _timer = SetTimer(kTimerID, kTimerElapse);
  if (_thread.Create(CThreadBenchmark::MyThreadFunction, &_threadBenchmark) != 0)
  {
    MyKillTimer();
    MessageBoxError_Status(L"Can't create thread");
  };
  return;
}


void CBenchmarkDialog::RestartBenchmark()
{
  if (ExitWasAsked_in_GUI)
    return;

  if (_thread.IsCreated())
  {
    NeedRestart = true;
    SendExit_Status(L"Stop for restart ...");
  }
  else
    StartBenchmark();
}


void CBenchmarkDialog::Disable_Stop_Button()
{
  // if we disable focused button, then focus will be lost
  if (GetFocus() == GetItem(IDB_STOP))
  {
    // SendMsg_NextDlgCtl_Prev();
    SendMsg_NextDlgCtl_CtlId(IDB_RESTART);
  }
  EnableItem(IDB_STOP, false);
}


void CBenchmarkDialog::OnStopButton()
{
  if (ExitWasAsked_in_GUI)
    return;

  Disable_Stop_Button();

  WasStopped_in_GUI = true;
  if (_thread.IsCreated())
  {
    SendExit_Status(L"Stop ...");
  }
}



void CBenchmarkDialog::OnCancel()
{
  ExitWasAsked_in_GUI = true;
  
  /*
  SendMsg_NextDlgCtl_Prev();
  EnableItem(IDCANCEL, false);
  */

  if (_thread.IsCreated())
    SendExit_Status(L"Cancel ...");
  else
    CModalDialog::OnCancel();
}


void CBenchmarkDialog::OnHelp()
{
  ShowHelpWindow(kHelpTopic);
}



// void GetTimeString(UInt64 timeValue, wchar_t *s);

void CBenchmarkDialog::PrintTime()
{
  const UInt32 curTime =
    _finishTime_WasSet ?
      _finishTime :
      ::GetTickCount();

  const UInt32 elapsedTime = (curTime - _startTime);

  WCHAR s[64];

  // GetTimeString(elapsedTime / 1000, s);
  ConvertUInt32ToString(elapsedTime / 1000, s);

  if (_finishTime_WasSet)
  {
    WCHAR *p = s + MyStringLen(s);
    *p++ = '.';
    UINT_TO_STR_3(p, elapsedTime % 1000);
  }

  // NumberToDot3((UInt64)elapsedTime, s);

  wcscat(s, L" s");

  // if (WasStopped_in_GUI) wcscat(s, L" X"); // for debug

  if (s == ElapsedSec_Prev)
    return;

  ElapsedSec_Prev = s;

  // static cnt = 0; cnt++; wcscat(s, L" ");
  // UString s2; s2.Add_UInt32(cnt); wcscat(s, s2.Ptr());

  SetItemText(IDT_BENCH_ELAPSED_VAL, s);
}


static UInt64 GetMips(UInt64 ips)
{
  return (ips + 500000) / 1000000;
}


static UInt64 GetUsagePercents(UInt64 usage)
{
  return Benchmark_GetUsage_Percents(usage);
}


static UInt32 GetRating(const CTotalBenchRes &info)
{
  UInt64 numIter = info.NumIterations2;
  if (numIter == 0)
    numIter = 1000000;
  const UInt64 rating64 = GetMips(info.Rating / numIter);
  // return rating64;
  UInt32 rating32 = (UInt32)rating64;
  if (rating32 != rating64)
    rating32 = (UInt32)(Int32)-1;
  return rating32;
};


static void AddUsageString(UString &s, const CTotalBenchRes &info)
{
  UInt64 numIter = info.NumIterations2;
  if (numIter == 0)
    numIter = 1000000;
  UInt64 usage = GetUsagePercents(info.Usage / numIter);

  wchar_t w[64];
  ConvertUInt64ToString(usage, w);
  unsigned len = MyStringLen(w);
  while (len < 5)
  {
    s.Add_Space();
    len++;
  }
  s += w;
  s += "%";
}


static void Add_Dot3String(UString &s, UInt64 val)
{
  WCHAR temp[32];
  NumberToDot3(val, temp);
  s += temp;
}


static void AddRatingString(UString &s, const CTotalBenchRes &info)
{
  // AddUsageString(s, info);
  // s += " ";
  // s.Add_UInt32(GetRating(info));
  Add_Dot3String(s, GetRating(info));
};


static void AddRatingsLine(UString &s, const CTotalBenchRes &enc, const CTotalBenchRes &dec
    #ifdef PRINT_ITER_TIME
    , DWORD ticks
    #endif
    )
{
  // AddUsageString(s, enc); s += " ";

  AddRatingString(s, enc);
  s += "  ";
  AddRatingString(s, dec);
  
  CTotalBenchRes tot_BenchRes;
  tot_BenchRes.SetSum(enc, dec);
  
  s += "  ";
  AddRatingString(s, tot_BenchRes);
  
  s += " "; AddUsageString(s, tot_BenchRes);

  
  #ifdef PRINT_ITER_TIME
  s += " ";
  {
    Add_Dot3String(s, ticks;
    s += " s";
    // s.Add_UInt32(ticks); s += " ms";
  }
  #endif
}


void CBenchmarkDialog::PrintRating(UInt64 rating, UINT controlID)
{
  // SetItemText_Number(controlID, GetMips(rating), kMIPS);
  WCHAR s[64];
  NumberToDot3(GetMips(rating), s);
  MyStringCat(s, L" GIPS");
  SetItemText(controlID, s);
}

void CBenchmarkDialog::PrintUsage(UInt64 usage, UINT controlID)
{
  SetItemText_Number(controlID, GetUsagePercents(usage), TEXT("%"));
}


// void SetItemText_Number

void CBenchmarkDialog::PrintBenchRes(
    const CTotalBenchRes2 &info,
    const UINT ids[])
{
  if (info.NumIterations2 == 0)
    return;
  if (ids[1] != 0)
    SetItemText_Number(ids[1], (info.Speed >> 10) / info.NumIterations2, kKBs);
  PrintRating(info.Rating / info.NumIterations2, ids[3]);
  PrintRating(info.RPU / info.NumIterations2, ids[2]);
  PrintUsage(info.Usage / info.NumIterations2, ids[0]);
  if (ids[4] != 0)
  {
    UInt64 val = info.UnpackSize;
    LPCTSTR kPostfix;
    if (val >= ((UInt64)1 << 40))
    {
      kPostfix = kGB;
      val >>= 30;
    }
    else
    {
      kPostfix = kMB;
      val >>= 20;
    }
    SetItemText_Number(ids[4], val, kPostfix);
  }
}


// static UInt32 k_Message_Finished_cnt = 0;
// static UInt32 k_OnTimer_cnt = 0;

bool CBenchmarkDialog::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
  if (message != k_Message_Finished)
    return CModalDialog::OnMessage(message, wParam, lParam);

  {
    if (wParam == k_Msg_WPARM_Thread_Finished)
    {
      _finishTime = GetTickCount();
      _finishTime_WasSet = true;
      MyKillTimer();

      if (_thread.Wait_Close() != 0)
      {
        MessageBoxError_Status(L"Thread Wait Error");
      }

      if (!WasStopped_in_GUI)
      {
        WasStopped_in_GUI = true;
        Disable_Stop_Button();
      }

      HRESULT res = Sync.BenchFinish_Thread_HRESULT;
      if (res != S_OK)
      // if (!ExitWasAsked_in_GUI || res != E_ABORT)
        MessageBoxError_Status(HResultToMessage(res));

      if (ExitWasAsked_in_GUI)
      {
        // SetItemText(IDT_BENCH_ERROR_MESSAGE, "before CModalDialog::OnCancel()");
        // Sleep (2000);
        // MessageBoxError(L"test");
        CModalDialog::OnCancel();
        return true;
      }
    
      SetItemText_Empty(IDT_BENCH_ERROR_MESSAGE);

      res = Sync.BenchFinish_Task_HRESULT;
      if (res != S_OK)
      {
        if (!WasStopped_in_GUI || res != E_ABORT)
        {
          UString m;
          if (res == S_FALSE)
            m = "Decoding error";
          else if (res == CLASS_E_CLASSNOTAVAILABLE)
            m = "Can't find 7z.dll";
          else
            m = HResultToMessage(res);
          MessageBoxError_Status(m);
        }
      }

      if (NeedRestart)
      {
        StartBenchmark();
        return true;
      }
    }
    // k_Message_Finished_cnt++;
    UpdateGui();
    return true;
  }
}


bool CBenchmarkDialog::OnTimer(WPARAM timerID, LPARAM /* callback */)
{
  // k_OnTimer_cnt++;
  if (timerID == kTimerID)
    UpdateGui();
  return true;
}


void CBenchmarkDialog::UpdateGui()
{
  PrintTime();

  if (TotalMode)
  {
    bool wasChanged = false;
    {
      NWindows::NSynchronization::CCriticalSectionLock lock(Sync.CS);
      
      if (Sync.TextWasChanged)
      {
        wasChanged = true;
        Bench2Text += Sync.Text;
        Sync.Text.Empty();
        Sync.TextWasChanged = false;
      }
    }
    if (wasChanged)
      _consoleEdit.SetText(Bench2Text);
    return;
  }

  CSyncData sd;
  CRecordVector<CBenchPassResult> RatingVector;

  {
    NWindows::NSynchronization::CCriticalSectionLock lock(Sync.CS);
    sd = Sync.sd;

    if (sd.NeedPrint_RatingVector)
      RatingVector = Sync.RatingVector;
    
    if (sd.NeedPrint_Freq)
    {
      Sync.FreqString_GUI = Sync.FreqString_Sync;
      sd.NeedPrint_RatingVector = true;
    }

    Sync.sd.NeedPrint_RatingVector = false;
    Sync.sd.NeedPrint_Enc_1 = false;
    Sync.sd.NeedPrint_Enc   = false;
    Sync.sd.NeedPrint_Dec_1 = false;
    Sync.sd.NeedPrint_Dec   = false;
    Sync.sd.NeedPrint_Tot   = false;
    Sync.sd.NeedPrint_Freq = false;
  }

  if (sd.NumPasses_Finished != NumPasses_Finished_Prev)
  {
    SetItemText_Number(IDT_BENCH_PASSES_VAL, sd.NumPasses_Finished, TEXT(" /"));
    NumPasses_Finished_Prev = sd.NumPasses_Finished;
  }

  if (sd.NeedPrint_Enc_1) PrintBenchRes(sd.Enc_BenchRes_1, k_Ids_Enc_1);
  if (sd.NeedPrint_Enc)   PrintBenchRes(sd.Enc_BenchRes,   k_Ids_Enc);
  if (sd.NeedPrint_Dec_1) PrintBenchRes(sd.Dec_BenchRes_1, k_Ids_Dec_1);
  if (sd.NeedPrint_Dec)   PrintBenchRes(sd.Dec_BenchRes,   k_Ids_Dec);
 
  if (sd.BenchWasFinished && sd.NeedPrint_Tot)
  {
    CTotalBenchRes2 tot_BenchRes = sd.Enc_BenchRes;
    tot_BenchRes.Update_With_Res2(sd.Dec_BenchRes);
    PrintBenchRes(tot_BenchRes, k_Ids_Tot);
  }


  if (sd.NeedPrint_RatingVector)
  // for (unsigned k = 0; k < 1; k++)
  {
    UString s;
    s += Sync.FreqString_GUI;
    if (!RatingVector.IsEmpty())
    {
      if (!s.IsEmpty())
        s.Add_LF();
      s += "Compr Decompr Total   CPU"
          #ifdef PRINT_ITER_TIME
          "   Time"
          #endif
          ;
      s.Add_LF();
    }
    // s += "GIPS    GIPS   GIPS    %   s"; s.Add_LF();
    for (unsigned i = 0; i < RatingVector.Size(); i++)
    {
      if (i != 0)
        s.Add_LF();
      if ((int)i == sd.RatingVector_DeletedIndex)
      {
        s += "...";
        s.Add_LF();
      }
      const CBenchPassResult &pair = RatingVector[i];
      /*
        s += "g:"; s.Add_UInt32((UInt32)pair.EncInfo.GlobalTime);
        s += " u:"; s.Add_UInt32((UInt32)pair.EncInfo.UserTime);
        s += " ";
      */
      AddRatingsLine(s, pair.Enc, pair.Dec
            #ifdef PRINT_ITER_TIME
            , pair.Ticks
            #endif
            );
      /*
      {
        UInt64 v = i + 1;
        if (sd.RatingVector_DeletedIndex >= 0 && i >= (unsigned)sd.RatingVector_DeletedIndex)
          v += sd.RatingVector_NumDeleted;
        char temp[64];
        ConvertUInt64ToString(v, temp);
        s += " : ";
        s += temp;
      }
      */
    }

    if (sd.BenchWasFinished)
    {
      s.Add_LF();
      s += "-------------";
      s.Add_LF();
      {
        // average time is not correct because of freq detection in first iteration
        AddRatingsLine(s, sd.Enc_BenchRes, sd.Dec_BenchRes
              #ifdef PRINT_ITER_TIME
              , (DWORD)(sd.TotalTicks / (sd.NumPasses_Finished ? sd.NumPasses_Finished : 1))
              #endif
              );
      }
    }
    // s.Add_LF(); s += "OnTimer: "; s.Add_UInt32(k_OnTimer_cnt);
    // s.Add_LF(); s += "finished Message: "; s.Add_UInt32(k_Message_Finished_cnt);
    // static cnt = 0; cnt++; s.Add_LF(); s += "Print: "; s.Add_UInt32(cnt);
    // s.Add_LF(); s += "NumEncProgress: "; s.Add_UInt32((UInt32)sd.NumEncProgress);
    // s.Add_LF(); s += "NumDecProgress: "; s.Add_UInt32((UInt32)sd.NumDecProgress);
    SetItemText(IDT_BENCH_LOG, s);
  }
}


bool CBenchmarkDialog::OnCommand(int code, int itemID, LPARAM lParam)
{
  if (code == CBN_SELCHANGE &&
      (itemID == IDC_BENCH_DICTIONARY ||
       itemID == IDC_BENCH_NUM_PASSES ||
       itemID == IDC_BENCH_NUM_THREADS))
  {
    RestartBenchmark();
    return true;
  }
  return CModalDialog::OnCommand(code, itemID, lParam);
}


bool CBenchmarkDialog::OnButtonClicked(int buttonID, HWND buttonHWND)
{
  switch (buttonID)
  {
    case IDB_RESTART:
      RestartBenchmark();
      return true;
    case IDB_STOP:
      OnStopButton();
      return true;
  }
  return CModalDialog::OnButtonClicked(buttonID, buttonHWND);
}





// ---------- Benchmark Thread ----------

struct CBenchCallback: public IBenchCallback
{
  UInt64 dictionarySize;
  CBenchProgressSync *Sync;
  CBenchmarkDialog *BenchmarkDialog;
  
  HRESULT SetEncodeResult(const CBenchInfo &info, bool final);
  HRESULT SetDecodeResult(const CBenchInfo &info, bool final);
};

HRESULT CBenchCallback::SetEncodeResult(const CBenchInfo &info, bool final)
{
  bool needPost = false;
  {
    NSynchronization::CCriticalSectionLock lock(Sync->CS);
    if (Sync->Exit)
      return E_ABORT;
    CSyncData &sd = Sync->sd;
    // sd.NumEncProgress++;
    CTotalBenchRes2 &br = sd.Enc_BenchRes_1;
    {
      UInt64 dictSize = Sync->DictSize;
      if (final)
      {
        // sd.EncInfo = info;
      }
      else
      {
        /* if (!final), then CBenchInfo::NumIterations means totalNumber of threads.
           so we can reduce the dictionary */
        if (dictSize > info.UnpackSize)
          dictSize = info.UnpackSize;
      }
      br.Rating = info.GetRating_LzmaEnc(dictSize);
    }
    br.SetFrom_BenchInfo(info);
    sd.NeedPrint_Enc_1 = true;
    if (final)
    {
      sd.Enc_BenchRes.Update_With_Res2(br);
      sd.NeedPrint_Enc = true;
      needPost = true;
    }
  }

  if (needPost)
    BenchmarkDialog->PostMsg(k_Message_Finished, k_Msg_WPARM_Enc1_Finished);

  return S_OK;
}


HRESULT CBenchCallback::SetDecodeResult(const CBenchInfo &info, bool final)
{
  NSynchronization::CCriticalSectionLock lock(Sync->CS);
  if (Sync->Exit)
    return E_ABORT;
  CSyncData &sd = Sync->sd;
  // sd.NumDecProgress++;
  CTotalBenchRes2 &br = sd.Dec_BenchRes_1;
  br.Rating = info.GetRating_LzmaDec();
  br.SetFrom_BenchInfo(info);
  sd.NeedPrint_Dec_1 = true;
  if (final)
    sd.Dec_BenchRes.Update_With_Res2(br);
  return S_OK;
}


struct CBenchCallback2: public IBenchPrintCallback
{
  CBenchProgressSync *Sync;
  bool TotalMode;

  void Print(const char *s);
  void NewLine();
  HRESULT CheckBreak();
};

void CBenchCallback2::Print(const char *s)
{
  if (TotalMode)
  {
    NSynchronization::CCriticalSectionLock lock(Sync->CS);
    Sync->Text += s;
    Sync->TextWasChanged = true;
  }
}

void CBenchCallback2::NewLine()
{
  Print("\xD\n");
}

HRESULT CBenchCallback2::CheckBreak()
{
  if (Sync->Exit)
    return E_ABORT;
  return S_OK;
}



struct CFreqCallback: public IBenchFreqCallback
{
  CBenchmarkDialog *BenchmarkDialog;

  virtual HRESULT AddCpuFreq(unsigned numThreads, UInt64 freq, UInt64 usage);
  virtual HRESULT FreqsFinished(unsigned numThreads);
};

HRESULT CFreqCallback::AddCpuFreq(unsigned numThreads, UInt64 freq, UInt64 usage)
{
  HRESULT res;
  {
    CBenchProgressSync &sync = BenchmarkDialog->Sync;
    NSynchronization::CCriticalSectionLock lock(sync.CS);
    UString &s = sync.FreqString_Sync;
    if (sync.NumFreqThreadsPrev != numThreads)
    {
      sync.NumFreqThreadsPrev = numThreads;
      if (!s.IsEmpty())
        s.Add_LF();
      s.Add_UInt32(numThreads);
      s += "T Frequency (MHz):";
      s.Add_LF();
    }
    s += " ";
    char temp[64];
    if (numThreads != 1)
    {
      ConvertUInt64ToString(GetUsagePercents(usage), temp);
      s += temp;
      s += '%';
      s.Add_Space();
    }
    ConvertUInt64ToString(GetMips(freq), temp);
    s += temp;
    // BenchmarkDialog->Sync.sd.NeedPrint_Freq = true;
    res = sync.Exit ? E_ABORT : S_OK;
  }
  // BenchmarkDialog->PostMsg(k_Message_Finished, k_Msg_WPARM_Enc1_Finished);
  return res;
}

HRESULT CFreqCallback::FreqsFinished(unsigned /* numThreads */)
{
  HRESULT res;
  {
    CBenchProgressSync &sync = BenchmarkDialog->Sync;
    NSynchronization::CCriticalSectionLock lock(sync.CS);
    sync.sd.NeedPrint_Freq = true;
    BenchmarkDialog->PostMsg(k_Message_Finished, k_Msg_WPARM_Enc1_Finished);
    res = sync.Exit ? E_ABORT : S_OK;
  }
  BenchmarkDialog->PostMsg(k_Message_Finished, k_Msg_WPARM_Enc1_Finished);
  return res;
}



// define USE_DUMMY only for debug
// #define USE_DUMMY
#ifdef USE_DUMMY
static unsigned dummy = 1;
static unsigned Dummy(unsigned limit)
{
  unsigned sum = 0;
  for (unsigned k = 0; k < limit; k++)
  {
    sum += dummy;
    if (sum == 0)
      break;
  }
  return sum;
}
#endif


HRESULT CThreadBenchmark::Process()
{
  /* the first benchmark pass can be slow,
     if we run benchmark while the window is being created,
     and (no freq detecion loop) && (dictionary is small) (-mtic is small) */
    
  // Sleep(300); // for debug
  #ifdef USE_DUMMY
  Dummy(1000 * 1000 * 1000); // for debug
  #endif

  CBenchProgressSync &sync = BenchmarkDialog->Sync;
  HRESULT finishHRESULT = S_OK;
  
  try
  {
    for (UInt32 passIndex = 0;; passIndex++)
    {
      // throw 1; // to debug
      // throw CSystemException(E_INVALIDARG); // to debug

      UInt64 dictionarySize;
      UInt32 numThreads;
      {
        NSynchronization::CCriticalSectionLock lock(sync.CS);
        if (sync.Exit)
          break;
        dictionarySize = sync.DictSize;
        numThreads = sync.NumThreads;
      }

      #ifdef PRINT_ITER_TIME
      const DWORD startTick = GetTickCount();
      #endif
      
      CBenchCallback callback;
      
      callback.dictionarySize = dictionarySize;
      callback.Sync = &sync;
      callback.BenchmarkDialog = BenchmarkDialog;
      
      CBenchCallback2 callback2;
      callback2.TotalMode = BenchmarkDialog->TotalMode;
      callback2.Sync = &sync;
      
      CFreqCallback freqCallback;
      freqCallback.BenchmarkDialog = BenchmarkDialog;

      HRESULT result;
     
      try
      {
        CObjectVector<CProperty> props;

        props = BenchmarkDialog->Props;

        if (BenchmarkDialog->TotalMode)
        {
          props = BenchmarkDialog->Props;
        }
        else
        {
          {
            CProperty prop;
            prop.Name = "mt";
            prop.Value.Add_UInt32(numThreads);
            props.Add(prop);
          }
          {
            CProperty prop;
            prop.Name = 'd';
            prop.Name.Add_UInt32((UInt32)(dictionarySize >> 10));
            prop.Name += 'k';
            props.Add(prop);
          }
        }
        
        result = Bench(EXTERNAL_CODECS_LOC_VARS
            BenchmarkDialog->TotalMode ? &callback2 : NULL,
            BenchmarkDialog->TotalMode ? NULL : &callback,
            props, 1, false,
            (!BenchmarkDialog->TotalMode) && passIndex == 0 ? &freqCallback: NULL);
        
        // result = S_FALSE; // for debug;
        // throw 1;
      }
      catch(...)
      {
        result = E_FAIL;
      }

      #ifdef PRINT_ITER_TIME
      const DWORD numTicks = GetTickCount() - startTick;
      #endif

      bool finished = true;

      NSynchronization::CCriticalSectionLock lock(sync.CS);

      if (result != S_OK)
      {
        sync.BenchFinish_Task_HRESULT = result;
        break;
      }

      {
        CSyncData &sd = sync.sd;

        sd.NumPasses_Finished++;
        #ifdef PRINT_ITER_TIME
        sd.TotalTicks += numTicks;
        #endif

        if (BenchmarkDialog->TotalMode)
          break;

        {
          CTotalBenchRes tot_BenchRes = sd.Enc_BenchRes_1;
          tot_BenchRes.Update_With_Res(sd.Dec_BenchRes_1);

          sd.NeedPrint_RatingVector = true;
          {
            CBenchPassResult pair;
            // pair.EncInfo = sd.EncInfo; // for debug
            pair.Enc = sd.Enc_BenchRes_1;
            pair.Dec = sd.Dec_BenchRes_1;
            #ifdef PRINT_ITER_TIME
            pair.Ticks = numTicks;
            #endif
            sync.RatingVector.Add(pair);
            // pair.Dec_Defined = true;
          }
        }
          
        sd.NeedPrint_Dec = true;
        sd.NeedPrint_Tot = true;

        if (sync.RatingVector.Size() > kRatingVector_NumBundlesMax)
        {
          // sd.RatingVector_NumDeleted++;
          sd.RatingVector_DeletedIndex = (int)(kRatingVector_NumBundlesMax / 4);
          sync.RatingVector.Delete((unsigned)(sd.RatingVector_DeletedIndex));
        }

        if (sync.sd.NumPasses_Finished < sync.NumPasses_Limit)
          finished = false;
        else
        {
          sync.sd.BenchWasFinished = true;
          // BenchmarkDialog->_finishTime = GetTickCount();
          // return 0;
        }
      }

      if (BenchmarkDialog->TotalMode)
        break;

      /*
      if (newTick - prevTick < 1000)
        numSameTick++;
      if (numSameTick > 5 || finished)
      {
        prevTick = newTick;
        numSameTick = 0;
      */
      // for (unsigned i = 0; i < 1; i++)
      {
        // we suppose that PostMsg messages will be processed in order.
        if (!BenchmarkDialog->PostMsg_Finish(k_Msg_WPARM_Iter_Finished))
        {
          finished = true;
          finishHRESULT = E_FAIL;
          // throw 1234567;
        }
      }
      if (finished)
        break;
    }
    // return S_OK;
  }
  catch(CSystemException &e)
  {
    finishHRESULT = e.ErrorCode;
    // BenchmarkDialog->MessageBoxError(HResultToMessage(e.ErrorCode));
    // return E_FAIL;
  }
  catch(...)
  {
    finishHRESULT = E_FAIL;
    // BenchmarkDialog->MessageBoxError(HResultToMessage(E_FAIL));
    // return E_FAIL;
  }

  if (finishHRESULT != S_OK)
  {
    NSynchronization::CCriticalSectionLock lock(sync.CS);
    sync.BenchFinish_Thread_HRESULT = finishHRESULT;
  }
  if (!BenchmarkDialog->PostMsg_Finish(k_Msg_WPARM_Thread_Finished))
  {
    // sync.BenchFinish_Thread_HRESULT = E_FAIL;
  }
  return 0;
}



static void ParseNumberString(const UString &s, NCOM::CPropVariant &prop)
{
  const wchar_t *end;
  UInt64 result = ConvertStringToUInt64(s, &end);
  if (*end != 0 || s.IsEmpty())
    prop = s;
  else if (result <= (UInt32)0xFFFFFFFF)
    prop = (UInt32)result;
  else
    prop = result;
}


HRESULT Benchmark(
    DECL_EXTERNAL_CODECS_LOC_VARS
    const CObjectVector<CProperty> &props, UInt32 numIterations, HWND hwndParent)
{
  CBenchmarkDialog bd;

  bd.TotalMode = false;
  bd.Props = props;
  if (numIterations == 0)
    numIterations = 1;
  bd.Sync.NumPasses_Limit = numIterations;
  bd.Sync.DictSize = (UInt64)(Int64)-1;
  bd.Sync.NumThreads = (UInt32)(Int32)-1;
  bd.Sync.Level = -1;

  COneMethodInfo method;

  UInt32 numCPUs = 1;
  #ifndef _7ZIP_ST
  numCPUs = NSystem::GetNumberOfProcessors();
  #endif
  UInt32 numThreads = numCPUs;

  FOR_VECTOR (i, props)
  {
    const CProperty &prop = props[i];
    UString name = prop.Name;
    name.MakeLower_Ascii();
    if (name.IsEqualTo_Ascii_NoCase("m") && prop.Value == L"*")
    {
      bd.TotalMode = true;
      continue;
    }

    NCOM::CPropVariant propVariant;
    if (!prop.Value.IsEmpty())
      ParseNumberString(prop.Value, propVariant);
    if (name.IsPrefixedBy(L"mt"))
    {
      #ifndef _7ZIP_ST
      RINOK(ParseMtProp(name.Ptr(2), propVariant, numCPUs, numThreads));
      if (numThreads != numCPUs)
        bd.Sync.NumThreads = numThreads;
      #endif
      continue;
    }
    /*
    if (name.IsEqualTo("time"))
    {
      // UInt32 testTime = 4;
      // RINOK(ParsePropToUInt32(L"", propVariant, testTime));
      continue;
    }
    RINOK(method.ParseMethodFromPROPVARIANT(name, propVariant));
    */
    // here we need to parse DictSize property, and ignore unknown properties
    method.ParseMethodFromPROPVARIANT(name, propVariant);
  }

  if (bd.TotalMode)
  {
    // bd.Bench2Text.Empty();
    bd.Bench2Text = "7-Zip " MY_VERSION_CPU;
    bd.Bench2Text += (char)0xD;
    bd.Bench2Text.Add_LF();
  }

  {
    UInt64 dict;
    if (method.Get_DicSize(dict))
      bd.Sync.DictSize = dict;
  }
  bd.Sync.Level = method.GetLevel();

  // Dummy(1000 * 1000 * 1);

  {
    CThreadBenchmark &benchmarker = bd._threadBenchmark;
    #ifdef EXTERNAL_CODECS
    benchmarker.__externalCodecs = __externalCodecs;
    #endif
    benchmarker.BenchmarkDialog = &bd;
  }

  bd.Create(hwndParent);

  return S_OK;
}


CBenchmarkDialog::~CBenchmarkDialog()
{
  if (_thread.IsCreated())
  {
    /* the following code will be not executed in normal code flow.
       it can be called, if there is some internal failure in dialog code. */
    Attach(NULL);
    MessageBoxError(L"The flaw in benchmark thread code");
    Sync.SendExit();
    _thread.Wait_Close();
  }
}
