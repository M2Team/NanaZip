#pragma once

#include <string>
#include <vector>

#include <Windows.h>
#include <MAPI.h>

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.h>

namespace winrt
{
    using Windows::Foundation::Collections::IVector;
    using Windows::Storage::IStorageItem;
}

class K7ShareExchangeInterop
{
private:
    std::vector<std::wstring> m_StdPaths;
    std::vector<MapiFileDescW> m_MapiFiles;
    winrt::IVector<winrt::IStorageItem> m_WinRtFiles{ nullptr };

public:
    K7ShareExchangeInterop(_In_ std::vector<std::wstring> const& SharingPaths);

    void SharingByWindowsMessaging(
    _In_opt_ HWND const& ParentWindowHandle);

    void SharingByDataTransferManager(
    _In_ HWND const& ParentWindowHandle);
};
