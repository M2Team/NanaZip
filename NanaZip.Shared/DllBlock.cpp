/*
 * PROJECT:    NanaZip
 * FILE:       DllBlock.cpp
 * PURPOSE:    Implementation for DLL blocker
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: dinhngtu (contact@tudinh.xyz)
 *             MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include <Mile.Internal.h>

#include <K7BaseDetours.h>
#include <K7BasePolicies.h>
#include <K7BaseMitigations.h>

#include <mutex>
#include <vector>
#include <intrin.h>

#include "DllBlock.h"
#include "Mitigations.h"

#ifdef NDEBUG

namespace
{
    namespace ModuleTypes
    {
        enum
        {
            Unknown,
            NeedsBlocking,
            NeedsDynamicCodeOptout,
        };
    }

    struct ModuleItem
    {
        MO_UINT8 ModuleType;
        MO_CONSTANT_STRING ModuleName;
    };

    /**
     * @remarks Make sure that this list is sorted for _stricmp.
     */
    const ModuleItem g_ModuleRules[] =
    {
        { ModuleTypes::NeedsDynamicCodeOptout, "BaseGUI.dll" },
        { ModuleTypes::NeedsBlocking, "ExplorerPatcher.amd64.dll" },
        { ModuleTypes::NeedsBlocking, "ExplorerPatcher.IA-32.dll" },
        { ModuleTypes::NeedsBlocking, "PrxDrvPE.dll" },
        { ModuleTypes::NeedsBlocking, "PrxDrvPE64.dll" },
        { ModuleTypes::NeedsBlocking, "TFMain32.dll" },
        { ModuleTypes::NeedsBlocking, "TFMain64.dll" },
    };

    static MO_UINT8 QueryModuleType(
        _In_ MO_CONSTANT_STRING ModuleName)
    {
        for (MO_UINT32 i = 0; i < MO_ARRAY_SIZE(g_ModuleRules); ++i)
        {
            if (!::_stricmp(ModuleName, g_ModuleRules[i].ModuleName))
            {
                return g_ModuleRules[i].ModuleType;
            }
        }
        return ModuleTypes::Unknown;
    }

    static std::mutex g_DynamicCodeRangeMutex;

    static bool IsCurrentProcessHandle(
        _In_ HANDLE ProcessHandle)
    {
        if (ProcessHandle == ::GetCurrentProcess() ||
            ::GetCurrentProcessId() == ::GetProcessId(ProcessHandle))
        {
            return true;
        }
        return false;
    }

    static bool IsPageExecute(
        _In_ DWORD Protect)
    {
        if (PAGE_EXECUTE == Protect ||
            PAGE_EXECUTE_READ == Protect ||
            PAGE_EXECUTE_READWRITE == Protect ||
            PAGE_EXECUTE_WRITECOPY == Protect)
        {
            return true;
        }
        return false;
    }

    namespace FunctionTypes
    {
        enum
        {
            NtMapViewOfSection,
            NtQuerySection,
            NtUnmapViewOfSection,
            VirtualAlloc,
            VirtualAllocEx,
            VirtualProtect,
            VirtualProtectEx,

            MaximumFunction
        };
    }

    struct FunctionItem
    {
        PVOID Original;
        PVOID Detoured;
    };

    static FunctionItem g_FunctionTable[FunctionTypes::MaximumFunction];

    static HMODULE GetNtDllModuleHandle()
    {
        static HMODULE CachedResult = ::GetModuleHandleW(L"ntdll.dll");
        return CachedResult;
    }

    static HMODULE GetKernel32ModuleHandle()
    {
        static HMODULE CachedResult = ::GetModuleHandleW(L"kernel32.dll");
        return CachedResult;
    }

    static NTSTATUS OriginalNtMapViewOfSection(
        _In_ HANDLE SectionHandle,
        _In_ HANDLE ProcessHandle,
        _Inout_ PVOID* BaseAddress,
        _In_ ULONG_PTR ZeroBits,
        _In_ SIZE_T CommitSize,
        _Inout_opt_ PLARGE_INTEGER SectionOffset,
        _Inout_ PSIZE_T ViewSize,
        _In_ SECTION_INHERIT InheritDisposition,
        _In_ ULONG AllocationType,
        _In_ ULONG PageProtection)
    {
        using FunctionType = decltype(::NtMapViewOfSection)*;
        FunctionType FunctionAddress = reinterpret_cast<FunctionType>(
            g_FunctionTable[FunctionTypes::NtMapViewOfSection].Original);
        if (!FunctionAddress)
        {
            return STATUS_NOINTERFACE;
        }
        return FunctionAddress(
            SectionHandle,
            ProcessHandle,
            BaseAddress,
            ZeroBits,
            CommitSize,
            SectionOffset,
            ViewSize,
            InheritDisposition,
            AllocationType,
            PageProtection);
    }

    static NTSTATUS OriginalNtQuerySection(
        _In_ HANDLE SectionHandle,
        _In_ SECTION_INFORMATION_CLASS SectionInformationClass,
        _Out_ PVOID SectionInformation,
        _In_ SIZE_T SectionInformationLength,
        _Out_opt_ PSIZE_T ReturnLength)
    {
        using FunctionType = decltype(::NtQuerySection)*;
        FunctionType FunctionAddress = reinterpret_cast<FunctionType>(
            g_FunctionTable[FunctionTypes::NtQuerySection].Original);
        if (!FunctionAddress)
        {
            return STATUS_NOINTERFACE;
        }
        return FunctionAddress(
            SectionHandle,
            SectionInformationClass,
            SectionInformation,
            SectionInformationLength,
            ReturnLength);
    }

    static NTSTATUS OriginalNtUnmapViewOfSection(
        _In_ HANDLE ProcessHandle,
        _In_opt_ PVOID BaseAddress)
    {
        using FunctionType = decltype(::NtUnmapViewOfSection)*;
        FunctionType FunctionAddress = reinterpret_cast<FunctionType>(
            g_FunctionTable[FunctionTypes::NtUnmapViewOfSection].Original);
        if (!FunctionAddress)
        {
            return STATUS_NOINTERFACE;
        }
        return FunctionAddress(
            ProcessHandle,
            BaseAddress);
    }

    static LPVOID OriginalVirtualAlloc(
        _In_opt_ LPVOID lpAddress,
        _In_ SIZE_T dwSize,
        _In_ DWORD flAllocationType,
        _In_ DWORD flProtect)
    {
        using FunctionType = decltype(::VirtualAlloc)*;
        FunctionType FunctionAddress = reinterpret_cast<FunctionType>(
            g_FunctionTable[FunctionTypes::VirtualAlloc].Original);
        if (!FunctionAddress)
        {
            ::SetLastError(ERROR_NOINTERFACE);
            return nullptr;
        }
        return FunctionAddress(
            lpAddress,
            dwSize,
            flAllocationType,
            flProtect);
    }

    static LPVOID OriginalVirtualAllocEx(
        _In_ HANDLE hProcess,
        _In_opt_ LPVOID lpAddress,
        _In_ SIZE_T dwSize,
        _In_ DWORD flAllocationType,
        _In_ DWORD flProtect)
    {
        using FunctionType = decltype(::VirtualAllocEx)*;
        FunctionType FunctionAddress = reinterpret_cast<FunctionType>(
            g_FunctionTable[FunctionTypes::VirtualAllocEx].Original);
        if (!FunctionAddress)
        {
            ::SetLastError(ERROR_NOINTERFACE);
            return nullptr;
        }
        return FunctionAddress(
            hProcess,
            lpAddress,
            dwSize,
            flAllocationType,
            flProtect);
    }

    static BOOL OriginalVirtualProtect(
        _In_ LPVOID lpAddress,
        _In_ SIZE_T dwSize,
        _In_ DWORD flNewProtect,
        _Out_ PDWORD lpflOldProtect)
    {
        using FunctionType = decltype(::VirtualProtect)*;
        FunctionType FunctionAddress = reinterpret_cast<FunctionType>(
            g_FunctionTable[FunctionTypes::VirtualProtect].Original);
        if (!FunctionAddress)
        {
            ::SetLastError(ERROR_NOINTERFACE);
            return FALSE;
        }
        return FunctionAddress(
            lpAddress,
            dwSize,
            flNewProtect,
            lpflOldProtect);
    }

    static BOOL OriginalVirtualProtectEx(
        _In_ HANDLE hProcess,
        _In_ LPVOID lpAddress,
        _In_ SIZE_T dwSize,
        _In_ DWORD flNewProtect,
        _Out_ PDWORD lpflOldProtect)
    {
        using FunctionType = decltype(::VirtualProtectEx)*;
        FunctionType FunctionAddress = reinterpret_cast<FunctionType>(
            g_FunctionTable[FunctionTypes::VirtualProtectEx].Original);
        if (!FunctionAddress)
        {
            ::SetLastError(ERROR_NOINTERFACE);
            return FALSE;
        }
        return FunctionAddress(
            hProcess,
            lpAddress,
            dwSize,
            flNewProtect,
            lpflOldProtect);
    }
}

