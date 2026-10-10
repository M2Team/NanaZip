// Common/Random.cpp

#include "StdAfx.h"

#include <stdlib.h>

#ifndef _WIN32
#include <time.h>
#else
#include "MyWindows.h"
#endif

#include "Random.h"

void CRandom::Init(unsigned seed) { srand(seed); }

void CRandom::Init()
{
  Init((unsigned)
    #ifdef _WIN32
    GetTickCount()
    #else
    time(NULL)
    #endif
    );
}

int CRandom::Generate() const { return rand(); }
