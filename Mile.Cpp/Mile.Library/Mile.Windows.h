/*
 * PROJECT:   Mouri Internal Library Essentials
 * FILE:      Mile.Windows.h
 * PURPOSE:   Definition for Windows
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: Mouri_Naruto (Mouri_Naruto AT Outlook.com)
 */

#ifndef MILE_WINDOWS
#define MILE_WINDOWS

#include "Mile.Portable.h"

#include <Windows.h>

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)
#include <ShellScalingApi.h>
#endif

namespace Mile
{
#pragma region Definitions and Implementations for Windows

    /**
     * @brief A type representing an HRESULT error code.
    */
    class HResult
    {
    public:

        /**
         * @brief The HRESULT error code represented by the HResult object.
        */
        HRESULT Value{ S_OK };

    public:

        /**
         * @brief Initializes a new instance of the HResult object.
         * @return A new instance of the HResult object.
        */
        constexpr HResult() noexcept = default;

        /**
         * @brief Initializes a new instance of the HResult object by an
         *        HRESULT code.
         * @param Value An HRESULT code that initializes the HResult object.
         * @return A new instance of the HResult object.
        */
        constexpr HResult(
            _In_ HRESULT const Value) noexcept : Value(Value)
        {
        }

        /**
         * @brief Initializes a new instance of the HResult object by a system
         *        error code.
         * @param Code The system error code.
         * @return A new instance of the HResult object.
        */
        static HResult FromWin32(
            _In_ DWORD Code) noexcept
        {
            return HResult(::HRESULT_FROM_WIN32(Code));
        }

    public:

        /**
         * @brief Retrieves the HRESULT error code for the error represented by
         *        the HResult object.
         * @return An HRESULT error code.
        */
        constexpr operator HRESULT() const noexcept
        {
            return this->Value;
        }

        /**
         * @brief Test for success on HRESULT error code represented by the
         *        HResult object.
         * @return The test result.
        */
        bool IsSucceeded() const noexcept
        {
            return SUCCEEDED(this->Value);
        }

        /**
         * @brief Test for failed on HRESULT error code represented by the
         *        HResult object.
         * @return The test result.
        */
        bool IsFailed() const noexcept
        {
            return FAILED(this->Value);
        }

        /**
         * @brief Test for errors on HRESULT error code represented by the
         *        HResult object.
         * @return The test result.
        */
        bool IsError() const noexcept
        {
            return IS_ERROR(this->Value);
        }

        /**
         * @brief Extracts the facility portion of HRESULT error code
         *        represented by the HResult object.
         * @return The facility portion value of HRESULT error code represented
         *         by the HResult object.
        */
        DWORD GetFacility() const noexcept
        {
            return HRESULT_FACILITY(this->Value);
        }

        /**
         * @brief Extracts the code portion of HRESULT error code represented
         *        by the HResult object.
         * @return The code portion value of HRESULT error code represented by
         *         the HResult object.
        */
        DWORD GetCode() const noexcept
        {
            return HRESULT_CODE(this->Value);
        }
    };

    /**
     * @brief A type representing a converter which converts the calling
     *        thread's last-error code to the HResult object.
    */
    class HResultFromLastError
    {
    private:

        /**
         * @brief Indicates needed the evaluation of the Win32 BOOL value.
        */
        bool m_EvaluateWithWin32Bool;

        /**
         * @brief The Win32 BOOL value.
        */
        BOOL m_Value;

    public:

        /**
         * @brief Initializes a new instance of the HResultFromLastError object
         *        by the calling thread's last-error code.
        */
        HResultFromLastError() :
            m_EvaluateWithWin32Bool(false),
            m_Value(FALSE)
        {
        }

        /**
         * @brief Initializes a new instance of the HResultFromLastError object
         *        by the calling thread's last-error code with the evaluation
         *        of the Win32 BOOL value.
         * @param Result The Win32 BOOL value.
        */
        HResultFromLastError(
            _In_ BOOL Result) :
            m_EvaluateWithWin32Bool(true),
            m_Value(Result)
        {
        }

        /**
         * @brief Converts the calling thread's last-error code to the HResult
         *        object.
        */
        operator HResult()
        {
            // Return if Win32 BOOL value is TRUE.
            // By design, If the this->m_Value is euqal to true,
            // the this->m_EvaluateWithWin32Bool is also euqal to true.
            if (this->m_Value)
            {
                return S_OK;
            }

            HResult hr = HResult::FromWin32(::GetLastError());

            // Set hr failed when hr succeed if it needs the evaluation of the
            // Win32 BOOL value and the Win32 BOOL value is FALSE.
            if (this->m_EvaluateWithWin32Bool && hr == S_OK)
            {
                hr = HResult::FromWin32(ERROR_FUNCTION_FAILED);
            }

            return hr;
        }

        /**
         * @brief Converts the calling thread's last-error code to the HRESULT
         *        value.
        */
        operator HRESULT()
        {
            return this->operator Mile::HResult();
        }
    };

    /**
     * @brief Wraps the Windows heap memory manager.
    */
    class HeapMemory
    {
    public:

        /**
         * @brief Allocates a block of memory from the default heap of the
         *        calling process. The allocated memory will be initialized to
         *        zero. The allocated memory is not movable.
         * @param Size The number of bytes to be allocated.
         * @return If the function succeeds, the return value is a pointer to
         *         the allocated memory block. If the function fails, the
         *         return value is nullptr.
        */
        static LPVOID Allocate(
            _In_ SIZE_T Size) noexcept
        {
            return ::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, Size);
        }