namespace
{
    static std::vector<std::pair<UINT_PTR, SIZE_T>> dynamic_code_ranges;

    static inline bool CheckExtents(size_t viewSize, size_t offset, size_t size)
    {
        return offset < viewSize && size <= viewSize && offset + size <= viewSize;
    }

    static bool GetDllExportName(char(&dllName)[256], const char* base, size_t viewSize)
    {
        if (viewSize < sizeof(IMAGE_DOS_HEADER))
        {
            return false;
        }
        const IMAGE_DOS_HEADER* dosHdr = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dosHdr->e_magic != IMAGE_DOS_SIGNATURE)
        {
            return false;
        }
        if (dosHdr->e_lfanew < 0 || !CheckExtents(viewSize, dosHdr->e_lfanew, sizeof(IMAGE_NT_HEADERS)))
        {
            return false;
        }
        const IMAGE_NT_HEADERS* ntHdr = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dosHdr->e_lfanew);
        if (ntHdr->Signature != IMAGE_NT_SIGNATURE || ntHdr->OptionalHeader.NumberOfRvaAndSizes < IMAGE_DIRECTORY_ENTRY_EXPORT)
            return false;
        const IMAGE_DATA_DIRECTORY* dirExport = &(ntHdr->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT]);
        if (dirExport->Size < sizeof(IMAGE_EXPORT_DIRECTORY) || !dirExport->VirtualAddress || !CheckExtents(viewSize, dirExport->VirtualAddress, sizeof(IMAGE_EXPORT_DIRECTORY)))
        {
            return false;
        }
        const IMAGE_EXPORT_DIRECTORY* exports = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(base + dirExport->VirtualAddress);
        // we don't know the export directory name size so assume that at least 256 bytes after the name are safe
        if (!exports->Name || !CheckExtents(viewSize, exports->Name, ARRAYSIZE(dllName)))
        {
            return false;
        }
        const char* name = base + exports->Name;
        if (strncpy_s(dllName, name, _TRUNCATE))
        {
            return false;
        }
        return true;
    }

    static NTSTATUS NTAPI MyNtMapViewOfSection(
        _In_ HANDLE SectionHandle,
        _In_ HANDLE ProcessHandle,
        _Inout_ _At_(*BaseAddress, _Readable_bytes_(*ViewSize) _Writable_bytes_(*ViewSize) _Post_readable_byte_size_(*ViewSize)) PVOID* BaseAddress,
        _In_ ULONG_PTR ZeroBits,
        _In_ SIZE_T CommitSize,
        _Inout_opt_ PLARGE_INTEGER SectionOffset,
        _Inout_ PSIZE_T ViewSize,
        _In_ SECTION_INHERIT InheritDisposition,
        _In_ ULONG AllocationType,
        _In_ ULONG Win32Protect)
    {
        NTSTATUS ret, status;
        ret = ::OriginalNtMapViewOfSection(
            SectionHandle,
            ProcessHandle,
            BaseAddress,
            ZeroBits,
            CommitSize,
            SectionOffset,
            ViewSize,
            InheritDisposition,
            AllocationType,
            Win32Protect);
        if (ret < 0 || !::IsCurrentProcessHandle(ProcessHandle) || !::IsPageExecute(Win32Protect))
        {
            return ret;
        }
        SECTION_BASIC_INFORMATION sbi = {};
        status = ::OriginalNtQuerySection(SectionHandle, SectionBasicInformation, &sbi, sizeof(sbi), NULL);
        if (status < 0 || !(sbi.AllocationAttributes & SEC_IMAGE))
        {
            return ret;
        }
        char dllName[256] = {};
        if (!GetDllExportName(dllName, reinterpret_cast<const char*>(*BaseAddress), *ViewSize))
        {
            return ret;
        }
        MO_UINT8 ModuleType = ::QueryModuleType(dllName);
        if (ModuleTypes::NeedsBlocking & ModuleType)
        {
            ::OriginalNtUnmapViewOfSection(ProcessHandle, *BaseAddress);
            return STATUS_ACCESS_DENIED;
        }
        else if (ModuleTypes::NeedsDynamicCodeOptout & ModuleType)
        {
            std::lock_guard<std::mutex> Lock(g_DynamicCodeRangeMutex);
            dynamic_code_ranges.push_back(std::make_pair(reinterpret_cast<UINT_PTR>(*BaseAddress), *ViewSize));
        }
        return ret;
    }

    static NTSTATUS NTAPI MyNtUnmapViewOfSection(
        _In_ HANDLE ProcessHandle,
        _In_opt_ PVOID BaseAddress)
    {
        NTSTATUS ret = ::OriginalNtUnmapViewOfSection(ProcessHandle, BaseAddress);
        if (ret < 0 || !::IsCurrentProcessHandle(ProcessHandle))
        {
            return ret;
        }
        UINT_PTR ptr = reinterpret_cast<UINT_PTR>(BaseAddress);
        {
            std::lock_guard<std::mutex> Lock(g_DynamicCodeRangeMutex);
            for (auto it = dynamic_code_ranges.begin(); it != dynamic_code_ranges.end(); it++)
            {
                if (ptr >= it->first && ptr < it->first + it->second)
                {
                    dynamic_code_ranges.erase(it);
                    break;
                }
            }
        }
        return ret;
    }

    static bool CallerUsesDynamicCode(void* pCaller)
    {
        UINT_PTR caller = reinterpret_cast<UINT_PTR>(pCaller);
        std::lock_guard<std::mutex> Lock(g_DynamicCodeRangeMutex);
        for (auto it = dynamic_code_ranges.begin(); it != dynamic_code_ranges.end(); it++)
        {
            if (caller >= it->first && caller < it->first + it->second)
            {
                return true;
            }
        }
        return false;
    }
}

