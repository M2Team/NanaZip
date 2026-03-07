#include "pch.h"
#include "CopyLocationPage.h"
#include "CopyLocationPage.g.cpp"
#include <Mile.Helpers.CppWinRT.h>
#include <shlobj.h>
#include <string>

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::NanaZip::Modern::implementation
{
    CopyLocationPage::CopyLocationPage(
        _In_opt_ HWND WindowHandle,
        _In_opt_ LPCWSTR Title,
        _In_opt_ LPCWSTR Subtitle,
        _In_opt_ LPCWSTR AdditionaInformation,
        _In_opt_ LPCWSTR InitialPath):
        m_WindowHandle(WindowHandle),
        m_Title(Title),
        m_Subtitle(Subtitle),
        m_AdditionalInformation(AdditionaInformation),
        m_InitialPath(InitialPath)
    {
    }

    void CopyLocationPage::InitializeComponent()
    {
        CopyLocationPageT::InitializeComponent();

        this->TitleTextBlock().Text(m_Title);
        this->SubtitleTextBlock().Text(m_Subtitle);
        this->AdditionalInformationTextBlock().Text(m_AdditionalInformation);
        this->PathTextBox().Text(m_InitialPath);
    }

    LPCWSTR CopyLocationPage::GetPath()
    {
        return this->PathTextBox().Text().c_str();
    }

    void CopyLocationPage::SetPath(LPCWSTR Path)
    {
        this->PathTextBox().Text(Path);
    }

    void CopyLocationPage::OkButtonClick(
        IInspectable const& sender,
        RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::SendMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(K7_COPY_LOCATION_DIALOG_RESULT_OK, BN_CLICKED),
            0);

        ::SendMessageW(
            this->m_WindowHandle,
            WM_CLOSE,
            0,
            0);
    }

    void CopyLocationPage::CancelButtonClick(
        IInspectable const& sender,
        RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);
        ::SendMessageW(
            this->m_WindowHandle,
            WM_CLOSE,
            0,
            0);
    }

    void CopyLocationPage::BrowseButtonClick(
        IInspectable const& sender,
        RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        winrt::com_ptr<IFileDialog> FileDialog =
            winrt::try_create_instance<IFileDialog>(
                CLSID_FileOpenDialog,
                CLSCTX_INPROC_SERVER);

        if (!FileDialog)
        {
            return;
        }

        FILEOPENDIALOGOPTIONS Options;
        if (FAILED(FileDialog->GetOptions(&Options)))
        {
            Options = 0;
        }
        if (FAILED(FileDialog->SetOptions(
            Options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM)))
        {
            return;
        }

        winrt::com_ptr<IShellItem> InitialFolder;

        if (SUCCEEDED(::SHCreateItemFromParsingName(
            this->GetPath(),
            nullptr,
            IID_IShellItem,
            InitialFolder.put_void())))
        {
            FileDialog->SetFolder(InitialFolder.get());
        }

        FileDialog->SetTitle(::Mile::WinRT::GetLocalizedString(
            L"NanaZip.Modern/CopyLocationPage/SetDestinationText",
            L"Select destination folder.")
            .c_str());

        if (FAILED(FileDialog->Show(this->m_WindowHandle)))
        {
            return;
        }

        winrt::com_ptr<IShellItem> Result;
        if (SUCCEEDED(FileDialog->GetResult(Result.put())))
        {
            PWSTR Path;
            if (SUCCEEDED(Result->GetDisplayName(SIGDN_FILESYSPATH, &Path)))
            {
                std::wstring PathStr(Path);
                if (!PathStr.ends_with(L"\\"))
                {
                    PathStr += L"\\";
                }
                this->PathTextBox().Text(PathStr);
                ::CoTaskMemFree(Path);
            }
        }
    }
}
