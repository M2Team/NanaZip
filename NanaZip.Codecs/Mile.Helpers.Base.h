/*
 * PROJECT:    Mouri Internal Library Essentials
 * FILE:       Mile.Helpers.Base.h
 * PURPOSE:    Definition for the essential Windows helper functions
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#pragma once

#ifndef MILE_WINDOWS_HELPERS_BASE
#define MILE_WINDOWS_HELPERS_BASE

#include <Windows.h>

/**
 * @brief Allocates a block of memory from the default heap of the calling
 *        process. The allocated memory will be initialized to zero. The
 *        allocated memory is not movable.
 * @param Size The number of bytes to be allocated.
 * @return If the function succeeds, the return value is a pointer to the
 *         allocated memory block. If the function fails, the return value is
 *         nullptr.
*/
EXTERN_C LPVOID WINAPI MileAllocateMemory(
    _In_ SIZE_T Size);

/**
 * @brief Reallocates a block of memory from the default heap of the calling
 *        process. If the reallocation request is for a larger size, the
 *        additional region of memory beyond the original size be initialized
 *        to zero. This function enables you to resize a memory block and
 *        change other memory block properties. The allocated memory is not
 *        movable.
 * @param Block A pointer to the block of memory that the function reallocates.
 *              This pointer is returned by an earlier call to
 *              MileAllocateMemory and MileReallocateMemory function.
 * @param Size The new size of the memory block, in bytes. A memory block's
 *             size can be increased or decreased by using this function.
 * @return If the function succeeds, the return value is a pointer to the
 *         reallocated memory block. If the function fails, the return value is
 *         nullptr.
*/
EXTERN_C LPVOID WINAPI MileReallocateMemory(
    _In_ PVOID Block,
    _In_ SIZE_T Size);

/**
 * @brief Frees a memory block allocated from a heap by the MileAllocateMemory
 *        and MileReallocateMemory function.
 * @param Block A pointer to the memory block to be freed. This pointer is
 *        returned by the Allocate or Reallocate method. If this pointer is
 *        nullptr, the behavior is undefined.
 * @return If the function succeeds, the return value is nonzero. If the
 *         function fails, the return value is zero. An application can call
 *         GetLastError for extended error information.
*/
EXTERN_C BOOL WINAPI MileFreeMemory(
    _In_ LPVOID Block);

/**
 * @brief Returns version information about the currently running operating
 *        system.
 * @param VersionInformation Pointer to either a OSVERSIONINFOW structure or a
 *                           OSVERSIONINFOEXW structure that contains the
 *                           version information about the currently running
 *                           operating system. A caller specifies which input
 *                           structure is used by setting the
 *                           dwOSVersionInfoSize member of the structure to the
 *                           size in bytes of the structure that is used.
 * @return TRUE if the function succeeds; otherwise, FALSE.
*/
EXTERN_C BOOL WINAPI MileGetWindowsVersion(
    _Inout_ LPOSVERSIONINFOW VersionInformation);

/**
 * @brief Indicates if the current OS version matches, or is greater than, the
 *        provided version information.
 * @param Major The major version number of the operating system.
 * @param Minor The minor version number of the operating system.
 * @param Build The build number of the operating system.
 * @return TRUE if the specified version matches, or is greater than, the
 *         version of the current Windows operating system; otherwise, FALSE.
*/
EXTERN_C BOOL WINAPI MileIsWindowsVersionAtLeast(
    _In_ DWORD Major,
    _In_ DWORD Minor,
    _In_ DWORD Build);

/**
 * @brief Retrieves the number of milliseconds that have elapsed since the
 *        system was started.
 * @return The number of milliseconds.
*/
EXTERN_C ULONGLONG WINAPI MileGetTickCount();

/**
 * @brief Creates a thread to execute within the virtual address space of the
 *        calling process.
 * @param lpThreadAttributes A pointer to a SECURITY_ATTRIBUTES structure that
 *                           determines whether the returned handle can be
 *                           inherited by child processes.
 * @param dwStackSize The initial size of the stack, in bytes.
 * @param lpStartAddress A pointer to the application-defined function to be
 *                       executed by the thread.
 * @param lpParameter A pointer to a variable to be passed to the thread.
 * @param dwCreationFlags The flags that control the creation of the thread.
 * @param lpThreadId A pointer to a variable that receives the thread
 *                   identifier.
 * @return If the function succeeds, the return value is a handle to the new
 *         thread. If the function fails, the return value is nullptr. To get
 *         extended error information, call GetLastError.
 * @remark For more information, see CreateThread.
*/
EXTERN_C HANDLE WINAPI MileCreateThread(
    _In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
    _In_ SIZE_T dwStackSize,
    _In_ LPTHREAD_START_ROUTINE lpStartAddress,
    _In_opt_ LPVOID lpParameter,
    _In_ DWORD dwCreationFlags,
    _Out_opt_ LPDWORD lpThreadId);

