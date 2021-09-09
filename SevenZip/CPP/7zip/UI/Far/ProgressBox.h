// ProgressBox.h

#ifndef __PROGRESS_BOX_H
#define __PROGRESS_BOX_H

#include "../../../Common/MyString.h"
#include "../../../Common/MyTypes.h"

struct CPercentPrinterState
{
  UInt64 Completed;
  UInt64 Total;
  
  UInt64 Files;
  UInt64 FilesTotal;

  AString Command;
  UString FileName;

  void ClearCurState();

  bool IsEqualTo(const CPercentPrinterState &s) const
  {
    return
           Completed == s.Completed
        && Total == s.Total
        && Files == s.Files
        && FilesTotal == s.FilesTotal
        && Command == s.Command
        && FileName == s.FileName;
  }

  CPercentPrinterState():
      Completed(0),
      Total((UInt64)(Int64)-1),
      Files(0),
      FilesTotal(0)
    {}
};

class CProgressBox: public CPercentPrinterState
{
  UInt32 _tickStep;
  DWORD _prevTick;
  DWORD _prevElapsedSec;

  bool _wasPrinted;

  UString _tempU;
  UString _name1U;
  UString _name2U;

  CPercentPrinterState _printedState;

  AString _title;
  
  AString _timeStr;
  AString _files;
  AString _sizesStr;
  AString _name1;
  AString _name2;
  AString _perc;

  void ReduceString(const UString &src, AString &dest);

public:
  DWORD StartTick;
  bool UseBytesForPercents;
  unsigned MaxLen;

  CProgressBox(UInt32 tickStep = 200):
      _tickStep(tickStep),
      _prevTick(0),
      StartTick(0),
      UseBytesForPercents(true),
      MaxLen(60)
    {}

  void Init(const char *title);
  void Print();
};

#endif
