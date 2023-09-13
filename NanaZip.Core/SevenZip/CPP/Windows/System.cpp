// Windows/System.cpp

#include "StdAfx.h"

#ifndef _WIN32
#include <unistd.h>
#include <limits.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#else
#include <sys/sysinfo.h>
#endif
#endif

#include "../Common/Defs.h"
// #include "../Common/MyWindows.h"

// #include "../../C/CpuArch.h"

#include "System.h"

namespace NWindows {
namespace NSystem {

#ifdef _WIN32

UInt32 CountAffinity(DWORD_PTR mask)
{
  UInt32 num = 0;
  for (unsigned i = 0; i < sizeof(mask) * 8; i++)
    num += (UInt32)((mask >> i) & 1);
  return num;
}

BOOL CProcessAffinity::Get()
{
  #ifndef UNDER_CE
  return GetProcessAffinityMask(GetCurrentProcess(), &processAffinityMask, &systemAffinityMask);
  #else
  return FALSE;
  #endif
}


UInt32 GetNumberOfProcessors()
{
  // We need to know how many threads we can use.
  // By default the process is assigned to one group.
  // So we get the number of logical processors (threads)
  // assigned to current process in the current group.
  // Group size can be smaller than total number logical processors, for exammple, 2x36

  CProcessAffinity pa;

  if (pa.Get() && pa.processAffinityMask != 0)
    return pa.GetNumProcessThreads();

  SYSTEM_INFO systemInfo;
  GetSystemInfo(&systemInfo);
  // the number of logical processors in the current group
  return (UInt32)systemInfo.dwNumberOfProcessors;
}

#else


BOOL CProcessAffinity::Get()
{
  numSysThreads = GetNumberOfProcessors();

  /*
  numSysThreads = 8;
  for (unsigned i = 0; i < numSysThreads; i++)
    CpuSet_Set(&cpu_set, i);
  return TRUE;
  */
  
  #ifdef Z7_AFFINITY_SUPPORTED
  
  // numSysThreads = sysconf(_SC_NPROCESSORS_ONLN); // The number of processors currently online
  if (sched_getaffinity(0, sizeof(cpu_set), &cpu_set) != 0)
    return FALSE;
  return TRUE;
  
  #else
  
  // cpu_set = ((CCpuSet)1 << (numSysThreads)) - 1;
  return TRUE;
  // errno = ENOSYS;
  // return FALSE;
  
  #endif
}

UInt32 GetNumberOfProcessors()
{
  #ifndef Z7_ST
  long n = sysconf(_SC_NPROCESSORS_CONF);  // The number of processors configured
  if (n < 1)
    n = 1;
  return (UInt32)n;
  #else
  return 1;
  #endif
}

#endif


#ifdef _WIN32

#ifndef UNDER_CE

#if !defined(_WIN64) && \
  (defined(__MINGW32_VERSION) || defined(Z7_OLD_WIN_SDK))

typedef struct _MY_MEMORYSTATUSEX {
  DWORD dwLength;
  DWORD dwMemoryLoad;
  DWORDLONG ullTotalPhys;
  DWORDLONG ullAvailPhys;
  DWORDLONG ullTotalPageFile;
  DWORDLONG ullAvailPageFile;
  DWORDLONG ullTotalVirtual;
  DWORDLONG ullAvailVirtual;
  DWORDLONG ullAvailExtendedVirtual;
} MY_MEMORYSTATUSEX, *MY_LPMEMORYSTATUSEX;

#else

#define MY_MEMORYSTATUSEX MEMORYSTATUSEX
#define MY_LPMEMORYSTATUSEX LPMEMORYSTATUSEX

#endif

typedef BOOL (WINAPI *Func_GlobalMemoryStatusEx)(MY_LPMEMORYSTATUSEX lpBuffer);

#endif // !UNDER_CE

  
bool GetRamSize(UInt64 &size)
{
  size = (UInt64)(sizeof(size_t)) << 29;

  #ifndef UNDER_CE
    MY_MEMORYSTATUSEX stat;
    stat.dwLength = sizeof(stat);
  #endif
  
  #ifdef _WIN64
    
    if (!::GlobalMemoryStatusEx(&stat))
      return false;
    size = MyMin(stat.ullTotalVirtual, stat.ullTotalPhys);
    return true;

  #else
    
    #ifndef UNDER_CE
      const
      Func_GlobalMemoryStatusEx fn = Z7_GET_PROC_ADDRESS(
      Func_GlobalMemoryStatusEx, ::GetModuleHandleA("kernel32.dll"),
          "GlobalMemoryStatusEx");
      if (fn && fn(&stat))
      {
        size = MyMin(stat.ullTotalVirtual, stat.ullTotalPhys);
        return true;
      }
    #endif
  
    {
      MEMORYSTATUS stat2;
      stat2.dwLength = sizeof(stat2);
      ::GlobalMemoryStatus(&stat2);
      size = MyMin(stat2.dwTotalVirtual, stat2.dwTotalPhys);
      return true;
    }
  #endif
}
  
#else

// POSIX
// #include <stdio.h>

bool GetRamSize(UInt64 &size)
{
  size = (UInt64)(sizeof(size_t)) << 29;

  #ifdef __APPLE__

    #ifdef HW_MEMSIZE
      uint64_t val = 0; // support 2Gb+ RAM
      int mib[2] = { CTL_HW, HW_MEMSIZE };
    #elif defined(HW_PHYSMEM64)
      uint64_t val = 0; // support 2Gb+ RAM
      int mib[2] = { CTL_HW, HW_PHYSMEM64 };
    #else
      unsigned int val = 0; // For old system
      int mib[2] = { CTL_HW, HW_PHYSMEM };
    #endif // HW_MEMSIZE
      size_t size_sys = sizeof(val);

      sysctl(mib, 2, &val, &size_sys, NULL, 0);
      if (val)
        size = val;

  #elif defined(_AIX)
  // fixme
  #elif defined(__gnu_hurd__)
  // fixme
  #elif defined(__FreeBSD_kernel__) && defined(__GLIBC__)
  // GNU/kFreeBSD Debian
  // fixme
  #else

  struct sysinfo info;
  if (::sysinfo(&info) != 0)
    return false;
  size = (UInt64)info.mem_unit * info.totalram;
  const UInt64 kLimit = (UInt64)1 << (sizeof(size_t) * 8 - 1);
  if (size > kLimit)
    size = kLimit;

  /*
  printf("\n mem_unit  = %lld", (UInt64)info.mem_unit);
  printf("\n totalram  = %lld", (UInt64)info.totalram);
  printf("\n freeram   = %lld", (UInt64)info.freeram);
  */

  #endif

  return true;
}

#endif


unsigned long Get_File_OPEN_MAX()
{
 #ifdef _WIN32
  return (1 << 24) - (1 << 16); // ~16M handles
 #else
  // some linux versions have default open file limit for user process of 1024 files.
  long n = sysconf(_SC_OPEN_MAX);
  // n = -1; // for debug
  // n = 9; // for debug
  if (n < 1)
  {
    // n = OPEN_MAX;  // ???
    // n = FOPEN_MAX; // = 16 : <stdio.h>
   #ifdef _POSIX_OPEN_MAX
    n = _POSIX_OPEN_MAX; // = 20 : <limits.h>
   #else
    n = 30; // our limit
   #endif
  }
  return (unsigned long)n;
 #endif
}

unsigned Get_File_OPEN_MAX_Reduced_for_3_tasks()
{
  unsigned long numFiles_OPEN_MAX = NSystem::Get_File_OPEN_MAX();
  const unsigned delta = 10; // the reserve for another internal needs of process
  if (numFiles_OPEN_MAX > delta)
    numFiles_OPEN_MAX -= delta;
  else
    numFiles_OPEN_MAX = 1;
  numFiles_OPEN_MAX /= 3; // we suppose that we have up to 3 tasks in total for multiple file processing
  numFiles_OPEN_MAX = MyMax(numFiles_OPEN_MAX, (unsigned long)3);
  unsigned n = (UInt32)(UInt32)-1;
  if (n > numFiles_OPEN_MAX)
    n = (unsigned)numFiles_OPEN_MAX;
  return n;
}

}}
