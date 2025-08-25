/*
 * PROJECT:    Mouri Internal Library Essentials
 * FILE:       Mile.Helpers.CppBase.h
 * PURPOSE:    Definition for the essential Windows helper functions
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#pragma once

#ifndef MILE_WINDOWS_HELPERS_CPPBASE
#define MILE_WINDOWS_HELPERS_CPPBASE

#include "Mile.Helpers.Base.h"

#include <string>
#include <vector>

namespace Mile
{
    /**
     * @brief Write formatted data to a wide characters string, assumed UTF-16
     *        in Windows.
     * @param Format Format-control string.
     * @param ArgList Pointer to list of optional arguments to be formatted.
     * @return A formatted string if successful, an empty string otherwise.
    */
    std::wstring VFormatWideString(
        _In_ wchar_t const* const Format,
        _In_ va_list ArgList);

    /**
     * @brief Write formatted data to a wide characters string, assumed UTF-16
     *        in Windows.
     * @param Format Format-control string.
     * @param ... Optional arguments to be formatted.
     * @return A formatted string if successful, an empty string otherwise.
    */
    std::wstring FormatWideString(
        _In_ wchar_t const* const Format,
        ...);

    /**
     * @brief Write formatted data to a onebyte or multibyte string, suggested
     *        encoding with UTF-8.
     * @param Format Format-control string.
     * @param ArgList Pointer to list of optional arguments to be formatted.
     * @return A formatted string if successful, an empty string otherwise.
    */
    std::string VFormatString(
        _In_ char const* const Format,
        _In_ va_list ArgList);

    /**
     * @brief Write formatted data to a onebyte or multibyte string, suggested
     *        encoding with UTF-8.
     * @param Format Format-control string.
     * @param ... Optional arguments to be formatted.
     * @return A formatted string if successful, an empty string otherwise.
    */
    std::string FormatString(
        _In_ char const* const Format,
        ...);

    /**
     * @brief Converts from the onebyte or multibyte string to the wide
     *        characters string.
     * @param CodePage Code page to use in performing the conversion. This
     *                 parameter can be set to the value of any code page
     *                 that is installed or available in the operating system.
     * @param InputString The onebyte or multibyte string you want to convert.
     * @return A converted wide characters string if successful, an empty
     *         string otherwise.
     * @remark For more information, see MultiByteToWideChar.
    */
    std::wstring ToWideString(
        std::uint32_t CodePage,
        std::string_view const& InputString);

    /**
     * @brief Converts from the wide characters string to the onebyte or
     *        multibyte string.
     * @param CodePage Code page to use in performing the conversion. This
     *                 parameter can be set to the value of any code page
     *                 that is installed or available in the operating system.
     * @param InputString The wide characters string you want to convert.
     * @return A converted onebyte or multibyte string if successful, an empty
     *         string otherwise.
     * @remark For more information, see WideCharToMultiByte.
    */
    std::string ToString(
        std::uint32_t CodePage,
        std::wstring_view const& InputString);

    /**
     * @brief Disables C++ class copy construction.
    */
    class DisableCopyConstruction
    {
    protected:

        DisableCopyConstruction() = default;
        ~DisableCopyConstruction() = default;

    private:

        DisableCopyConstruction(
            const DisableCopyConstruction&) = delete;
        DisableCopyConstruction& operator=(
            const DisableCopyConstruction&) = delete;
    };

    /**
     * @brief Disables C++ class move construction.
    */
    class DisableMoveConstruction
    {
    protected:

        DisableMoveConstruction() = default;
        ~DisableMoveConstruction() = default;

    private:

        DisableMoveConstruction(
            const DisableMoveConstruction&&) = delete;
        DisableMoveConstruction& operator=(
            const DisableMoveConstruction&&) = delete;
    };

    /**
     * @brief The template for defining the task when exit the scope.
     * @tparam TaskHandlerType The type of the task handler.
     * @remark For more information, see ScopeGuard.
    */
    template<typename TaskHandlerType>
    class ScopeExitTaskHandler :
        DisableCopyConstruction,
        DisableMoveConstruction
    {
    private:

        bool m_Canceled;
        TaskHandlerType m_TaskHandler;

    public:

        /**
         * @brief Creates the instance for the task when exit the scope.
         * @param EventHandler The instance of the task handler.
        */
        explicit ScopeExitTaskHandler(TaskHandlerType&& EventHandler) :
            m_Canceled(false),
            m_TaskHandler(std::forward<TaskHandlerType>(EventHandler))
        {

        }

        /**
         * @brief Executes and uninitializes the instance for the task when
         *        exit the scope.
        */
        ~ScopeExitTaskHandler()
        {
            if (!this->m_Canceled)
            {
                this->m_TaskHandler();
            }
        }

        /**
         * @brief Cancels the task when exit the scope.
        */
        void Cancel()
        {
            this->m_Canceled = true;
        }
    };

    /**
     * @brief Parses a command line string and returns an array of the command
     *        line arguments, along with a count of such arguments, in a way
     *        that is similar to the standard C run-time.
     * @param CommandLine A string that contains the full command line. If this
     *                    parameter is an empty string the function returns an
     *                    array with only one empty string.
     * @return An array of the command line arguments, along with a count of
     *         such arguments.
    */
    std::vector<std::wstring> SplitCommandLineWideString(
        std::wstring const& CommandLine);

    /**
     * @brief Parses a command line string and returns an array of the command
     *        line arguments, along with a count of such arguments, in a way
     *        that is similar to the standard C run-time.
     * @param CommandLine A string that contains the full command line. If this
     *                    parameter is an empty string the function returns an
     *                    array with only one empty string.
     * @return An array of the command line arguments, along with a count of
     *         such arguments.
    */
    std::vector<std::string> SplitCommandLineString(
        std::string const& CommandLine);

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

        return ::MileCreateThread(
            lpThreadAttributes,
            dwStackSize,
            ThreadFunctionInternal,
            reinterpret_cast<LPVOID>(new FuncType(std::move(StartFunction))),
            dwCreationFlags,
            lpThreadId);
    }

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
    BOOL EnumerateFileByHandle(
        _In_ HANDLE FileHandle,
        _In_ CallbackType&& CallbackFunction)
    {
        BOOL Result = FALSE;

        CallbackType* CallbackObject = new CallbackType(
            std::move(CallbackFunction));
        if (CallbackObject)
        {
            auto FileEnumerateCallback = [](
                _In_ PMILE_FILE_ENUMERATE_INFORMATION Information,
                _In_opt_ LPVOID Context) -> BOOL
            {
                auto Callback = reinterpret_cast<CallbackType*>(Context);
                BOOL Result = (*Callback)(Information);

                return Result;
            };

            Result = ::MileEnumerateFileByHandle(
                FileHandle,
                FileEnumerateCallback,
                reinterpret_cast<LPVOID>(CallbackObject));

            delete CallbackObject;
        }
        else
        {
            ::SetLastError(ERROR_OUTOFMEMORY);
        }

        return Result;
    }

    /**
     * @brief Converts from the onebyte or multibyte string to the Int32 integer.
     * @param Source The onebyte or multibyte string.
     * @param Radix The number base to use.
     * @return The converted Int32 integer value.
     * @remark For more information, see strtol.
     */
    std::int32_t ToInt32(
        std::string const& Source,
        std::uint8_t const& Radix = 10);

    /**
     * @brief Converts from the onebyte or multibyte string to the Int64 integer.
     * @param Source The onebyte or multibyte string.
     * @param Radix The number base to use.
     * @return The converted Int64 integer value.
     * @remark For more information, see strtoll.
     */
    std::int64_t ToInt64(
        std::string const& Source,
        std::uint8_t const& Radix = 10);

    /**
     * @brief Converts from the onebyte or multibyte string to the UInt32 integer.
     * @param Source The onebyte or multibyte string.
     * @param Radix The number base to use.
     * @return The converted UInt32 integer value.
     * @remark For more information, see strtoul.
     */
    std::uint32_t ToUInt32(
        std::string const& Source,
        std::uint8_t const& Radix = 10);

    /**
     * @brief Converts from the onebyte or multibyte string to the UInt64 integer.
     * @param Source The onebyte or multibyte string.
     * @param Radix The number base to use.
     * @return The converted UInt64 integer value.
     * @remark For more information, see strtoull.
     */
    std::uint64_t ToUInt64(
        std::string const& Source,
        std::uint8_t const& Radix = 10);

    /**
     * @brief The COM object interface chaining helper template struct.
     */
    template <typename I0, typename I1, typename ...Interfaces>
    struct ComObjectChainHelper
    {
        static_assert(
            std::is_base_of<I1, I0>::value,
            "Interface has to derive from I0");

        static bool Includes(
            _In_ REFIID riid)
        {
            return riid == __uuidof(I1)
                || ComObjectChainHelper<I0, Interfaces...>::Includes(riid);
        }
    };

    /**
     * @brief Specialized COM object interface chaining helper template struct
     *        for the case when there are only two interfaces.
     */
    template <typename I0, typename I1>
    struct ComObjectChainHelper<I0, I1>
    {
        static_assert(
            std::is_base_of<I1, I0>::value,
            "Interface has to derive from I0");

        static bool Includes(
            _In_ REFIID riid)
        {
            return riid == __uuidof(I0) || riid == __uuidof(I1);
        }
    };

    /**
     * @brief Primary COM object interfaces query helper template struct.
     */
    template <typename ...Interfaces>
    struct ComObjectQueryHelper;

    /**
     * @brief Specialized primary COM object interfaces query helper template
     *        struct for handling multiple interfaces.
     */
    template <typename I0, typename ...Interfaces>
    struct ComObjectQueryHelper<I0, Interfaces...> :
        I0,
        ComObjectQueryHelper<Interfaces...>
    {
        bool Matches(
            _In_ REFIID riid,
            _Out_ LPVOID* ppvObject)
        {
            if (riid == __uuidof(I0))
            {
                *ppvObject = static_cast<I0*>(this);
                return true;
            }

            return ComObjectQueryHelper<Interfaces...>::Matches(
                riid,
                ppvObject);
        }
    };

    /**
     * @brief Specialized primary COM object interfaces query helper template
     *        struct for handling interface chains.
     */
    template <typename I0, typename ...Chain, typename ...Interfaces>
    struct ComObjectQueryHelper<
        ComObjectChainHelper<I0, Chain...>,
        Interfaces...> :
        ComObjectQueryHelper<I0, Interfaces...>
    {
        bool Matches(
            _In_ REFIID riid,
            _Out_ LPVOID* ppvObject)
        {
            if (ComObjectChainHelper<I0, Chain...>::Includes(riid))
            {
                *ppvObject = reinterpret_cast<I0*>(this);
                return true;
            }

            return ComObjectQueryHelper<I0, Interfaces...>::Matches(
                riid,
                ppvObject);
        }
    };

    /**
     * @brief Specialized primary COM object interfaces query helper template
     *        struct for terminal case which always returns false.
     */
    template <>
    struct ComObjectQueryHelper<>
    {
        bool Matches(
            _In_ REFIID riid,
            _Out_ LPVOID* ppvObject)
        {
            UNREFERENCED_PARAMETER(riid);
            *ppvObject = nullptr;
            return false;
        }
    };

    /**
     * @brief The template class for implementing COM objects.
     */
    template <class Derived, typename... Interfaces>
    class ComObject : public ComObjectQueryHelper<Interfaces...>
    {
    private:

        ULONG m_ReferenceCount = 1;

    public:

        ComObject() = default;

        virtual ~ComObject() = default;

        virtual HRESULT STDMETHODCALLTYPE QueryInterface(
            _In_ REFIID riid,
            _Out_ LPVOID* ppvObject) noexcept override
        {
            if (!ppvObject)
            {
                return E_POINTER;
            }
            *ppvObject = nullptr;

            if (ComObjectQueryHelper<Interfaces...>::Matches(riid, ppvObject))
            {
                this->AddRef();
                return S_OK;
            }

            if (riid == __uuidof(IUnknown))
            {
                *ppvObject = reinterpret_cast<LPVOID>(
                    reinterpret_cast<IUnknown*>(this));
                this->AddRef();
                return S_OK;
            }

            return E_NOINTERFACE;
        }

        virtual ULONG STDMETHODCALLTYPE AddRef() noexcept override
        {
            return ::InterlockedIncrement(&this->m_ReferenceCount);
        }

        virtual ULONG STDMETHODCALLTYPE Release() noexcept override
        {
            ULONG CurrentReferenceCount =
                ::InterlockedDecrement(&this->m_ReferenceCount);
            if (CurrentReferenceCount == 0)
            {
                delete this;
            }
            return CurrentReferenceCount;
        }
    };
}

#endif // !MILE_WINDOWS_HELPERS_CPPBASE