        /**
         * @brief Reallocates a block of memory from the default heap of the
         *        calling process. If the reallocation request is for a larger
         *        size, the additional region of memory beyond the original
         *        size be initialized to zero. This function enables you to
         *        resize a memory block and change other memory block
         *        properties. The allocated memory is not movable.
         * @param Block A pointer to the block of memory that the function
         *              reallocates. This pointer is returned by an earlier
         *              call to Allocate and Reallocate method.
         * @param Size The new size of the memory block, in bytes. A memory
         *             block's size can be increased or decreased by using this
         *             function.
         * @return If the function succeeds, the return value is a pointer to
         *         the reallocated memory block. If the function fails, the
         *         return value is nullptr.
        */
        static LPVOID Reallocate(
            _In_ PVOID Block,
            _In_ SIZE_T Size) noexcept
        {
            return ::HeapReAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, Block, Size);
        }

        /**
         * @brief Frees a memory block allocated from a heap by the Allocate or
         *        Reallocate method.
         * @param Block A pointer to the memory block to be freed. This pointer
         *              is returned by the Allocate or Reallocate method. If
         *              this pointer is nullptr, the behavior is undefined.
         * @return If the function succeeds, the return value is nonzero. If
         *         the function fails, the return value is zero. An application
         *         can call GetLastError for extended error information.
        */
        static BOOL Free(
            _In_ LPVOID Block) noexcept
        {
            return ::HeapFree(::GetProcessHeap(), 0, Block);
        }
    };

    /**
     * @brief Wraps a critical section object.
    */
    class CriticalSection : DisableCopyConstruction, DisableMoveConstruction
    {
    public:

        /**
         * @brief Initializes a critical section object.
         * @param lpCriticalSection A pointer to the critical section object.
         * @remark For more information, see InitializeCriticalSection.
         */
        static void Initialize(
            _Out_ LPCRITICAL_SECTION lpCriticalSection) noexcept
        {
            ::InitializeCriticalSection(lpCriticalSection);
        }

        /**
         * @brief Releases all resources used by an unowned critical section
         *        object.
         * @param lpCriticalSection A pointer to the critical section object.
         * @remark For more information, see DeleteCriticalSection.
         */
        static void Delete(
            _Inout_ LPCRITICAL_SECTION lpCriticalSection) noexcept
        {
            ::DeleteCriticalSection(lpCriticalSection);
        }

        /**
         * @brief Waits for ownership of the specified critical section object.
         *        The function returns when the calling thread is granted
         *        ownership.
         * @param lpCriticalSection A pointer to the critical section object.
         * @remark For more information, see EnterCriticalSection.
         */
        static void Enter(
            _Inout_ LPCRITICAL_SECTION lpCriticalSection) noexcept
        {
            ::EnterCriticalSection(lpCriticalSection);
        }

        /**
         * @brief Attempts to enter a critical section without blocking. If the
         *        call is successful, the calling thread takes ownership of the
         *        critical section.
         * @param lpCriticalSection A pointer to the critical section object.
         * @return If the critical section is successfully entered or the
         *         current thread already owns the critical section, the return
         *         value is true. If another thread already owns the critical
         *         section, the return value is false.
         * @remark For more information, see TryEnterCriticalSection.
         */
        static bool TryEnter(
            _Inout_ LPCRITICAL_SECTION lpCriticalSection) noexcept
        {
            return FALSE != ::TryEnterCriticalSection(lpCriticalSection);
        }

        /**
         * @brief Releases ownership of the specified critical section object.
         * @param lpCriticalSection A pointer to the critical section object.
         * @remark For more information, see LeaveCriticalSection.
         */
        static void Leave(
            _Inout_ LPCRITICAL_SECTION lpCriticalSection) noexcept
        {
            ::LeaveCriticalSection(lpCriticalSection);
        }

    private:

        /**
         * @brief The raw critical section object.
        */
        CRITICAL_SECTION m_RawObject;

    public:

        /**
         * @brief Initializes the critical section object.
        */
        CriticalSection() noexcept
        {
            Initialize(&this->m_RawObject);
        }

        /**
         * @brief Releases all resources used by the critical section object.
        */
        ~CriticalSection() noexcept
        {
            Delete(&this->m_RawObject);
        }

        /**
         * @brief Waits for ownership of the critical section object. The
         *        function returns when the calling thread is granted ownership.
        */
        void Lock() noexcept
        {
            Enter(&this->m_RawObject);
        }

        /**
         * @brief Attempts to enter the critical section without blocking. If
         *        the call is successful, the calling thread takes ownership of
         *        the critical section.
         * @return If the critical section is successfully entered or the
         *         current thread already owns the critical section, the return
         *         value is true. If another thread already owns the critical
         *         section, the return value is false.
        */
        bool TryLock() noexcept
        {
            return TryEnter(&this->m_RawObject);
        }

        /**
         * @brief Releases ownership of the critical section object.
        */
        void Unlock() noexcept
        {
            Leave(&this->m_RawObject);
        }
    };

    /**
     * @brief Provides automatic locking and unlocking of a critical section.
    */
    class AutoCriticalSectionLock
    {
    private:

        /**
         * @brief The critical section object.
        */
        CriticalSection& m_Object;

    public:

        /**
         * @brief Lock the critical section object.
         * @param Object The critical section object.
        */
        explicit AutoCriticalSectionLock(
            CriticalSection& Object) noexcept :
            m_Object(Object)
        {
            this->m_Object.Lock();
        }

        /**
         * @brief Unlock the critical section object.
        */
        ~AutoCriticalSectionLock() noexcept
        {
            this->m_Object.Unlock();
        }
    };

    /**
     * @brief Provides automatic trying to locking and unlocking of a critical
     *        section.
    */
    class AutoCriticalSectionTryLock
    {
    private:

        /**
         * @brief The critical section object.
        */
        CriticalSection& m_Object;

        /**
         * @brief The lock status.
        */
        bool m_IsLocked;

    public:

        /**
         * @brief Try to lock the critical section object.
         * @param Object The critical section object.
        */
        explicit AutoCriticalSectionTryLock(
            CriticalSection& Object) noexcept :
            m_Object(Object)
        {
            this->m_IsLocked = this->m_Object.TryLock();
        }

        /**
         * @brief Try to unlock the critical section object.
        */
        ~AutoCriticalSectionTryLock() noexcept
        {
            if (this->m_IsLocked)
            {
                this->m_Object.Unlock();
            }
        }

        /**
         * @brief Check the lock status.
         * @return The lock status.
        */
        bool IsLocked() const
        {
            return this->m_IsLocked;
        }
    };

    /**
     * @brief Provides automatic locking and unlocking of a raw critical section.
    */
    class AutoRawCriticalSectionLock
    {
    private:

        /**
         * @brief The raw critical section object.
        */
        CRITICAL_SECTION& m_Object;

    public:

        /**
         * @brief Lock the raw critical section object.
         * @param Object The raw critical section object.
        */
        explicit AutoRawCriticalSectionLock(
            CRITICAL_SECTION& Object) noexcept :
            m_Object(Object)
        {
            CriticalSection::Enter(&this->m_Object);
        }

        /**
         * @brief Unlock the raw critical section object.
        */
        ~AutoRawCriticalSectionLock() noexcept
        {
            CriticalSection::Leave(&this->m_Object);
        }
    };

    /**
     * @brief Provides automatic trying to locking and unlocking of a raw
     *        critical section.
    */
    class AutoRawCriticalSectionTryLock
    {
    private:

        /**
         * @brief The raw critical section object.
        */
        CRITICAL_SECTION& m_Object;

        /**
         * @brief The lock status.
        */
        bool m_IsLocked;

    public:

        /**
         * @brief Try to lock the raw critical section object.
         * @param Object The raw critical section object.
        */
        explicit AutoRawCriticalSectionTryLock(
            CRITICAL_SECTION& Object) noexcept :
            m_Object(Object)
        {
            this->m_IsLocked = CriticalSection::TryEnter(&this->m_Object);
        }

        /**
         * @brief Try to unlock the raw critical section object.
        */
        ~AutoRawCriticalSectionTryLock() noexcept
        {
            if (this->m_IsLocked)
            {
                CriticalSection::Leave(&this->m_Object);
            }
        }

        /**
         * @brief Check the lock status.
         * @return The lock status.
        */
        bool IsLocked() const
        {
            return this->m_IsLocked;
        }
    };

    /**
     * @brief Wraps a slim reader/writer (SRW) lock.
    */
    class SRWLock : DisableCopyConstruction, DisableMoveConstruction
    {
    public:

        /**
         * @brief Initialize a slim reader/writer (SRW) lock.
         * @param SRWLock A pointer to the SRW lock.
         * @remark For more information, see InitializeSRWLock.
         */
        static void Initialize(
            _Out_ PSRWLOCK SRWLock) noexcept
        {
            ::InitializeSRWLock(SRWLock);
        }

        /**
         * @brief Acquires a slim reader/writer (SRW) lock in exclusive mode.
         * @param SRWLock A pointer to the SRW lock.
         * @remark For more information, see AcquireSRWLockExclusive.
         */
        static void AcquireExclusive(
            _Inout_ PSRWLOCK SRWLock) noexcept
        {
            ::AcquireSRWLockExclusive(SRWLock);
        }

        /**
         * @brief Attempts to acquire a slim reader/writer (SRW) lock in
         *        exclusive mode. If the call is successful, the calling thread
         *        takes ownership of the lock.
         * @param SRWLock A pointer to the SRW lock.
         * @return If the lock is successfully acquired, the return value is
         *         true. If the current thread could not acquire the lock, the
         *         return value is false.
         * @remark For more information, see TryAcquireSRWLockExclusive.
         */
        static bool TryAcquireExclusive(
            _Inout_ PSRWLOCK SRWLock) noexcept
        {
            return FALSE != ::TryAcquireSRWLockExclusive(SRWLock);
        }

        /**
         * @brief Releases a slim reader/writer (SRW) lock that was acquired in
         *        exclusive mode.
         *
         * @param SRWLock A pointer to the SRW lock.
         * @remark For more information, see ReleaseSRWLockExclusive.
         */
        static void ReleaseExclusive(
            _Inout_ PSRWLOCK SRWLock) noexcept
        {
            ::ReleaseSRWLockExclusive(SRWLock);
        }

        /**
         * @brief Acquires a slim reader/writer (SRW) lock in shared mode.
         * @param SRWLock A pointer to the SRW lock.
         * @remark For more information, see AcquireSRWLockShared.
         */
        static void AcquireShared(
            _Inout_ PSRWLOCK SRWLock) noexcept
        {
            ::AcquireSRWLockShared(SRWLock);
        }

        /**
         * @brief Attempts to acquire a slim reader/writer (SRW) lock in shared
         *        mode. If the call is successful, the calling thread takes
         *        ownership of the lock.
         * @param SRWLock A pointer to the SRW lock.
         * @return If the lock is successfully acquired, the return value is
         *         true. If the current thread could not acquire the lock, the
         *         return value is false.
         * @remark For more information, see TryAcquireSRWLockShared.
         */
        static bool TryAcquireShared(
            _Inout_ PSRWLOCK SRWLock) noexcept
        {
            return FALSE != ::TryAcquireSRWLockShared(SRWLock);
        }

        /**
         * @brief Releases a slim reader/writer (SRW) lock that was acquired in
         *        shared mode.
         * @param SRWLock A pointer to the SRW lock.
         * @remark For more information, see ReleaseSRWLockShared.
         */
        static void ReleaseShared(
            _Inout_ PSRWLOCK SRWLock) noexcept
        {
            ::ReleaseSRWLockShared(SRWLock);
        }

    private:

        /**
         * @brief The raw slim reader/writer (SRW) lock object.
        */
        SRWLOCK m_RawObject;

    public:

        /**
         * @brief Initialize the slim reader/writer (SRW) lock.
        */
        SRWLock() noexcept
        {
            Initialize(&this->m_RawObject);
        }

        /**
         * @brief Acquires the slim reader/writer (SRW) lock in exclusive mode.
        */
        void LockExclusive() noexcept
        {
            AcquireExclusive(&this->m_RawObject);
        }

        /**
         * @brief Attempts to acquire the slim reader/writer (SRW) lock in
         *        exclusive mode. If the call is successful, the calling thread
         *        takes ownership of the lock.
         * @return If the lock is successfully acquired, the return value is
         *         true. If the current thread could not acquire the lock, the
         *         return value is false.
        */
        bool TryLockExclusive() noexcept
        {
            return TryAcquireExclusive(&this->m_RawObject);
        }

        /**
         * @brief Releases the slim reader/writer (SRW) lock that was acquired
         *        in exclusive mode.
        */
        void UnlockExclusive() noexcept
        {
            ReleaseExclusive(&this->m_RawObject);
        }

        /**
         * @brief Acquires the slim reader/writer (SRW) lock in shared mode.
        */
        void LockShared() noexcept
        {
            AcquireShared(&this->m_RawObject);
        }

        /**
         * @brief Attempts to acquire the slim reader/writer (SRW) lock in
         *        shared mode. If the call is successful, the calling thread
         *        takes ownership of the lock.
         * @return If the lock is successfully acquired, the return value is
         *         true. If the current thread could not acquire the lock, the
         *         return value is false.
        */
        bool TryLockShared() noexcept
        {
            return TryAcquireShared(&this->m_RawObject);
        }

        /**
         * @brief Releases the slim reader/writer (SRW) lock that was acquired
         *        in shared mode.
        */
        void UnlockShared() noexcept
        {
            ReleaseShared(&this->m_RawObject);
        }
    };

    /**
     * @brief Provides automatic exclusive locking and unlocking of a slim
     *        reader/writer (SRW) lock.
    */
    class AutoSRWExclusiveLock
    {
    private:

        /**
         * @brief The slim reader/writer (SRW) lock object.
        */
        SRWLock& m_Object;

    public:

        /**
         * @brief Exclusive lock the slim reader/writer (SRW) lock object.
         * @param Object The slim reader/writer (SRW) lock object.
        */
        explicit AutoSRWExclusiveLock(
            SRWLock& Object) noexcept :
            m_Object(Object)
        {
            this->m_Object.LockExclusive();
        }

        /**
         * @brief Exclusive unlock the slim reader/writer (SRW) lock object.
        */
        ~AutoSRWExclusiveLock() noexcept
        {
            this->m_Object.UnlockExclusive();
        }
    };

    /**
     * @brief Provides automatic trying to exclusive locking and unlocking of a
     *        slim reader/writer (SRW) lock.
    */
    class AutoSRWExclusiveTryLock
    {
    private:

        /**
         * @brief The slim reader/writer (SRW) lock object.
        */
        SRWLock& m_Object;

        /**
         * @brief The lock status.
        */
        bool m_IsLocked;

    public:

        /**
         * @brief Try to exclusive lock the slim reader/writer (SRW) lock
         *        object.
         * @param Object The slim reader/writer (SRW) lock object.
        */
        explicit AutoSRWExclusiveTryLock(
            SRWLock& Object) noexcept :
            m_Object(Object)
        {
            this->m_IsLocked = this->m_Object.TryLockExclusive();
        }

        /**
         * @brief Try to exclusive unlock the slim reader/writer (SRW) lock
         *        object.
        */
        ~AutoSRWExclusiveTryLock() noexcept
        {
            if (this->m_IsLocked)
            {
                this->m_Object.UnlockExclusive();
            }
        }

        /**
         * @brief Check the lock status.
         * @return The lock status.
        */
        bool IsLocked() const
        {
            return this->m_IsLocked;
        }
    };

    /**
     * @brief Provides automatic shared locking and unlocking of a slim
     *        reader/writer (SRW) lock.
    */
    class AutoSRWSharedLock
    {
    private:

        /**
         * @brief The slim reader/writer (SRW) lock object.
        */
        SRWLock& m_Object;

    public:

        /**
         * @brief Shared lock the slim reader/writer (SRW) lock object.
         * @param Object The slim reader/writer (SRW) lock object.
        */
        explicit AutoSRWSharedLock(
            SRWLock& Object) noexcept :
            m_Object(Object)
        {
            this->m_Object.LockShared();
        }

        /**
         * @brief Shared unlock the slim reader/writer (SRW) lock object.
        */
        ~AutoSRWSharedLock() noexcept
        {
            this->m_Object.UnlockShared();
        }
    };

    /**
     * @brief Provides automatic trying to shared locking and unlocking of a
     *        slim reader/writer (SRW) lock.
    */
    class AutoSRWSharedTryLock
    {
    private:

        /**
         * @brief The slim reader/writer (SRW) lock object.
        */
        SRWLock& m_Object;

        /**
         * @brief The lock status.
        */
        bool m_IsLocked;

    public:

        /**
         * @brief Try to shared lock the slim reader/writer (SRW) lock object.
         * @param Object The slim reader/writer (SRW) lock object.
        */
        explicit AutoSRWSharedTryLock(
            SRWLock& Object) noexcept :
            m_Object(Object)
        {
            this->m_IsLocked = this->m_Object.TryLockShared();
        }

        /**
         * @brief Try to shared unlock the slim reader/writer (SRW) lock
         *        object.
        */
        ~AutoSRWSharedTryLock() noexcept
        {
            if (this->m_IsLocked)
            {
                this->m_Object.UnlockShared();
            }
        }

        /**
         * @brief Check the lock status.
         * @return The lock status.
        */
        bool IsLocked() const
        {
            return this->m_IsLocked;
        }
    };

    /**
     * @brief Provides automatic exclusive locking and unlocking of a raw slim
     *        reader/writer (SRW) lock.
    */
    class AutoRawSRWExclusiveLock
    {
    private:

        /**
         * @brief The slim reader/writer (SRW) lock object.
        */
        SRWLOCK& m_Object;

    public:

        /**
         * @brief Exclusive lock the raw slim reader/writer (SRW) lock object.
         * @param Object The raw slim reader/writer (SRW) lock object.
        */
        explicit AutoRawSRWExclusiveLock(
            SRWLOCK& Object) noexcept :
            m_Object(Object)
        {
            SRWLock::AcquireExclusive(&this->m_Object);
        }

        /**
         * @brief Exclusive unlock the raw slim reader/writer (SRW) lock
         *        object.
        */
        ~AutoRawSRWExclusiveLock() noexcept
        {
            SRWLock::ReleaseExclusive(&this->m_Object);
        }
    };

    /**
     * @brief Provides automatic trying to exclusive locking and unlocking of a
     *        raw slim reader/writer (SRW) lock.
    */
    class AutoRawSRWExclusiveTryLock
    {
    private:

        /**
         * @brief The raw slim reader/writer (SRW) lock object.
        */
        SRWLOCK& m_Object;

        /**
         * @brief The lock status.
        */
        bool m_IsLocked;

    public:

        /**
         * @brief Try to exclusive lock the raw slim reader/writer (SRW) lock
         *        object.
         * @param Object The slim reader/writer (SRW) lock object.
        */
        explicit AutoRawSRWExclusiveTryLock(
            SRWLOCK& Object) noexcept :
            m_Object(Object)
        {
            this->m_IsLocked = SRWLock::TryAcquireExclusive(&this->m_Object);
        }

        /**
         * @brief Try to exclusive unlock the raw slim reader/writer (SRW) lock
         *        object.
        */
        ~AutoRawSRWExclusiveTryLock() noexcept
        {
            if (this->m_IsLocked)
            {
                SRWLock::ReleaseExclusive(&this->m_Object);
            }
        }

        /**
         * @brief Check the lock status.
         * @return The lock status.
        */
        bool IsLocked() const
        {
            return this->m_IsLocked;
        }
    };

    /**
     * @brief Provides automatic shared locking and unlocking of a raw slim
     *        reader/writer (SRW) lock.
    */
    class AutoRawSRWSharedLock
    {
    private:

        /**
         * @brief The raw slim reader/writer (SRW) lock object.
        */
        SRWLOCK& m_Object;

    public:

        /**
         * @brief Shared lock the raw slim reader/writer (SRW) lock object.
         * @param Object The raw slim reader/writer (SRW) lock object.
        */
        explicit AutoRawSRWSharedLock(
            SRWLOCK& Object) noexcept :
            m_Object(Object)
        {
            SRWLock::AcquireShared(&this->m_Object);
        }

        /**
         * @brief Shared unlock the raw slim reader/writer (SRW) lock object.
        */
        ~AutoRawSRWSharedLock() noexcept
        {
            SRWLock::ReleaseShared(&this->m_Object);
        }
    };

    /**
     * @brief Provides automatic trying to shared locking and unlocking of a
     *        raw slim reader/writer (SRW) lock.
    */
    class AutoRawSRWSharedTryLock
    {
    private:

        /**
         * @brief The raw slim reader/writer (SRW) lock object.
        */
        SRWLOCK& m_Object;

        /**
         * @brief The lock status.
        */
        bool m_IsLocked;

    public:

        /**
         * @brief Try to shared lock the raw slim reader/writer (SRW) lock object.
         * @param Object The raw slim reader/writer (SRW) lock object.
        */
        explicit AutoRawSRWSharedTryLock(
            SRWLOCK& Object) noexcept :
            m_Object(Object)
        {
            this->m_IsLocked = SRWLock::TryAcquireShared(&this->m_Object);
        }

        /**
         * @brief Try to shared unlock the raw slim reader/writer (SRW) lock
         *        object.
        */
        ~AutoRawSRWSharedTryLock() noexcept
        {
            if (this->m_IsLocked)
            {
                SRWLock::ReleaseShared(&this->m_Object);
            }
        }

        /**
         * @brief Check the lock status.
         * @return The lock status.
        */
        bool IsLocked() const
        {
            return this->m_IsLocked;
        }
    };

