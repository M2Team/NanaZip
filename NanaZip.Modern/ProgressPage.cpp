#include "pch.h"
#include "ProgressPage.h"
#include "ProgressPage.g.cpp"

#include <Mile.Helpers.CppBase.h>
#include <Mile.Helpers.CppWinRT.h>

namespace winrt::Mile
{
    using namespace ::Mile;
}

// Defined in SevenZip/CPP/7zip/UI/FileManager/resourceGui.h
#define IDI_ICON 1

namespace winrt
{
    using Windows::System::DispatcherQueuePriority;
}

namespace
{
    std::wstring ConvertByteSizeToString(
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

        return Mile::FormatWideString(
            (UnitIndex > 1) ? L"%.2f %s" : L"%.0f %s",
            Result,
            Units[UnitIndex]);
    }

    std::wstring ConvertSecondsToTimeString(
        std::uint64_t Seconds)
    {
        if (static_cast<uint64_t>(-1) == Seconds)
        {
            return L"N/A";
        }

        int Hour = static_cast<int>(Seconds / 3600);
        int Minute = static_cast<int>(Seconds / 60 % 60);
        int Second = static_cast<int>(Seconds % 60);

        return Mile::FormatWideString(
            L"%d:%02d:%02d",
            Hour,
            Minute,
            Second);
    }
}

namespace winrt::NanaZip::Modern::implementation
{
    ProgressPage::ProgressPage(
        _In_opt_ HWND WindowHandle,
        _In_opt_ LPCWSTR Title) :
        m_WindowHandle(WindowHandle),
        m_Title(Title ? std::wstring(Title) : L"")
    {

    }

    void ProgressPage::UpdateWindowTitle()
    {
        std::wstring Title;
        if (this->m_Paused)
        {
            Title.append(std::wstring(
                this->m_PausedTitleText.c_str(),
                this->m_PausedTitleText.size()));
            Title.push_back(L' ');
        }
        if (static_cast<std::uint64_t>(-1) != this->m_PercentageProgress)
        {
            Title.append(Mile::FormatWideString(
                L"%llu%% ",
                this->m_PercentageProgress));
        }
        if (this->m_BackgroundMode)
        {
            Title.append(std::wstring(
                this->m_BackgroundedTitleText.c_str(),
                this->m_BackgroundedTitleText.size()));
            Title.push_back(L' ');
        }
        Title.append(this->m_Title);

        ::SetWindowTextW(this->m_WindowHandle, Title.c_str());
        this->TitleTextBlock().Text(Title);
    }

    void ProgressPage::InitializeComponent()
    {
        ProgressPageT::InitializeComponent();

        HICON ApplicationIconHandle = reinterpret_cast<HICON>(::LoadImageW(
            ::GetModuleHandleW(nullptr),
            MAKEINTRESOURCE(IDI_ICON),
            IMAGE_ICON,
            256,
            256,
            LR_SHARED));
        if (ApplicationIconHandle)
        {
            ::SendMessageW(
                this->m_WindowHandle,
                WM_SETICON,
                TRUE,
                reinterpret_cast<LPARAM>(ApplicationIconHandle));
            ::SendMessageW(
                this->m_WindowHandle,
                WM_SETICON,
                FALSE,
                reinterpret_cast<LPARAM>(ApplicationIconHandle));
        }

        this->m_DispatcherQueue = DispatcherQueue::GetForCurrentThread();

        this->m_BackgroundButtonText = Mile::WinRT::GetLocalizedString(
            L"NanaZip.Modern/ProgressPage/BackgroundButtonText");
        this->m_ForegroundButtonText = Mile::WinRT::GetLocalizedString(
            L"NanaZip.Modern/ProgressPage/ForegroundButtonText");
        this->m_PauseButtonText = Mile::WinRT::GetLocalizedString(
            L"NanaZip.Modern/ProgressPage/PauseButtonText");
        this->m_ContinueButtonText = Mile::WinRT::GetLocalizedString(
            L"NanaZip.Modern/ProgressPage/ContinueButtonText");

        this->m_BackgroundedTitleText = this->m_BackgroundButtonText;
        this->m_PausedTitleText = Mile::WinRT::GetLocalizedString(
            L"NanaZip.Modern/ProgressPage/PausedText");

        this->UpdateWindowTitle();

        this->BackgroundButton().Content(winrt::box_value(
            this->m_BackgroundButtonText));
        this->PauseButton().Content(winrt::box_value(
            this->m_PauseButtonText));
    }

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
        SpeedText,
        winrt::hstring,
        ProgressPage,
        winrt::NanaZip::Modern::ProgressPage,
        L""
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

