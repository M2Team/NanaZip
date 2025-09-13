#include "pch.h"
#include "AboutPage.h"
#if __has_include("AboutPage.g.cpp")
#include "AboutPage.g.cpp"
#endif

#include "NanaZip.Modern.h"

#include <Mile.Helpers.CppBase.h>
#include <Mile.Helpers.CppWinRT.h>

namespace winrt::Mile
{
    using namespace ::Mile;
}

#include <winrt/Windows.UI.Xaml.Documents.h>

#include <string>

#include <Mile.Project.Version.h>

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::NanaZip::Modern::implementation
{
    AboutPage::AboutPage(
        _In_opt_ HWND WindowHandle,
        _In_opt_ LPCWSTR ExtendedMessage) :
        m_WindowHandle(WindowHandle),
        m_ExtendedMessage(ExtendedMessage)
    {

    }

    void AboutPage::InitializeComponent()
    {
        AboutPageT::InitializeComponent();

        winrt::hstring WindowTitle = Mile::WinRT::GetLocalizedString(
            L"Legacy/Resource2900");
        if (WindowTitle.empty())
        {
            WindowTitle = L"About NanaZip";
        }
        ::SetWindowTextW(this->m_WindowHandle, WindowTitle.c_str());

        std::wstring Version = std::wstring(
            "NanaZip " MILE_PROJECT_VERSION_STRING);
        Version.append(
            L" (" MILE_PROJECT_DOT_VERSION_STRING L")");
#if defined(_M_AMD64)
        Version.append(L" (x64)");
#elif defined(_M_ARM64)
        Version.append(L" (arm64)");
#endif

        std::wstring Content = std::wstring(
            Mile::WinRT::GetLocalizedString(L"Legacy/Resource2901"));
        if (Content.empty())
        {
            Content = L"NanaZip is free software";
        }
        if (!this->m_ExtendedMessage.empty())
        {
            Content.append(L"\r\n\r\n");
            Content.append(this->m_ExtendedMessage);
        }

        this->GridTitleTextBlock().Text(WindowTitle);
        this->Version().Text(Version);
        this->Content().Text(Content);
        this->CancelButton().Content(winrt::box_value(
            Mile::WinRT::GetLocalizedString(L"Legacy/Resource402")));
    }

    void AboutPage::GitHubButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        SHELLEXECUTEINFOW ExecInfo = {};
        ExecInfo.cbSize = sizeof(SHELLEXECUTEINFOW);
        ExecInfo.lpVerb = L"open";
        ExecInfo.lpFile = L"https://github.com/M2Team/NanaZip";
        ExecInfo.nShow = SW_SHOWNORMAL;
        ::ShellExecuteExW(&ExecInfo);
    }

    void AboutPage::CancelButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::SendMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
    }
}
