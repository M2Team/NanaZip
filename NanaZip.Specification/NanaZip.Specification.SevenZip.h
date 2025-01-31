/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Specification.SevenZip.h
 * PURPOSE:   Definition for 7-Zip Codec Interface Specification
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef NANAZIP_SPECIFICATION_SEVENZIP
#define NANAZIP_SPECIFICATION_SEVENZIP

#include <Windows.h>

#define SEVENZIP_ERROR_WRITING_WAS_CUT ((HRESULT)0x20000010)

// 7-Zip Interface GUID Format: 23170F69-40C1-278A-0000-00yy00xx0000

const UINT32 SevenZipGuidData1 = 0x23170F69;
const UINT16 SevenZipGuidData2 = 0x40C1;
const UINT16 SevenZipGuidData3Common = 0x278A;
const UINT16 SevenZipGuidData3Decoder = 0x2790;
const UINT16 SevenZipGuidData3Encoder = 0x2791;
const UINT16 SevenZipGuidData3Hasher = 0x2792;

MIDL_INTERFACE("23170F69-40C1-278A-0000-000000050000")
IProgress : public IUnknown
{
public:

    virtual HRESULT STDMETHODCALLTYPE SetTotal(
        _In_ UINT64 Total) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetCompleted(
        _In_ const PUINT64 CompleteValue) = 0;
};

MIDL_INTERFACE("23170F69-40C1-278A-0000-000300010000")
ISequentialInStream : public IUnknown
{
public:

    /**
     * @brief Reads data from the sequential input stream.
     * @param Data The buffer for data.
     * @param Size The size of the buffer.
     * @param ProcessedSize The processed size. It must not be nullptr.
     * @return If the function succeeds, it returns S_OK. Otherwise, it returns
     *         an HRESULT error code.
     * @remark The caller must call Read() in a loop if the caller needs to read
     *         exact amount of data.
     *         The callee should accept (ProcessedSize == nullptr) for
     *         compatibility reasons.
     *         The callee need to process the Size parameter as follows:
     *         if (Size == 0)
     *         {
     *             // Returns S_OK and (*ProcessedSize) is set to 0.
     *         }
     *         else
     *         {
     *             // If partial read is allowed, the following conditions must
     *             // be met:
     *             // - (*ProcessedSize <= AvailableSize)
     *             // - (*ProcessedSize <= Size)
     *             // Where (AvailableSize) is the size of the remaining bytes
     *             // in the stream.
     *             // If (AvailableSize) is not zero, the function must read at
     *             // least one byte and (*ProcessedSize > 0).
     *         }
     *         If the seek pointer before Read() call was changed to a position
     *         past the end of the stream, the function returns S_OK and
     *         (*ProcessedSize) is set to 0.
     *         If the function returns error code, then (*ProcessedSize) is size
     *         of data written to (Data) buffer which can be data before error
     *         or data with errors. The recommended way for callee to work with
     *         reading errors:
     *           - Write part of data before error to (Data) buffer and return
     *             S_OK.
     *           - Return error code for further calls of Read().
     */
    virtual HRESULT STDMETHODCALLTYPE Read(
        _Out_opt_ LPVOID Data,
        _In_ UINT32 Size,
        _Out_ PUINT32 ProcessedSize) = 0;
};

MIDL_INTERFACE("23170F69-40C1-278A-0000-000300020000")
ISequentialOutStream : public IUnknown
{
public:

    /**
     * @brief Writes data to the sequential output stream.
     * @param Data The buffer for data.
     * @param Size The size of the buffer.
     * @param ProcessedSize The processed size. It must not be nullptr.
     * @return If the function succeeds, it returns S_OK. Otherwise, it returns
     *         an HRESULT error code.
     * @remark The caller must call Write() in a loop if the caller needs to
     *         write exact amount of data.
     *         The callee should accept (ProcessedSize == nullptr) for
     *         compatibility reasons.
     *         The callee need to process the Size parameter as follows:
     *         if (Size != 0)
     *         {
     *             // If partial write is allowed, the following conditions must
     *             // be met:
     *             // - (*ProcessedSize <= Size)
     *             // But this function must write at least 1 byte and
     *             // (*ProcessedSize > 0).
     *         }
     *         If the function returns error code, then (*ProcessedSize) is size
     *         of data written from (Data) buffer.
     */
    virtual HRESULT STDMETHODCALLTYPE Write(
        _In_opt_ LPCVOID Data,
        _In_ UINT32 Size,
        _Out_ PUINT32 ProcessedSize) = 0;
};

