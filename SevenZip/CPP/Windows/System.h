// Windows/System.h

#ifndef __WINDOWS_SYSTEM_H
#define __WINDOWS_SYSTEM_H

#ifndef _WIN32
// #include <sched.h>
#include "../../C/Threads.h"
#endif

#include "../Common/MyTypes.h"

namespace NWindows {
namespace NSystem {


#ifdef _WIN32

UInt32 CountAffinity(DWORD_PTR mask);

struct CProcessAffinity
{
  // UInt32 numProcessThreads;
  // UInt32 numSysThreads;
  DWORD_PTR processAffinityMask;
  DWORD_PTR systemAffinityMask;

  void InitST()
  {
    // numProcessThreads = 1;
    // numSysThreads = 1;
    processAffinityMask = 1;
    systemAffinityMask = 1;
  }

  void CpuZero()
  {
    processAffinityMask = 0;
  }

  void CpuSet(unsigned cpuIndex)
  {
    processAffinityMask |= ((DWORD_PTR)1 << cpuIndex);
  }

  UInt32 GetNumProcessThreads() const { return CountAffinity(processAffinityMask); }
  UInt32 GetNumSystemThreads() const { return CountAffinity(systemAffinityMask); }

  BOOL Get();

  BOOL SetProcAffinity() const
  {
    return SetProcessAffinityMask(GetCurrentProcess(), processAffinityMask);
  }
};


#else // WIN32

struct CProcessAffinity
{
  UInt32 numSysThreads;

  UInt32 GetNumSystemThreads() const { return (UInt32)numSysThreads; }
  BOOL Get();

  #ifdef _7ZIP_AFFINITY_SUPPORTED

  CCpuSet cpu_set;

  void InitST()
  {
    numSysThreads = 1;
    CpuSet_Zero(&cpu_set);
    CpuSet_Set(&cpu_set, 0);
  }

  UInt32 GetNumProcessThreads() const { return (UInt32)CPU_COUNT(&cpu_set); }
  void CpuZero()              { CpuSet_Zero(&cpu_set); }
  void CpuSet(unsigned cpuIndex)   { CpuSet_Set(&cpu_set, cpuIndex); }
  int IsCpuSet(unsigned cpuIndex) const { return CpuSet_IsSet(&cpu_set, cpuIndex); }
  // void CpuClr(int cpuIndex) { CPU_CLR(cpuIndex, &cpu_set); }

  BOOL SetProcAffinity() const
  {
    return sched_setaffinity(0, sizeof(cpu_set), &cpu_set) == 0;
  }

  #else

  void InitST()
  {
    numSysThreads = 1;
  }
  
  UInt32 GetNumProcessThreads() const
  {
    return numSysThreads;
    /*
    UInt32 num = 0;
    for (unsigned i = 0; i < sizeof(cpu_set) * 8; i++)
      num += (UInt32)((cpu_set >> i) & 1);
    return num;
    */
  }
  
  void CpuZero() { }
  void CpuSet(unsigned cpuIndex) { UNUSED_VAR(cpuIndex); }
  int IsCpuSet(unsigned cpuIndex) const { return (cpuIndex < numSysThreads) ? 1 : 0; }

  BOOL SetProcAffinity() const
  {
    errno = ENOSYS;
    return FALSE;
  }
  
  #endif
};

#endif


UInt32 GetNumberOfProcessors();

bool GetRamSize(UInt64 &size); // returns false, if unknown ram size

}}

#endif
