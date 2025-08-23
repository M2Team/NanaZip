﻿// NewHandler.cpp

#include "StdAfx.h"

#include <stdlib.h>

#include "NewHandler.h"

// #define DEBUG_MEMORY_LEAK

#ifndef DEBUG_MEMORY_LEAK

#ifdef _7ZIP_REDEFINE_OPERATOR_NEW

/*
void * my_new(size_t size)
{
  // void *p = ::HeapAlloc(::GetProcessHeap(), 0, size);
  void *p = ::malloc(size);
  if (p == 0)
    throw CNewException();
  return p;
}

void my_delete(void *p) throw()
{
  // if (p == 0) return; ::HeapFree(::GetProcessHeap(), 0, p);
  ::free(p);
}

void * my_Realloc(void *p, size_t newSize, size_t oldSize)
{
  void *newBuf = my_new(newSize);
  if (oldSize != 0)
    memcpy(newBuf, p, oldSize);
  my_delete(p);
  return newBuf;
}
*/

void *
#ifdef _MSC_VER
__cdecl
#endif
operator new(size_t size)
{
  // void *p = ::HeapAlloc(::GetProcessHeap(), 0, size);
  void *p = ::malloc(size);
  if (p == 0)
    throw CNewException();
  return p;
}

void
#ifdef _MSC_VER
__cdecl
#endif
operator delete(void *p) throw()
{
  // if (p == 0) return; ::HeapFree(::GetProcessHeap(), 0, p);
  ::free(p);
}

/*
void *
#ifdef _MSC_VER
__cdecl
#endif
operator new[](size_t size)
{
  // void *p = ::HeapAlloc(::GetProcessHeap(), 0, size);
  void *p = ::malloc(size);
  if (p == 0)
    throw CNewException();
  return p;
}

void
#ifdef _MSC_VER
__cdecl
#endif
operator delete[](void *p) throw()
{
  // if (p == 0) return; ::HeapFree(::GetProcessHeap(), 0, p);
  ::free(p);
}
*/

#endif

#else

#include <stdio.h>

// #pragma init_seg(lib)
const int kDebugSize = 1000000;
static void *a[kDebugSize];
static int index = 0;

static bool wasInit = false;
static CRITICAL_SECTION cs;

static int numAllocs = 0;
void * __cdecl operator new(size_t size)
{
  if (!wasInit)
  {
    InitializeCriticalSection(&cs);
    wasInit = true;
  }
  EnterCriticalSection(&cs);

  numAllocs++;
  int loc = numAllocs;
  void *p = HeapAlloc(GetProcessHeap(), 0, size);
  /*
  if (index < kDebugSize)
  {
    a[index] = p;
    index++;
  }
  */
  printf("Alloc %6d, size = %8u\n", loc, (unsigned)size);
  LeaveCriticalSection(&cs);
  if (p == 0)
    throw CNewException();
  return p;
}

class CC
{
public:
  CC()
  {
    for (int i = 0; i < kDebugSize; i++)
      a[i] = 0;
  }
  ~CC()
  {
    printf("\nDestructor: %d\n", numAllocs);
    for (int i = 0; i < kDebugSize; i++)
      if (a[i] != 0)
        return;
  }
} g_CC;


void __cdecl operator delete(void *p)
{
  if (p == 0)
    return;
  EnterCriticalSection(&cs);
  /*
  for (int i = 0; i < index; i++)
    if (a[i] == p)
      a[i] = 0;
  */
  HeapFree(GetProcessHeap(), 0, p);
  numAllocs--;
  printf("Free %d\n", numAllocs);
  LeaveCriticalSection(&cs);
}

#endif

/*
int MemErrorVC(size_t)
{
  throw CNewException();
  // return 1;
}
CNewHandlerSetter::CNewHandlerSetter()
{
  // MemErrorOldVCFunction = _set_new_handler(MemErrorVC);
}
CNewHandlerSetter::~CNewHandlerSetter()
{
  // _set_new_handler(MemErrorOldVCFunction);
}
*/
