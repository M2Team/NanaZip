// BenchCon.cpp

#include "StdAfx.h"

#include "../Common/Bench.h"

#include "BenchCon.h"
#include "ConsoleClose.h"

struct CPrintBenchCallback: public IBenchPrintCallback
{
  FILE *_file;

  void Print(const char *s);
  void NewLine();
  HRESULT CheckBreak();
};

void CPrintBenchCallback::Print(const char *s)
{
  fputs(s, _file);
}

void CPrintBenchCallback::NewLine()
{
  fputc('\n', _file);
}

HRESULT CPrintBenchCallback::CheckBreak()
{
  return NConsoleClose::TestBreakSignal() ? E_ABORT: S_OK;
}

HRESULT BenchCon(DECL_EXTERNAL_CODECS_LOC_VARS
    const CObjectVector<CProperty> &props, UInt32 numIterations, FILE *f)
{
  CPrintBenchCallback callback;
  callback._file = f;
  return Bench(EXTERNAL_CODECS_LOC_VARS
      &callback, NULL, props, numIterations, true);
}
