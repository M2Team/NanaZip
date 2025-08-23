﻿// Windows/SystemInfo.cpp

#include "StdAfx.h"

#include "../../C/CpuArch.h"

#include "../Common/IntToString.h"

#ifdef _WIN32

#include "Registry.h"

#else

#include <unistd.h>
#include <sys/utsname.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#elif !defined(_AIX)

#include <sys/auxv.h>

// #undef AT_HWCAP    // to debug
// #undef AT_HWCAP2   // to debug

/* the following patch for some debian systems.
   Is it OK to define AT_HWCAP and AT_HWCAP2 here with these constant numbers? */
/*
#if defined(__FreeBSD_kernel__) && defined(__GLIBC__)
  #ifndef AT_HWCAP
    #define AT_HWCAP 16
  #endif
  #ifndef AT_HWCAP2
    #define AT_HWCAP2 26
  #endif
#endif
*/

#ifdef MY_CPU_ARM_OR_ARM64
#include <asm/hwcap.h>
#endif
#endif

#ifdef __linux__
#include "../Windows/FileIO.h"
#endif

#endif // WIN32

#include "SystemInfo.h"
#include "System.h"

using namespace NWindows;

#ifdef __linux__

static bool ReadFile_to_Buffer(CFSTR fileName, CByteBuffer &buf)
{
  NWindows::NFile::NIO::CInFile file;
  if (!file.Open(fileName))
    return false;
  /*
  UInt64 size;
  if (!file.GetLength(size))
  {
    // GetLength() doesn't work "/proc/cpuinfo"
    return false;
  }
  if (size >= ((UInt32)1 << 29))
    return false;
  */
  size_t size = 0;
  size_t addSize = ((size_t)1 << 12);
  for (;;)
  {
    // printf("\nsize = %d\n", (unsigned)size);
    buf.ChangeSize_KeepData(size + addSize, size);
    size_t processed;
    if (!file.ReadFull(buf + size, addSize, processed))
      return false;
    if (processed == 0)
    {
      buf.ChangeSize_KeepData(size, size);
      return true;
    }
    size += processed;
    addSize *= 2;
  }
}

#endif


#if defined(_WIN32) || defined(AT_HWCAP) || defined(AT_HWCAP2)
static void PrintHex(AString &s, UInt64 v)
{
  char temp[32];
  ConvertUInt64ToHex(v, temp);
  s += temp;
}
#endif

#ifdef MY_CPU_X86_OR_AMD64

static void PrintCpuChars(AString &s, UInt32 v)
{
  for (int j = 0; j < 4; j++)
  {
    Byte b = (Byte)(v & 0xFF);
    v >>= 8;
    if (b == 0)
      break;
    s += (char)b;
  }
}


static void x86cpuid_to_String(const Cx86cpuid &c, AString &s, AString &ver)
{
  s.Empty();

  UInt32 maxFunc2 = 0;
  UInt32 t[3];

  MyCPUID(0x80000000, &maxFunc2, &t[0], &t[1], &t[2]);

  bool fullNameIsAvail = (maxFunc2 >= 0x80000004);

  if (fullNameIsAvail)
  {
    for (unsigned i = 0; i < 3; i++)
    {
      UInt32 d[4] = { 0 };
      MyCPUID(0x80000002 + i, &d[0], &d[1], &d[2], &d[3]);
      for (unsigned j = 0; j < 4; j++)
        PrintCpuChars(s, d[j]);
    }
  }

  s.Trim();
  
  if (s.IsEmpty())
  {
    for (int i = 0; i < 3; i++)
      PrintCpuChars(s, c.vendor[i]);
    s.Trim();
  }

  {
    char temp[32];
    ConvertUInt32ToHex(c.ver, temp);
    ver += temp;
  }
}

/*
static void x86cpuid_all_to_String(AString &s)
{
  Cx86cpuid p;
  if (!x86cpuid_CheckAndRead(&p))
    return;
  s += "x86cpuid maxFunc = ";
  s.Add_UInt32(p.maxFunc);
  for (unsigned j = 0; j <= p.maxFunc; j++)
  {
    s.Add_LF();
    // s.Add_UInt32(j); // align
    {
      char temp[32];
      ConvertUInt32ToString(j, temp);
      unsigned len = (unsigned)strlen(temp);
      while (len < 8)
      {
        len++;
        s.Add_Space();
      }
      s += temp;
    }

    s += ":";
    UInt32 d[4] = { 0 };
    MyCPUID(j, &d[0], &d[1], &d[2], &d[3]);
    for (unsigned i = 0; i < 4; i++)
    {
      char temp[32];
      ConvertUInt32ToHex8Digits(d[i], temp);
      s += " ";
      s += temp;
    }
  }
}
*/