MIDL_INTERFACE("23170F69-40C1-278A-0000-000300030000")
IInStream : public ISequentialInStream
{
public:

    /**
     * @brief Seeks to the specified position in the input stream.
     * @param Offset The offset relative to the origin.
     * 
     * @param SeekOrigin The origin of the seek operation. Here are available
     *                   modes:
     *                       STREAM_SEEK_SET
     *                       STREAM_SEEK_CUR
     *                       STREAM_SEEK_END
     * @param NewPosition The new position.
     * @return If the function succeeds, it returns S_OK. Otherwise, it returns
     *         an HRESULT error code.
     * @remark If the caller seek to the position before the beginning of the
     *         stream, the callee is recommended to return the error code
     *         HRESULT_FROM_WIN32(ERROR_NEGATIVE_SEEK), but the error code
     *         STG_E_INVALIDFUNCTION is also acceptable.
     *         It is allowed to seek past the end of the stream.
     *         If Seek() returns error, then the value of (*NewPosition) is
     *         undefined.
     */
    virtual HRESULT STDMETHODCALLTYPE Seek(
        _In_ INT64 Offset,
        _In_ UINT32 SeekOrigin,
        _Out_opt_ PUINT64 NewPosition) = 0;
};

MIDL_INTERFACE("23170F69-40C1-278A-0000-000300040000")
IOutStream : public ISequentialOutStream
{
public:

    /**
     * @brief Seeks to the specified position in the input stream.
     * @param Offset The offset relative to the origin.
     * @param SeekOrigin The origin of the seek operation. Here are available
     *                   modes:
     *                       STREAM_SEEK_SET
     *                       STREAM_SEEK_CUR
     *                       STREAM_SEEK_END
     * @param NewPosition The new position.
     * @return If the function succeeds, it returns S_OK. Otherwise, it returns
     *         an HRESULT error code.
     * @remark If the caller seek to the position before the beginning of the
     *         stream, the callee is recommended to return the error code
     *         HRESULT_FROM_WIN32(ERROR_NEGATIVE_SEEK), but the error code
     *         STG_E_INVALIDFUNCTION is also acceptable.
     *         It is allowed to seek past the end of the stream.
     *         If Seek() returns error, then the value of (*NewPosition) is
     *         undefined.
     */
    virtual HRESULT STDMETHODCALLTYPE Seek(
        _In_ INT64 Offset,
        _In_ UINT32 SeekOrigin,
        _Out_opt_ PUINT64 NewPosition) = 0;

    /**
     * @brief Sets the size of the output stream.
     * @param NewSize The new size of the output stream.
     * @return If the function succeeds, it returns S_OK. Otherwise, it returns
     *         an HRESULT error code.
     */
    virtual HRESULT STDMETHODCALLTYPE SetSize(
        _In_ UINT64 NewSize) = 0;
};

MIDL_INTERFACE("23170F69-40C1-278A-0000-000400040000")
ICompressProgressInfo : public IUnknown
{
public:

    /**
     * @brief Reports the progress for the compression or decompression.
     * @param InSize The size of the processed input data.
     * @param OutSize The size of the processed output data.
     * @return If the function succeeds, it returns S_OK. If the user canceled
     *         the operation, it returns E_ABORT. Otherwise, it returns an
     *         HRESULT error code.
     */
    virtual HRESULT STDMETHODCALLTYPE SetRatioInfo(
        _In_opt_ const PUINT64 InSize,
        _In_opt_ const PUINT64 OutSize) = 0;
};

MIDL_INTERFACE("23170F69-40C1-278A-0000-000400050000")
ICompressCoder : public IUnknown
{
public:

    virtual HRESULT STDMETHODCALLTYPE Code(
        _In_ ISequentialInStream* InStream,
        _In_ ISequentialOutStream* OutStream,
        _In_opt_ const PUINT64 InSize,
        _In_opt_ const PUINT64 OutSize,
        _In_opt_ ICompressProgressInfo* Progress) = 0;
};

