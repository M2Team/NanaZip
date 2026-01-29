// BenchmarkDialog.h

#ifndef ZIP7_INC_BENCHMARK_DIALOG_H
#define ZIP7_INC_BENCHMARK_DIALOG_H

#include "../../Common/CreateCoder.h"
#include "../../UI/Common/Property.h"

const UInt32 k_NumBenchIterations_Default = 10;

HRESULT Benchmark(
    DECL_EXTERNAL_CODECS_LOC_VARS
    const CObjectVector<CProperty> &props, UInt32 numIterations, HWND hwndParent = NULL);

#endif
