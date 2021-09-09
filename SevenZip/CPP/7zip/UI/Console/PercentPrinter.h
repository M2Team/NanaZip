// PercentPrinter.h

#ifndef __PERCENT_PRINTER_H
#define __PERCENT_PRINTER_H

#include "../../../Common/StdOutStream.h"

struct CPercentPrinterState
{
  UInt64 Completed;
  UInt64 Total;
  
  UInt64 Files;

  AString Command;
  UString FileName;

  void ClearCurState();

  CPercentPrinterState():
      Completed(0),
      Total((UInt64)(Int64)-1),
      Files(0)
    {}
};

class CPercentPrinter: public CPercentPrinterState
{
  UInt32 _tickStep;
  DWORD _prevTick;

  AString _s;

  AString _printedString;
  AString _temp;
  UString _tempU;

  CPercentPrinterState _printedState;
  AString _printedPercents;

  void GetPercents();

public:
  CStdOutStream *_so;

  bool NeedFlush;
  unsigned MaxLen;
  
  CPercentPrinter(UInt32 tickStep = 200):
      _tickStep(tickStep),
      _prevTick(0),
      NeedFlush(true),
      MaxLen(80 - 1)
  {}

  ~CPercentPrinter();

  void ClosePrint(bool needFlush);
  void Print();
};

#endif