MIDL_INTERFACE("23170F69-40C1-278A-0000-000400200000")
ICompressSetCoderProperties : public IUnknown
{
public:

    virtual HRESULT STDMETHODCALLTYPE SetCoderProperties(
        _In_ const PROPID* PropIds,
        _In_ REFPROPVARIANT Props,
        _In_ UINT32 NumProps) = 0;
};

MIDL_INTERFACE("23170F69-40C1-278A-0000-000400220000")
ICompressSetDecoderProperties2 : public IUnknown
{
public:

    /**
     * @return S_OK for success, E_NOTIMPL for unsupported properties,
     *         E_INVALIDARG for incorrect (or unsupported) properties,
     *         and E_OUTOFMEMORY for memory allocation error.
     */
    virtual HRESULT STDMETHODCALLTYPE SetDecoderProperties2(
        _In_ LPCBYTE Data,
        _In_ UINT32 Size) = 0;
};

MIDL_INTERFACE("23170F69-40C1-278A-0000-000400230000")
ICompressWriteCoderProperties : public IUnknown
{
public:

    virtual HRESULT STDMETHODCALLTYPE WriteCoderProperties(
        _In_ ISequentialOutStream* OutStream) = 0;
};

MIDL_INTERFACE("23170F69-40C1-278A-0000-000400240000")
ICompressGetInStreamProcessedSize : public IUnknown
{
public:

    virtual HRESULT STDMETHODCALLTYPE GetInStreamProcessedSize(
        _Out_ PUINT64 Value) = 0;
};

MIDL_INTERFACE("23170F69-40C1-278A-0000-000400250000")
ICompressSetCoderMt : public IUnknown
{
public:

    virtual HRESULT STDMETHODCALLTYPE SetNumberOfThreads(
        _In_ UINT32 NumThreads) = 0;
};

typedef enum _SEVENZIP_FINISH_MODE_TYPE
{
    SevenZipFinishModePartialDecoding = 0,
    SevenZipFinishModeFullDecoding = 1,
} SEVENZIP_FINISH_MODE_TYPE, *PSEVENZIP_FINISH_MODE_TYPE;

MIDL_INTERFACE("23170F69-40C1-278A-0000-000400260000")
ICompressSetFinishMode : public IUnknown
{
public:

    virtual HRESULT STDMETHODCALLTYPE SetFinishMode(
        _In_ UINT32 FinishMode) = 0;
};

MIDL_INTERFACE("23170F69-40C1-278A-0000-000400310000")
ICompressSetInStream : public IUnknown
{
public:

    virtual HRESULT STDMETHODCALLTYPE SetInStream(
        _In_ ISequentialInStream* InStream) = 0;

    virtual HRESULT STDMETHODCALLTYPE ReleaseInStream() = 0;
};

MIDL_INTERFACE("23170F69-40C1-278A-0000-000400340000")
ICompressSetOutStreamSize : public IUnknown
{
public:

    virtual HRESULT STDMETHODCALLTYPE SetOutStreamSize(
        _In_ const PUINT64 OutSize) = 0;
};

EXTERN_C HRESULT WINAPI GetNumberOfMethods(
    _Out_ PUINT32 NumMethods);

EXTERN_C HRESULT WINAPI GetMethodProperty(
    _In_ UINT32 Index,
    _In_ PROPID PropId,
    _Inout_ LPPROPVARIANT Value);

EXTERN_C HRESULT WINAPI CreateDecoder(
    _In_ UINT32 Index,
    _In_ REFIID Iid,
    _Out_ LPVOID* OutObject);

EXTERN_C HRESULT WINAPI CreateEncoder(
    _In_ UINT32 Index,
    _In_ REFIID Iid,
    _Out_ LPVOID* OutObject);