#pragma endregion

#pragma region Definitions for Windows (Win32 Style)

    /**
     * @brief The definition of the file enumerator handle.
    */
    typedef void* FILE_ENUMERATOR_HANDLE;
    typedef FILE_ENUMERATOR_HANDLE* PFILE_ENUMERATOR_HANDLE;

    /**
     * @brief The information about a found file or directory queried from the
     *        file enumerator.
    */
    typedef struct _FILE_ENUMERATE_INFORMATION
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
    } FILE_ENUMERATE_INFORMATION, *PFILE_ENUMERATE_INFORMATION;

    /**
     * @brief The file enumerate callback type.
     * @param Information The file enumerate information.
     * @param Context The user context.
     * @return If the return value is non-zero, the file enumerate will be
     *         continued. If the return value is zero, the file enumerate will
     *         be terminated.
    */
    typedef BOOL(WINAPI* ENUMERATE_FILE_CALLBACK_TYPE)(
        _In_ Mile::PFILE_ENUMERATE_INFORMATION Information,
        _In_opt_ LPVOID Context);

    /**
     * @brief The resource info struct.
    */
    typedef struct _RESOURCE_INFO
    {
        DWORD Size;
        LPVOID Pointer;
    } RESOURCE_INFO, *PRESOURCE_INFO;

    /**
     * @brief Sends a control code directly to a specified device driver,
     *        causing the corresponding device to perform the corresponding
     *        operation.
     * @param hDevice A handle to the device on which the operation is to be
     *                performed. The device is typically a volume, directory,
     *                file, or stream. To retrieve a device handle, use the
     *                CreateFile function.
     * @param dwIoControlCode The control code for the operation. This value
     *                        identifies the specific operation to be performed
     *                        and the type of device on which to perform it.
     * @param lpInBuffer A pointer to the input buffer that contains the data
     *                   required to perform the operation. The format of this
     *                   data depends on the value of the dwIoControlCode
     *                   parameter. This parameter can be nullptr if
     *                   dwIoControlCode specifies an operation that does not
     *                   require input data.
     * @param nInBufferSize The size of the input buffer, in bytes.
     * @param lpOutBuffer A pointer to the output buffer that is to receive the
     *                    data returned by the operation. The format of this
     *                    data depends on the value of the dwIoControlCode
     *                    parameter. This parameter can be nullptr if
     *                    dwIoControlCode specifies an operation that does not
     *                    return data.
     * @param nOutBufferSize The size of the output buffer, in bytes.
     * @param lpBytesReturned A pointer to a variable that receives the size of
     *                        the data stored in the output buffer, in bytes.
     *                        If the output buffer is too small to receive any
     *                        data, the call fails, GetLastError returns
     *                        ERROR_INSUFFICIENT_BUFFER, and lpBytesReturned is
     *                        zero. If the output buffer is too small to hold
     *                        all of the data but can hold some entries, some
     *                        drivers will return as much data as fits. In this
     *                        case, the call fails, GetLastError returns
     *                        ERROR_MORE_DATA, and lpBytesReturned indicates
     *                        the amount of data received. Your application
     *                        should call DeviceIoControl again with the same
     *                        operation, specifying a new starting point.
     * @return An HResultFromLastError object An containing the HResult object
     *         containing the error code.
     * @remark For more information, see DeviceIoControl.
    */
    HResultFromLastError DeviceIoControl(
        _In_ HANDLE hDevice,
        _In_ DWORD dwIoControlCode,
        _In_opt_ LPVOID lpInBuffer,
        _In_ DWORD nInBufferSize,
        _Out_opt_ LPVOID lpOutBuffer,
        _In_ DWORD nOutBufferSize,
        _Out_opt_ LPDWORD lpBytesReturned);

    /**
     * @brief Gets the NTFS compression attribute.
     * @param FileHandle A handle to the file or directory on which the
     *                   operation is to be performed. To retrieve a handle,
     *                   use the CreateFile or a similar API.
     * @param CompressionAlgorithm Specifies the compression algorithm that is
     *                             used to compress this file. Currently
     *                             defined algorithms are:
     *                             COMPRESSION_FORMAT_NONE
     *                                 Uncompress the file or directory.
     *                             COMPRESSION_FORMAT_DEFAULT
     *                                 Compress the file or directory, using
     *                                 the default compression format.
     *                             COMPRESSION_FORMAT_LZNT1
     *                                 Compress the file or directory, using
     *                                 the LZNT1 compression format.
     * @return An HResultFromLastError object An containing the HResult object
     *         containing the error code.
    */
    HResultFromLastError GetNtfsCompressionAttribute(
        _In_ HANDLE FileHandle,
        _Out_ PUSHORT CompressionAlgorithm);

    /**
     * @brief Sets the NTFS compression attribute.
     * @param FileHandle A handle to the file or directory on which the
     *                   operation is to be performed. To retrieve a handle,
     *                   use the CreateFile or a similar API.
     * @param CompressionAlgorithm Specifies the compression algorithm that is
     *                             used to compress this file. Currently
     *                             defined algorithms are:
     *                             COMPRESSION_FORMAT_NONE
     *                                 Uncompress the file or directory.
     *                             COMPRESSION_FORMAT_DEFAULT
     *                                 Compress the file or directory, using
     *                                 the default compression format.
     *                             COMPRESSION_FORMAT_LZNT1
     *                                 Compress the file or directory, using
     *                                 the LZNT1 compression format.
     * @return An HResultFromLastError object An containing the HResult object
     *         containing the error code.
    */
    HResultFromLastError SetNtfsCompressionAttribute(
        _In_ HANDLE FileHandle,
        _In_ USHORT CompressionAlgorithm);

    /**
     * @brief Gets the Windows Overlay Filter file compression attribute.
     * @param FileHandle A handle to the file on which the operation is to be
     *                   performed. To retrieve a handle, use the CreateFile or
     *                   a similar API.
     * @param CompressionAlgorithm Specifies the compression algorithm that is
     *                             used to compress this file. Currently
     *                             defined algorithms are:
     *                             FILE_PROVIDER_COMPRESSION_XPRESS4K
     *                                 Indicates that the data for the file
     *                                 should be compressed in 4kb chunks with
     *                                 the XPress algorithm. This algorithm is
     *                                 designed to be computationally
     *                                 lightweight, and provides for rapid
     *                                 access to data.
     *                             FILE_PROVIDER_COMPRESSION_LZX
     *                                 Indicates that the data for the file
     *                                 should be compressed in 32kb chunks with
     *                                 the LZX algorithm. This algorithm is
     *                                 designed to be highly compact, and
     *                                 provides for small footprint for
     *                                 infrequently accessed data.
     *                             FILE_PROVIDER_COMPRESSION_XPRESS8K
     *                                 Indicates that the data for the file
     *                                 should be compressed in 8kb chunks with
     *                                 the XPress algorithm.
     *                             FILE_PROVIDER_COMPRESSION_XPRESS16K
     *                                 Indicates that the data for the file
     *                                 should be compressed in 16kb chunks with
     *                                 the XPress algorithm.
     * @return An HResult object containing the error code.
    */
    HResult GetWofCompressionAttribute(
        _In_ HANDLE FileHandle,
        _Out_ PDWORD CompressionAlgorithm);

    /**
     * @brief Sets the Windows Overlay Filter file compression attribute.
     * @param FileHandle A handle to the file on which the operation is to be
     *                   performed. To retrieve a handle, use the CreateFile or
     *                   a similar API.
     * @param CompressionAlgorithm Specifies the compression algorithm that is
     *                             used to compress this file. Currently
     *                             defined algorithms are:
     *                             FILE_PROVIDER_COMPRESSION_XPRESS4K
     *                                 Indicates that the data for the file
     *                                 should be compressed in 4kb chunks with
     *                                 the XPress algorithm. This algorithm is
     *                                 designed to be computationally
     *                                 lightweight, and provides for rapid
     *                                 access to data.
     *                             FILE_PROVIDER_COMPRESSION_LZX
     *                                 Indicates that the data for the file
     *                                 should be compressed in 32kb chunks with
     *                                 the LZX algorithm. This algorithm is
     *                                 designed to be highly compact, and
     *                                 provides for small footprint for
     *                                 infrequently accessed data.
     *                             FILE_PROVIDER_COMPRESSION_XPRESS8K
     *                                 Indicates that the data for the file
     *                                 should be compressed in 8kb chunks with
     *                                 the XPress algorithm.
     *                             FILE_PROVIDER_COMPRESSION_XPRESS16K
     *                                 Indicates that the data for the file
     *                                 should be compressed in 16kb chunks with
     *                                 the XPress algorithm.
     * @return An HResult object containing the error code.
    */
    HResult SetWofCompressionAttribute(
        _In_ HANDLE FileHandle,
        _In_ DWORD CompressionAlgorithm);

    /**
     * @brief Removes the Windows Overlay Filter file compression attribute.
     * @param FileHandle A handle to the file on which the operation is to be
     *                   performed. To retrieve a handle, use the CreateFile or
     *                   a similar API.
     * @return An HResult object containing the error code.
    */
    HResult RemoveWofCompressionAttribute(
        _In_ HANDLE FileHandle);

    /**
     * @brief Sets the system's compression (Compact OS) state.
     * @param DeploymentState The system's compression (Compact OS) state. If
     *                        this value is TRUE, the system state means
     *                        Compact. If it is FALSE, the system state means
     *                        non Compact.
     * @return An HResult object containing the error code.
    */
    HResult GetCompactOsDeploymentState(
        _Out_ PDWORD DeploymentState);

    /**
     * @brief Gets the system's compression (Compact OS) state.
     * @param DeploymentState The system's compression (Compact OS) state. If
     *                        this value is TRUE, the function sets the system
     *                        state to Compact. If it is FALSE, the function
     *                        sets the system state to non Compact.
     * @return An HResult object containing the error code.
    */
    HResult SetCompactOsDeploymentState(
        _In_ DWORD DeploymentState);

    /**
     * @brief Enumerates files in a directory.
     * @param FileHandle The handle of the file to be searched a directory for
     *                   a file or subdirectory with a name. This handle must
     *                   be opened with the appropriate permissions for the
     *                   requested change. This handle should not be a pipe
     *                   handle.
     * @param Callback The file enumerate callback.
     * @param Context The user context.
     * @return An HResult object containing the error code.
    */
    HResult EnumerateFile(
        _In_ HANDLE FileHandle,
        _In_ Mile::ENUMERATE_FILE_CALLBACK_TYPE Callback,
        _In_opt_ LPVOID Context);

    /**
     * @brief Retrieves the size of the specified file.
     * @param FileHandle A handle to the file that contains the information to
     *                   be retrieved. This handle should not be a pipe handle.
     * @param FileSize A pointer to a ULONGLONG value that receives the file
     *                 size, in bytes.
     * @return An HResultFromLastError object An containing the HResult object
     *         containing the error code.
    */
    HResultFromLastError GetFileSize(
        _In_ HANDLE FileHandle,
        _Out_ PULONGLONG FileSize);

    /**
     * @brief Retrieves the amount of space that is allocated for the file.
     * @param FileHandle A handle to the file that contains the information to
     *                   be retrieved. This handle should not be a pipe handle.
     * @param AllocationSize A pointer to a ULONGLONG value that receives the
     *                       amount of space that is allocated for the file, in
     *                       bytes.
     * @return An HResultFromLastError object An containing the HResult object
     *         containing the error code.
    */
    HResultFromLastError GetFileAllocationSize(
        _In_ HANDLE FileHandle,
        _Out_ PULONGLONG AllocationSize);

    /**
     * @brief Retrieves the actual number of bytes of disk storage used to
     *        store a specified file. If the file is located on a volume that
     *        supports compression and the file is compressed, the value
     *        obtained is the compressed size of the specified file. If the
     *        file is located on a volume that supports sparse files and the
     *        file is a sparse file, the value obtained is the sparse size of
     *        the specified file.
     * @param FileHandle A handle to the file that contains the information to
     *                   be retrieved. This handle should not be a pipe handle.
     * @param CompressedFileSize A pointer to a ULONGLONG value that receives
     *                           the compressed file size, in bytes.
     * @return An HResultFromLastError object An containing the HResult object
     *         containing the error code.
    */
    HResultFromLastError GetCompressedFileSizeByHandle(
        _In_ HANDLE FileHandle,
        _Out_ PULONGLONG CompressedFileSize);

    /**
     * @brief Retrieves file system attributes for a specified file or
     *        directory.
     * @param FileHandle A handle to the file that contains the information to
     *                   be retrieved. This handle should not be a pipe handle.
     * @param FileAttributes The attributes of the specified file or directory.
     *                       For a list of attribute values and their
     *                       descriptions, see File Attribute Constants. If the
     *                       function fails, the return value is
     *                       INVALID_FILE_ATTRIBUTES.
     * @return An HResultFromLastError object An containing the HResult object
     *         containing the error code.
    */
    HResultFromLastError GetFileAttributesByHandle(
        _In_ HANDLE FileHandle,
        _Out_ PDWORD FileAttributes);

    /**
     * @brief Sets the attributes for a file or directory.
     * @param FileHandle A handle to the file for which to change information.
     *                   This handle must be opened with the appropriate
     *                   permissions for the requested change. This handle
     *                   should not be a pipe handle.
     * @param FileAttributes The file attributes to set for the file. This
     *                       parameter can be one or more values, combined
     *                       using the bitwise - OR operator. However, all
     *                       other values override FILE_ATTRIBUTE_NORMAL. For
     *                       more information, see the SetFileAttributes
     *                       function.
     * @return An HResultFromLastError object An containing the HResult object
     *         containing the error code.
    */
    HResultFromLastError SetFileAttributesByHandle(
        _In_ HANDLE FileHandle,
        _In_ DWORD FileAttributes);

    /**
     * @brief Retrieves hardlink count for a specified file.
     * @param FileHandle A handle to the file that contains the information to
     *                   be retrieved. This handle should not be a pipe handle.
     * @param HardlinkCount The hardlink count for the file.
     * @return An HResultFromLastError object An containing the HResult object
     *         containing the error code.
    */
    Mile::HResultFromLastError GetFileHardlinkCountByHandle(
        _In_ HANDLE FileHandle,
        _Out_ PDWORD HardlinkCount);

    /**
     * @brief Deletes an existing file.
     * @param FileHandle The handle of the file to be deleted. This handle must
     *                   be opened with the appropriate permissions for the
     *                   requested change. This handle should not be a pipe
     *                   handle.
     * @return An HResultFromLastError object An containing the HResult object
     *         containing the error code.
    */
    HResultFromLastError DeleteFileByHandle(
        _In_ HANDLE FileHandle);

    /**
     * @brief Deletes an existing file, even the file have the readonly
     *        attribute.
     * @param FileHandle The handle of the file to be deleted. This handle must
     *                   be opened with the appropriate permissions for the
     *                   requested change. This handle should not be a pipe
     *                   handle.
     * @return An HResult object containing the error code.
    */
    HResult DeleteFileByHandleIgnoreReadonlyAttribute(
        _In_ HANDLE FileHandle);

    /**
     * @brief Tests for the current directory and parent directory markers
     *        while iterating through files.
     * @param Name The name of the file or directory for testing.
     * @return Nonzero if the found file has the name "." or "..", which
     *         indicates that the found file is actually a directory. Otherwise
     *         zero.
    */
    BOOL IsDotsName(
        _In_ LPCWSTR Name);

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)

    /**
     * @brief Loads the specified module in the system directory into the
     *        address space of the calling process. The specified module may
     *        cause other modules to be loaded.
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
     *         module. If the function fails, the return value is nullptr. To
     *         get extended error information, call GetLastError.
    */
    HMODULE LoadLibraryFromSystem32(
        _In_ LPCWSTR lpLibFileName);