/**
 * @brief Retrieves the number of logical processors in the current group.
 * @return The number of logical processors in the current group.
*/
EXTERN_C DWORD WINAPI MileGetNumberOfHardwareThreads();

/**
 * @brief Loads the specified module in the system directory into the address
 *        space of the calling process. The specified module may cause other
 *        modules to be loaded.
 * @param lpLibFileName The name of the module. This can be either a
 *                      library module (a .dll file) or an executable
 *                      module (an .exe file). The name specified is the
 *                      file name of the module and is not related to the
 *                      name stored in the library module itself, as
 *                      specified by the LIBRARY keyword in the
 *                      module-definition (.def) file.
 *                      If the string specifies a full path, the function
 *                      searches only that path for the module.
 *                      If the string specifies a relative path or a module
 *                      name without a path, the function uses a standard
 *                      search strategy to find the module.
 *                      If the function cannot find the module, the
 *                      function fails. When specifying a path, be sure to
 *                      use backslashes (\), not forward slashes (/).
 *                      If the string specifies a module name without a
 *                      path and the file name extension is omitted, the
 *                      function appends the default library extension .dll
 *                      to the module name. To prevent the function from
 *                      appending .dll to the module name, include a
 *                      trailing point character (.) in the module name
 *                      string.
 * @return If the function succeeds, the return value is a handle to the
 *         module. If the function fails, the return value is nullptr. To get
 *         extended error information, call GetLastError.
*/
EXTERN_C HMODULE WINAPI MileLoadLibraryFromSystem32(
    _In_ LPCWSTR lpLibFileName);

/**
 * @brief Indicates if the current process is elevated.
 * @return TRUE if the current process is elevated; otherwise, FALSE.
*/
EXTERN_C BOOL WINAPI MileIsCurrentProcessElevated();

/**
 * @brief Starts a service if not started and retrieves the current status of
 *        the specified service.
 * @param ServiceHandle A handle to the service. This handle is returned by the
 *                      OpenService or CreateService function, and it must have
 *                      the SERVICE_START access right.
 * @param NumServiceArgs The number of strings in the ServiceArgVectors array.
 *                       If ServiceArgVectors is nullptr, this parameter can be
 *                       zero.
 * @param ServiceArgVectors The null-terminated strings to be passed to the
 *                          ServiceMain function for the service as arguments.
 *                          If there are no arguments, this parameter can be
 *                          nullptr. Otherwise, the first argument
 *                          (ServiceArgVectors[0]) is the name of the service,
 *                          followed by any additional arguments
 *                          (ServiceArgVectors[1] through
 *                          ServiceArgVectors[NumServiceArgs-1]).
 *                          Driver services do not receive these arguments.
 * @param ServiceStatus A pointer to the process status information for a
 *                      service.
 * @return If the function succeeds, the return value is TRUE. If the function
 *         fails, the return value is FALSE. To get extended error information,
 *         call GetLastError.
 */
EXTERN_C BOOL WINAPI MileStartServiceByHandle(
    _In_ SC_HANDLE ServiceHandle,
    _In_ DWORD NumServiceArgs,
    _In_opt_ LPCWSTR* ServiceArgVectors,
    _Out_ LPSERVICE_STATUS_PROCESS ServiceStatus);

/**
 * @brief Stops a service if started and retrieves the current status of the
 *        specified service.
 * @param ServiceHandle A handle to the service. This handle is returned by the
 *                      OpenService or CreateService function, and it must have
 *                      the SERVICE_STOP access right.
 * @param ServiceStatus A pointer to the process status information for a
 *                      service.
 * @return If the function succeeds, the return value is TRUE. If the function
 *         fails, the return value is FALSE. To get extended error information,
 *         call GetLastError.
 */
EXTERN_C BOOL WINAPI MileStopServiceByHandle(
    _In_ SC_HANDLE ServiceHandle,
    _Out_ LPSERVICE_STATUS_PROCESS ServiceStatus);