MIDL_INTERFACE("23170F69-40C1-278A-0000-000400C00000")
IHasher : public IUnknown
{
public:

    virtual void STDMETHODCALLTYPE Init() = 0;

    virtual void STDMETHODCALLTYPE Update(
        _In_ LPCVOID Data,
        _In_ UINT32 Size) = 0;

    virtual void STDMETHODCALLTYPE Final(
        _Out_ PBYTE Digest) = 0;

    virtual UINT32 STDMETHODCALLTYPE GetDigestSize() = 0;
};

typedef enum _SEVENZIP_HASHER_PROPERTY_TYPE
{
    SevenZipHasherId = 0, // VT_UI8
    SevenZipHasherName = 1, // VT_BSTR
    SevenZipHasherEncoder = 3, // VT_BSTR (Actually GUID structure)
    SevenZipHasherDigestSize = 9, // VT_UI4
} SEVENZIP_HASHER_PROPERTY_TYPE, *PSEVENZIP_HASHER_PROPERTY_TYPE;

MIDL_INTERFACE("23170F69-40C1-278A-0000-000400C10000")
IHashers : public IUnknown
{
public:

    virtual UINT32 STDMETHODCALLTYPE GetNumHashers() = 0;

    virtual HRESULT STDMETHODCALLTYPE GetHasherProp(
        _In_ UINT32 Index,
        _In_ PROPID PropId,
        _Inout_ LPPROPVARIANT Value) = 0;

    virtual HRESULT STDMETHODCALLTYPE CreateHasher(
        _In_ UINT32 Index,
        _Out_ IHasher** Hasher) = 0;
};

EXTERN_C HRESULT WINAPI GetHashers(
    _Out_ IHashers** Hashers);

// ISetProperties

MIDL_INTERFACE("23170F69-40C1-278A-0000-000600100000")
IArchiveOpenCallback : public IUnknown
{
public:

    virtual HRESULT STDMETHODCALLTYPE SetTotal(
        _In_ const PUINT64 Files,
        _In_ const PUINT64 Bytes) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetCompleted(
        _In_ const PUINT64 Files,
        _In_ const PUINT64 Bytes) = 0;
};

typedef enum _SEVENZIP_EXTRACT_ASK_MODE
{
    SevenZipExtractAskModeExtract = 0,
    SevenZipExtractAskModeTest = 1,
    SevenZipExtractAskModeSkip = 2,
    SevenZipExtractAskModeReadExternal = 3,
} SEVENZIP_EXTRACT_ASK_MODE, *PSEVENZIP_EXTRACT_ASK_MODE;

typedef enum _SEVENZIP_EXTRACT_OPERATION_RESULT
{
    SevenZipExtractOperationResultSuccess = 0,
    SevenZipExtractOperationResultUnsupportedMethod = 1,
    SevenZipExtractOperationResultDataError = 2,
    SevenZipExtractOperationResultCRCError = 3,
    SevenZipExtractOperationResultUnavailable = 4,
    SevenZipExtractOperationResultUnexpectedEnd = 5,
    SevenZipExtractOperationResultDataAfterEnd = 6,
    SevenZipExtractOperationResultIsNotArchive = 7,
    SevenZipExtractOperationResultHeadersError = 8,
    SevenZipExtractOperationResultWrongPassword = 9,
} SEVENZIP_EXTRACT_OPERATION_RESULT, *PSEVENZIP_EXTRACT_OPERATION_RESULT;

MIDL_INTERFACE("23170F69-40C1-278A-0000-000600200000")
IArchiveExtractCallback : public IProgress
{
public:

    virtual HRESULT STDMETHODCALLTYPE GetStream(
        _In_ UINT32 Index,
        _Out_ ISequentialOutStream** Stream,
        _In_ INT32 AskExtractMode) = 0;

    virtual HRESULT STDMETHODCALLTYPE PrepareOperation(
        _In_ INT32 AskExtractMode) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetOperationResult(
        _In_ INT32 OperationResult) = 0;
};

MIDL_INTERFACE("23170F69-40C1-278A-0000-000600400000")
IInArchiveGetStream : public IUnknown
{
public:

    virtual HRESULT STDMETHODCALLTYPE GetStream(
        _In_ UINT32 Index,
        _Out_ ISequentialInStream** Stream) = 0;
};

