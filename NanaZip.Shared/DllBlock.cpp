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

#include <mutex>
#include <array>
#include <vector>
#include <intrin.h>

#include "DllBlock.h"
#include "Mitigations.h"

#ifdef NDEBUG

namespace
{
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
        using FunctionType = decltype(NtMapViewOfSection)*;
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
        using FunctionType = decltype(VirtualAlloc)*;
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
        using FunctionType = decltype(VirtualAllocEx)*;
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
        using FunctionType = decltype(VirtualProtect)*;
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
        using FunctionType = decltype(VirtualProtectEx)*;
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
    enum DllFlags : unsigned int {
        UnknownDll = 0,
        DllNeedsBlocking = 1,
        DllNeedsDynamicCodeOptout = 2,
    };

    static std::vector<std::pair<UINT_PTR, SIZE_T>> dynamic_code_ranges;
    static std::mutex dynamic_code_range_lock;

    using DllType = std::pair<const char*, DllFlags>;
    // Make sure that this list is sorted for _stricmp.
    static const std::array<DllType, 7> DllList = {
        std::make_pair("BaseGUI.dll", DllNeedsDynamicCodeOptout),
        std::make_pair("ExplorerPatcher.amd64.dll", DllNeedsBlocking),
        std::make_pair("ExplorerPatcher.IA-32.dll", DllNeedsBlocking),
        std::make_pair("PrxDrvPE.dll", DllNeedsBlocking),
        std::make_pair("PrxDrvPE64.dll", DllNeedsBlocking),
        std::make_pair("TFMain32.dll", DllNeedsBlocking),
        std::make_pair("TFMain64.dll", DllNeedsBlocking),
    };

    static DllFlags FindDll(const char* dllName)
    {
        for (auto it = DllList.begin(); it != DllList.end(); it++) {
            if (!_stricmp(it->first, dllName)) {
                return it->second;
            }
        }
        return UnknownDll;
    }

    static inline bool CheckExtents(size_t viewSize, size_t offset, size_t size) {
        return offset < viewSize && size <= viewSize && offset + size <= viewSize;
    }