/**
 * @brief Starts a service if not started and retrieves the current status of
 *        the specified service.
 * @param ServiceName The name of the service to be started. This is the name
 *                    specified by the ServiceName parameter of the
 *                    CreateService function when the service object was
 *                    created, not the service display name that is shown by
 *                    user interface applications to identify the service. The
 *                    maximum string length is 256 characters. The service
 *                    control manager database preserves the case of the
 *                    characters, but service name comparisons are always case
 *                    insensitive. Forward-slash (/) and backslash (\\) are
 *                    invalid service name characters.
 * @param ServiceStatus A pointer to the process status information for a
 *                      service.
 * @return If the function succeeds, the return value is TRUE. If the function
 *         fails, the return value is FALSE. To get extended error information,
 *         call GetLastError.
*/
EXTERN_C BOOL WINAPI MileStartService(
    _In_ LPCWSTR ServiceName,
    _Out_ LPSERVICE_STATUS_PROCESS ServiceStatus);

/**
 * @brief Stops a service if started and retrieves the current status of the
 *        specified service.
 * @param ServiceName The name of the service to be stopped. This is the name
 *                    specified by the ServiceName parameter of the
 *                    CreateService function when the service object was
 *                    created, not the service display name that is shown by
 *                    user interface applications to identify the service. The
 *                    maximum string length is 256 characters. The service
 *                    control manager database preserves the case of the
 *                    characters, but service name comparisons are always case
 *                    insensitive. Forward-slash (/) and backslash (\\) are
 *                    invalid service name characters.
 * @param ServiceStatus A pointer to the process status information for a
 *                      service.
 * @return If the function succeeds, the return value is TRUE. If the function
 *         fails, the return value is FALSE. To get extended error information,
 *         call GetLastError.
 */
EXTERN_C BOOL WINAPI MileStopService(
    _In_ LPCWSTR ServiceName,
    _Out_ LPSERVICE_STATUS_PROCESS ServiceStatus);

/**
 * @brief The information about a found file or directory queried from the
 *        file enumerator.
*/
typedef struct _MILE_FILE_ENUMERATE_INFORMATION
{
    FILETIME CreationTime;
    FILETIME LastAccessTime;
    FILETIME LastWriteTime;
    FILETIME ChangeTime;
    UINT64 FileSize;
    UINT64 AllocationSize;
    DWORD FileAttributes;
    DWORD EaSize;
    LARGE_INTEGER FileId;
    WCHAR ShortName[16];
    WCHAR FileName[256];
} MILE_FILE_ENUMERATE_INFORMATION, *PMILE_FILE_ENUMERATE_INFORMATION;

/**
 * @brief The file enumerate callback type.
 * @param Information The file enumerate information.
 * @param Context The user context.
 * @return If the return value is non-zero, the file enumerate will be
 *         continued. If the return value is zero, the file enumerate
 *         will be terminated.
*/
typedef BOOL(WINAPI* MILE_ENUMERATE_FILE_CALLBACK_TYPE)(
    _In_ PMILE_FILE_ENUMERATE_INFORMATION Information,
    _In_opt_ LPVOID Context);

/**
 * @brief Enumerates files in a directory.
 * @param FileHandle The handle of the file to be searched a directory for a
 *                   file or subdirectory with a name. This handle must be
 *                   opened with the appropriate permissions for the requested
 *                   change. This handle should not be a pipe handle.
 * @param Callback The file enumerate callback.
 * @param Context The user context.
 * @return If the function succeeds, the return value is TRUE. If the function
 *         fails, the return value is FALSE. To get extended error information,
 *         call GetLastError.
*/
EXTERN_C BOOL WINAPI MileEnumerateFileByHandle(
    _In_ HANDLE FileHandle,
    _In_ MILE_ENUMERATE_FILE_CALLBACK_TYPE Callback,
    _In_opt_ LPVOID Context);

