#pragma once

#include "InformationPage.g.h"
#include "ControlMacros.h"
#include <Windows.h>
#include <Mile.Helpers.CppWinRT.h>

namespace winrt
{
    using Windows::Foundation::IInspectable;
    using Windows::UI::Xaml::RoutedEventArgs;
}

namespace winrt::NanaZip::Modern::implementation
{
    struct InformationPage : InformationPageT<InformationPage>
    {
        InformationPage(
            _In_opt_ HWND WindowHandle,
            winrt::hstring const& WindowTitle,
            winrt::hstring const& WindowContent);

        void InitializeComponent();

        void CloseButtonClickedHandler(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& args);

    private:
        HWND m_WindowHandle = nullptr;
        winrt::hstring m_WindowTitle;
        winrt::hstring m_WindowContent;
    };
}