#endif

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)

    /**
     * @brief Enables the Per-Monitor DPI Aware for the specified dialog.
     * @return If the function fails, the return value is -1.
     * @remarks You need to use this function in Windows 10 Threshold 1 or
     *          Windows 10 Threshold 2.
    */
    INT EnablePerMonitorDialogScaling();

#endif

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)

    /**
     * @brief Enables WM_DPICHANGED message for child window for the associated
     *        window.
     * @param WindowHandle The window you want to enable WM_DPICHANGED message
     *                     for child window.
     * @return If the function succeeds, the return value is non-zero. If the
     *         function fails, the return value is zero.
     * @remarks You need to use this function in Windows 10 Threshold 1 or
     *          Windows 10 Threshold 2.
    */
    BOOL EnableChildWindowDpiMessage(
        _In_ HWND WindowHandle);

#endif

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)

    /**
     * @brief Queries the dots per inch (dpi) of a display.
     * @param hMonitor Handle of the monitor being queried.
     * @param dpiType The type of DPI being queried. Possible values are from
     *                the MONITOR_DPI_TYPE enumeration.
     * @param dpiX The value of the DPI along the X axis. This value always
     *             refers to the horizontal edge, even when the screen is
     *             rotated.
     * @param dpiY The value of the DPI along the Y axis. This value always
     *             refers to the vertical edge, even when the screen is
     *             rotated.
     * @return An HResult object containing the error code.
     * @remark For more information, see GetDpiForMonitor.
    */
    HResult GetDpiForMonitor(
        _In_ HMONITOR hMonitor,
        _In_ MONITOR_DPI_TYPE dpiType,
        _Out_ UINT* dpiX,
        _Out_ UINT* dpiY);