#endif



#ifdef _WIN32

static const char * const k_PROCESSOR_ARCHITECTURE[] =
{
    "x86" // "INTEL"
  , "MIPS"
  , "ALPHA"
  , "PPC"
  , "SHX"
  , "ARM"
  , "IA64"
  , "ALPHA64"
  , "MSIL"
  , "x64" // "AMD64"
  , "IA32_ON_WIN64"
  , "NEUTRAL"
  , "ARM64"
  , "ARM32_ON_WIN64"
};

#define MY__PROCESSOR_ARCHITECTURE_INTEL 0
#define MY__PROCESSOR_ARCHITECTURE_AMD64 9


#define MY__PROCESSOR_INTEL_PENTIUM  586
#define MY__PROCESSOR_AMD_X8664      8664

/*
static const CUInt32PCharPair k_PROCESSOR[] =
{
  { 2200, "IA64" },
  { 8664, "x64" }
};

#define PROCESSOR_INTEL_386      386
#define PROCESSOR_INTEL_486      486
#define PROCESSOR_INTEL_PENTIUM  586
#define PROCESSOR_INTEL_860      860
#define PROCESSOR_INTEL_IA64     2200
#define PROCESSOR_AMD_X8664      8664
#define PROCESSOR_MIPS_R2000     2000
#define PROCESSOR_MIPS_R3000     3000
#define PROCESSOR_MIPS_R4000     4000
#define PROCESSOR_ALPHA_21064    21064
#define PROCESSOR_PPC_601        601
#define PROCESSOR_PPC_603        603
#define PROCESSOR_PPC_604        604
#define PROCESSOR_PPC_620        620
#define PROCESSOR_HITACHI_SH3    10003
#define PROCESSOR_HITACHI_SH3E   10004
#define PROCESSOR_HITACHI_SH4    10005
#define PROCESSOR_MOTOROLA_821   821
#define PROCESSOR_SHx_SH3        103
#define PROCESSOR_SHx_SH4        104
#define PROCESSOR_STRONGARM      2577    // 0xA11
#define PROCESSOR_ARM720         1824    // 0x720
#define PROCESSOR_ARM820         2080    // 0x820
#define PROCESSOR_ARM920         2336    // 0x920
#define PROCESSOR_ARM_7TDMI      70001
#define PROCESSOR_OPTIL          18767   // 0x494f
*/


/*
static const char * const k_PF[] =
{
    "FP_ERRATA"
  , "FP_EMU"
  , "CMPXCHG"
  , "MMX"
  , "PPC_MOVEMEM_64BIT"
  , "ALPHA_BYTE"
  , "SSE"
  , "3DNOW"
  , "RDTSC"
  , "PAE"
  , "SSE2"
  , "SSE_DAZ"
  , "NX"
  , "SSE3"
  , "CMPXCHG16B"
  , "CMP8XCHG16"
  , "CHANNELS"
  , "XSAVE"
  , "ARM_VFP_32"
  , "ARM_NEON"
  , "L2AT"
  , "VIRT_FIRMWARE"
  , "RDWRFSGSBASE"
  , "FASTFAIL"
  , "ARM_DIVIDE"
  , "ARM_64BIT_LOADSTORE_ATOMIC"
  , "ARM_EXTERNAL_CACHE"
  , "ARM_FMAC"
  , "RDRAND"
  , "ARM_V8"
  , "ARM_V8_CRYPTO"
  , "ARM_V8_CRC32"
  , "RDTSCP"
  , "RDPID"
  , "ARM_V81_ATOMIC"
  , "MONITORX"
};
*/

#endif


#ifdef _WIN32

static void PrintPage(AString &s, UInt32 v)
{
  if ((v & 0x3FF) == 0)
  {
    s.Add_UInt32(v >> 10);
    s += "K";
  }
  else
    s.Add_UInt32(v >> 10);
}

