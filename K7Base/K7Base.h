/*
 * PROJECT:    NanaZip Platform Base Library (K7Base)
 * FILE:       K7Base.h
 * PURPOSE:    Definition for NanaZip Platform Base Public Interfaces
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef K7_BASE
#define K7_BASE

#include <Mile.Mobility.Portable.Types.h>
#ifndef MILE_MOBILITY_ENABLE_MINIMUM_SAL
#include <sal.h>
#endif // !MILE_MOBILITY_ENABLE_MINIMUM_SAL

// Workaround for Windows SDK, which defines these types in #ifndef VOID block.
// That design is terrible, but needs to have a workaround.
#ifdef VOID
typedef char CHAR;
typedef short SHORT;
typedef long LONG;
#endif // VOID

#include <Windows.h>

#ifndef K7_BASE_DETOURS
#define K7_BASE_DETOURS

/**
 * @brief Begin a new transaction for attaching or detaching detours.
 * @return Returns NO_ERROR if successful; otherwise, returns an error code.
 * @remark For more information, see DetourTransactionBegin.
 */
EXTERN_C LONG MOAPI K7BaseDetourTransactionBegin();

/**
 * @brief Abort the current transaction for attaching or detaching detours.
 * @return Returns NO_ERROR if the pending transaction was completely aborted;
 *         otherwise, returns an error code.
 * @remark For more information, see DetourTransactionAbort.
 */
EXTERN_C LONG MOAPI K7BaseDetourTransactionAbort();

/**
 * @brief Commit the current transaction for attaching or detaching detours.
 * @return Returns NO_ERROR if successful; otherwise, returns an error code.
 * @remark For more information, see DetourTransactionCommit.
 */
EXTERN_C LONG MOAPI K7BaseDetourTransactionCommit();

/**
 * @brief Enlist a thread for update in the current transaction.
 * @param ThreadHandle The handle of the thread to be updated with the pending
 *                     transaction. If hThread is equal to the current threads
 *                     pseudo handle (as returned by GetCurrentThread()) no
 *                     action is performed and NO_ERROR is returned.
 * @return Returns NO_ERROR if successful; otherwise, returns an error code.
 * @remark For more information, see DetourUpdateThread.
 */
EXTERN_C LONG MOAPI K7BaseDetourUpdateThread(
    _In_ HANDLE ThreadHandle);

/**
 * @brief Attach a detour to a target function.
 * @param OriginalFunction Pointer to the target pointer to which the detour
 *                         will be attached.
 * @param DetouredFunction Pointer to the detour function.
 * @return Returns NO_ERROR if successful; otherwise, returns an error code.
 * @remark For more information, see DetourAttach.
 */
EXTERN_C LONG MOAPI K7BaseDetourAttach(
    _Inout_ PMO_POINTER OriginalFunction,
    _In_ MO_POINTER DetouredFunction);

/**
 * @brief Detach a detour from a target function.
 * @param OriginalFunction Pointer to the target pointer from which the detour
 *                         will be detached.
 * @param DetouredFunction Pointer to the detour function.
 * @return Returns NO_ERROR if successful; otherwise, returns an error code.
 * @remark For more information, see DetourDetach.
 */
EXTERN_C LONG MOAPI K7BaseDetourDetach(
    _Inout_ PMO_POINTER OriginalFunction,
    _In_ MO_POINTER DetouredFunction);

#endif // !K7_BASE_DETOURS

#ifndef K7_BASE_HASH
#define K7_BASE_HASH

/**
 * @brief Enumeration for supported hash algorithm types.
 * @remarks The sequence and values of this enumeration must keep the same for
 *          binary-level backward compatibility.
 */
typedef enum _K7_BASE_HASH_ALGORITHM_TYPE
{
    /**
     * @brief The MD2 hash algorithm.
     */
    K7_BASE_HASH_ALGORITHM_MD2 = 0,

    /**
     * @brief The MD4 hash algorithm.
     */
    K7_BASE_HASH_ALGORITHM_MD4 = 1,

    /**
     * @brief The MD5 hash algorithm.
     */
    K7_BASE_HASH_ALGORITHM_MD5 = 2,

    /**
     * @brief The SHA-1 hash algorithm.
     */
    K7_BASE_HASH_ALGORITHM_SHA1 = 3,

    /**
     * @brief The SHA-256 hash algorithm.
     */
    K7_BASE_HASH_ALGORITHM_SHA256 = 4,

    /**
     * @brief The SHA-384 hash algorithm.
     */
    K7_BASE_HASH_ALGORITHM_SHA384 = 5,

    /**
     * @brief The SHA-512 hash algorithm.
     */
    K7_BASE_HASH_ALGORITHM_SHA512 = 6,

    /**
     * @brief Indicates the maximum value for the K7_BASE_HASH_ALGORITHM_TYPE
     *        enumeration, which is useful for validation purposes.
     */
    K7_BASE_HASH_ALGORITHM_MAXIMUM
} K7_BASE_HASH_ALGORITHM_TYPE, *PK7_BASE_HASH_ALGORITHM_TYPE;

MO_DECLARE_HANDLE(K7_BASE_HASH_HANDLE);
typedef K7_BASE_HASH_HANDLE *PK7_BASE_HASH_HANDLE;

/**
 * @brief Creates a hash handle for the specified algorithm.
 * @param HashHandle A pointer to receive the created hash handle.
 * @param Algorithm The hash algorithm type.
 * @param SecretBuffer An optional buffer containing the secret key for HMAC.
 * @param SecretSize The size of the secret key in bytes.
 * @return If the function succeeds, it returns MO_RESULT_SUCCESS_OK. Otherwise,
 *         it returns an MO_RESULT error code.
 */