#endif

    /**
     * @brief Retrieves the number of milliseconds that have elapsed since the
     *        system was started.
     * @return The number of milliseconds.
    */
    ULONGLONG GetTickCount();

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)

    /**
     * @brief Starts a service if not started and retrieves the current status
     *        of the specified service.
     * @param ServiceName The name of the service to be started. This is the
     *                    name specified by the ServiceName parameter of the
     *                    CreateService function when the service object was
     *                    created, not the service display name that is shown
     *                    by user interface applications to identify the
     *                    service. The maximum string length is 256 characters.
     *                    The service control manager database preserves the
     *                    case of the characters, but service name comparisons
     *                    are always case insensitive. Forward-slash (/) and
     *                    backslash (\) are invalid service name characters.
     * @param ServiceStatus A pointer to the process status information for a
     *                      service.
     * @return An HResult object containing the error code.
    */
    HResult StartServiceW(
        _In_ LPCWSTR ServiceName,
        _Out_ LPSERVICE_STATUS_PROCESS ServiceStatus);

#endif

    /**
     * @brief Creates a thread to execute within the virtual address space of
     *        the calling process.
     * @param lpThreadAttributes A pointer to a SECURITY_ATTRIBUTES structure
     *                           that determines whether the returned handle
     *                           can be inherited by child processes.
     * @param dwStackSize The initial size of the stack, in bytes.
     * @param lpStartAddress A pointer to the application-defined function to
     *                       be executed by the thread.
     * @param lpParameter A pointer to a variable to be passed to the thread.
     * @param dwCreationFlags The flags that control the creation of the
     *                        thread.
     * @param lpThreadId A pointer to a variable that receives the thread
     *                   identifier.
     * @return If the function succeeds, the return value is a handle to the
     *         new thread. If the function fails, the return value is nullptr.
     *         To get extended error information, call GetLastError.
     * @remark For more information, see CreateThread.
    */
    HANDLE CreateThread(
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
    DWORD GetNumberOfHardwareThreads();

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)

    /**
     * @brief Obtains the primary access token of the logged-on user specified
     *        by the session ID. To call this function successfully, the
     *        calling application must be running within the context of the
     *        LocalSystem account and have the SE_TCB_NAME privilege.
     * @param SessionId A Remote Desktop Services session identifier.
     * @param TokenHandle If the function succeeds, receives a pointer to the
     *                    token handle for the logged-on user. Note that you
     *                    must call the CloseHandle function to close this
     *                    handle.
     * @return An HResultFromLastError object An containing the HResult object
     *         containing the error code.
     * @remark For more information, see WTSQueryUserToken.
    */
    HResultFromLastError CreateSessionToken(
        _In_ DWORD SessionId,
        _Out_ PHANDLE TokenHandle);