namespace
{
    static LPVOID DetouredVirtualAlloc(
        _In_opt_ LPVOID lpAddress,
        _In_ SIZE_T dwSize,
        _In_ DWORD flAllocationType,
        _In_ DWORD flProtect)
    {
        if (!::IsPageExecute(flProtect) ||
            !::CallerUsesDynamicCode(_ReturnAddress()))
        {
            return ::OriginalVirtualAlloc(
                lpAddress,
                dwSize,
                flAllocationType,
                flProtect);
        }
        ::K7BaseSetCurrentThreadDynamicCodePolicyOptOut(MO_TRUE);
        LPVOID Result = ::OriginalVirtualAlloc(
            lpAddress,
            dwSize,
            flAllocationType,
            flProtect);
        ::K7BaseSetCurrentThreadDynamicCodePolicyOptOut(MO_FALSE);
        return Result;
    }

    static LPVOID DetouredVirtualAllocEx(
        _In_ HANDLE hProcess,
        _In_opt_ LPVOID lpAddress,
        _In_ SIZE_T dwSize,
        _In_ DWORD flAllocationType,
        _In_ DWORD flProtect)
    {
        if (!::IsCurrentProcessHandle(hProcess) ||
            !::IsPageExecute(flProtect) ||
            !::CallerUsesDynamicCode(_ReturnAddress()))
        {
            return ::OriginalVirtualAllocEx(
                hProcess,
                lpAddress,
                dwSize,
                flAllocationType,
                flProtect);
        }
        ::K7BaseSetCurrentThreadDynamicCodePolicyOptOut(MO_TRUE);
        LPVOID Result = ::OriginalVirtualAllocEx(
            hProcess,
            lpAddress,
            dwSize,
            flAllocationType,
            flProtect);
        ::K7BaseSetCurrentThreadDynamicCodePolicyOptOut(MO_FALSE);
        return Result;
    }

