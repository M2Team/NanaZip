#include "pch.h"
#include "ProgressPage.h"
#include "ProgressPage.g.cpp"

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::NanaZip::Modern::implementation
{
    ProgressPage::ProgressPage(
        _In_opt_ HWND WindowHandle) :
        m_WindowHandle(WindowHandle)
    {

    }

    void ProgressPage::InitializeComponent()
    {
        ProgressPageT::InitializeComponent();
    }

    DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(
        ActionText,
        winrt::hstring,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        L""
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(
        FileNameText,
        winrt::hstring,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        L""
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(
        ElapsedTimeText,
        winrt::hstring,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        L""
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(
        RemainingTimeText,
        winrt::hstring,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        L""
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(
        FilesText,
        winrt::hstring,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        L""
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(
        SpeedText,
        winrt::hstring,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        L""
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(
        TotalSizeText,
        winrt::hstring,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        L""
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(
        ProcessedText,
        winrt::hstring,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        L""
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(
        PackedSizeText,
        winrt::hstring,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        L""
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(
        CompressionRatioText,
        winrt::hstring,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        L""
    );

    DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(
        BackgroundButtonText,
        winrt::hstring,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        L"[Background]"
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(
        PauseButtonText,
        winrt::hstring,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        L"[Pause]"
    );

    DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(
        ProgressBarMinimum,
        double,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        0
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(
        ProgressBarMaximum,
        double,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        1
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(
        ProgressBarValue,
        double,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        0
    );

    DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(
        ShowPackedValue,
        bool,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        true
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(
        ShowCompressionRatioValue,
        bool,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        true
    );

    DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(
        ShowPaused,
        bool,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        false
    );

    void ProgressPage::BackgroundButtonClickedHandler(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        this->BackgroundButtonClicked(*this, e);
    }

    void ProgressPage::PauseButtonClickedHandler(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        this->PauseButtonClicked(*this, e);
    }

    void ProgressPage::CancelButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(IDCANCEL, BN_CLICKED),
            0);
    }
}

EXTERN_C LPVOID WINAPI K7ModernCreateProgressPage(
    _In_ HWND ParentWindowHandle)
{
    using Interface =
        winrt::NanaZip::Modern::ProgressPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::ProgressPage;

    Interface Window = winrt::make<Implementation>(
        ParentWindowHandle);
    return winrt::detach_abi(Window);
}