#endif

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)

    /**
     * @brief Obtains the primary access token of the SYSTEM user. To call this
     *        function successfully, the calling application must be running
     *        within the context of the Administrator account and have the
     *        SE_DEBUG_NAME privilege enabled.
     * @param DesiredAccess The access to the process object. This access right
     *                      is checked against the security descriptor for the
     *                      process. This parameter can be one or more of the
     *                      process access rights.
     * @param TokenHandle If the function succeeds, receives a pointer to the
     *                    token handle for the SYSTEM user. Note that you
     *                    must call the CloseHandle function to close this
     *                    handle.
     * @return An HResultFromLastError object An containing the HResult object
     *         containing the error code.
    */
    HResultFromLastError CreateSystemToken(
        _In_ DWORD DesiredAccess,
        _Out_ PHANDLE TokenHandle);

#endif

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)

    /**
     * @brief Retrieves the session identifier of the active session.
     * @return The session identifier of the active session. If there is no
     *         active session attached, this function returns 0xFFFFFFFF.
    */
    DWORD GetActiveSessionID();

#endif

    /**
     * @brief Sets mandatory label for a specified access token. The
     *        information that this function sets replaces existing
     *        information. The calling process must have appropriate access
     *        rights to set the information.
     * @param TokenHandle A handle to the access token for which information is
     *                    to be set.
     * @param MandatoryLabelRid The value of the mandatory label for the
     *                          process. This parameter can be one of the
     *                          following values.
     *                          SECURITY_MANDATORY_UNTRUSTED_RID
     *                          SECURITY_MANDATORY_LOW_RID
     *                          SECURITY_MANDATORY_MEDIUM_RID
     *                          SECURITY_MANDATORY_MEDIUM_PLUS_RID
     *                          SECURITY_MANDATORY_HIGH_RID
     *                          SECURITY_MANDATORY_SYSTEM_RID
     *                          SECURITY_MANDATORY_PROTECTED_PROCESS_RID
     * @return An HResultFromLastError object An containing the HResult object
     *         containing the error code.
    */
    HResultFromLastError SetTokenMandatoryLabel(
        _In_ HANDLE TokenHandle,
        _In_ DWORD MandatoryLabelRid);

    /**
     * @brief Retrieves a specified type of information about an access token.
     *        The calling process must have appropriate access rights to obtain
     *        the information.
     * @param TokenHandle A handle to an access token from which information is
     *                    retrieved.
     * @param TokenInformationClass Specifies a value from the
     *                              TOKEN_INFORMATION_CLASS enumerated type to
     *                              identify the type of information the
     *                              function retrieves.
     * @param OutputInformation A pointer to a buffer the function fills with
     *                          the requested information. When you have
     *                          finished using the information, free it by
     *                          calling the Mile::HeapMemory::Free method. You
     *                          should also set the pointer to nullptr.
     * @return An HResultFromLastError object An containing the HResult object
     *         containing the error code.
     * @remark For more information, see GetTokenInformation.
    */
    HResultFromLastError GetTokenInformationWithMemory(
        _In_ HANDLE TokenHandle,
        _In_ TOKEN_INFORMATION_CLASS TokenInformationClass,
        _Out_ PVOID* OutputInformation);

    /**
     * @brief Creates a new access token that is a LUA version of an existing
     *        access token.
     * @param ExistingTokenHandle A handle to a primary or impersonation token.
     *                            The token can also be a restricted token. The
     *                            handle must have TOKEN_DUPLICATE access to
     *                            the token.
     * @param TokenHandle A pointer to a variable that receives a handle to the
     *                    new restricted token.
     * @return An HResult object containing the error code.
    */
    HResult CreateLUAToken(
        _In_ HANDLE ExistingTokenHandle,
        _Out_ PHANDLE TokenHandle);

    /**
     * @brief Creates a single uninitialized object of the class associated
     *        with a specified CLSID.
     * @param lpszCLSID The string representation of the CLSID.
     * @param pUnkOuter If nullptr, indicates that the object is not being
     *                  created as part of an aggregate. If non-nullptr,
     *                  pointer to the aggregate object's IUnknown interface
     *                  (the controlling IUnknown).
     * @param dwClsContext Context in which the code that manages the newly
     *                     created object will run. The values are taken from
     *                     the enumeration CLSCTX.
     * @param lpszIID The string representation of the IID.
     * @param ppv Address of pointer variable that receives the interface
     *            pointer requested in riid. Upon successful return, *ppv
     *            contains the requested interface pointer. Upon failure, *ppv
     *            contains nullptr.
     * @return An HResult object containing the error code.
    */
    HResult CoCreateInstanceByString(
        _In_ LPCWSTR lpszCLSID,
        _In_opt_ LPUNKNOWN pUnkOuter,
        _In_ DWORD dwClsContext,
        _In_ LPCWSTR lpszIID,
        _Out_ LPVOID* ppv);

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)

    /**
     * @brief Retrieves the string type data for the specified value name
     *        associated with an open registry key.
     * @param hKey A handle to an open registry key.
     * @param lpValueName The name of the registry value.
     * @param lpData A pointer to a buffer that receives the value's data. When
     *               you have finished using the information, free it by
     *               calling the Mile::HeapMemory::Free method. You should also
     *               set the pointer to nullptr.
     * @return An HResult object containing the error code.
    */
    HResult RegQueryStringValue(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpValueName,
        _Out_ LPWSTR* lpData);