/**
 * @brief Sends a control code directly to a specified device driver, causing
 *        the corresponding device to perform the corresponding operation.
 * @param DeviceHandle A handle to the device on which the operation is to be
 *                     performed. The device is typically a volume, directory,
 *                     file, or stream. To retrieve a device handle, use the
 *                     CreateFile function.
 * @param IoControlCode The control code for the operation. This value
 *                      identifies the specific operation to be performed
 *                      and the type of device on which to perform it.
 * @param InputBuffer A pointer to the input buffer that contains the data
 *                    required to perform the operation. The format of this data
 *                    depends on the value of the IoControlCode parameter. This
 *                    parameter can be nullptr if IoControlCode specifies an
 *                    operation that does not require input data.
 * @param InputBufferSize The size of the input buffer, in bytes.
 * @param OutputBuffer A pointer to the output buffer that is to receive the
 *                     data returned by the operation. The format of this data
 *                     depends on the value of the IoControlCode parameter.
 *                     This parameter can be nullptr if IoControlCode specifies
 *                     an operation that does not return data.
 * @param OutputBufferSize The size of the output buffer, in bytes.
 * @param BytesReturned A pointer to a variable that receives the size of the
 *                      data stored in the output buffer, in bytes. If the
 *                      output buffer is too small to receive any data, the call
 *                      fails, GetLastError returns ERROR_INSUFFICIENT_BUFFER,
 *                      and BytesReturned is zero. If the output buffer is too
 *                      small to hold all of the data but can hold some entries,
 *                      some drivers will return as much data as fits. In this
 *                      case, the call fails, GetLastError returns
 *                      ERROR_MORE_DATA, and BytesReturned indicates the amount
 *                      of data received. Your application should call
 *                      DeviceIoControl again with the same operation,
 *                      specifying a new starting point.
 * @return If the function succeeds, the return value is nonzero. If the
 *         function fails, the return value is zero. To get extended error
 *         information, call GetLastError.
 * @remark For more information, see DeviceIoControl.
*/
EXTERN_C BOOL WINAPI MileDeviceIoControl(
    _In_ HANDLE DeviceHandle,
    _In_ DWORD IoControlCode,
    _In_opt_ LPVOID InputBuffer,
    _In_ DWORD InputBufferSize,
    _Out_opt_ LPVOID OutputBuffer,
    _In_ DWORD OutputBufferSize,
    _Out_opt_ LPDWORD BytesReturned);

/**
 * @brief Retrieves file system attributes for a specified file or directory.
 * @param FileHandle A handle to the file that contains the information to be
 *                   retrieved. This handle should not be a pipe handle.
 * @param FileAttributes The attributes of the specified file or directory. For
 *                       a list of attribute values and their descriptions, see
 *                       File Attribute Constants. If the function fails, the
 *                       return value is INVALID_FILE_ATTRIBUTES.
 * @return If the function succeeds, the return value is nonzero. If the
 *         function fails, the return value is zero. To get extended error
 *         information, call GetLastError.
*/
EXTERN_C BOOL WINAPI MileGetFileAttributesByHandle(
    _In_ HANDLE FileHandle,
    _Out_ PDWORD FileAttributes);

/**
 * @brief Sets the attributes for a file or directory.
 * @param FileHandle A handle to the file for which to change information. This
 *                   handle must be opened with the appropriate permissions for
 *                   the requested change. This handle should not be a pipe
 *                   handle.
 * @param FileAttributes The file attributes to set for the file. This parameter
 *                       can be one or more values, combined using the bitwise -
 *                       OR operator. However, all other values override
 *                       FILE_ATTRIBUTE_NORMAL. For more information, see the
 *                       SetFileAttributes function.
 * @return If the function succeeds, the return value is nonzero. If the
 *         function fails, the return value is zero. To get extended error
 *         information, call GetLastError.
*/
EXTERN_C BOOL WINAPI MileSetFileAttributesByHandle(
    _In_ HANDLE FileHandle,
    _In_ DWORD FileAttributes);

/**
 * @brief Retrieves hardlink count for a specified file.
 * @param FileHandle A handle to the file that contains the information to be
 *                   retrieved. This handle should not be a pipe handle.
 * @param HardlinkCount The hardlink count for the file.
 * @return If the function succeeds, the return value is nonzero. If the
 *         function fails, the return value is zero. To get extended error
 *         information, call GetLastError.
*/
EXTERN_C BOOL WINAPI MileGetFileHardlinkCountByHandle(
    _In_ HANDLE FileHandle,
    _Out_ PDWORD HardlinkCount);

/**
 * @brief Deletes an existing file.
 * @param FileHandle The handle of the file to be deleted. This handle must be
 *                   opened with the appropriate permissions for the requested
 *                   change. This handle should not be a pipe handle.
 * @return If the function succeeds, the return value is nonzero. If the
 *         function fails, the return value is zero. To get extended error
 *         information, call GetLastError.
*/
EXTERN_C BOOL WINAPI MileDeleteFileByHandle(
    _In_ HANDLE FileHandle);

/**
 * @brief Deletes an existing file, even the file have the readonly attribute.
 * @param FileHandle The handle of the file to be deleted. This handle must be
 *                   opened with the appropriate permissions for the requested
 *                   change. This handle should not be a pipe handle.
 * @return If the function succeeds, the return value is nonzero. If the
 *         function fails, the return value is zero. To get extended error
 *         information, call GetLastError.
*/
EXTERN_C BOOL WINAPI MileDeleteFileIgnoreReadonlyAttributeByHandle(
    _In_ HANDLE FileHandle);