static AString TypeToString2(const char * const table[], unsigned num, UInt32 value)
{
  char sz[16];
  const char *p = NULL;
  if (value < num)
    p = table[value];
  if (!p)
  {
    ConvertUInt32ToString(value, sz);
    p = sz;
  }
  return (AString)p;
}

// #if defined(_7ZIP_LARGE_PAGES) || defined(_WIN32)
// #ifdef _WIN32
void PrintSize_KMGT_Or_Hex(AString &s, UInt64 v)
{
  char c = 0;
  if ((v & 0x3FF) == 0) { v >>= 10; c = 'K';
  if ((v & 0x3FF) == 0) { v >>= 10; c = 'M';
  if ((v & 0x3FF) == 0) { v >>= 10; c = 'G';
  if ((v & 0x3FF) == 0) { v >>= 10; c = 'T';
  }}}}
  else
  {
    PrintHex(s, v);
    return;
  }
  char temp[32];
  ConvertUInt64ToString(v, temp);
  s += temp;
  if (c)
    s += c;
}
// #endif
// #endif

static void SysInfo_To_String(AString &s, const SYSTEM_INFO &si)
{
  s += TypeToString2(k_PROCESSOR_ARCHITECTURE, ARRAY_SIZE(k_PROCESSOR_ARCHITECTURE), si.wProcessorArchitecture);

  if (!( (si.wProcessorArchitecture == MY__PROCESSOR_ARCHITECTURE_INTEL && si.dwProcessorType == MY__PROCESSOR_INTEL_PENTIUM)
      || (si.wProcessorArchitecture == MY__PROCESSOR_ARCHITECTURE_AMD64 && si.dwProcessorType == MY__PROCESSOR_AMD_X8664)))
  {
    s += " ";
    // s += TypePairToString(k_PROCESSOR, ARRAY_SIZE(k_PROCESSOR), si.dwProcessorType);
    s.Add_UInt32(si.dwProcessorType);
  }
  s += " ";
  PrintHex(s, si.wProcessorLevel);
  s += ".";
  PrintHex(s, si.wProcessorRevision);
  if ((UInt64)si.dwActiveProcessorMask + 1 != ((UInt64)1 << si.dwNumberOfProcessors))
  if ((UInt64)si.dwActiveProcessorMask + 1 != 0 || si.dwNumberOfProcessors != sizeof(UInt64) * 8)
  {
    s += " act:";
    PrintHex(s, si.dwActiveProcessorMask);
  }
  s += " cpus:";
  s.Add_UInt32(si.dwNumberOfProcessors);
  if (si.dwPageSize != 1 << 12)
  {
    s += " page:";
    PrintPage(s, si.dwPageSize);
  }
  if (si.dwAllocationGranularity != 1 << 16)
  {
    s += " gran:";
    PrintPage(s, si.dwAllocationGranularity);
  }
  s += " ";

  DWORD_PTR minAdd = (DWORD_PTR)si.lpMinimumApplicationAddress;
  UInt64 maxSize = (UInt64)(DWORD_PTR)si.lpMaximumApplicationAddress + 1;
  const UInt32 kReserveSize = ((UInt32)1 << 16);
  if (minAdd != kReserveSize)
  {
    PrintSize_KMGT_Or_Hex(s, minAdd);
    s += "-";
  }
  else
  {
    if ((maxSize & (kReserveSize - 1)) == 0)
      maxSize += kReserveSize;
  }
  PrintSize_KMGT_Or_Hex(s, maxSize);
}

#ifndef _WIN64
EXTERN_C_BEGIN
typedef VOID (WINAPI *Func_GetNativeSystemInfo)(LPSYSTEM_INFO lpSystemInfo);
EXTERN_C_END
#endif

#endif

#ifdef __APPLE__
#ifndef MY_CPU_X86_OR_AMD64
static void Add_sysctlbyname_to_String(const char *name, AString &s)
{
  size_t bufSize = 256;
  char buf[256];
  if (My_sysctlbyname_Get(name, &buf, &bufSize) == 0)
    s += buf;
}
#endif
#endif

