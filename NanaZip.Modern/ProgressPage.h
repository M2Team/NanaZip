#pragma once

#include "ProgressPage.g.h"

#include <Mile.Helpers.CppWinRT.h>
#include "ControlMacros.h"

namespace winrt
{
    using namespace Windows::Foundation;
    using namespace Windows::UI::Xaml;
}

namespace winrt::NanaZip::Modern::implementation
{
    struct ProgressPage : ProgressPageT<ProgressPage>
    {
        ProgressPage() 
        {
            // Xaml objects should not call InitializeComponent during construction.
            // See https://github.com/microsoft/cppwinrt/tree/master/nuget#initializecomponent
        }

        DEPENDENCY_PROPERTY_HEADER(
            ActionText,
            winrt::hstring
        );
        DEPENDENCY_PROPERTY_HEADER(
            FileNameText,
            winrt::hstring
        );
        DEPENDENCY_PROPERTY_HEADER(
            ElapsedTimeText,
            winrt::hstring
        );
        DEPENDENCY_PROPERTY_HEADER(
            RemainingTimeText,
            winrt::hstring
        );
        DEPENDENCY_PROPERTY_HEADER(
            FilesText,
            winrt::hstring
        );
        DEPENDENCY_PROPERTY_HEADER(
            SpeedText,
            winrt::hstring
        );
        DEPENDENCY_PROPERTY_HEADER(
            TotalSizeText,
            winrt::hstring
        );
        DEPENDENCY_PROPERTY_HEADER(
            ProcessedText,
            winrt::hstring
        );
        DEPENDENCY_PROPERTY_HEADER(
            PackedSizeText,
            winrt::hstring
        );
        DEPENDENCY_PROPERTY_HEADER(
            CompressionRatioText,
            winrt::hstring
        );
        DEPENDENCY_PROPERTY_HEADER(
            ProgressBarMinimum,
            double
        );
        DEPENDENCY_PROPERTY_HEADER(
            ProgressBarMaximum,
            double
        );
        DEPENDENCY_PROPERTY_HEADER(
            ProgressBarValue,
            double
        );
        DEPENDENCY_PROPERTY_HEADER(
            ShowBackgroundButton,
            bool
        );
        DEPENDENCY_PROPERTY_HEADER(
            ShowPauseButton,
            bool
        );
        DEPENDENCY_PROPERTY_HEADER(
            ShowPackedValue,
            bool
        );
        DEPENDENCY_PROPERTY_HEADER(
            ShowCompressionRatioValue,
            bool
        );

        void BackgroundButtonClickedHandler(
            winrt::IInspectable const&,
            winrt::RoutedEventArgs const&
        );
        void PauseButtonClickedHandler(
            winrt::IInspectable const&,
            winrt::RoutedEventArgs const&
        );
        void CancelButtonClickedHandler(
            winrt::IInspectable const&,
            winrt::RoutedEventArgs const&
        );

        Mile::WinRT::Event<winrt::RoutedEventHandler>
            BackgroundButtonClicked;
        Mile::WinRT::Event<winrt::RoutedEventHandler>
            PauseButtonClicked;
        Mile::WinRT::Event<winrt::RoutedEventHandler>
            CancelButtonClicked;
    };
}

namespace winrt::NanaZip::Modern::factory_implementation
{
    struct ProgressPage : ProgressPageT<ProgressPage, implementation::ProgressPage>
    {
    };
}
