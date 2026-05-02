#include "pch.h"
#include "PasswordPage.h"
#if __has_include("PasswordPage.g.cpp")
#include "PasswordPage.g.cpp"
#endif

#include "NanaZip.Modern.h"

#include <Mile.Helpers.h>
#include <winrt/Windows.UI.Xaml.Input.h>

namespace winrt::NanaZip::Modern::implementation
{
    PasswordPage::PasswordPage(
        _In_ HWND WindowHandle,
        _Out_ LPWSTR* InputPassword) :
        m_WindowHandle(WindowHandle),
        m_Password(InputPassword)
    {

    }

    void PasswordPage::InitializeComponent()
    {
        PasswordPageT::InitializeComponent();

        this->OkKeyAcc().ScopeOwner(this->PasswordBoxInput());

        winrt::hstring WindowTitle = winrt::hstring(
            ::K7ModernGetLegacyStringResource(3800));
        if (WindowTitle.empty())
        {
            WindowTitle = L"Enter Password";
        }
        winrt::check_bool(::SetWindowTextW(this->m_WindowHandle, WindowTitle.c_str()));
    }

    void PasswordPage::OKButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        *m_Password = nullptr;

        //std::wstring_view PasswordView = static_cast<std::wstring_view>(this->PasswordBoxInput().Password());
        //*m_Password = reinterpret_cast<LPWSTR>(MileAllocateMemory(sizeof(WCHAR) * (PasswordView.size() + 1)));
        //PasswordView.copy(*m_Password, PasswordView.size());

        winrt::hstring const& Password = this->PasswordBoxInput().Password();
        *m_Password = reinterpret_cast<LPWSTR>(MileAllocateMemory((static_cast<SIZE_T>(Password.size()) + 1) * sizeof(WCHAR)));
        ::wcscpy_s(*m_Password, Password.size() + 1, Password.data());

        ::PostQuitMessage(IDOK);
    }
}
