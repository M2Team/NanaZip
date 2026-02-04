#include "pch.h"
#include "ProgressPage.h"
#include "ProgressPage.g.cpp"

#include <Mile.Helpers.CppBase.h>
#include <Mile.Helpers.CppWinRT.h>

namespace winrt::Mile
{
    using namespace ::Mile;
}

namespace winrt::NanaZip::Modern::implementation
{
    winrt::hstring ConvertByteSizeToString(
        std::uint64_t ByteSize)
    {
        const wchar_t* Units[] =
        {
            L"Byte",
            L"Bytes",
            L"KiB",
            L"MiB",
            L"GiB",
            L"TiB",
            L"PiB",
            L"EiB"
        };
        const std::size_t UnitsCount = sizeof(Units) / sizeof(*Units);

        // Output Format:
        // For ByteSize is 0 or 1: x Byte
        // For ByteSize is from 2 to 1023: x Bytes
        // For ByteSize is larger than 1023: x.xx {KiB, MiB, GiB, TiB, PiB, EiB}

        std::size_t UnitIndex = 0;
        double Result = static_cast<double>(ByteSize);

        if (ByteSize > 1)
        {
            for (UnitIndex = 1; UnitIndex < UnitsCount; ++UnitIndex)
            {
                if (1024.0 > Result)
                    break;

                Result /= 1024.0;
            }

            // Keep two digits after the decimal point.
            Result = static_cast<std::uint64_t>(Result * 100) / 100.0;
        }

        return winrt::hstring(Mile::FormatWideString(
            (UnitIndex > 1) ? L"%.2f %s" : L"%.0f %s",
            Result,
            Units[UnitIndex]));
    }

    winrt::hstring ConvertSecondsToTimeString(
        std::uint64_t Seconds)
    {
        if (static_cast<uint64_t>(-1) == Seconds)
        {
            return L"N/A";
        }

        int Hour = static_cast<int>(Seconds / 3600);
        int Minute = static_cast<int>(Seconds / 60 % 60);
        int Second = static_cast<int>(Seconds % 60);

        return winrt::hstring(Mile::FormatWideString(
            L"%d:%02d:%02d",
            Hour,
            Minute,
            Second));
    }

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
