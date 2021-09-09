// HashCon.cpp

#include "StdAfx.h"

#include "../../../Common/IntToString.h"

#include "ConsoleClose.h"
#include "HashCon.h"

static const char * const kEmptyFileAlias = "[Content]";

static const char * const kScanningMessage = "Scanning";

static HRESULT CheckBreak2()
{
  return NConsoleClose::TestBreakSignal() ? E_ABORT : S_OK;
}

HRESULT CHashCallbackConsole::CheckBreak()
{
  return CheckBreak2();
}

HRESULT CHashCallbackConsole::StartScanning()
{
  if (PrintHeaders && _so)
    *_so << kScanningMessage << endl;
  if (NeedPercents())
  {
    _percent.ClearCurState();
    _percent.Command = "Scan";
  }
  return CheckBreak2();
}

HRESULT CHashCallbackConsole::ScanProgress(const CDirItemsStat &st, const FString &path, bool /* isDir */)
{
  if (NeedPercents())
  {
    _percent.Files = st.NumDirs + st.NumFiles + st.NumAltStreams;
    _percent.Completed = st.GetTotalBytes();
    _percent.FileName = fs2us(path);
    _percent.Print();
  }
  return CheckBreak2();
}

HRESULT CHashCallbackConsole::ScanError(const FString &path, DWORD systemError)
{
  return ScanError_Base(path, systemError);
}

void Print_DirItemsStat(AString &s, const CDirItemsStat &st);

HRESULT CHashCallbackConsole::FinishScanning(const CDirItemsStat &st)
{
  if (NeedPercents())
  {
    _percent.ClosePrint(true);
    _percent.ClearCurState();
  }
  if (PrintHeaders && _so)
  {
    Print_DirItemsStat(_s, st);
    *_so << _s << endl << endl;
  }
  return CheckBreak2();
}

HRESULT CHashCallbackConsole::SetNumFiles(UInt64 /* numFiles */)
{
  return CheckBreak2();
}

HRESULT CHashCallbackConsole::SetTotal(UInt64 size)
{
  if (NeedPercents())
  {
    _percent.Total = size;
    _percent.Print();
  }
  return CheckBreak2();
}

HRESULT CHashCallbackConsole::SetCompleted(const UInt64 *completeValue)
{
  if (completeValue && NeedPercents())
  {
    _percent.Completed = *completeValue;
    _percent.Print();
  }
  return CheckBreak2();
}

static void AddMinuses(AString &s, unsigned num)
{
  for (unsigned i = 0; i < num; i++)
    s += '-';
}

static void AddSpaces_if_Positive(AString &s, int num)
{
  for (int i = 0; i < num; i++)
    s.Add_Space();
}

static void SetSpacesAndNul(char *s, unsigned num)
{
  for (unsigned i = 0; i < num; i++)
    s[i] = ' ';
  s[num] = 0;
}

static const unsigned kSizeField_Len = 13;
static const unsigned kNameField_Len = 12;

static const unsigned kHashColumnWidth_Min = 4 * 2;

static unsigned GetColumnWidth(unsigned digestSize)
{
  unsigned width = digestSize * 2;
  return width < kHashColumnWidth_Min ? kHashColumnWidth_Min: width;
}

void CHashCallbackConsole::PrintSeparatorLine(const CObjectVector<CHasherState> &hashers)
{
  _s.Empty();
  
  for (unsigned i = 0; i < hashers.Size(); i++)
  {
    if (i != 0)
      _s.Add_Space();
    const CHasherState &h = hashers[i];
    AddMinuses(_s, GetColumnWidth(h.DigestSize));
  }

  if (PrintSize)
  {
    _s.Add_Space();
    AddMinuses(_s, kSizeField_Len);
  }

  if (PrintName)
  {
    AddSpacesBeforeName();
    AddMinuses(_s, kNameField_Len);
  }
  
  *_so << _s << endl;
}

HRESULT CHashCallbackConsole::BeforeFirstFile(const CHashBundle &hb)
{
  if (PrintHeaders && _so)
  {
    _s.Empty();
    ClosePercents_for_so();
    
    FOR_VECTOR (i, hb.Hashers)
    {
      if (i != 0)
        _s.Add_Space();
      const CHasherState &h = hb.Hashers[i];
      _s += h.Name;
      AddSpaces_if_Positive(_s, (int)GetColumnWidth(h.DigestSize) - (int)h.Name.Len());
    }
    
    if (PrintSize)
    {
      _s.Add_Space();
      const AString s2 ("Size");
      AddSpaces_if_Positive(_s, (int)kSizeField_Len - (int)s2.Len());
      _s += s2;
    }
    
    if (PrintName)
    {
      AddSpacesBeforeName();
      _s += "Name";
    }
    
    *_so << _s << endl;
    PrintSeparatorLine(hb.Hashers);
  }
  
  return CheckBreak2();
}

