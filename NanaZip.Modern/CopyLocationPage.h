#pragma once

#include "CopyLocationPage.g.h"

#include "NanaZip.Modern.h"

namespace winrt
{
    using Windows::Foundation::IInspectable;
    using Windows::UI::Xaml::RoutedEventArgs;
}

namespace winrt::NanaZip::Modern::implementation
{
    struct CopyLocationPage : CopyLocationPageT<CopyLocationPage>
    {
        CopyLocationPage(
            _In_opt_ HWND WindowHandle = nullptr,
            _In_opt_ LPCWSTR Title = nullptr,
            _In_opt_ LPCWSTR Subtitle = nullptr,
            _In_opt_ LPCWSTR AdditionalInformation = nullptr,
            _In_opt_ LPCWSTR InitialPath = nullptr,
            _In_ BOOL ShowExtractAll = FALSE
        );

        void InitializeComponent();

        LPCWSTR GetPath();

        void ExtractAllButtonClick(
            IInspectable const& sender,
            RoutedEventArgs const& e);

        void OkButtonClick(
            IInspectable const& sender,
            RoutedEventArgs const& e);

        void CancelButtonClick(
            IInspectable const& sender,
            RoutedEventArgs const& e);

        void BrowseButtonClick(
            IInspectable const& sender,
            RoutedEventArgs const& e);

    private:

        HWND m_WindowHandle = nullptr;
        LPCWSTR m_Title = nullptr;
        LPCWSTR m_Subtitle = nullptr;
        LPCWSTR m_AdditionalInformation = nullptr;
        LPCWSTR m_InitialPath = nullptr;
        BOOL m_ShowExtractAll = FALSE;
        std::wstring m_FinalPath;
    };
}
