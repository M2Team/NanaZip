#include "pch.h"
#include "ProgressPage.h"
#include "ProgressPage.g.cpp"

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::NanaZip::Modern::implementation
{
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
        CancelButtonText,
        winrt::hstring,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        L"[Cancel]"
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
        ShowBackgroundButton,
        bool,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        true
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(
        ShowPauseButton,
        bool,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        true
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
    DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(
        ShowResults,
        bool,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        false
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(
        ShowProgress,
        bool,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        true
    );

    DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(
        ResultsText,
        winrt::hstring,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        L""
    );

    void ProgressPage::BackgroundButtonClickedHandler(
        winrt::IInspectable const&,
        winrt::RoutedEventArgs const& args
    )
    {
        this->BackgroundButtonClicked(*this, args);
    }

    void ProgressPage::PauseButtonClickedHandler(
        winrt::IInspectable const&,
        winrt::RoutedEventArgs const& args
    )
    {
        this->PauseButtonClicked(*this, args);
    }

    void ProgressPage::CancelButtonClickedHandler(
        winrt::IInspectable const&,
        winrt::RoutedEventArgs const& args
    )
    {
        this->CancelButtonClicked(*this, args);
    }
}