HRESULT CHashCallbackConsole::OpenFileError(const FString &path, DWORD systemError)
{
  return OpenFileError_Base(path, systemError);
}

HRESULT CHashCallbackConsole::GetStream(const wchar_t *name, bool /* isFolder */)
{
  _fileName = name;

  if (NeedPercents())
  {
    if (PrintNameInPercents)
    {
      _percent.FileName.Empty();
      if (name)
        _percent.FileName = name;
    }
   _percent.Print();
  }
  return CheckBreak2();
}

void CHashCallbackConsole::PrintResultLine(UInt64 fileSize,
    const CObjectVector<CHasherState> &hashers, unsigned digestIndex, bool showHash)
{
  ClosePercents_for_so();

  _s.Empty();

  FOR_VECTOR (i, hashers)
  {
    const CHasherState &h = hashers[i];
    char s[k_HashCalc_DigestSize_Max * 2 + 64];
    s[0] = 0;
    if (showHash)
      AddHashHexToString(s, h.Digests[digestIndex], h.DigestSize);
    const unsigned pos = (unsigned)strlen(s);
    SetSpacesAndNul(s + pos, GetColumnWidth(h.DigestSize) - pos);
    if (i != 0)
      _s.Add_Space();
    _s += s;
  }
  
  if (PrintSize)
  {
    _s.Add_Space();

    char s[kSizeField_Len + 32];
    char *p = s;
    
    SetSpacesAndNul(s, kSizeField_Len);
    if (showHash)
    {
      p = s + kSizeField_Len;
      ConvertUInt64ToString(fileSize, p);
      int numSpaces = (int)kSizeField_Len - (int)strlen(p);
      if (numSpaces > 0)
        p -= (unsigned)numSpaces;
    }
    
    _s += p;
  }

  if (PrintName)
    AddSpacesBeforeName();
  
  *_so << _s;
}

HRESULT CHashCallbackConsole::SetOperationResult(UInt64 fileSize, const CHashBundle &hb, bool showHash)
{
  if (_so)
  {
    PrintResultLine(fileSize, hb.Hashers, k_HashCalc_Index_Current, showHash);
    if (PrintName)
    {
      if (_fileName.IsEmpty())
        *_so << kEmptyFileAlias;
      else
        _so->NormalizePrint_UString(_fileName);
    }
    *_so << endl;
  }
  
  if (NeedPercents())
  {
    _percent.Files++;
    _percent.Print();
  }

  return CheckBreak2();
}

static const char * const k_DigestTitles[] =
{
    " : "
  , " for data:              "
  , " for data and names:    "
  , " for streams and names: "
};

static void PrintSum(CStdOutStream &so, const CHasherState &h, unsigned digestIndex)
{
  so << h.Name;
  
  {
    AString temp;
    AddSpaces_if_Positive(temp, 6 - (int)h.Name.Len());
    so << temp;
  }

  so << k_DigestTitles[digestIndex];

  char s[k_HashCalc_DigestSize_Max * 2 + 64];
  s[0] = 0;
  AddHashHexToString(s, h.Digests[digestIndex], h.DigestSize);
  so << s << endl;
}

void PrintHashStat(CStdOutStream &so, const CHashBundle &hb)
{
  FOR_VECTOR (i, hb.Hashers)
  {
    const CHasherState &h = hb.Hashers[i];
    PrintSum(so, h, k_HashCalc_Index_DataSum);
    if (hb.NumFiles != 1 || hb.NumDirs != 0)
      PrintSum(so, h, k_HashCalc_Index_NamesSum);
    if (hb.NumAltStreams != 0)
      PrintSum(so, h, k_HashCalc_Index_StreamsSum);
    so << endl;
  }
}

void CHashCallbackConsole::PrintProperty(const char *name, UInt64 value)
{
  char s[32];
  s[0] = ':';
  s[1] = ' ';
  ConvertUInt64ToString(value, s + 2);
  *_so << name << s << endl;
}

HRESULT CHashCallbackConsole::AfterLastFile(CHashBundle &hb)
{
  ClosePercents2();
  
  if (PrintHeaders && _so)
  {
    PrintSeparatorLine(hb.Hashers);
    
    PrintResultLine(hb.FilesSize, hb.Hashers, k_HashCalc_Index_DataSum, true);
    
    *_so << endl << endl;
    
    if (hb.NumFiles != 1 || hb.NumDirs != 0)
    {
      if (hb.NumDirs != 0)
        PrintProperty("Folders", hb.NumDirs);
      PrintProperty("Files", hb.NumFiles);
    }
    
    PrintProperty("Size", hb.FilesSize);
    
    if (hb.NumAltStreams != 0)
    {
      PrintProperty("Alternate streams", hb.NumAltStreams);
      PrintProperty("Alternate streams size", hb.AltStreamsSize);
    }
    
    *_so << endl;
    PrintHashStat(*_so, hb);
  }

  return S_OK;
}