    static BOOL DetouredVirtualProtect(
        _In_ LPVOID lpAddress,
        _In_ SIZE_T dwSize,
        _In_ DWORD flNewProtect,
        _Out_ PDWORD lpflOldProtect)
    {
        if (!::IsPageExecute(flNewProtect) ||
            !::CallerUsesDynamicCode(_ReturnAddress()))
        {
            return ::OriginalVirtualProtect(
                lpAddress,
                dwSize,
                flNewProtect,
                lpflOldProtect);
        }
        ::K7BaseSetCurrentThreadDynamicCodePolicyOptOut(MO_TRUE);
        BOOL Result = ::OriginalVirtualProtect(
            lpAddress,
            dwSize,
            flNewProtect,
            lpflOldProtect);
        ::K7BaseSetCurrentThreadDynamicCodePolicyOptOut(MO_FALSE);
        return Result;
    }

    static BOOL DetouredVirtualProtectEx(
        _In_ HANDLE hProcess,
        _In_ LPVOID lpAddress,
        _In_ SIZE_T dwSize,
        _In_ DWORD flNewProtect,
        _Out_ PDWORD lpflOldProtect)
    {
        if (!::IsCurrentProcessHandle(hProcess) ||
            !::IsPageExecute(flNewProtect) ||
            !::CallerUsesDynamicCode(_ReturnAddress()))
        {
            return ::OriginalVirtualProtectEx(
                hProcess,
                lpAddress,
                dwSize,
                flNewProtect,
                lpflOldProtect);
        }
        ::K7BaseSetCurrentThreadDynamicCodePolicyOptOut(MO_TRUE);
        BOOL Result = ::OriginalVirtualProtectEx(
            hProcess,
            lpAddress,
            dwSize,
            flNewProtect,
            lpflOldProtect);
        ::K7BaseSetCurrentThreadDynamicCodePolicyOptOut(MO_FALSE);
        return Result;
    }
}

