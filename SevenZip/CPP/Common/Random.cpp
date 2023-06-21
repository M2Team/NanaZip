// Common/Random.cpp

#include "../../../ThirdParty/LZMA/CPP/Common/StdAfx.h"

#include <stdlib.h>

#ifndef _WIN32
#include <time.h>
#else
#include "../../../ThirdParty/LZMA/CPP/Common/MyWindows.h"
#endif

#include "Random.h"

void CRandom::Init(unsigned int seed) { srand(seed); }

void CRandom::Init()
{
  Init((unsigned int)
    #ifdef _WIN32
    GetTickCount()
    #else
    time(NULL)
    #endif
    );
}

int CRandom::Generate() const { return rand(); }