/**
 * @brief Retrieves the size of the specified file.
 * @param FileHandle A handle to the file that contains the information to be
 *                   retrieved. This handle should not be a pipe handle.
 * @param FileSize A pointer to a ULONGLONG value that receives the file size,
 *                 in bytes.
 * @return If the function succeeds, the return value is nonzero. If the
 *         function fails, the return value is zero. To get extended error
 *         information, call GetLastError.
*/
EXTERN_C BOOL WINAPI MileGetFileSizeByHandle(
    _In_ HANDLE FileHandle,
    _Out_ PULONGLONG FileSize);

/**
 * @brief Retrieves the amount of space that is allocated for the file.
 * @param FileHandle A handle to the file that contains the information to be
 *                   retrieved. This handle should not be a pipe handle.
 * @param AllocationSize A pointer to a ULONGLONG value that receives the amount
 *                       of space that is allocated for the file, in bytes.
 * @return If the function succeeds, the return value is nonzero. If the
 *         function fails, the return value is zero. To get extended error
 *         information, call GetLastError.
*/
EXTERN_C BOOL WINAPI MileGetFileAllocationSizeByHandle(
    _In_ HANDLE FileHandle,
    _Out_ PULONGLONG AllocationSize);

/**
 * @brief Retrieves the actual number of bytes of disk storage used to store a
 *        specified file. If the file is located on a volume that supports
 *        compression and the file is compressed, the value obtained is the
 *        compressed size of the specified file. If the file is located on a
 *        volume that supports sparse files and the file is a sparse file, the
 *        value obtained is the sparse size of the specified file.
 * @param FileHandle A handle to the file that contains the information to be
 *                   retrieved. This handle should not be a pipe handle.
 * @param CompressedFileSize A pointer to a ULONGLONG value that receives the
 *                           compressed file size, in bytes.
 * @return If the function succeeds, the return value is nonzero. If the
 *         function fails, the return value is zero. To get extended error
 *         information, call GetLastError.
*/
EXTERN_C BOOL WINAPI MileGetCompressedFileSizeByHandle(
    _In_ HANDLE FileHandle,
    _Out_ PULONGLONG CompressedFileSize);

/**
 * @brief Reads data from the specified file or input/output (I/O) device. Reads
 *        occur at the position specified by the file pointer if supported by
 *        the device. This function is adapted for both synchronous and
 *        asynchronous operation.
 * @param FileHandle A handle to the device (for example, a file, file stream,
 *                   physical disk, volume, console buffer, tape drive, socket,
 *                   communications resource, mailslot, or pipe). The FileHandle
 *                   parameter must have been created with read access
 * @param Buffer A pointer to the buffer that receives the data read from a file
 *               or device.
 * @param NumberOfBytesToRead The maximum number of bytes to be read.
 * @param NumberOfBytesRead A pointer to the variable that receives the number
 *                          of bytes read.
 * @return If the function succeeds, the return value is nonzero. If the
 *         function fails, the return value is zero. To get extended error
 *         information, call GetLastError.
*/
EXTERN_C BOOL WINAPI MileReadFile(
    _In_ HANDLE FileHandle,
    _Out_opt_ LPVOID Buffer,
    _In_ DWORD NumberOfBytesToRead,
    _Out_opt_ LPDWORD NumberOfBytesRead);

/**
 * @brief Writes data to the specified file or input/output (I/O) device. This
 *        function is adapted for both synchronous and asynchronous operation.
 * @param FileHandle A handle to the file or I/O device (for example, a file,
 *                   file stream, physical disk, volume, console buffer, tape
 *                   drive, socket, communications resource, mailslot, or pipe).
 *                   The FileHandle parameter must have been created with the
 *                   write access.
 * @param Buffer A pointer to the buffer containing the data to be written to
 *               the file or device.
 * @param NumberOfBytesToWrite The number of bytes to be written to the file or
 *                             device. A value of zero specifies a null write
 *                             operation. The behavior of a null write operation
 *                             depends on the underlying file system or
 *                             communications technology.
 * @param NumberOfBytesWritten A pointer to the variable that receives the
 *                             number of bytes written.
 * @return If the function succeeds, the return value is nonzero. If the
 *         function fails, the return value is zero. To get extended error
 *         information, call GetLastError.
*/
EXTERN_C BOOL WINAPI MileWriteFile(
    _In_ HANDLE FileHandle,
    _In_opt_ LPCVOID Buffer,
    _In_ DWORD NumberOfBytesToWrite,
    _Out_opt_ LPDWORD NumberOfBytesWritten);