void GetSysInfo(AString &s1, AString &s2);
void GetSysInfo(AString &s1, AString &s2)
{
  s1.Empty();
  s2.Empty();

  #ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    {
      SysInfo_To_String(s1, si);
      // s += " : ";
    }
    
    #if !defined(_WIN64) && !defined(UNDER_CE)
    Func_GetNativeSystemInfo fn_GetNativeSystemInfo = (Func_GetNativeSystemInfo)(void *)GetProcAddress(
        GetModuleHandleA("kernel32.dll"), "GetNativeSystemInfo");
    if (fn_GetNativeSystemInfo)
    {
      SYSTEM_INFO si2;
      fn_GetNativeSystemInfo(&si2);
      // if (memcmp(&si, &si2, sizeof(si)) != 0)
      {
        // s += " - ";
        SysInfo_To_String(s2, si2);
      }
    }
    #endif
  #endif
}


void GetCpuName(AString &s);

static void AddBracedString(AString &dest, AString &src)
{
  if (!src.IsEmpty())
  {
    AString s;
    s += '(';
    s += src;
    s += ')';
    dest.Add_OptSpaced(s);
  }
}

struct CCpuName
{
  AString CpuName;
  AString Revision;
  AString Microcode;
  AString LargePages;

  void Fill();

  void Get_Revision_Microcode_LargePages(AString &s)
  {
    s.Empty();
    AddBracedString(s, Revision);
    AddBracedString(s, Microcode);
    s.Add_OptSpaced(LargePages);
  }
};

void CCpuName::Fill()
{
  CpuName.Empty();
  Revision.Empty();
  Microcode.Empty();
  LargePages.Empty();

  AString &s = CpuName;

  #ifdef MY_CPU_X86_OR_AMD64
  {
    Cx86cpuid cpuid;
    if (x86cpuid_CheckAndRead(&cpuid))
    {
      x86cpuid_to_String(cpuid, s, Revision);
    }
    else
    {
    #ifdef MY_CPU_AMD64
    s += "x64";
    #else
    s += "x86";
    #endif
    }
  }
  #elif defined(__APPLE__)
  {
    Add_sysctlbyname_to_String("machdep.cpu.brand_string", s);
  }
  #endif


  if (s.IsEmpty())
  {
    #ifdef MY_CPU_LE
      s += "LE";
    #elif defined(MY_CPU_BE)
      s += "BE";
    #endif
  }
  
  #ifdef __APPLE__
  {
    AString s2;
    UInt32 v = 0;
    if (My_sysctlbyname_Get_UInt32("machdep.cpu.core_count", &v) == 0)
    {
      s2.Add_UInt32(v);
      s2 += 'C';
    }
    if (My_sysctlbyname_Get_UInt32("machdep.cpu.thread_count", &v) == 0)
    {
      s2.Add_UInt32(v);
      s2 += 'T';
    }
    if (!s2.IsEmpty())
    {
      s.Add_Space_if_NotEmpty();
      s += s2;
    }
  }
  #endif

  
  #ifdef _WIN32
  {
    NRegistry::CKey key;
    if (key.Open(HKEY_LOCAL_MACHINE, TEXT("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"), KEY_READ) == ERROR_SUCCESS)
    {
      LONG res[2];
      CByteBuffer bufs[2];
      {
        for (int i = 0; i < 2; i++)
        {
          UInt32 size = 0;
          res[i] = key.QueryValue(i == 0 ?
            TEXT("Previous Update Revision") :
            TEXT("Update Revision"), bufs[i], size);
          if (res[i] == ERROR_SUCCESS)
            if (size != bufs[i].Size())
              res[i] = ERROR_SUCCESS + 1;
        }
      }
      if (res[0] == ERROR_SUCCESS || res[1] == ERROR_SUCCESS)
      {
        for (int i = 0; i < 2; i++)
        {
          if (i == 1)
            Microcode += "->";
          if (res[i] != ERROR_SUCCESS)
            continue;
          const CByteBuffer &buf = bufs[i];
          if (buf.Size() == 8)
          {
            UInt32 high = GetUi32(buf);
            if (high != 0)
            {
              PrintHex(Microcode, high);
              Microcode += ".";
            }
            PrintHex(Microcode, GetUi32(buf + 4));
          }
        }
      }
    }
  }
  #endif


  #ifdef _7ZIP_LARGE_PAGES
  Add_LargePages_String(LargePages);
  #endif
}

