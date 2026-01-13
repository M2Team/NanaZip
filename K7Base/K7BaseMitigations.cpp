/*
 * PROJECT:    NanaZip Platform Base Library (K7Base)
 * FILE:       K7BaseMitigations.cpp
 * PURPOSE:    Implementation for NanaZip Platform Base Mitigations Interfaces
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: dinhngtu (contact@tudinh.xyz)
 *             MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include <Mile.Internal.h>

#include "K7BaseMitigations.h"

#include "K7BaseDetours.h"
#include "K7BasePolicies.h"

#include <intrin.h>

#include <mutex>
#include <vector>

EXTERN_C MO_RESULT MOAPI K7BaseEnableMandatoryMitigations()
{
    {
        PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY Policy = {};
        Policy.RaiseExceptionOnInvalidHandleReference = 1;
        Policy.HandleExceptionsPermanentlyEnabled = 1;
        if (!::SetProcessMitigationPolicy(
            ProcessStrictHandleCheckPolicy,
            &Policy,
            sizeof(PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY)))
        {
            return MO_RESULT_ERROR_FAIL;
        }
    }

    {
        PROCESS_MITIGATION_IMAGE_LOAD_POLICY Policy = {};
        Policy.NoRemoteImages = 1;
        Policy.NoLowMandatoryLabelImages = 1;
        if (!::SetProcessMitigationPolicy(
            ProcessImageLoadPolicy,
            &Policy,
            sizeof(PROCESS_MITIGATION_IMAGE_LOAD_POLICY)))
        {
            return MO_RESULT_ERROR_FAIL;
        }
    }

    return MO_RESULT_SUCCESS_OK;
}

EXTERN_C MO_RESULT MOAPI K7BaseDisableDynamicCodeGeneration()
{
#ifdef NDEBUG
    if (!::K7BaseGetAllowDynamicCodeGenerationPolicy())
    {
        PROCESS_MITIGATION_DYNAMIC_CODE_POLICY Policy = {};
        Policy.ProhibitDynamicCode = 1;
        Policy.AllowThreadOptOut = 1;
        if (!::SetProcessMitigationPolicy(
            ProcessDynamicCodePolicy,
            &Policy,
            sizeof(PROCESS_MITIGATION_DYNAMIC_CODE_POLICY)))
        {
            return MO_RESULT_ERROR_FAIL;
        }
    }
#endif // NDEBUG

    return MO_RESULT_SUCCESS_OK;
}

EXTERN_C MO_RESULT MOAPI K7BaseDisableChildProcessCreation()
{
    if (!::K7BaseGetAllowChildProcessCreationPolicy())
    {
        PROCESS_MITIGATION_CHILD_PROCESS_POLICY Policy = {};
        Policy.NoChildProcessCreation = 1;
        if (!::SetProcessMitigationPolicy(
            ProcessChildProcessPolicy,
            &Policy,
            sizeof(PROCESS_MITIGATION_CHILD_PROCESS_POLICY)))
        {
            return MO_RESULT_ERROR_FAIL;
        }
    }
    return MO_RESULT_SUCCESS_OK;
}

EXTERN_C MO_RESULT MOAPI K7BaseSetCurrentThreadDynamicCodePolicyOptOut(
    _In_ MO_BOOL AllowDynamicCodeGeneration)
{
#ifdef NDEBUG
    if (::K7BaseGetAllowDynamicCodeGenerationPolicy())
    {
        return TRUE;
    }
    DWORD PolicyValue = AllowDynamicCodeGeneration
        ? THREAD_DYNAMIC_CODE_ALLOW
        : 0;
    if (!::SetThreadInformation(
        ::GetCurrentThread(),
        ThreadDynamicCodePolicy,
        &PolicyValue,
        sizeof(DWORD)))
    {
        return MO_RESULT_ERROR_FAIL;
    }
#else
    UNREFERENCED_PARAMETER(AllowDynamicCodeGeneration);
#endif
    return MO_RESULT_SUCCESS_OK;
}

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
    static std::vector<std::pair<MO_UINTN, MO_UINTN>> g_DynamicCodeRanges;

    static bool IsDynamicCodeAllowed(
        _In_ MO_POINTER CallerPointer)
    {
        MO_UINTN CallerAddress = reinterpret_cast<MO_UINTN>(CallerPointer);
        std::lock_guard<std::mutex> Lock(g_DynamicCodeRangeMutex);
        for (auto const& Range : g_DynamicCodeRanges)
        {
            if (CallerAddress >= Range.first &&
                CallerAddress < Range.first + Range.second)
            {
                return true;
            }
        }
        return false;
    }

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

    static inline bool CheckExtents(
        _In_ MO_UINTN ViewSize,
        _In_ MO_UINTN Offset,
        _In_ MO_UINTN Size)
    {
        return (
            Offset < ViewSize &&
            Size <= ViewSize &&
            Offset + Size <= ViewSize);
    }

    static bool GetModuleExportName(
        _In_ MO_CHAR(&ModuleName)[256],
        _In_ MO_STRING Base,
        _In_ MO_UINTN ViewSize)
    {
        if (sizeof(IMAGE_DOS_HEADER) > ViewSize)
        {
            return false;
        }

        PIMAGE_DOS_HEADER DosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(Base);
        if (IMAGE_DOS_SIGNATURE != DosHeader->e_magic)
        {
            return false;
        }
        if (0 > DosHeader->e_lfanew ||
            !::CheckExtents(
                ViewSize,
                DosHeader->e_lfanew,
                sizeof(IMAGE_NT_HEADERS)))
        {
            return false;
        }

        PIMAGE_NT_HEADERS NtHeader = reinterpret_cast<PIMAGE_NT_HEADERS>(
            Base + DosHeader->e_lfanew);
        if (IMAGE_NT_SIGNATURE != NtHeader->Signature ||
            IMAGE_DIRECTORY_ENTRY_EXPORT >
            NtHeader->OptionalHeader.NumberOfRvaAndSizes)
        {
            return false;
        }

        PIMAGE_DATA_DIRECTORY ExportDirectory =
            &(NtHeader->OptionalHeader.DataDirectory[
                IMAGE_DIRECTORY_ENTRY_EXPORT]);
        if (sizeof(IMAGE_EXPORT_DIRECTORY) > ExportDirectory->Size ||
            !ExportDirectory->VirtualAddress ||
            !::CheckExtents(
                ViewSize,
                ExportDirectory->VirtualAddress,
                sizeof(IMAGE_EXPORT_DIRECTORY)))
        {
            return false;
        }

        PIMAGE_EXPORT_DIRECTORY Exports =
            reinterpret_cast<PIMAGE_EXPORT_DIRECTORY>(
                Base + ExportDirectory->VirtualAddress);
        // We don't know the export directory name size so assume that at least
        // 256 bytes after the name are safe.
        if (!Exports->Name ||
            !::CheckExtents(
                ViewSize,
                Exports->Name,
                MO_ARRAY_SIZE(ModuleName)))
        {
            return false;
        }

        const char* Name = Base + Exports->Name;
        if (::strncpy_s(ModuleName, Name, _TRUNCATE))
        {
            return false;
        }

        return true;
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

    static NTSTATUS NTAPI OriginalNtMapViewOfSection(
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

    static NTSTATUS NTAPI OriginalNtQuerySection(
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

    static NTSTATUS NTAPI OriginalNtUnmapViewOfSection(
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

    static LPVOID WINAPI OriginalVirtualAlloc(
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

    static LPVOID WINAPI OriginalVirtualAllocEx(
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

    static BOOL WINAPI OriginalVirtualProtect(
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

    static BOOL WINAPI OriginalVirtualProtectEx(
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

    static NTSTATUS NTAPI DetouredNtMapViewOfSection(
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
        NTSTATUS Status = ::OriginalNtMapViewOfSection(
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
        if (!NT_SUCCESS(Status) ||
            !::IsCurrentProcessHandle(ProcessHandle) ||
            !::IsPageExecute(PageProtection))
        {
            return Status;
        }
        {
            SECTION_BASIC_INFORMATION Information = {};
            if (!NT_SUCCESS(::OriginalNtQuerySection(
                SectionHandle,
                SectionBasicInformation,
                &Information,
                sizeof(SECTION_BASIC_INFORMATION),
                nullptr)) ||
                !(SEC_IMAGE & Information.AllocationAttributes))
            {
                return Status;
            }
        }
        MO_CHAR ModuleName[256] = {};
        if (!::GetModuleExportName(
            ModuleName,
            reinterpret_cast<MO_STRING>(*BaseAddress),
            *ViewSize))
        {
            return Status;
        }
        MO_UINT8 ModuleType = ::QueryModuleType(ModuleName);
        if (ModuleTypes::NeedsBlocking & ModuleType)
        {
            ::OriginalNtUnmapViewOfSection(ProcessHandle, *BaseAddress);
            return STATUS_ACCESS_DENIED;
        }
        else if (ModuleTypes::NeedsDynamicCodeOptout & ModuleType)
        {
            std::lock_guard<std::mutex> Lock(g_DynamicCodeRangeMutex);
            g_DynamicCodeRanges.emplace_back(
                reinterpret_cast<MO_UINTN>(*BaseAddress),
                *ViewSize);
        }
        return Status;
    }

    static NTSTATUS NTAPI DetouredNtUnmapViewOfSection(
        _In_ HANDLE ProcessHandle,
        _In_opt_ PVOID BaseAddress)
    {
        NTSTATUS Status = ::OriginalNtUnmapViewOfSection(
            ProcessHandle,
            BaseAddress);
        if (NT_SUCCESS(Status) &&
            ::IsCurrentProcessHandle(ProcessHandle))
        {
            MO_UINTN BaseStart = reinterpret_cast<MO_UINTN>(BaseAddress);
            std::lock_guard<std::mutex> Lock(g_DynamicCodeRangeMutex);
            for (auto Iterator = g_DynamicCodeRanges.begin();
                Iterator != g_DynamicCodeRanges.end();
                ++Iterator)
            {
                if (BaseStart >= Iterator->first &&
                    BaseStart < Iterator->first + Iterator->second)
                {
                    g_DynamicCodeRanges.erase(Iterator);
                    break;
                }
            }
        }
        return Status;
    }

    static LPVOID WINAPI DetouredVirtualAlloc(
        _In_opt_ LPVOID lpAddress,
        _In_ SIZE_T dwSize,
        _In_ DWORD flAllocationType,
        _In_ DWORD flProtect)
    {
        if (!::IsPageExecute(flProtect) ||
            !::IsDynamicCodeAllowed(::_ReturnAddress()))
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

    static LPVOID WINAPI DetouredVirtualAllocEx(
        _In_ HANDLE hProcess,
        _In_opt_ LPVOID lpAddress,
        _In_ SIZE_T dwSize,
        _In_ DWORD flAllocationType,
        _In_ DWORD flProtect)
    {
        if (!::IsCurrentProcessHandle(hProcess) ||
            !::IsPageExecute(flProtect) ||
            !::IsDynamicCodeAllowed(::_ReturnAddress()))
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

    static BOOL WINAPI DetouredVirtualProtect(
        _In_ LPVOID lpAddress,
        _In_ SIZE_T dwSize,
        _In_ DWORD flNewProtect,
        _Out_ PDWORD lpflOldProtect)
    {
        if (!::IsPageExecute(flNewProtect) ||
            !::IsDynamicCodeAllowed(::_ReturnAddress()))
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

    static BOOL WINAPI DetouredVirtualProtectEx(
        _In_ HANDLE hProcess,
        _In_ LPVOID lpAddress,
        _In_ SIZE_T dwSize,
        _In_ DWORD flNewProtect,
        _Out_ PDWORD lpflOldProtect)
    {
        if (!::IsCurrentProcessHandle(hProcess) ||
            !::IsPageExecute(flNewProtect) ||
            !::IsDynamicCodeAllowed(::_ReturnAddress()))
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
#endif // NDEBUG

EXTERN_C MO_RESULT MOAPI K7BaseInitializeDynamicLinkLibraryBlocker()
{
#ifdef NDEBUG
    if (::K7BaseGetAllowDynamicCodeGenerationPolicy())
    {
        return MO_RESULT_SUCCESS_OK;
    }

    g_FunctionTable[FunctionTypes::NtMapViewOfSection].Original =
        ::GetProcAddress(::GetNtDllModuleHandle(), "NtMapViewOfSection");
    g_FunctionTable[FunctionTypes::NtMapViewOfSection].Detoured =
        ::DetouredNtMapViewOfSection;

    g_FunctionTable[FunctionTypes::NtQuerySection].Original =
        ::GetProcAddress(::GetNtDllModuleHandle(), "NtQuerySection");
    g_FunctionTable[FunctionTypes::NtQuerySection].Detoured =
        nullptr;

    g_FunctionTable[FunctionTypes::NtUnmapViewOfSection].Original =
        ::GetProcAddress(::GetNtDllModuleHandle(), "NtUnmapViewOfSection");
    g_FunctionTable[FunctionTypes::NtUnmapViewOfSection].Detoured =
        ::DetouredNtUnmapViewOfSection;

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
                return MO_RESULT_ERROR_FAIL;
            }
        }
    }
    ::K7BaseDetourTransactionCommit();
#endif // NDEBUG

    return MO_RESULT_SUCCESS_OK;
}