/**
 * @brief Gets the NTFS compression attribute.
 * @param FileHandle A handle to the file or directory on which the
 *                   operation is to be performed. To retrieve a handle,
 *                   use the CreateFile or a similar API.
 * @param CompressionAlgorithm Specifies the compression algorithm that is
 *                             used to compress this file. Currently defined
 *                             algorithms are:
 *                             COMPRESSION_FORMAT_NONE
 *                                 Uncompress the file or directory.
 *                             COMPRESSION_FORMAT_DEFAULT
 *                                 Compress the file or directory, using the
 *                                 default compression format.
 *                             COMPRESSION_FORMAT_LZNT1
 *                                 Compress the file or directory, using the
 *                                 LZNT1 compression format.
 * @return If the function succeeds, the return value is nonzero. If the
 *         function fails, the return value is zero. To get extended error
 *         information, call GetLastError.
*/
EXTERN_C BOOL WINAPI MileGetNtfsCompressionAttributeByHandle(
    _In_ HANDLE FileHandle,
    _Out_ PUSHORT CompressionAlgorithm);

/**
 * @brief Sets the NTFS compression attribute.
 * @param FileHandle A handle to the file or directory on which the
 *                   operation is to be performed. To retrieve a handle,
 *                   use the CreateFile or a similar API.
 * @param CompressionAlgorithm Specifies the compression algorithm that is
 *                             used to compress this file. Currently defined
 *                             algorithms are:
 *                             COMPRESSION_FORMAT_NONE
 *                                 Uncompress the file or directory.
 *                             COMPRESSION_FORMAT_DEFAULT
 *                                 Compress the file or directory, using the
 *                                 default compression format.
 *                             COMPRESSION_FORMAT_LZNT1
 *                                 Compress the file or directory, using the
 *                                 LZNT1 compression format.
 * @return If the function succeeds, the return value is nonzero. If the
 *         function fails, the return value is zero. To get extended error
 *         information, call GetLastError.
*/
EXTERN_C BOOL WINAPI MileSetNtfsCompressionAttributeByHandle(
    _In_ HANDLE FileHandle,
    _In_ USHORT CompressionAlgorithm);

/**
 * @brief Gets the Windows Overlay Filter file compression attribute.
 * @param FileHandle A handle to the file on which the operation is to be
 *                   performed. To retrieve a handle, use the CreateFile
 *                   or a similar API.
 * @param CompressionAlgorithm Specifies the compression algorithm that is
 *                             used to compress this file. Currently defined
 *                             algorithms are:
 *                             FILE_PROVIDER_COMPRESSION_XPRESS4K
 *                                 Indicates that the data for the file should
 *                                 be compressed in 4kb chunks with the XPress
 *                                 algorithm. This algorithm is designed to be
 *                                 computationally lightweight, and provides
 *                                 for rapid access to data.
 *                             FILE_PROVIDER_COMPRESSION_LZX
 *                                 Indicates that the data for the file should
 *                                 be compressed in 32kb chunks with the LZX
 *                                 algorithm. This algorithm is designed to be
 *                                 highly compact, and provides for small
 *                                 footprint for infrequently accessed data.
 *                             FILE_PROVIDER_COMPRESSION_XPRESS8K
 *                                 Indicates that the data for the file should
 *                                 be compressed in 8kb chunks with the XPress
 *                                 algorithm.
 *                             FILE_PROVIDER_COMPRESSION_XPRESS16K
 *                                 Indicates that the data for the file should
 *                                 be compressed in 16kb chunks with the XPress
 *                                 algorithm.
 * @return If the function succeeds, the return value is nonzero. If the
 *         function fails, the return value is zero. To get extended error
 *         information, call GetLastError.
*/
EXTERN_C BOOL WINAPI MileGetWofFileCompressionAttributeByHandle(
    _In_ HANDLE FileHandle,
    _Out_ PDWORD CompressionAlgorithm);

