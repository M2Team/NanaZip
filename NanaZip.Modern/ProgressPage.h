#pragma once

#include "ProgressPage.g.h"

#include <Mile.Helpers.CppWinRT.h>
#include "ControlMacros.h"

#include <Windows.h>

namespace winrt
{
    using Windows::Foundation::IInspectable;
    using Windows::UI::Xaml::RoutedEventArgs;
    using Windows::UI::Xaml::RoutedEventHandler;
}

namespace winrt::NanaZip::Modern::implementation
{
    struct ProgressPage : ProgressPageT<ProgressPage>
    {
    public:

        ProgressPage(
            _In_opt_ HWND WindowHandle = nullptr);

        void InitializeComponent();

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
            BackgroundButtonText,
            winrt::hstring
        );
        DEPENDENCY_PROPERTY_HEADER(
            PauseButtonText,
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
            ShowPackedValue,
            bool
        );
        DEPENDENCY_PROPERTY_HEADER(
            ShowCompressionRatioValue,
            bool
        );

        DEPENDENCY_PROPERTY_HEADER(
            ShowPaused,
            bool
        );

        void BackgroundButtonClickedHandler(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);
        void PauseButtonClickedHandler(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        Mile::WinRT::Event<winrt::RoutedEventHandler>
            BackgroundButtonClicked;
        Mile::WinRT::Event<winrt::RoutedEventHandler>
            PauseButtonClicked;

        void CancelButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

    private:

        HWND m_WindowHandle;
    };
}

namespace winrt::NanaZip::Modern::factory_implementation
{
    struct ProgressPage : ProgressPageT<
        ProgressPage,
        implementation::ProgressPage>
    {
    };
}
