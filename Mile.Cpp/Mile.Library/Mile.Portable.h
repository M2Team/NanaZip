/*
 * PROJECT:   Mouri Internal Library Essentials
 * FILE:      Mile.Portable.h
 * PURPOSE:   Definition for all platforms
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: Mouri_Naruto (Mouri_Naruto AT Outlook.com)
 */

#ifndef MILE_PORTABLE
#define MILE_PORTABLE

#if (defined(__cplusplus) && __cplusplus >= 201402L)
#elif (defined(_MSVC_LANG) && _MSVC_LANG >= 201402L)
#else
#error "[Mile] You should use a C++ compiler with the C++14 standard."
#endif

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace Mile
{
    /**
     * @brief Suppresses the "unreferenced parameter" compiler warning by
     *        referencing a variable which is not used.
     * @tparam VariableType The type of the variable which is not used.
     * @param The variable which is not used.
     * @remark For more information, see UNREFERENCED_PARAMETER.
    */
    template<typename VariableType>
    void UnreferencedParameter(VariableType const&)
    {
    }

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
     * @brief The template for defining the unique objects.
     * @tparam TraitsType A traits type that specifies the kind of unique
     *                    object being represented.
    */
    template <typename TraitsType>
    class UniqueObject : DisableCopyConstruction
    {
    public:

        /**
         * @brief The object type alias of the unique object.
        */
        using ObjectType = typename TraitsType::Type;

    private:

        /**
         * @brief The raw value of the unique object.
        */
        ObjectType m_Value = TraitsType::Invalid();

    public:

        /**
         * @brief Closes the underlying unique object.
        */
        void Close() noexcept
        {
            if (*this)
            {
                TraitsType::Close(this->m_Value);
                this->m_Value = TraitsType::Invalid();
            }
        }

        /**
         * @brief Returns the underlying unique object value, should you need
         *        to pass it to a function.
         * @return The underlying unique object value represented by the unique
         *         object.
        */
        ObjectType Get() const noexcept
        {
            return this->m_Value;
        }

        /**
         * @brief Returns the address of the unique object value; this function
         *        helps you call methods that return references as out
         *        parameters via a pointer to a unique object.
         * @return The address of the underlying unique object value
         *         represented by the unique object.
        */
        ObjectType* Put() noexcept
        {
            return &this->m_Value;
        }

        /**
         * @brief Attaches to a unique object value, and takes ownership of it.
         * @param Value A unique object value to attach to.
        */
        void Attach(ObjectType Value) noexcept
        {
            this->Close();
            *this->Put() = Value;
        }

        /**
         * @brief Detaches from the underlying unique object value.
         * @return The underlying unique object value formerly represented by
         *         the unique object.
        */
        ObjectType Detach() noexcept
        {
            ObjectType Value = this->m_Value;
            this->m_Value = TraitsType::Invalid();
            return Value;
        }

    public:

        /**
         * @brief Initializes a new instance of the unique object.
        */
        UniqueObject() noexcept = default;

        /**
         * @brief Initializes a new instance of the unique object.
         * @param Value A value that initializes the unique object.
        */
        explicit UniqueObject(ObjectType Value) noexcept :
            m_Value(Value)
        {
        }

        /**
         * @brief Initializes a new instance of the unique object.
         * @param Other Another unique object that initializes the unique
         *        object.
        */
        UniqueObject(UniqueObject&& Other) noexcept :
            m_Value(Other.Detach())
        {
        }

        /**
         * @brief Assigns a value to the unique object.
         * @param Other A unique object value to assign to the unique object.
         * @return A reference to the unique object.
        */
        UniqueObject& operator=(UniqueObject&& Other) noexcept
        {
            if (this != &Other)
            {
                this->Attach(Other.Detach());
            }

            return *this;
        }

        /**
         * @brief Uninitializes the instance of the unique object.
        */
        ~UniqueObject() noexcept
        {
            this->Close();
        }

        /**
         * @brief Checks whether or not the unique object currently represents
         *        a valid unique object value.
         * @return true if the unique object currently represents a valid
         *         unique object value, otherwise false.
        */
        explicit operator bool() const noexcept
        {
            return TraitsType::Invalid() != this->m_Value;
        }

        /**
         * @brief Swaps the contents of the two unique object parameters so
         *        that they contain one another's unique object.
         * @param Left A unique object value whose handle to mutually swap with
         *             that of the other parameter.
         * @param Right A unique object value whose handle to mutually swap
         *              with that of the other parameter.
        */
        friend void swap(UniqueObject& Left, UniqueObject& Right) noexcept
        {
            std::swap(Left.m_Value, Right.m_Value);
        }
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
         * @param TaskHandler The instance of the task handler.
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
     * @return An array of the command line arguments, along with a count of such
     *         arguments.
    */
    std::vector<std::wstring> SpiltCommandLine(
        std::wstring const& CommandLine);

    /**
     * @brief Parses a command line string and get more friendly result.
     * @param CommandLine A string that contains the full command line. If this
     *                    parameter is an empty string the function returns an
     *                    array with only one empty string.
     * @param OptionPrefixes One or more of the prefixes of option we want to
     *                       use.
     * @param OptionParameterSeparators One or more of the separators of option
     *                                  we want to use.
     * @param ApplicationName The application name.
     * @param OptionsAndParameters The options and parameters.
     * @param UnresolvedCommandLine The unresolved command line.
    */
    void SpiltCommandLineEx(
        std::wstring const& CommandLine,
        std::vector<std::wstring> const& OptionPrefixes,
        std::vector<std::wstring> const& OptionParameterSeparators,
        std::wstring& ApplicationName,
        std::map<std::wstring, std::wstring>& OptionsAndParameters,
        std::wstring& UnresolvedCommandLine);

    /**
     * @brief Parses a command arguments string and returns an array of the
     *        command arguments, along with a count of such arguments, in a way
     *        that is similar to the standard C run-time.
     * @param Arguments A string that contains the full command arguments. If
     *                  this parameter is an empty string the function returns
     *                  an array with only one empty string.
     * @return An array of the command arguments, along with a count of such
     *         arguments.
    */
    std::vector<std::wstring> SpiltCommandArguments(
        std::wstring const& Arguments);
}

#endif // !MILE_PORTABLE