/**
 * @brief Sets the Windows Overlay Filter file compression attribute.
 * @param FileHandle A handle to the file on which the operation is to be
 *                   performed. To retrieve a handle, use the CreateFile
 *                   or a similar API.
 * @param CompressionAlgorithm Specifies the compression algorithm that is
 *                             used to compress this file. Currently defined
 *                             algorithms are:
 *                             FILE_PROVIDER_COMPRESSION_XPRESS4K
 *                                 Indicates that the data for the file should
 *                                 be compressed in 4kb chunks with the XPress
 *                                 algorithm. This algorithm is designed to be
 *                                 computationally lightweight, and provides
 *                                 for rapid access to data.
 *                             FILE_PROVIDER_COMPRESSION_LZX
 *                                 Indicates that the data for the file should
 *                                 be compressed in 32kb chunks with the LZX
 *                                 algorithm. This algorithm is designed to be
 *                                 highly compact, and provides for small
 *                                 footprint for infrequently accessed data.
 *                             FILE_PROVIDER_COMPRESSION_XPRESS8K
 *                                 Indicates that the data for the file should
 *                                 be compressed in 8kb chunks with the XPress
 *                                 algorithm.
 *                             FILE_PROVIDER_COMPRESSION_XPRESS16K
 *                                 Indicates that the data for the file should
 *                                 be compressed in 16kb chunks with the XPress
 *                                 algorithm.
 * @return If the function succeeds, the return value is nonzero. If the
 *         function fails, the return value is zero. To get extended error
 *         information, call GetLastError.
*/
EXTERN_C BOOL WINAPI MileSetWofFileCompressionAttributeByHandle(
    _In_ HANDLE FileHandle,
    _In_ DWORD CompressionAlgorithm);

/**
 * @brief Removes the Windows Overlay Filter file compression attribute.
 * @param FileHandle A handle to the file on which the operation is to be
 *                   performed. To retrieve a handle, use the CreateFile
 *                   or a similar API.
 * @return If the function succeeds, the return value is nonzero. If the
 *         function fails, the return value is zero. To get extended error
 *         information, call GetLastError.
*/
EXTERN_C BOOL WINAPI MileRemoveWofFileCompressionAttributeByHandle(
    _In_ HANDLE FileHandle);

/**
 * @brief The resource info struct.
*/
typedef struct _MILE_RESOURCE_INFO
{
    DWORD Size;
    LPVOID Pointer;
} MILE_RESOURCE_INFO, *PMILE_RESOURCE_INFO;

/**
 * @brief Obtain the best matching resource with the specified type and name in
 *        the specified module.
 * @param ResourceInfo The resource info which contains the pointer and size.
 * @param ModuleHandle A handle to the module whose portable executable file or
 *                     an accompanying MUI file contains the resource. If this
 *                     parameter is nullptr, the function searches the module
 *                     used to create the current process.
 * @param Type The resource type. Alternately, rather than a pointer, this
 *             parameter can be MAKEINTRESOURCE(ID), where ID is the integer
 *             identifier of the given resource type.
 * @param Name The name of the resource. Alternately, rather than a pointer,
 *             this parameter can be MAKEINTRESOURCE(ID), where ID is the
 *             integer identifier of the resource.
 * @param Language The language of the resource. If this parameter is
 *                 MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), the current
 *                 language associated with the calling thread is used. To
 *                 specify a language other than the current language, use
 *                 the MAKELANGID macro to create this parameter.
 * @return If the function succeeds, the return value is TRUE. If the function
 *         fails, the return value is FALSE. To get extended error information,
 *         call GetLastError.
*/
EXTERN_C BOOL WINAPI MileLoadResource(
    _Out_ PMILE_RESOURCE_INFO ResourceInfo,
    _In_opt_ HMODULE ModuleHandle,
    _In_ LPCWSTR Type,
    _In_ LPCWSTR Name,
    _In_ WORD Language);

/**
 * @brief Creates or opens a file, directory or drive with long path support.
 *        The function returns a handle that can be used to access the file,
 *        directory or drive depending on the flags and attributes specified.
 * @param FileName The name of the file or device to be created or opened. You
 *                 may use either forward slashes (/) or backslashes (\\) in
 *                 this name.
 * @param DesiredAccess The requested access to the file or device, which can
 *                      be summarized as read, write, both or neither zero).
 * @param ShareMode The requested sharing mode of the file or device, which can
 *                  be read, write, both, delete, all of these, or none (refer
 *                  to the following table). Access requests to attributes or
 *                  extended attributes are not affected by this flag.
 * @param SecurityAttributes A pointer to a SECURITY_ATTRIBUTES structure that
 *                           contains two separate but related data members: an
 *                           optional security descriptor, and a Boolean value
 *                           that determines whether the returned handle can be
 *                           inherited by child processes. This parameter can be
 *                           nullptr.
 * @param CreationDisposition An action to take on a file or device that exists
 *                            or does not exist.
 * @param FlagsAndAttributes The file or device attributes and flags,
 *                           FILE_ATTRIBUTE_NORMAL being the most common default
 *                           value for files.
 * @param TemplateFile A valid handle to a template file with the GENERIC_READ
 *                     access right. The template file supplies file attributes
 *                     and extended attributes for the file that is being
 *                     created. This parameter can be nullptr.
 * @return If the function succeeds, the return value is an open handle to the
 *         specified file, directory or drive. If the function fails, the return
 *         value is INVALID_HANDLE_VALUE. To get extended error information,
 *         call GetLastError.
 * @remark For more information, see CreateFileW.
*/
EXTERN_C HANDLE WINAPI MileCreateFile(
    _In_ LPCWSTR FileName,
    _In_ DWORD DesiredAccess,
    _In_ DWORD ShareMode,
    _In_opt_ LPSECURITY_ATTRIBUTES SecurityAttributes,
    _In_ DWORD CreationDisposition,
    _In_ DWORD FlagsAndAttributes,
    _In_opt_ HANDLE TemplateFile);