EXTERN_C MO_RESULT MOAPI K7BaseHashCreate(
    _Inout_ PK7_BASE_HASH_HANDLE HashHandle,
    _In_ K7_BASE_HASH_ALGORITHM_TYPE Algorithm,
    _In_opt_ MO_POINTER SecretBuffer,
    _In_ MO_UINT32 SecretSize);

/**
 * @brief Destroys the specified hash handle and releases associated resources.
 * @param HashHandle The handle of the hash to be destroyed.
 * @return If the function succeeds, it returns MO_RESULT_SUCCESS_OK. Otherwise,
 *         it returns an MO_RESULT error code.
 */
EXTERN_C MO_RESULT MOAPI K7BaseHashDestroy(
    _Inout_opt_ K7_BASE_HASH_HANDLE HashHandle);

/**
 * @brief Updates the hash with the specified input data.
 * @param HashHandle The handle of the hash to be updated.
 * @param InputBuffer The input data to be hashed.
 * @param InputSize The size of the input data in bytes.
 * @return If the function succeeds, it returns MO_RESULT_SUCCESS_OK. Otherwise,
 *         it returns an MO_RESULT error code.
 */
EXTERN_C MO_RESULT MOAPI K7BaseHashUpdate(
    _Inout_ K7_BASE_HASH_HANDLE HashHandle,
    _In_ MO_POINTER InputBuffer,
    _In_ MO_UINT32 InputSize);

/**
 * @brief Finalizes the hash computation and retrieves the hash value.
 * @param HashHandle The handle of the hash to be finalized.
 * @param OutputBuffer The buffer to receive the computed hash value.
 * @param OutputSize The size of the output buffer in bytes.
 * @return If the function succeeds, it returns MO_RESULT_SUCCESS_OK. Otherwise,
 *         it returns an MO_RESULT error code.
 */
EXTERN_C MO_RESULT MOAPI K7BaseHashFinal(
    _Inout_ K7_BASE_HASH_HANDLE HashHandle,
    _Out_ MO_POINTER OutputBuffer,
    _In_ MO_UINT32 OutputSize);

/**
 * @brief Retrieves the size of the hash value for the specified hash handle.
 * @param HashHandle The handle of the hash.
 * @param HashSize A pointer to receive the size of the hash value in bytes.
 * @return If the function succeeds, it returns MO_RESULT_SUCCESS_OK. Otherwise,
 *         it returns an MO_RESULT error code.
 */
EXTERN_C MO_RESULT MOAPI K7BaseHashGetSize(
    _In_ K7_BASE_HASH_HANDLE HashHandle,
    _Out_ PMO_UINT32 HashSize);

/**
 * @brief Duplicates the specified hash handle.
 * @param SourceHashHandle The handle of the hash to be duplicated.
 * @param DestinationHashHandle A pointer to receive the duplicated hash handle.
 * @return If the function succeeds, it returns MO_RESULT_SUCCESS_OK. Otherwise,
 *         it returns an MO_RESULT error code.
 */
EXTERN_C MO_RESULT MOAPI K7BaseHashDuplicate(
    _In_ K7_BASE_HASH_HANDLE SourceHashHandle,
    _Out_ PK7_BASE_HASH_HANDLE DestinationHashHandle);

#endif // !K7_BASE_HASH

#ifndef K7_BASE_POLICIES
#define K7_BASE_POLICIES

/**
 * @brief Retrieves the policy setting for allowing a specific handler.
 * @param Name The name of the handler to check.
 * @return Returns MO_TRUE if the handler is allowed, or MO_FALSE if not.
 */
EXTERN_C MO_BOOL MOAPI K7BaseGetAllowedHandlerPolicy(
    _In_ MO_CONSTANT_STRING Name);

/**
 * @brief Retrieves the policy setting for allowing a specific codec.
 * @param Name The name of the codec to check.
 * @return Returns MO_TRUE if the codec is allowed, or MO_FALSE if not.
 */
EXTERN_C MO_BOOL MOAPI K7BaseGetAllowedCodecPolicy(
    _In_ MO_CONSTANT_STRING Name);

/**
 * @brief Retrieves the policy setting for Zone.Id stream propagation.
 * @return Returns a DWORD corresponding to a NZoneIdMode value if the policy is
 *         defined, or MAXDWORD if not.
 */
EXTERN_C DWORD MOAPI K7BaseGetWriteZoneIdExtractPolicy();

#endif // !K7_BASE_POLICIES

#ifndef K7_BASE_INITIALIZE
#define K7_BASE_INITIALIZE

/**
 * @brief Checks whether the K7Base library has been initialized.
 * @return Returns MO_TRUE if the library is initialized, or MO_FALSE if not.
 * @remarks Only after K7BaseInitialize has been called successfully at least
 *          twice, this function will return MO_TRUE.
 */
EXTERN_C MO_BOOL MOAPI K7BaseGetInitialized();

/**
 * @brief Initializes the K7Base library.
 * @return If the function succeeds, it returns MO_RESULT_SUCCESS_OK. Otherwise,
 *         it returns an MO_RESULT error code.
 * @remarks The function should be called twice or thrice during the
 *          initialization phase as early as possible. The first call
 *          initializes the core components before applying dynamic code
 *          generation mitigations. The second call initializes the core
 *          components after applying dynamic code generation mitigations.
 *          The third call applies the child process creation mitigations,
 *          which is optional.
 */
EXTERN_C MO_RESULT MOAPI K7BaseInitialize();

#endif // !K7_BASE_INITIALIZE

#endif // !K7_BASE
