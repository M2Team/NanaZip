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

#ifdef _SFX
#include "../Windows/ResourceString.h"
#else
#include <winrt/windows.foundation.h>
#include <winrt/windows.foundation.collections.h>
#include <winrt/windows.applicationmodel.resources.core.h>
#endif

#include <map>

#ifdef _SFX
std::map<UInt32, UString> g_LanguageMap;
#else
std::map<UInt32, winrt::hstring> g_LanguageMap;
#endif

const wchar_t *CLang::Get(UInt32 id) const throw()
{
    auto Iterator = g_LanguageMap.find(id);
    if (Iterator == g_LanguageMap.end())
    {
#ifdef _SFX
        UString Content = NWindows::MyLoadString(id);
        if (Content.IsEmpty())
        {
            return nullptr;
        }

        return g_LanguageMap.emplace(id, Content).first->second;
#else
        using winrt::Windows::ApplicationModel::Resources::Core::ResourceManager;
        using winrt::Windows::ApplicationModel::Resources::Core::ResourceMap;

        ResourceMap CurrentResourceMap =
            ResourceManager::Current().MainResourceMap().GetSubtree(L"Legacy");

        winrt::hstring ResourceName = L"Resource" + winrt::to_hstring(id);

        if (CurrentResourceMap.HasKey(ResourceName))
        {
            winrt::hstring Content = CurrentResourceMap.Lookup(
                ResourceName).Candidates().GetAt(0).ValueAsString();
            g_LanguageMap.emplace(id, Content);
            return Content.data();
        }
        else
        {
            return nullptr;
        }
#endif    
    }

#ifdef _SFX
    return Iterator->second;
#else
    return Iterator->second.data();
#endif
}
