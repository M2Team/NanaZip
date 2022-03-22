// Common/Lang.cpp

#include "StdAfx.h"

#include "Lang.h"
#include "StringToInt.h"
#include "UTFConvert.h"

#include "../Windows/FileIO.h"

void CLang::Clear() throw()
{

}

bool CLang::OpenFromString(const AString &s2)
{
    UNREFERENCED_PARAMETER(s2);
    return true;
}

bool CLang::Open(CFSTR fileName, const char *id)
{
    UNREFERENCED_PARAMETER(fileName);
    UNREFERENCED_PARAMETER(id);
    return true;
}

#include <winrt/windows.foundation.h>
#include <winrt/windows.foundation.collections.h>
#include <winrt/windows.applicationmodel.resources.core.h>

#include <map>

std::map<UInt32, winrt::hstring> g_LanguageMap;

const wchar_t *CLang::Get(UInt32 id) const throw()
{
    auto Iterator = g_LanguageMap.find(id);
    if (Iterator == g_LanguageMap.end())
    {
        using winrt::Windows::ApplicationModel::Resources::Core::NamedResource;
        using winrt::Windows::ApplicationModel::Resources::Core::ResourceManager;
        using winrt::Windows::ApplicationModel::Resources::Core::ResourceMap;

        ResourceMap CurrentResourceMap =
            ResourceManager::Current().MainResourceMap().GetSubtree(L"Legacy");

        winrt::hstring ResourceName = L"Resource" + winrt::to_hstring(id);

        if (CurrentResourceMap.HasKey(ResourceName))
        {
            winrt::hstring Content = CurrentResourceMap.Lookup(
                ResourceName).Resolve().ValueAsString();
            g_LanguageMap.emplace(id, Content);
            return Content.data();
        }
        else
        {
            return nullptr;
        }
    }

    auto fuck = LocaleNameToLCID(L"zh-cn", 0);
    fuck = fuck;

    return Iterator->second.data();
}