    static bool GetDllExportName(char(&dllName)[256], const char* base, size_t viewSize)
    {
        if (viewSize < sizeof(IMAGE_DOS_HEADER)) {
            return false;
        }
        const IMAGE_DOS_HEADER* dosHdr = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dosHdr->e_magic != IMAGE_DOS_SIGNATURE) {
            return false;
        }
        if (dosHdr->e_lfanew < 0 || !CheckExtents(viewSize, dosHdr->e_lfanew, sizeof(IMAGE_NT_HEADERS))) {
            return false;
        }
        const IMAGE_NT_HEADERS* ntHdr = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dosHdr->e_lfanew);
        if (ntHdr->Signature != IMAGE_NT_SIGNATURE || ntHdr->OptionalHeader.NumberOfRvaAndSizes < IMAGE_DIRECTORY_ENTRY_EXPORT)
            return false;
        const IMAGE_DATA_DIRECTORY* dirExport = &(ntHdr->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT]);
        if (dirExport->Size < sizeof(IMAGE_EXPORT_DIRECTORY) || !dirExport->VirtualAddress || !CheckExtents(viewSize, dirExport->VirtualAddress, sizeof(IMAGE_EXPORT_DIRECTORY))) {
            return false;
        }
        const IMAGE_EXPORT_DIRECTORY* exports = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(base + dirExport->VirtualAddress);
        // we don't know the export directory name size so assume that at least 256 bytes after the name are safe
        if (!exports->Name || !CheckExtents(viewSize, exports->Name, ARRAYSIZE(dllName))) {
            return false;
        }
        const char* name = base + exports->Name;
        if (strncpy_s(dllName, name, _TRUNCATE)) {
            return false;
        }
        return true;
    }

    static bool HandleIsCurrentProcess(HANDLE ProcessHandle) {
        return ProcessHandle == GetCurrentProcess() || GetProcessId(ProcessHandle) == GetCurrentProcessId();
    }

    static bool ProtectionIsExecute(DWORD Protect) {
        return Protect == PAGE_EXECUTE ||
            Protect == PAGE_EXECUTE_READ ||
            Protect == PAGE_EXECUTE_READWRITE ||
            Protect == PAGE_EXECUTE_WRITECOPY;
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
        if (ret < 0 || !HandleIsCurrentProcess(ProcessHandle) || !ProtectionIsExecute(Win32Protect)) {
            return ret;
        }
        SECTION_BASIC_INFORMATION sbi = {};
        status = ::OriginalNtQuerySection(SectionHandle, SectionBasicInformation, &sbi, sizeof(sbi), NULL);
        if (status < 0 || !(sbi.AllocationAttributes & SEC_IMAGE)) {
            return ret;
        }
        char dllName[256] = {};
        if (!GetDllExportName(dllName, reinterpret_cast<const char*>(*BaseAddress), *ViewSize)) {
            return ret;
        }
        DllFlags dllType = FindDll(dllName);
        if (dllType & DllNeedsBlocking) {
            ::OriginalNtUnmapViewOfSection(ProcessHandle, *BaseAddress);
            return STATUS_ACCESS_DENIED;
        }
        else if (dllType & DllNeedsDynamicCodeOptout) {
            std::lock_guard<std::mutex> lock(dynamic_code_range_lock);
            dynamic_code_ranges.push_back(std::make_pair(reinterpret_cast<UINT_PTR>(*BaseAddress), *ViewSize));
        }
        return ret;
    }

    static NTSTATUS NTAPI MyNtUnmapViewOfSection(
        _In_ HANDLE ProcessHandle,
        _In_opt_ PVOID BaseAddress)
    {
        NTSTATUS ret = ::OriginalNtUnmapViewOfSection(ProcessHandle, BaseAddress);
        if (ret < 0 || !HandleIsCurrentProcess(ProcessHandle)) {
            return ret;
        }
        UINT_PTR ptr = reinterpret_cast<UINT_PTR>(BaseAddress);
        {
            std::lock_guard<std::mutex> lock(dynamic_code_range_lock);
            for (auto it = dynamic_code_ranges.begin(); it != dynamic_code_ranges.end(); it++) {
                if (ptr >= it->first && ptr < it->first + it->second) {
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
        std::lock_guard<std::mutex> lock(dynamic_code_range_lock);
        for (auto it = dynamic_code_ranges.begin(); it != dynamic_code_ranges.end(); it++) {
            if (caller >= it->first && caller < it->first + it->second) {
                return true;
            }
        }
        return false;
    }

    static _Ret_maybenull_ _Post_writable_byte_size_(dwSize) LPVOID WINAPI MyVirtualAlloc(
        _In_opt_ LPVOID lpAddress,
        _In_ SIZE_T dwSize,
        _In_ DWORD flAllocationType,
        _In_ DWORD flProtect)
    {
        if (!ProtectionIsExecute(flProtect) ||
            !CallerUsesDynamicCode(_ReturnAddress())) {
            return ::OriginalVirtualAlloc(lpAddress, dwSize, flAllocationType, flProtect);
        }
        // what do we even do if it fails? so, no error checking.
        NanaZipSetThreadDynamicCodeOptout(TRUE);
        LPVOID ret = ::OriginalVirtualAlloc(lpAddress, dwSize, flAllocationType, flProtect);
        NanaZipSetThreadDynamicCodeOptout(FALSE);
        return ret;
    }

    static _Ret_maybenull_ _Post_writable_byte_size_(dwSize) LPVOID WINAPI MyVirtualAllocEx(
        _In_ HANDLE hProcess,
        _In_opt_ LPVOID lpAddress,
        _In_ SIZE_T dwSize,
        _In_ DWORD flAllocationType,
        _In_ DWORD flProtect)
    {
        if (!HandleIsCurrentProcess(hProcess) ||
            !ProtectionIsExecute(flProtect) ||
            !CallerUsesDynamicCode(_ReturnAddress())) {
            return ::OriginalVirtualAllocEx(hProcess, lpAddress, dwSize, flAllocationType, flProtect);
        }
        NanaZipSetThreadDynamicCodeOptout(TRUE);
        LPVOID ret = ::OriginalVirtualAllocEx(hProcess, lpAddress, dwSize, flAllocationType, flProtect);
        NanaZipSetThreadDynamicCodeOptout(FALSE);
        return ret;
    }

    static _Success_(return != FALSE) BOOL WINAPI MyVirtualProtect(
        _In_ LPVOID lpAddress,
        _In_ SIZE_T dwSize,
        _In_ DWORD flNewProtect,
        _Out_ PDWORD lpflOldProtect)
    {
        if (!ProtectionIsExecute(flNewProtect) ||
            !CallerUsesDynamicCode(_ReturnAddress())) {
            return ::OriginalVirtualProtect(lpAddress, dwSize, flNewProtect, lpflOldProtect);
        }
        NanaZipSetThreadDynamicCodeOptout(TRUE);
        BOOL ret = ::OriginalVirtualProtect(lpAddress, dwSize, flNewProtect, lpflOldProtect);
        NanaZipSetThreadDynamicCodeOptout(FALSE);
        return ret;
    }

    static _Success_(return != FALSE) BOOL WINAPI MyVirtualProtectEx(
        _In_ HANDLE hProcess,
        _In_ LPVOID lpAddress,
        _In_ SIZE_T dwSize,
        _In_ DWORD flNewProtect,
        _Out_ PDWORD lpflOldProtect)
    {
        if (!HandleIsCurrentProcess(hProcess) ||
            !ProtectionIsExecute(flNewProtect) ||
            !CallerUsesDynamicCode(_ReturnAddress())) {
            return ::OriginalVirtualProtectEx(hProcess, lpAddress, dwSize, flNewProtect, lpflOldProtect);
        }
        NanaZipSetThreadDynamicCodeOptout(TRUE);
        BOOL ret = ::OriginalVirtualProtectEx(hProcess, lpAddress, dwSize, flNewProtect, lpflOldProtect);
        NanaZipSetThreadDynamicCodeOptout(FALSE);
        return ret;
    }
}

EXTERN_C BOOL WINAPI NanaZipBlockDlls()
{
    if (::K7BaseIsSecurityMitigationPoliciesDisabled())
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
        ::MyVirtualAlloc;

    g_FunctionTable[FunctionTypes::VirtualAllocEx].Original =
        ::GetProcAddress(::GetKernel32ModuleHandle(), "VirtualAllocEx");
    g_FunctionTable[FunctionTypes::VirtualAllocEx].Detoured =
        ::MyVirtualAllocEx;

    g_FunctionTable[FunctionTypes::VirtualProtect].Original =
        ::GetProcAddress(::GetKernel32ModuleHandle(), "VirtualProtect");
    g_FunctionTable[FunctionTypes::VirtualProtect].Detoured =
        ::MyVirtualProtect;

    g_FunctionTable[FunctionTypes::VirtualProtectEx].Original =
        ::GetProcAddress(::GetKernel32ModuleHandle(), "VirtualProtectEx");
    g_FunctionTable[FunctionTypes::VirtualProtectEx].Detoured =
        ::MyVirtualProtectEx;

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
