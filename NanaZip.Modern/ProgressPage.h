#pragma once

#include "ProgressPage.g.h"

#include <Mile.Helpers.CppWinRT.h>
#include "ControlMacros.h"

#include "NanaZip.Modern.h"

#include <winrt/Windows.System.h>

namespace winrt
{
    using Windows::Foundation::IInspectable;
    using Windows::System::DispatcherQueue;
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

        void UpdateStatus(
            _In_ PK7_PROGRESS_DIALOG_STATUS Status);

    private:

        HWND m_WindowHandle;
        DispatcherQueue m_DispatcherQueue = nullptr;

        // Title
        // FilePath
        std::uint64_t m_TotalSize = static_cast<std::uint64_t>(-1);
        //std::uint64_t m_ProcessedSize = static_cast<std::uint64_t>(-1);
        //std::uint64_t m_TotalFiles = static_cast<std::uint64_t>(-1);
        //std::uint64_t m_ProcessedFiles = static_cast<std::uint64_t>(-1);
        //std::uint64_t m_InputSize = static_cast<std::uint64_t>(-1);
        //std::uint64_t m_OutputSize = static_cast<std::uint64_t>(-1);

        std::uint64_t m_TotalProgress = static_cast<std::uint64_t>(-1);
        std::uint64_t m_ProcessedProgress = static_cast<std::uint64_t>(-1);
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
