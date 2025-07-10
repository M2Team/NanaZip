#pragma once

#include "InformationPage.g.h"
#include "ControlMacros.h"
#include <Mile.Helpers.CppWinRT.h>

namespace winrt
{
    using namespace Windows::Foundation;
    using namespace Windows::UI::Xaml;
}

namespace winrt::NanaZip::Modern::implementation
{
    struct InformationPage : InformationPageT<InformationPage>
    {
        InformationPage(
            HWND windowHandle,
            LPCWSTR title,
            LPCWSTR text);

        void InitializeComponent();

        void CloseButtonClickedHandler(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& args);

    private:
        HWND m_WindowHandle{ nullptr };
        LPCWSTR m_Title{ nullptr };
        LPCWSTR m_Text{ nullptr };
    };
}
