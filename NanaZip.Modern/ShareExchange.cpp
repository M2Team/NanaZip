#include "ShareExchange.h"

#include <ShObjIdl_core.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Storage.h>

namespace winrt
{
    using Windows::ApplicationModel::DataTransfer::DataTransferManager;
    using Windows::ApplicationModel::DataTransfer::DataRequestedEventArgs;
    using Windows::ApplicationModel::DataTransfer::DataPackageOperation;
    using Windows::Storage::IStorageItem;
    using Windows::Storage::StorageFile;
}

K7ShareExchangeInterop::K7ShareExchangeInterop(
    _In_ std::vector<std::wstring> const& SharingPaths) :
    m_StdPaths(SharingPaths)
{
}

void K7ShareExchangeInterop::SharingByWindowsMessaging(
    _In_opt_ HWND const& ParentWindowHandle)
{
    if (this->m_MapiFiles.empty())
    {
        this->m_MapiFiles.reserve(m_StdPaths.size());
        for (auto& Path : m_StdPaths)
        {
            MapiFileDescW Cache{};
            Cache.nPosition = static_cast<ULONG>(-1);
            Cache.lpszPathName = Path.data();
            //Cache.lpszFileName =
            this->m_MapiFiles.emplace_back(Cache);
        }
    }

    static decltype(::MAPISendMailW)* volatile pMAPISendMailW{
        reinterpret_cast<decltype(::MAPISendMailW)*>(
            winrt::check_pointer(::GetProcAddress(
            winrt::check_pointer(::LoadLibraryExW(
                L"mapi32.dll",
                nullptr,
                LOAD_LIBRARY_SEARCH_SYSTEM32)),
            "MAPISendMailW"))) };

    MapiMessageW Message{};
    Message.nFileCount = static_cast<ULONG>(this->m_MapiFiles.size());
    Message.lpFiles = this->m_MapiFiles.data();

    ::EnableWindow(ParentWindowHandle, false);
    pMAPISendMailW(
        reinterpret_cast<LHANDLE>(nullptr),
        reinterpret_cast<ULONG_PTR>(ParentWindowHandle),
        &Message,
        MAPI_DIALOG & MAPI_LOGON_UI & MAPI_FORCE_UNICODE,
        0);
    ::EnableWindow(ParentWindowHandle, true);
}

void K7ShareExchangeInterop::SharingByDataTransferManager(
    _In_ HWND const& ParentWindowHandle)
{
    winrt::DataTransferManager Manager{ nullptr };
    winrt::com_ptr<IDataTransferManagerInterop> Interop{
        winrt::get_activation_factory
            <winrt::DataTransferManager, IDataTransferManagerInterop>() };
    winrt::check_hresult(Interop->GetForWindow(
        ParentWindowHandle,
        winrt::guid_of<winrt::DataTransferManager>(),
        winrt::put_abi(Manager)));

    if (Manager.IsSupported())
    {
        if (!this->m_WinRtFiles)
        {
            this->m_WinRtFiles =
                winrt::single_threaded_vector
                <winrt::Windows::Storage::IStorageItem>();
            for (auto const& Path : this->m_StdPaths)
            {
                this->m_WinRtFiles.Append(
                    winrt::StorageFile::GetFileFromPathAsync(Path).get());
            }
        }

        Manager.DataRequested(
            [this](winrt::DataTransferManager const&,
                winrt::DataRequestedEventArgs const& Args)
            {
                Args.Request().Data().Properties().Title(L"NanaZip");
                Args.Request().Data().SetStorageItems(this->m_WinRtFiles);
                Args.Request().Data().RequestedOperation(
                    winrt::DataPackageOperation::None);
            });
        winrt::check_hresult(Interop->ShowShareUIForWindow(
            ParentWindowHandle));
    }
}