#endif

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)

    /**
     * @brief Determines whether the interface id have the correct interface
     *        name.
     * @param InterfaceID A pointer to the string representation of the IID.
     * @param InterfaceName A pointer to the interface name string.
     * @return An HResult object containing the error code.
    */
    HResult CoCheckInterfaceName(
        _In_ LPCWSTR InterfaceID,
        _In_ LPCWSTR InterfaceName);

#endif

    /**
     * @brief Opens the access token associated with a process.
     * @param ProcessId The identifier of the local process to be opened.
     * @param DesiredAccess The access to the process object. This access right
     *                      is checked against the security descriptor for the
     *                      process. This parameter can be one or more of the
     *                      process access rights.
     * @param TokenHandle A pointer to a handle that identifies the newly
     *                    opened access token when the function returns.
     * @return An HResultFromLastError object An containing the HResult object
     *         containing the error code.
    */
    HResultFromLastError OpenProcessTokenByProcessId(
        _In_ DWORD ProcessId,
        _In_ DWORD DesiredAccess,
        _Out_ PHANDLE TokenHandle);

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)

    /**
     * @brief Opens the access token associated with a service process, the
     *        calling application must be running within the context of the
     *        Administrator account and have the SE_DEBUG_NAME privilege
     *        enabled.
     * @param ServiceName The name of the service to be started. This is the
     *                    name specified by the ServiceName parameter of the
     *                    CreateService function when the service object was
     *                    created, not the service display name that is shown
     *                    by user interface applications to identify the
     *                    service. The maximum string length is 256 characters.
     *                    The service control manager database preserves the
     *                    case of the characters, but service name comparisons
     *                    are always case insensitive. Forward-slash (/) and
     *                    backslash (\) are invalid service name characters.
     * @param DesiredAccess The access to the process object. This access right
     *                      is checked against the security descriptor for the
     *                      process. This parameter can be one or more of the
     *                      process access rights.
     * @param TokenHandle A pointer to a handle that identifies the newly
     *                    opened access token when the function returns.
     * @return An HResult object containing the error code.
    */
    HResult OpenServiceProcessToken(
        _In_ LPCWSTR ServiceName,
        _In_ DWORD DesiredAccess,
        _Out_ PHANDLE TokenHandle);

#endif

    /**
     * @brief Enables or disables privileges in the specified access token.
     * @param TokenHandle A handle to the access token that contains the
     *                    privileges to be modified. The handle must have
     *                    TOKEN_ADJUST_PRIVILEGES access to the token.
     * @param Privileges A pointer to an array of LUID_AND_ATTRIBUTES
     *                   structures that specifies an array of privileges and
     *                   their attributes. Each structure contains the LUID and
     *                   attributes of a privilege. To get the name of the
     *                   privilege associated with a LUID, call the
     *                   LookupPrivilegeValue function, passing the address of
     *                   the LUID as the value of the lpLuid parameter. The
     *                   attributes of a privilege can be a combination of the
     *                   following values.
     *                   SE_PRIVILEGE_ENABLED
     *                       The function enables the privilege.
     *                   SE_PRIVILEGE_REMOVED
     *                       The privilege is removed from the list of
     *                       privileges in the token.
     *                   None
     *                       The function disables the privilege.
     * @param PrivilegeCount The number of entries in the Privileges array.
     * @return An HResult object containing the error code.
    */
    HResult AdjustTokenPrivilegesSimple(
        _In_ HANDLE TokenHandle,
        _In_ PLUID_AND_ATTRIBUTES Privileges,
        _In_ DWORD PrivilegeCount);

    /**
     * @brief Enables or disables all privileges in the specified access token.
     * @param TokenHandle A handle to the access token that contains the
     *                    privileges to be modified. The handle must have
     *                    TOKEN_ADJUST_PRIVILEGES access to the token.
     * @param Attributes The attributes of all privileges can be a combination
     *                   of the following values.
     *                   SE_PRIVILEGE_ENABLED
     *                       The function enables the privilege.
     *                   SE_PRIVILEGE_REMOVED
     *                       The privilege is removed from the list of
     *                       privileges in the token.
     *                   None
     *                       The function disables the privilege.
     * @return An HResult object containing the error code.
    */
    HResult AdjustTokenAllPrivileges(
        _In_ HANDLE TokenHandle,
        _In_ DWORD Attributes);

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)

    /**
     * @brief Obtain the best matching resource with the specified type and
     *        name in the specified module.
     * @param ResourceInfo The resource info which contains the pointer and
     *                     size.
     * @param ModuleHandle A handle to the module whose portable executable
     *                     file or an accompanying MUI file contains the
     *                     resource. If this parameter is nullptr, the
     *                     function searches the module used to create the
     *                     current process.
     * @param Type The resource type. Alternately, rather than a pointer,
     *             this parameter can be MAKEINTRESOURCE(ID), where ID is
     *             the integer identifier of the given resource type.
     * @param Name The name of the resource. Alternately, rather than a
     *             pointer, this parameter can be MAKEINTRESOURCE(ID),
     *             where ID is the integer identifier of the resource.
     * @return An HResultFromLastError object An containing the HResult object
     *         containing the error code.
    */
    HResultFromLastError LoadResource(
        _Out_ PRESOURCE_INFO ResourceInfo,
        _In_opt_ HMODULE ModuleHandle,
        _In_ LPCWSTR Type,
        _In_ LPCWSTR Name);

