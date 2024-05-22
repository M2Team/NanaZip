#pragma once

#include "SponsorPage.g.h"

#include <Windows.h>

namespace winrt
{
    using Windows::Foundation::IInspectable;
    using Windows::UI::Xaml::RoutedEventArgs;
}

namespace winrt::NanaZip::Modern::implementation
{
    struct SponsorPage : SponsorPageT<SponsorPage>
    {
    public:

        SponsorPage(
            _In_ HWND WindowHandle = nullptr);

        void InitializeComponent();

        void ContributeButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        winrt::fire_and_forget BuySponsorEditionButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void SponsorEditionPolicyButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

    private:

        HWND m_WindowHandle;
    };
}

namespace winrt::NanaZip::Modern::factory_implementation
{
    struct SponsorPage :
        SponsorPageT<SponsorPage, implementation::SponsorPage>
    {
    };
}