    void ProgressPage::BackgroundButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        this->m_BackgroundMode = !this->m_BackgroundMode;
        ::SetPriorityClass(
            ::GetCurrentProcess(),
            this->m_BackgroundMode
            ? IDLE_PRIORITY_CLASS
            : NORMAL_PRIORITY_CLASS);
        this->BackgroundButton().Content(winrt::box_value(
            this->m_BackgroundMode
            ? this->m_ForegroundButtonText
            : this->m_BackgroundButtonText));
        this->UpdateWindowTitle();
    }

    void ProgressPage::PauseButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        this->m_Paused = !this->m_Paused;
        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(K7_PROGRESS_WINDOW_COMMAND_PAUSE, BN_CLICKED),
            0);
        this->ProgressBar().ShowPaused(this->m_Paused);
        this->PauseButton().Content(winrt::box_value(
            this->m_Paused
            ? this->m_ContinueButtonText
            : this->m_PauseButtonText));
        this->UpdateWindowTitle();
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

    void ProgressPage::UpdateStatus(
        _In_ PK7_PROGRESS_WINDOW_STATUS StatusRequest)
    {
        if (!StatusRequest)
        {
            return;
        }

        std::wstring Title = StatusRequest->Title
            ? std::wstring(StatusRequest->Title)
            : L"";
        std::wstring FilePath = StatusRequest->FilePath
            ? std::wstring(StatusRequest->FilePath)
            : L"";
        std::uint64_t TotalSize = StatusRequest->TotalSize;
        std::uint64_t ProcessedSize = StatusRequest->ProcessedSize;
        std::uint64_t TotalFiles = StatusRequest->TotalFiles;
        std::uint64_t ProcessedFiles = StatusRequest->ProcessedFiles;
        std::uint64_t CompressedSize = StatusRequest->CompressionMode
            ? StatusRequest->OutputSize
            : StatusRequest->InputSize;
        std::uint64_t DecompressedSize = StatusRequest->CompressionMode
            ? StatusRequest->InputSize
            : StatusRequest->OutputSize;
        std::wstring Status = StatusRequest->Status
            ? std::wstring(StatusRequest->Status)
            : L"";

        std::uint64_t TotalProgress = StatusRequest->BytesProgressMode
            ? TotalSize
            : TotalFiles;
        std::uint64_t ProcessedProgress = StatusRequest->BytesProgressMode
            ? ProcessedSize
            : ProcessedFiles;

        std::uint64_t PercentageProgress = static_cast<std::uint64_t>(-1);
        if (static_cast<std::uint64_t>(-1) != ProcessedProgress &&
            static_cast<std::uint64_t>(-1) != TotalProgress &&
            0 != TotalProgress)
        {
            PercentageProgress = (ProcessedProgress * 100) / TotalProgress;
        }
        else
        {
            PercentageProgress = 0;
        }

        if (!this->m_DispatcherQueue)
        {
            return;
        }
        this->m_DispatcherQueue.TryEnqueue(
            winrt::DispatcherQueuePriority::Normal,
            [=]()
        {
            {
                bool NeedUpdateTitle = false;

                if (Title != this->m_Title)
                {
                    this->m_Title = Title;
                    NeedUpdateTitle = true;
                }

                if (PercentageProgress != this->m_PercentageProgress)
                {
                    this->m_PercentageProgress = PercentageProgress;
                    NeedUpdateTitle = true;
                }

                if (NeedUpdateTitle)
                {
                    this->UpdateWindowTitle();
                }
            }

            if (FilePath != this->m_FilePath)
            {
                std::wstring String;
                std::size_t Index = FilePath.rfind(L'\\');
                if (std::wstring::npos == Index)
                {
                    String = FilePath;
                }
                else
                {
                    String = FilePath.substr(0, Index + 1);
                    String.append(L"\r\n");
                    String.append(FilePath.substr(Index + 1));
                }
                this->FilePathTextBlock().Text(String);
                this->m_FilePath = FilePath;
            }

            if (Status != this->m_Status)
            {
                this->StatusTextBlock().Text(Status);
                this->m_Status = Status;
            }

            if (TotalProgress != this->m_TotalProgress ||
                ProcessedProgress != this->m_ProcessedProgress)
            {
                if (static_cast<std::uint64_t>(-1) != TotalProgress)
                {
                    this->ProgressBar().Minimum(0.0);
                    this->ProgressBar().Maximum(
                        static_cast<double>(TotalProgress));
                    this->ProgressBar().Value(
                        static_cast<double>(ProcessedProgress));
                }
                else
                {
                    this->ProgressBar().Minimum(0.0);
                    this->ProgressBar().Maximum(1.0);
                    this->ProgressBar().Value(0.0);
                }

                this->m_TotalProgress = TotalProgress;
                this->m_ProcessedProgress = ProcessedProgress;
            }

            if (TotalFiles != this->m_TotalFiles ||
                ProcessedFiles != this->m_ProcessedFiles)
            {
                std::wstring String = Mile::FormatWideString(
                    L"%llu / %llu",
                    ((static_cast<std::uint64_t>(-1) != ProcessedFiles)
                        ? ProcessedFiles
                        : 0),
                    ((static_cast<std::uint64_t>(-1) != TotalFiles)
                        ? TotalFiles
                        : 0));
                this->FilesTextBlock().Text(String);

                this->m_TotalFiles = TotalFiles;
                this->m_ProcessedFiles = ProcessedFiles;
            }

            if (TotalSize != this->m_TotalSize)
            {
                std::wstring String = L"";
                if (static_cast<std::uint64_t>(-1) != TotalSize)
                {
                    String = ::ConvertByteSizeToString(TotalSize);
                    this->TotalSizeTextBlock().Text(String);
                }
                this->m_TotalSize = TotalSize;
            }

            if (CompressedSize != this->m_CompressedSize ||
                DecompressedSize != this->m_DecompressedSize)
            {
                if (DecompressedSize != this->m_DecompressedSize)
                {
                    std::wstring String = L"";
                    if (static_cast<std::uint64_t>(-1) != DecompressedSize)
                    {
                        String = ::ConvertByteSizeToString(DecompressedSize);
                        this->ProcessedSizeTextBlock().Text(String);
                    }
                }

                if (CompressedSize != this->m_CompressedSize)
                {
                    std::wstring String = L"";
                    if (static_cast<std::uint64_t>(-1) != CompressedSize)
                    {
                        String = ::ConvertByteSizeToString(CompressedSize);
                        this->PackedSizeTextBlock().Text(String);
                    }
                }

                if (static_cast<std::uint64_t>(-1) != CompressedSize &&
                    static_cast<std::uint64_t>(-1) != DecompressedSize &&
                    0 != DecompressedSize)
                {
                    std::wstring String = Mile::FormatWideString(
                        L"%llu %%",
                        static_cast<std::uint64_t>(
                            (CompressedSize * 100) / DecompressedSize));
                    this->CompressionRatioTextBlock().Text(String);
                }

                this->m_CompressedSize = CompressedSize;
                this->m_DecompressedSize = DecompressedSize;
            }
            else
            {
                if (ProcessedSize != this->m_ProcessedSize)
                {
                    if (static_cast<std::uint64_t>(-1) != ProcessedSize)
                    {
                        std::wstring String = ::ConvertByteSizeToString(
                            ProcessedSize);
                        this->ProcessedSizeTextBlock().Text(String);
                    }

                    this->m_ProcessedSize = ProcessedSize;
                }
            }
        });
    }
}
