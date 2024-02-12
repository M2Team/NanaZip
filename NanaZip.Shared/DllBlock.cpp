/*
 * PROJECT:   NanaZip
 * FILE:      DllBlock.cpp
 * PURPOSE:   Implementation for DLL blocker
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: dinhngtu (contact@tudinh.xyz)
 *            MouriNaruto (KurikoMouri@outlook.jp)
 */

#include <Mint.h>

#include <Detours.h>

#include "DllBlock.h"

#ifdef NDEBUG

namespace
{
    static decltype(NtMapViewOfSection)* RealNtMapViewOfSection = nullptr;
    static decltype(NtQuerySection)* RealNtQuerySection = nullptr;
    static decltype(NtUnmapViewOfSection)* RealNtUnmapViewOfSection = nullptr;

    static bool DllNeedsBlocking(const char* dllName) {
        if (!::_stricmp(dllName, "ExplorerPatcher.amd64.dll")) {
            return true;
        }
        else if (!::_stricmp(dllName, "ExplorerPatcher.IA-32.dll")) {
            return true;
        }
        return false;
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
        if (dirExport->Size < sizeof(IMAGE_EXPORT_DIRECTORY) || !CheckExtents(viewSize, dirExport->VirtualAddress, sizeof(IMAGE_EXPORT_DIRECTORY))) {
            return false;
        }
        const IMAGE_EXPORT_DIRECTORY* exports = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(base + dirExport->VirtualAddress);
        // we don't know the export directory name size so assume that at least 256 bytes after the name are safe
        if (!CheckExtents(viewSize, exports->Name, ARRAYSIZE(dllName))) {
            return false;
        }
        const char* name = base + exports->Name;
        if (strcpy_s(dllName, name)) {
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
        if (!RealNtMapViewOfSection)
            return STATUS_ACCESS_DENIED;
        ret = RealNtMapViewOfSection(
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
        if (ret < 0) {
            return ret;
        }
        if (!(Win32Protect == PAGE_EXECUTE ||
            Win32Protect == PAGE_EXECUTE_READ ||
            Win32Protect == PAGE_EXECUTE_READWRITE ||
            Win32Protect == PAGE_EXECUTE_WRITECOPY)) {
            return ret;
        }
        SECTION_BASIC_INFORMATION sbi = { 0 };
        status = RealNtQuerySection(SectionHandle, SectionBasicInformation, &sbi, sizeof(sbi), NULL);
        if (status < 0 || !(sbi.AllocationAttributes & SEC_IMAGE)) {
            return ret;
        }
        char dllName[256] = { 0 };
        if (!GetDllExportName(dllName, reinterpret_cast<const char*>(*BaseAddress), *ViewSize)) {
            return ret;
        }
        bool needsBlocking = DllNeedsBlocking(dllName);
        if (needsBlocking) {
            RealNtUnmapViewOfSection(ProcessHandle, *BaseAddress);
            return STATUS_ACCESS_DENIED;
        }
        return ret;
    }
}

EXTERN_C BOOL WINAPI NanaZipBlockDlls()
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    RealNtMapViewOfSection = static_cast<decltype(NtMapViewOfSection)*>(DetourFindFunction("ntdll.dll", "NtMapViewOfSection"));
    RealNtQuerySection = static_cast<decltype(NtQuerySection)*>(DetourFindFunction("ntdll.dll", "NtQuerySection"));
    RealNtUnmapViewOfSection = static_cast<decltype(NtUnmapViewOfSection)*>(DetourFindFunction("ntdll.dll", "NtUnmapViewOfSection"));
    if (DetourAttach(&RealNtMapViewOfSection, MyNtMapViewOfSection) != NO_ERROR) {
        DetourTransactionAbort();
        return FALSE;
    }
    DetourTransactionCommit();
    return TRUE;
}

#else

EXTERN_C BOOL WINAPI NanaZipBlockDlls()
{
    return TRUE;
}

#endif
