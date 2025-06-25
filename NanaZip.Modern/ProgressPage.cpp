#include "pch.h"
#include "ProgressPage.h"
#include "ProgressPage.g.cpp"

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::NanaZip::Modern::implementation
{
    DEPENDENCY_PROPERTY_SOURCE_BOX(
        ActionText,
        winrt::hstring,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        L""
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX(
        FileNameText,
        winrt::hstring,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        L""
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX(
        ElapsedTimeText,
        winrt::hstring,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        L""
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX(
        RemainingTimeText,
        winrt::hstring,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        L""
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX(
        FilesText,
        winrt::hstring,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        L""
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX(
        SpeedText,
        winrt::hstring,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        L""
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX(
        TotalSizeText,
        winrt::hstring,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        L""
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX(
        ProcessedText,
        winrt::hstring,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        L""
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX(
        PackedSizeText,
        winrt::hstring,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        L""
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX(
        CompressionRatioText,
        winrt::hstring,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        L""
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX(
        ProgressBarMinimum,
        double,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        0
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX(
        ProgressBarMaximum,
        double,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        1
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX(
        ProgressBarValue,
        double,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        0
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX(
        ShowBackgroundButton,
        bool,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        true
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX(
        ShowPauseButton,
        bool,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        true
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX(
        ShowPackedValue,
        bool,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        true
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX(
        ShowCompressionRatioValue,
        bool,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        true
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