typedef enum _SEVENZIP_ARCHIVE_PROPERTY_TYPE
{
    SevenZipArchiveNoProperty = 0, // VT_EMPTY
    SevenZipArchiveMainSubfile, // VT_UI4
    SevenZipArchiveHandlerItemIndex, // VT_UI4
    SevenZipArchivePath, // VT_BSTR
    SevenZipArchiveName, // VT_BSTR
    SevenZipArchiveExtension, // VT_BSTR
    SevenZipArchiveIsDirectory, // VT_BOOL
    SevenZipArchiveSize, // VT_UI8
    SevenZipArchivePackSize, // VT_UI8
    SevenZipArchiveAttributes, // VT_UI4
    SevenZipArchiveCreationTime, // VT_FILETIME
    SevenZipArchiveAccessTime, // VT_FILETIME
    SevenZipArchiveModifiedTime, // VT_FILETIME
    SevenZipArchiveSolid, // VT_BOOL
    SevenZipArchiveCommented, // VT_BOOL
    SevenZipArchiveEncrypted, // VT_BOOL
    SevenZipArchiveSplitBefore, // VT_BOOL
    SevenZipArchiveSplitAfter, // VT_BOOL
    SevenZipArchiveDictionarySize, // VT_UI4
    SevenZipArchiveCrc, // VT_UI4
    SevenZipArchiveType, // VT_BSTR
    SevenZipArchiveIsAnti, // VT_BOOL
    SevenZipArchiveMethod, // VT_BSTR
    SevenZipArchiveHostOs, // VT_BSTR
    SevenZipArchiveFileSystem, // VT_BSTR
    SevenZipArchiveUser, // VT_BSTR
    SevenZipArchiveGroup, // VT_BSTR
    SevenZipArchiveBlock, // VT_UI8
    SevenZipArchiveComment, // VT_BSTR
    SevenZipArchivePosition, // VT_UI8
    SevenZipArchivePrefix, // VT_BSTR
    SevenZipArchiveNumberOfSubDirectories, // VT_UI8
    SevenZipArchiveNumberOfSubFiles, // VT_UI8
    SevenZipArchiveUnpackVersion, // VT_BSTR or VT_UI8
    SevenZipArchiveVolume, // VT_UI4 or VT_UI8
    SevenZipArchiveIsVolume, // VT_BOOL
    SevenZipArchiveOffset, // VT_UI8
    SevenZipArchiveLinks, // VT_UI8
    SevenZipArchiveNumberOfBlocks, // VT_UI8
    SevenZipArchiveNumberOfVolumes, // VT_UI8
    SevenZipArchiveTimeType, // VT_UI4
    SevenZipArchiveIs64Bit, // VT_BOOL
    SevenZipArchiveBigEndian, // VT_BOOL
    SevenZipArchiveCpu, // VT_BSTR
    SevenZipArchivePhysicalSize, // VT_UI8
    SevenZipArchiveHeadersSize, // VT_UI8
    SevenZipArchiveChecksum, // VT_UI4
    SevenZipArchiveCharacteristics, // VT_BSTR
    SevenZipArchiveVirtualAddress, // VT_UI8
    SevenZipArchiveId, // VT_BSTR or VT_UI8
    SevenZipArchiveShortName, // VT_BSTR
    SevenZipArchiveCreatorApplication, // VT_BSTR
    SevenZipArchiveSectorSize, // VT_UI4
    SevenZipArchivePosixAttributes, // VT_UI4
    SevenZipArchiveSymbolicLink, // VT_BSTR
    SevenZipArchiveError, // VT_BSTR
    SevenZipArchiveTotalSize, // VT_UI8
    SevenZipArchiveFreeSpace, // VT_UI8
    SevenZipArchiveClusterSize, // VT_UI4
    SevenZipArchiveVolumeName, // VT_BSTR
    SevenZipArchiveLocalName, // VT_BSTR
    SevenZipArchiveProvider, // VT_BSTR
    SevenZipArchiveNtSecurity, // VT_BSTR
    SevenZipArchiveIsAlternateStream, // VT_BOOL
    SevenZipArchiveIsAux, // VT_BOOL
    SevenZipArchiveIsDeleted, // VT_BOOL
    SevenZipArchiveIsTree, // VT_BOOL
    SevenZipArchiveSha1, // VT_BSTR (Actually SHA-1 structure)
    SevenZipArchiveSha256, // VT_BSTR (Actually SHA-256 structure)
    SevenZipArchiveErrorType, // VT_BSTR
    SevenZipArchiveNumberOfErrors, // VT_UI8
    SevenZipArchiveErrorFlags, // VT_UI4
    SevenZipArchiveWarningFlags, // VT_UI4
    SevenZipArchiveWarning, // VT_BSTR
    SevenZipArchiveNumberOfStreams, // VT_UI8
    SevenZipArchiveNumberOfAlternateStreams, // VT_UI8
    SevenZipArchiveAlternateStreamsSize, // VT_UI8
    SevenZipArchiveVirtualSize, // VT_UI8
    SevenZipArchiveUnpackSize, // VT_UI8
    SevenZipArchiveTotalPhysicalSize, // VT_UI8
    SevenZipArchiveVolumeIndex, // VT_UI8
    SevenZipArchiveSubType, // VT_BSTR
    SevenZipArchiveShortComment, // VT_BSTR
    SevenZipArchiveCodePage, // VT_BSTR
    SevenZipArchiveIsNotArchiveType, // VT_BOOL
    SevenZipArchivePhysicalSizeCantBeDetected, // VT_BOOL
    SevenZipArchiveZerosTailIsAllowed, // VT_BOOL
    SevenZipArchiveTailSize, // VT_UI8
    SevenZipArchiveEmbeddedStubSize, // VT_UI8
    SevenZipArchiveNtReparsePoint, // VT_BSTR
    SevenZipArchiveHardLink, // VT_BSTR
    SevenZipArchiveInode, // VT_UI8
    SevenZipArchiveStreamId, // VT_UI8
    SevenZipArchiveReadOnly, // VT_BOOL
    SevenZipArchiveOutputName, // VT_BSTR
    SevenZipArchiveCopyLink, // VT_BSTR
    SevenZipArchiveArchiveFileName, // VT_BSTR
    SevenZipArchiveIsHash, // VT_BOOL
    SevenZipArchiveMetadataChangedTime, // VT_FILETIME
    SevenZipArchiveUserId, // VT_UI4
    SevenZipArchiveGroupId, // VT_UI4
    SevenZipArchiveDeviceMajor, // VT_UI4
    SevenZipArchiveDeviceMinor, // VT_UI4
    SevenZipArchiveDevMajor, // VT_UI4
    SevenZipArchiveDevMinor, // VT_UI4
    SevenZipArchiveMaximumDefined,
    SevenZipArchiveUserDefined = 0x10000
} SEVENZIP_ARCHIVE_PROPERTY_TYPE, *PSEVENZIP_ARCHIVE_PROPERTY_TYPE;