EXTERN_C BOOL WINAPI NanaZipBlockDlls()
{
    if (::K7BaseGetAllowDynamicCodeGenerationPolicy())
    {
        return TRUE;
    }

    g_FunctionTable[FunctionTypes::NtMapViewOfSection].Original =
        ::GetProcAddress(::GetNtDllModuleHandle(), "NtMapViewOfSection");
    g_FunctionTable[FunctionTypes::NtMapViewOfSection].Detoured =
        ::MyNtMapViewOfSection;

    g_FunctionTable[FunctionTypes::NtQuerySection].Original =
        ::GetProcAddress(::GetNtDllModuleHandle(), "NtQuerySection");
    g_FunctionTable[FunctionTypes::NtQuerySection].Detoured =
        nullptr;

    g_FunctionTable[FunctionTypes::NtUnmapViewOfSection].Original =
        ::GetProcAddress(::GetNtDllModuleHandle(), "NtUnmapViewOfSection");
    g_FunctionTable[FunctionTypes::NtUnmapViewOfSection].Detoured =
        ::MyNtUnmapViewOfSection;

    g_FunctionTable[FunctionTypes::VirtualAlloc].Original =
        ::GetProcAddress(::GetKernel32ModuleHandle(), "VirtualAlloc");
    g_FunctionTable[FunctionTypes::VirtualAlloc].Detoured =
        ::DetouredVirtualAlloc;

    g_FunctionTable[FunctionTypes::VirtualAllocEx].Original =
        ::GetProcAddress(::GetKernel32ModuleHandle(), "VirtualAllocEx");
    g_FunctionTable[FunctionTypes::VirtualAllocEx].Detoured =
        ::DetouredVirtualAllocEx;

    g_FunctionTable[FunctionTypes::VirtualProtect].Original =
        ::GetProcAddress(::GetKernel32ModuleHandle(), "VirtualProtect");
    g_FunctionTable[FunctionTypes::VirtualProtect].Detoured =
        ::DetouredVirtualProtect;

    g_FunctionTable[FunctionTypes::VirtualProtectEx].Original =
        ::GetProcAddress(::GetKernel32ModuleHandle(), "VirtualProtectEx");
    g_FunctionTable[FunctionTypes::VirtualProtectEx].Detoured =
        ::DetouredVirtualProtectEx;

    ::K7BaseDetourTransactionBegin();
    ::K7BaseDetourUpdateThread(::GetCurrentThread());
    for (size_t i = 0; i < FunctionTypes::MaximumFunction; ++i)
    {
        if (g_FunctionTable[i].Original &&
            g_FunctionTable[i].Detoured)
        {
            if (NO_ERROR != ::K7BaseDetourAttach(
                    &g_FunctionTable[i].Original,
                    g_FunctionTable[i].Detoured))
            {
                ::K7BaseDetourTransactionAbort();
                return FALSE;
            }
        }
    }
    ::K7BaseDetourTransactionCommit();
    return TRUE;
}

#else

EXTERN_C BOOL WINAPI NanaZipBlockDlls()
{
    return TRUE;
}

#endif
