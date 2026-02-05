#pragma once

#include "ProgressPage.g.h"

#include <Mile.Helpers.CppWinRT.h>
#include "ControlMacros.h"

#include "NanaZip.Modern.h"

#include <ShObjIdl_core.h>

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
            _In_opt_ HWND WindowHandle = nullptr,
            _In_opt_ LPCWSTR Title = nullptr,
            _In_ BOOL ShowCompressionInformation = TRUE);

        void UpdateWindowTitle();

        void UpdateTaskbarProgressState();

        void InitializeComponent();

        void BackgroundButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void PauseButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void CancelButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void UpdateStatus(
            _In_ PK7_PROGRESS_WINDOW_STATUS Status);

        void SetPausedMode(
            _In_ BOOL Paused);

    private:

        HWND m_WindowHandle;
        BOOL m_ShowCompressionInformation;

        winrt::com_ptr<ITaskbarList3> m_TaskbarList;
        HWND m_WindowHandleForTaskbar = nullptr;

        winrt::hstring m_BackgroundButtonText;
        winrt::hstring m_ForegroundButtonText;
        winrt::hstring m_PauseButtonText;
        winrt::hstring m_ContinueButtonText;

        winrt::hstring m_BackgroundedTitleText;
        winrt::hstring m_PausedTitleText;

        bool m_BackgroundMode = false;
        bool m_Paused = false;
        bool m_HaveError = false;
        std::uint64_t m_PreviousTime = 0;
        std::uint64_t m_ElapsedTime = 0;

        std::wstring m_Title;
        std::wstring m_FilePath;
        std::uint64_t m_TotalSize = static_cast<std::uint64_t>(-1);
        std::uint64_t m_ProcessedSize = static_cast<std::uint64_t>(-1);
        std::uint64_t m_TotalFiles = static_cast<std::uint64_t>(-1);
        std::uint64_t m_ProcessedFiles = static_cast<std::uint64_t>(-1);
        std::uint64_t m_CompressedSize = static_cast<std::uint64_t>(-1);
        std::uint64_t m_DecompressedSize = static_cast<std::uint64_t>(-1);
        std::wstring m_Status;

        std::uint64_t m_TotalProgress = static_cast<std::uint64_t>(-1);
        std::uint64_t m_ProcessedProgress = static_cast<std::uint64_t>(-1);
        std::uint64_t m_PercentageProgress = static_cast<std::uint64_t>(-1);
    };
}