void AddCpuFeatures(AString &s);
void AddCpuFeatures(AString &s)
{
  #ifdef _WIN32
  // const unsigned kNumFeatures_Extra = 32; // we check also for unknown features
  // const unsigned kNumFeatures = ARRAY_SIZE(k_PF) + kNumFeatures_Extra;
  const unsigned kNumFeatures = 64;
  UInt64 flags = 0;
  for (unsigned i = 0; i < kNumFeatures; i++)
  {
    if (IsProcessorFeaturePresent(i))
    {
      flags += (UInt64)1 << i;
      // s.Add_Space_if_NotEmpty();
      // s += TypeToString2(k_PF, ARRAY_SIZE(k_PF), i);
    }
  }
  s.Add_OptSpaced("f:");
  PrintHex(s, flags);
  
  #elif defined(__APPLE__)
  {
    UInt32 v = 0;
    if (My_sysctlbyname_Get_UInt32("hw.pagesize", &v) == 0)
    {
      s += "PageSize:";
      s.Add_UInt32(v >> 10);
      s += "KB";
    }
  }

  #else

  const long v = sysconf(_SC_PAGESIZE);
  if (v != -1)
  {
    s.Add_Space_if_NotEmpty();
    s += "PageSize:";
    s.Add_UInt32((UInt32)(v >> 10));
    s += "KB";
  }

  #if !defined(_AIX)

  #ifdef __linux__

  CByteBuffer buf;
  if (ReadFile_to_Buffer("/sys/kernel/mm/transparent_hugepage/enabled", buf))
  // if (ReadFile_to_Buffer("/proc/cpuinfo", buf))
  {
    s.Add_OptSpaced("THP:");
    AString s2;
    s2.SetFrom_CalcLen((const char *)(const void *)(const Byte *)buf, (unsigned)buf.Size());
    const int pos = s2.Find('[');
    if (pos >= 0)
    {
      const int pos2 = s2.Find(']', pos + 1);
      if (pos2 >= 0)
      {
        s2.DeleteFrom(pos2);
        s2.DeleteFrontal(pos + 1);
      }
    }
    s += s2;
  }
  // else throw CSystemException(MY_SRes_HRESULT_FROM_WRes(errno));

  #endif


  #ifdef AT_HWCAP
  s.Add_OptSpaced("hwcap:");
  {
    unsigned long h = getauxval(AT_HWCAP);
    PrintHex(s, h);
    #ifdef MY_CPU_ARM64
    if (h & HWCAP_CRC32)  s += ":CRC32";
    if (h & HWCAP_SHA1)   s += ":SHA1";
    if (h & HWCAP_SHA2)   s += ":SHA2";
    if (h & HWCAP_AES)    s += ":AES";
    if (h & HWCAP_ASIMD)  s += ":ASIMD";
    #elif defined(MY_CPU_ARM)
    if (h & HWCAP_NEON)   s += ":NEON";
    #endif
  }
  #endif // AT_HWCAP
 
  #ifdef AT_HWCAP2
  {
    unsigned long h = getauxval(AT_HWCAP2);
    #ifndef MY_CPU_ARM
    if (h != 0)
    #endif
    {
      s += " hwcap2:";
      PrintHex(s, h);
      #ifdef MY_CPU_ARM
      if (h & HWCAP2_CRC32)  s += ":CRC32";
      if (h & HWCAP2_SHA1)   s += ":SHA1";
      if (h & HWCAP2_SHA2)   s += ":SHA2";
      if (h & HWCAP2_AES)    s += ":AES";
      #endif
    }
  }
  #endif // AT_HWCAP2
  #endif // _AIX
  #endif // _WIN32
}


#ifdef _WIN32
#ifndef UNDER_CE

EXTERN_C_BEGIN
typedef void (WINAPI * Func_RtlGetVersion) (OSVERSIONINFOEXW *);
EXTERN_C_END

static BOOL My_RtlGetVersion(OSVERSIONINFOEXW *vi)
{
  HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
  if (!ntdll)
    return FALSE;
  Func_RtlGetVersion func = (Func_RtlGetVersion)(void *)GetProcAddress(ntdll, "RtlGetVersion");
  if (!func)
    return FALSE;
  func(vi);
  return TRUE;
}

#endif
#endif


