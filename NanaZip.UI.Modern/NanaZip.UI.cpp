/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.UI.cpp
 * PURPOSE:   Implementation for NanaZip Modern UI Shared Infrastructures
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.UI.h"

#include <Mile.Helpers.h>
#include <Mile.Xaml.h>

#include <NanaZip.Modern.h>

#include "../SevenZip/CPP/Common/Common.h"
#include "../SevenZip/CPP/7zip/UI/Common/LoadCodecs.h"

extern CCodecs* g_CodecsObj;

winrt::handle NanaZip::UI::ShowAboutDialog(
    _In_ HWND ParentWindowHandle)
{
    return winrt::handle(Mile::CreateThread([=]()
    {
        UString ExtendedMessage;
        if (g_CodecsObj)
        {
            g_CodecsObj->GetCodecsErrorMessage(ExtendedMessage);
        }

        ::K7ModernShowAboutDialog(ParentWindowHandle, ExtendedMessage);
    }));
}

namespace
{
    static void SplitCommandLineEx(
        std::wstring const& CommandLine,
        std::vector<std::wstring> const& OptionPrefixes,
        std::vector<std::wstring> const& OptionParameterSeparators,
        std::wstring& ApplicationName,
        std::map<std::wstring, std::wstring>& OptionsAndParameters,
        std::wstring& UnresolvedCommandLine)
    {
        ApplicationName.clear();
        OptionsAndParameters.clear();
        UnresolvedCommandLine.clear();

        size_t arg_size = 0;
        for (auto& SplitArgument : Mile::SplitCommandLineWideString(CommandLine))
        {
            // We need to process the application name at the beginning.
            if (ApplicationName.empty())
            {
                // For getting the unresolved command line, we need to cumulate
                // length which including spaces.
                arg_size += SplitArgument.size() + 1;

                // Save
                ApplicationName = SplitArgument;
            }
            else
            {
                bool IsOption = false;
                size_t OptionPrefixLength = 0;

                for (auto& OptionPrefix : OptionPrefixes)
                {
                    if (0 == _wcsnicmp(
                        SplitArgument.c_str(),
                        OptionPrefix.c_str(),
                        OptionPrefix.size()))
                    {
                        IsOption = true;
                        OptionPrefixLength = OptionPrefix.size();
                    }
                }

                if (IsOption)
                {
                    // For getting the unresolved command line, we need to cumulate
                    // length which including spaces.
                    arg_size += SplitArgument.size() + 1;

                    // Get the option name and parameter.

                    wchar_t* OptionStart = &SplitArgument[0] + OptionPrefixLength;
                    wchar_t* ParameterStart = nullptr;

                    for (auto& OptionParameterSeparator
                        : OptionParameterSeparators)
                    {
                        wchar_t* Result = wcsstr(
                            OptionStart,
                            OptionParameterSeparator.c_str());
                        if (nullptr == Result)
                        {
                            continue;
                        }

                        Result[0] = L'\0';
                        ParameterStart = Result + OptionParameterSeparator.size();

                        break;
                    }

                    // Save
                    OptionsAndParameters[(OptionStart ? OptionStart : L"")] =
                        (ParameterStart ? ParameterStart : L"");
                }
                else
                {
                    // Get the approximate location of the unresolved command line.
                    // We use "(arg_size - 1)" to ensure that the program path
                    // without quotes can also correctly parse.
                    wchar_t* search_start =
                        const_cast<wchar_t*>(CommandLine.c_str()) + (arg_size - 1);

                    // Get the unresolved command line. Search for the beginning of
                    // the first parameter delimiter called space and exclude the
                    // first space by adding 1 to the result.
                    wchar_t* command = wcsstr(search_start, L" ") + 1;

                    // Omit the space. (Thanks to wzzw.)
                    while (command && *command == L' ')
                    {
                        ++command;
                    }

                    // Save
                    if (command)
                    {
                        UnresolvedCommandLine = command;
                    }

                    break;
                }
            }
        }
    }
}

void NanaZip::UI::SpecialCommandHandler()
{
    std::wstring ApplicationName;
    std::map<std::wstring, std::wstring> OptionsAndParameters;
    std::wstring UnresolvedCommandLine;

    ::SplitCommandLineEx(
        std::wstring(::GetCommandLineW()),
        std::vector<std::wstring>{ L"-", L"/", L"--" },
        std::vector<std::wstring>{ L"=", L":" },
        ApplicationName,
        OptionsAndParameters,
        UnresolvedCommandLine);

    bool AcquireSponsorEdition = false;

    for (auto& Current : OptionsAndParameters)
    {
        if (0 == _wcsicmp(Current.first.c_str(), L"AcquireSponsorEdition"))
        {
            AcquireSponsorEdition = true;
        }
    }

    if (AcquireSponsorEdition)
    {
        ::MileXamlThreadInitialize();
        ::ExitProcess(::K7ModernShowSponsorDialog(nullptr));
    }
}
