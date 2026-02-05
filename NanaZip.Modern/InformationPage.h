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
            _In_opt_ HWND WindowHandle = nullptr,
            _In_opt_ LPCWSTR Title = nullptr,
            _In_opt_ LPCWSTR Content = nullptr);

        void InitializeComponent();

        void CloseButtonClickedHandler(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& args);

    private:

        HWND m_WindowHandle = nullptr;
        winrt::hstring m_Title;
        winrt::hstring m_Content;
    };
}
