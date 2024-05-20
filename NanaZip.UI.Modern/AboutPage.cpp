#include "pch.h"
#include "AboutPage.h"
#if __has_include("AboutPage.g.cpp")
#include "AboutPage.g.cpp"
#endif

#include "NanaZip.UI.h"

#include <winrt/Windows.UI.Xaml.Documents.h>

#include <string>

#include "../SevenZip/CPP/Common/Common.h"
#include <Mile.Project.Version.h>
#include "../SevenZip/C/CpuArch.h"
#include "../SevenZip/CPP/7zip/UI/Common/LoadCodecs.h"
#include "../SevenZip/CPP/7zip/UI/FileManager/LangUtils.h"
#include "../SevenZip/CPP/7zip/UI/FileManager/resourceGui.h"

using namespace winrt;
using namespace Windows::UI::Xaml;

extern CCodecs* g_CodecsObj;

#define IDD_ABOUT  2900
#define IDT_ABOUT_INFO  2901
#define IDB_ABOUT_HOMEPAGE   110

namespace winrt::NanaZip::Modern::implementation
{
    AboutPage::AboutPage(
        _In_ HWND WindowHandle) :
        m_WindowHandle(WindowHandle)
    {

    }

    void AboutPage::InitializeComponent()
    {
        AboutPageT::InitializeComponent();

        std::wstring Title = std::wstring(
            ::LangString(IDD_ABOUT));
        if (Title.empty())
        {
            Title = L"About NanaZip";
        }
        ::SetWindowTextW(
            this->m_WindowHandle,
            Title.c_str());

        std::wstring Version = std::wstring(
            "NanaZip " MILE_PROJECT_VERSION_STRING);
        Version.append(
            L" (" MILE_PROJECT_DOT_VERSION_STRING L")");
        Version.append(
            UString(" (" MY_CPU_NAME ")"));

        std::wstring Content = std::wstring(
            ::LangString(IDT_ABOUT_INFO));
        if (Content.empty())
        {
            Content = L"NanaZip is free software";
        }
#ifdef EXTERNAL_CODECS
        if (g_CodecsObj)
        {
            UString s;
            g_CodecsObj->GetCodecsErrorMessage(s);
            if (!s.IsEmpty())
            {
                Content.append(L"\r\n\r\n");
                Content.append(s);
            }
        }
#endif

        this->GridTitleTextBlock().Text(Title);
        this->Version().Text(Version);
        this->Content().Text(Content);
        this->CancelButton().Content(winrt::box_value(
            Mile::WinRT::GetLocalizedString(
                L"Legacy/Resource402")));
    }

    void AboutPage::GitHubButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        SHELLEXECUTEINFOW ExecInfo = { 0 };
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

        ::DestroyWindow(this->m_WindowHandle);
    }
}