MIDL_INTERFACE("23170F69-40C1-278A-0000-000600600000")
IInArchive : public IUnknown
{
public:

    virtual HRESULT STDMETHODCALLTYPE Open(
        _In_ IInStream* Stream,
        _In_opt_ const PUINT64 MaxCheckStartPosition,
        _In_opt_ IArchiveOpenCallback* OpenCallback) = 0;

    virtual HRESULT STDMETHODCALLTYPE Close() = 0;

    virtual HRESULT STDMETHODCALLTYPE GetNumberOfItems(
        _Out_ PUINT32 NumItems) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetProperty(
        _In_ UINT32 Index,
        _In_ PROPID PropId,
        _Inout_ LPPROPVARIANT Value) = 0;

    virtual HRESULT STDMETHODCALLTYPE Extract(
        _In_opt_ const PUINT32 Indices,
        _In_ UINT32 NumItems,
        _In_ BOOL TestMode,
        _In_ IArchiveExtractCallback* ExtractCallback) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetArchiveProperty(
        _In_ PROPID PropId,
        _Inout_ LPPROPVARIANT Value) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetNumberOfProperties(
        _Out_ PUINT32 NumProps) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetPropertyInfo(
        _In_ UINT32 Index,
        _Out_ BSTR* Name,
        _Out_ PROPID* PropId,
        _Out_ VARTYPE* VarType) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetNumberOfArchiveProperties(
        _Out_ PUINT32 NumProps) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetArchivePropertyInfo(
        _In_ UINT32 Index,
        _Out_ BSTR* Name,
        _Out_ PROPID* PropId,
        _Out_ VARTYPE* VarType) = 0;
};