#endif

#pragma endregion

#pragma region Definitions for Windows (C++ Style)

    /**
     * @brief Retrieves the message for the error represented by the HResult object.
     * @param Value The HResult object which need to retrieve the message.
     * @return A std::wstring containing the error messsage.
    */
    std::wstring GetHResultMessage(
        HResult const& Value);

    /**
     * @brief Converts from the UTF-8 string to the UTF-16 string.
     * @param Utf8String The UTF-8 string you want to convert.
     * @return A converted UTF-16 string if successful, an empty string
     *         otherwise.
    */
    std::wstring ToUtf16String(
        std::string const& Utf8String);

    /**
     * @brief Converts from the UTF-16 string to the UTF-8 string.
     * @param Utf16String The UTF-16 string you want to convert.
     * @return A converted UTF-8 string if successful, an empty string
     *         otherwise.
    */
    std::string ToUtf8String(
        std::wstring const& Utf16String);

    /**
     * @brief Converts from the UTF-16 string to the string encoded with the
     *        console code page.
     * @param Utf16String The UTF-16 string you want to convert.
     * @return A converted string encoded with the console code page if
     *         successful, an empty string otherwise.
    */
    std::string ToConsoleString(
        std::wstring const& Utf16String);

    /**
     * @brief Retrieves the path of the system directory. The system directory
     *        contains system files such as dynamic-link libraries and drivers.
     * @return The path of the system directory if successful, an empty string
     *         otherwise.
    */
    std::wstring GetSystemDirectoryW();

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)

    /**
     * @brief Retrieves the path of the shared Windows directory on a
     *        multi-user system.
     * @return The path of the shared Windows directory on a multi-user system
     *         if successful, an empty string otherwise.
    */
    std::wstring GetWindowsDirectoryW();

#endif

    /**
     * @brief Expands environment variable strings and replaces them with the
     *        values defined for the current user.
     * @param SourceString The string that contains one or more environment
                           variable strings (in the %variableName% form) you
                           need to expand.
     * @return The result string of expanding the environment variable strings
     *         if successful, an empty string otherwise.
    */
    std::wstring ExpandEnvironmentStringsW(
        std::wstring const& SourceString);

    /**
     * @brief Retrieves the path of the executable file of the current process.
     * @return The path of the executable file of the current process if
     *         successful, an empty string otherwise.
    */
    std::wstring GetCurrentProcessModulePath();

    /**
     * @brief Creates a thread to execute within the virtual address space of
     *        the calling process.
     * @tparam FuncType The function type.
     * @param StartFunction The start function.
     * @param lpThreadAttributes A pointer to a SECURITY_ATTRIBUTES structure
     *                           that determines whether the returned handle
     *                           can be inherited by child processes.
     * @param dwStackSize The initial size of the stack, in bytes.
     * @param dwCreationFlags The flags that control the creation of the
     *                        thread.
     * @param lpThreadId A pointer to a variable that receives the thread
     *                   identifier.
     * @return If the function succeeds, the return value is a handle to the
     *         new thread. If the function fails, the return value is nullptr.
     *         To get extended error information, call GetLastError.
     * @remark For more information, see CreateThread.
    */
    template<class FuncType>
    HANDLE CreateThread(
        _In_ FuncType&& StartFunction,
        _In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes = nullptr,
        _In_ SIZE_T dwStackSize = 0,
        _In_ DWORD dwCreationFlags = 0,
        _Out_opt_ LPDWORD lpThreadId = nullptr)
    {
        auto ThreadFunctionInternal = [](LPVOID lpThreadParameter) -> DWORD
        {
            auto function = reinterpret_cast<FuncType*>(
                lpThreadParameter);
            (*function)();
            delete function;
            return 0;
        };

        return Mile::CreateThread(
            lpThreadAttributes,
            dwStackSize,
            ThreadFunctionInternal,
            reinterpret_cast<LPVOID>(new FuncType(std::move(StartFunction))),
            dwCreationFlags,
            lpThreadId);
    }

    /**
     * @brief Write formatted data to a UTF-16 string.
     * @param Format Format-control string.
     * @param ArgList Pointer to list of optional arguments to be formatted.
     * @return A formatted string if successful, an empty string otherwise.
    */
    std::wstring VFormatUtf16String(
        _In_z_ _Printf_format_string_ wchar_t const* const Format,
        _In_z_ _Printf_format_string_ va_list ArgList);

    /**
     * @brief Write formatted data to a UTF-8 string.
     * @param Format Format-control string.
     * @param ArgList Pointer to list of optional arguments to be formatted.
     * @return A formatted string if successful, an empty string otherwise.
    */
    std::string VFormatUtf8String(
        _In_z_ _Printf_format_string_ char const* const Format,
        _In_z_ _Printf_format_string_ va_list ArgList);

    /**
     * @brief Write formatted data to a UTF-16 string.
     * @param Format Format-control string.
     * @param ... Optional arguments to be formatted.
     * @return A formatted string if successful, an empty string otherwise.
    */
    std::wstring FormatUtf16String(
        _In_z_ _Printf_format_string_ wchar_t const* const Format,
        ...);

    /**
     * @brief Write formatted data to a UTF-8 string.
     * @param Format Format-control string.
     * @param ... Optional arguments to be formatted.
     * @return A formatted string if successful, an empty string otherwise.
    */
    std::string FormatUtf8String(
        _In_z_ _Printf_format_string_ char const* const Format,
        ...);

    /**
     * @brief Converts a numeric value into a UTF-16 string that represents
     *        the number expressed as a size value in byte, bytes, kibibytes,
     *        mebibytes, gibibytes, tebibytes, pebibytes or exbibytes,
     *        depending on the size.
     * @param ByteSize The numeric byte size value to be converted.
     * @return A formatted string if successful, an empty string otherwise.
    */
    std::wstring ConvertByteSizeToUtf16String(
        std::uint64_t ByteSize);

    /**
     * @brief Converts a numeric value into a UTF-8 string that represents
     *        the number expressed as a size value in byte, bytes, kibibytes,
     *        mebibytes, gibibytes, tebibytes, pebibytes or exbibytes,
     *        depending on the size.
     * @param ByteSize The numeric byte size value to be converted.
     * @return A formatted string if successful, an empty string otherwise.
    */
    std::string ConvertByteSizeToUtf8String(
        std::uint64_t ByteSize);

    /**
     * @brief Enumerates files in a directory.
     * @tparam CallbackType The callback type.
     * @param FileHandle The handle of the file to be searched a directory for
     *                   a file or subdirectory with a name. This handle must
     *                   be opened with the appropriate permissions for the
     *                   requested change. This handle should not be a pipe
     *                   handle.
     * @param CallbackFunction The file enumerate callback function. 
     * @return An HResult object containing the error code.
    */
    template<class CallbackType>
    HResult EnumerateFile(
        _In_ HANDLE FileHandle,
        _In_ CallbackType&& CallbackFunction)
    {
        Mile::HResult hr = S_OK;

        CallbackType* CallbackObject = new CallbackType(
            std::move(CallbackFunction));
        if (CallbackObject)
        {
            auto FileEnumerateCallback = [](
                _In_ Mile::PFILE_ENUMERATE_INFORMATION Information,
                _In_opt_ LPVOID Context) -> BOOL
            {
                auto Callback = reinterpret_cast<CallbackType*>(Context);
                BOOL Result = (*Callback)(Information);

                return Result;
            };

            hr = Mile::EnumerateFile(
                FileHandle,
                FileEnumerateCallback,
                reinterpret_cast<LPVOID>(CallbackObject));

            delete CallbackObject;
        }
        else
        {
            hr = E_OUTOFMEMORY;
        }

        return hr;
    }

#pragma endregion
}

#endif // !MILE_WINDOWS
