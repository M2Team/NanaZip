#pragma once

#include "InformationPage.g.h"

#include <Windows.h>

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
            _In_opt_ LPCWSTR Title,
            _In_opt_ LPCWSTR Content);

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
