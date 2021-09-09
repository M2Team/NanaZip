// Windows/Synchronization.cpp

#include "StdAfx.h"

#ifndef _WIN32

#include "Synchronization.h"

namespace NWindows {
namespace NSynchronization {

/*
#define INFINITE  0xFFFFFFFF
#define MAXIMUM_WAIT_OBJECTS 64
#define STATUS_ABANDONED_WAIT_0 ((NTSTATUS)0x00000080L)
#define WAIT_ABANDONED   ((STATUS_ABANDONED_WAIT_0 ) + 0 )
#define WAIT_ABANDONED_0 ((STATUS_ABANDONED_WAIT_0 ) + 0 )
// WINAPI
DWORD WaitForMultipleObjects(DWORD count, const HANDLE *handles, BOOL wait_all, DWORD timeout);
*/

DWORD WINAPI WaitForMultiObj_Any_Infinite(DWORD count, const CHandle_WFMO *handles)
{
  if (count < 1)
  {
    // abort();
    SetLastError(EINVAL);
    return WAIT_FAILED;
  }

  CSynchro *synchro = handles[0]->_sync;
  synchro->Enter();
  
  // #ifdef DEBUG_SYNCHRO
  for (DWORD i = 1; i < count; i++)
  {
    if (synchro != handles[i]->_sync)
    {
      // abort();
      synchro->Leave();
      SetLastError(EINVAL);
      return WAIT_FAILED;
    }
  }
  // #endif

  for (;;)
  {
    for (DWORD i = 0; i < count; i++)
    {
      if (handles[i]->IsSignaledAndUpdate())
      {
        synchro->Leave();
        return WAIT_OBJECT_0 + i;
      }
    }
    synchro->WaitCond();
  }
}

}}

#endif