void GetOsInfoText(AString &sRes)
{
  sRes.Empty();
    AString s;

    #ifdef _WIN32
    #ifndef UNDER_CE
      // OSVERSIONINFO vi;
      OSVERSIONINFOEXW vi;
      vi.dwOSVersionInfoSize = sizeof(vi);
      // if (::GetVersionEx(&vi))
      if (My_RtlGetVersion(&vi))
      {
        s += "Windows";
        if (vi.dwPlatformId != VER_PLATFORM_WIN32_NT)
          s.Add_UInt32(vi.dwPlatformId);
        s += " "; s.Add_UInt32(vi.dwMajorVersion);
        s += "."; s.Add_UInt32(vi.dwMinorVersion);
        s += " "; s.Add_UInt32(vi.dwBuildNumber);

        if (vi.wServicePackMajor != 0 || vi.wServicePackMinor != 0)
        {
          s += " SP:"; s.Add_UInt32(vi.wServicePackMajor);
          s += "."; s.Add_UInt32(vi.wServicePackMinor);
        }
        // s += " Suite:"; PrintHex(s, vi.wSuiteMask);
        // s += " Type:"; s.Add_UInt32(vi.wProductType);
        // s += " "; s += GetOemString(vi.szCSDVersion);
      }
      /*
      {
        s += " OEMCP:"; s.Add_UInt32(GetOEMCP());
        s += " ACP:";   s.Add_UInt32(GetACP());
      }
      */
    #endif
    #else // _WIN32

      if (!s.IsEmpty())
        s.Add_LF();
      struct utsname un;
      if (uname(&un) == 0)
      {
        s += un.sysname;
        // s += " : "; s += un.nodename; // we don't want to show name of computer
        s += " : "; s += un.release;
        s += " : "; s += un.version;
        s += " : "; s += un.machine;

        #ifdef __APPLE__
          // Add_sysctlbyname_to_String("kern.version", s);
          // it's same as "utsname.version"
        #endif
      }
    #endif  // _WIN32

    sRes += s;
}



void GetSystemInfoText(AString &sRes)
{
  GetOsInfoText(sRes);
  sRes.Add_LF();

    {
      AString s, s1, s2;
      GetSysInfo(s1, s2);
      if (!s1.IsEmpty() || !s2.IsEmpty())
      {
        s = s1;
        if (s1 != s2 && !s2.IsEmpty())
        {
          s += " - ";
          s += s2;
        }
      }
      {
        AddCpuFeatures(s);
        if (!s.IsEmpty())
        {
          sRes += s;
          sRes.Add_LF();
        }
      }
    }
    {
      AString s;
      GetCpuName(s);
      if (!s.IsEmpty())
      {
        sRes += s;
        sRes.Add_LF();
      }
    }
    /*
    #ifdef MY_CPU_X86_OR_AMD64
    {
      AString s;
      x86cpuid_all_to_String(s);
      if (!s.IsEmpty())
      {
        printCallback->Print(s);
        printCallback->NewLine();
      }
    }
    #endif
    */
}


void GetCpuName(AString &s);
void GetCpuName(AString &s)
{
  CCpuName cpuName;
  cpuName.Fill();
  s = cpuName.CpuName;
  AString s2;
  cpuName.Get_Revision_Microcode_LargePages(s2);
  s.Add_OptSpaced(s2);
}


void GetCpuName_MultiLine(AString &s);
void GetCpuName_MultiLine(AString &s)
{
  CCpuName cpuName;
  cpuName.Fill();
  s = cpuName.CpuName;
  AString s2;
  cpuName.Get_Revision_Microcode_LargePages(s2);
  if (!s2.IsEmpty())
  {
    s.Add_LF();
    s += s2;
  }
}

void GetCompiler(AString &s)
{
  #ifdef __VERSION__
    s += __VERSION__;
  #endif

  #ifdef __GNUC__
    s += " GCC ";
    s.Add_UInt32(__GNUC__);
    s += '.';
    s.Add_UInt32(__GNUC_MINOR__);
    s += '.';
    s.Add_UInt32(__GNUC_PATCHLEVEL__);
  #endif

  #ifdef __clang__
    s += " CLANG ";
    s.Add_UInt32(__clang_major__);
    s += '.';
    s.Add_UInt32(__clang_minor__);
  #endif

  #ifdef __xlC__
    s += " XLC ";
    s.Add_UInt32(__xlC__ >> 8);
    s += '.';
    s.Add_UInt32(__xlC__ & 0xFF);
    #ifdef __xlC_ver__
      s += '.';
      s.Add_UInt32(__xlC_ver__ >> 8);
      s += '.';
      s.Add_UInt32(__xlC_ver__ & 0xFF);
    #endif
  #endif

  #ifdef _MSC_VER
    s += " MSC ";
    s.Add_UInt32(_MSC_VER);
  #endif
}
