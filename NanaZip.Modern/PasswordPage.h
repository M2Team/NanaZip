#pragma once

#include "PasswordPage.g.h"

#include <Windows.h>

namespace winrt
{
    using Windows::Foundation::IInspectable;
    using Windows::UI::Xaml::RoutedEventArgs;
}

namespace winrt::NanaZip::Modern::implementation
{
    struct PasswordPage : PasswordPageT<PasswordPage>
    {
    public:

        PasswordPage(
            _In_ HWND WindowHandle,
            _Out_ LPWSTR* InputPassword);

        void InitializeComponent();

        void OKButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

    private:

        HWND m_WindowHandle;
        LPWSTR* m_Password;
    };
}
