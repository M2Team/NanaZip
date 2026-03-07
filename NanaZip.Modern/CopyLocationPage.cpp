#include "pch.h"
#include "CopyLocationPage.h"
#include "CopyLocationPage.g.cpp"

#include <Mile.Helpers.CppWinRT.h>

#include <shlobj.h>

#include <string>

namespace winrt::NanaZip::Modern::implementation
{
    CopyLocationPage::CopyLocationPage(
        _In_opt_ HWND WindowHandle,
        _In_opt_ LPCWSTR Title,
        _In_opt_ LPCWSTR Subtitle,
        _In_opt_ LPCWSTR AdditionalInformation,
        _In_opt_ LPCWSTR InitialPath):
        m_WindowHandle(WindowHandle),
        m_Title(Title),
        m_Subtitle(Subtitle),
        m_AdditionalInformation(AdditionalInformation),
        m_InitialPath(InitialPath)
    {
    }

    void CopyLocationPage::InitializeComponent()
    {
        CopyLocationPageT::InitializeComponent();

        this->TitleTextBlock().Text(this->m_Title);
        this->SubtitleTextBlock().Text(this->m_Subtitle);
        this->AdditionalInformationTextBlock().Text(
            this->m_AdditionalInformation);
        this->PathTextBox().Text(this->m_InitialPath);
    }

    LPCWSTR CopyLocationPage::GetPath()
    {
        return this->PathTextBox().Text().c_str();
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

        ::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
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

        FileDialog->SetTitle(Mile::WinRT::GetLocalizedString(
            L"NanaZip.Modern/CopyLocationPage/SelectDestinationText",
            L"Select destination folder.").c_str());

        {
            IShellItem* DefaultFolder = nullptr;
            if (SUCCEEDED(::SHCreateItemFromParsingName(
                this->PathTextBox().Text().c_str(),
                nullptr,
                IID_PPV_ARGS(&DefaultFolder))))
            {
                FileDialog->SetFolder(DefaultFolder);
                DefaultFolder->Release();
            }
        }

        if (SUCCEEDED(FileDialog->Show(this->m_WindowHandle)))
        {
            IShellItem* Result = nullptr;
            if (SUCCEEDED(FileDialog->GetResult(&Result)))
            {
                std::wstring Path;
                {
                    LPWSTR RawPath = nullptr;
                    if (SUCCEEDED(Result->GetDisplayName(
                        SIGDN_FILESYSPATH,
                        &RawPath)))
                    {
                        Path = std::wstring(RawPath);
                        ::CoTaskMemFree(RawPath);
                    }
                }
                if (!Path.empty())
                {
                    if (!Path.ends_with(L"\\"))
                    {
                        Path.append(L"\\");
                    }
                    this->PathTextBox().Text(Path);
                }

                Result->Release();
            }
        }
    }
}
