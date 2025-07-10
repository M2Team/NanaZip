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
            _In_opt_ HWND WindowHandle,
            winrt::hstring WindowTitle,
            winrt::hstring WindowContent);

        void InitializeComponent();

        void CloseButtonClickedHandler(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& args);

    private:
        HWND m_WindowHandle{ nullptr };
        winrt::hstring m_WindowTitle;
        winrt::hstring m_WindowContent;
    };
}