/**
 * @brief Deletes an existing file, even the file have the readonly attribute.
 * @param FileName The name of the file to be deleted. You may use either
 *                 forward slashes (/) or backslashes (\\) in this name.
 * @return If the function succeeds, the return value is nonzero. If the
 *         function fails, the return value is zero. To get extended error
 *         information, call GetLastError.
*/
EXTERN_C BOOL WINAPI MileDeleteFileIgnoreReadonlyAttribute(
    _In_ LPCWSTR FileName);

/**
 * @brief Tests for the current directory and parent directory markers while
 *        iterating through files.
 * @param Name The name of the file or directory for testing.
 * @return Nonzero if the found file has the name "." or "..", which indicates
 *         that the found file is actually a directory. Otherwise zero.
*/
EXTERN_C BOOL WINAPI MileIsDotsName(
    _In_ LPCWSTR Name);

/**
 * @brief Creates a new directory. If one or more of the intermediate
 *        directories do not exist, it creates them.
 * @param PathName The path of the directory to be created. You may use either
 *                 forward slashes (/) or backslashes (\\) in this name.
 * @return If the function succeeds, the return value is nonzero. If the
 *         function fails, the return value is zero. To get extended error
 *         information, call GetLastError.
*/
EXTERN_C BOOL WINAPI MileCreateDirectory(
    _In_ LPCWSTR PathName);

#ifdef _WINSOCK2API_

/**
 * @brief Receives data from a connected socket or a bound connectionless
 *        socket.
 * @param SocketHandle A descriptor identifying a connected socket.
 * @param Buffer A pointer to the buffer that receives the data read from a
 *               connected socket or a bound connectionless socket.
 * @param NumberOfBytesToRecv The maximum number of bytes to be received.
 * @param NumberOfBytesRecvd A pointer to the number, in bytes, of data
 *                           received by this call if the receive operation
 *                           completes immediately.
 * @param Flags A pointer to flags used to modify the behavior.
 * @return If the function succeeds, the return value is nonzero. If the
 *         function fails, the return value is zero. To get extended error
 *         information, call WSAGetLastError.
 * @remark For more information, see WSARecv.
 */
EXTERN_C BOOL WINAPI MileSocketRecv(
    _In_ SOCKET SocketHandle,
    _Out_opt_ LPVOID Buffer,
    _In_ DWORD NumberOfBytesToRecv,
    _Out_opt_ LPDWORD NumberOfBytesRecvd,
    _Inout_ LPDWORD Flags);

/**
 * @brief Sends data on a connected socket.
 * @param SocketHandle A descriptor that identifies a connected socket.
 * @param Buffer A pointer to the buffer containing the data to be sent to a
 *               connected socket.
 * @param NumberOfBytesToSend The number of bytes to be sent to a connected
 *                            socket.
 * @param NumberOfBytesSent A pointer to the number, in bytes, sent by this
 *                          call if the I/O operation completes immediately.
 * @param Flags A pointer to flags used to modify the behavior.
 * @return If the function succeeds, the return value is nonzero. If the
 *         function fails, the return value is zero. To get extended error
 *         information, call WSAGetLastError.
 * @remark For more information, see WSASend.
 */
EXTERN_C BOOL WINAPI MileSocketSend(
    _In_ SOCKET SocketHandle,
    _In_opt_ LPCVOID Buffer,
    _In_ DWORD NumberOfBytesToSend,
    _Out_opt_ LPDWORD NumberOfBytesSent,
    _In_ DWORD Flags);

#endif // _WINSOCK2API_

#endif // !MILE_WINDOWS_HELPERS_BASE