// IInArchive

// IArchiveOpenSeq

// IArchiveGetRawProps

// IOutArchive

typedef enum _SEVENZIP_HANDLER_PROPERTY_TYPE
{
    SevenZipHandlerName = 0, // VT_BSTR
    SevenZipHandlerClassId, // VT_BSTR (Actually GUID structure)
    SevenZipHandlerExtension, // VT_BSTR
    SevenZipHandlerAddExtension, // VT_BSTR
    SevenZipHandlerUpdate, // VT_BOOL
    SevenZipHandlerKeepName, // VT_BOOL
    SevenZipHandlerSignature, // VT_BSTR (Actually BYTE array)
    SevenZipHandlerMultiSignature, // VT_BSTR (Actually BYTE array)
    SevenZipHandlerSignatureOffset, // VT_UI4
    SevenZipHandlerAlternateStream, // VT_BOOL
    SevenZipHandlerNtSecurity, // VT_BOOL
    SevenZipHandlerFlags, // VT_UI4
    SevenZipHandlerTimeFlags, // VT_UI4
} SEVENZIP_HANDLER_PROPERTY_TYPE, *PSEVENZIP_HANDLER_PROPERTY_TYPE;

typedef enum _SEVENZIP_HANDLER_FLAG_TYPE
{
    // Keep name of file in archive name
    SevenZipHandlerFlagKeepName = 1 << 0,
    // The handler supports alternate streams
    SevenZipHandlerFlagAlternateStreams = 1 << 1,
    // The handler supports NT security
    SevenZipHandlerFlagNtSecurity = 1 << 2,
    // The handler can find start of archive
    SevenZipHandlerFlagFindSignature = 1 << 3,
    // There are several signatures
    SevenZipHandlerFlagMultiSignature = 1 << 4,
    // The seek position of stream must be set as global offset
    SevenZipHandlerFlagUseGlobalOffset = 1 << 5,
    // Call handler for each start position
    SevenZipHandlerFlagStartOpen = 1 << 6,
    // Call handler only for start of file
    SevenZipHandlerFlagPureStartOpen = 1 << 7,
    // Archive can be open backward
    SevenZipHandlerFlagBackwardOpen = 1 << 8,
    // Such archive can be stored before real archive (like SFX stub)
    SevenZipHandlerFlagPreArchive = 1 << 9,
    // The handler supports symbolic links
    SevenZipHandlerFlagSymbolicLinks = 1 << 10,
    // The handler supports hard links
    SevenZipHandlerFlagHardLinks = 1 << 11,
    // Call handler only if file extension matches
    SevenZipHandlerFlagByExtensionOnlyOpen = 1 << 12,
    // The handler contains the hashes (checksums)
    SevenZipHandlerFlagHashHandler = 1 << 13,
    SevenZipHandlerFlagCreationTime = 1 << 14,
    SevenZipHandlerFlagCreationTimeDefault = 1 << 15,
    SevenZipHandlerFlagAccessTime = 1 << 16,
    SevenZipHandlerFlagAccessTimeDefault = 1 << 17,
    SevenZipHandlerFlagModifiedTime = 1 << 18,
    SevenZipHandlerFlagModifiedTimeDefault = 1 << 19,
} SEVENZIP_HANDLER_FLAG_TYPE, *PSEVENZIP_HANDLER_FLAG_TYPE;

EXTERN_C HRESULT WINAPI CreateObject(
    _In_ REFCLSID Clsid,
    _In_ REFIID Iid,
    _Out_ LPVOID* OutObject);

EXTERN_C HRESULT WINAPI GetNumberOfFormats(
    _Out_ PUINT32 NumFormats);

EXTERN_C HRESULT WINAPI GetHandlerProperty2(
    _In_ UINT32 Index,
    _In_ PROPID PropId,
    _Inout_ LPPROPVARIANT Value);

#endif /* !NANAZIP_SPECIFICATION_SEVENZIP */
