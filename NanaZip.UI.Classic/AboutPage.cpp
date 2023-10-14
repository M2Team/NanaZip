#include "pch.h"
#include "AboutPage.h"
#if __has_include("AboutPage.g.cpp")
#include "AboutPage.g.cpp"
#endif

#include <string>

#include "../SevenZip/CPP/Common/Common.h"
#include <Mile.Project.Version.h>
#include "../SevenZip/C/CpuArch.h"
#include "../SevenZip/CPP/7zip/UI/Common/LoadCodecs.h"
#include "../SevenZip/CPP/7zip/UI/FileManager/LangUtils.h"
#include "../SevenZip/CPP/7zip/UI/FileManager/resourceGui.h"

using namespace winrt;
using namespace Windows::UI::Xaml;

extern CCodecs* g_CodecsObj;

#define IDD_ABOUT  2900
#define IDT_ABOUT_INFO  2901
#define IDB_ABOUT_HOMEPAGE   110

namespace winrt::NanaZip::implementation
{
    AboutPage::AboutPage()
    {
        InitializeComponent();

        std::wstring Title = std::wstring(
            ::LangString(IDD_ABOUT));
        if (Title.empty())
        {
            Title = L"About NanaZip";
        }

        std::wstring Version = std::wstring(
            "NanaZip " MILE_PROJECT_VERSION_STRING);
        Version.append(
            L" (" MILE_PROJECT_DOT_VERSION_STRING L")");
        Version.append(
            UString(" (" MY_CPU_NAME ")"));

        std::wstring Content = std::wstring(
            ::LangString(IDT_ABOUT_INFO));
        if (Content.empty())
        {
            Content = L"NanaZip is free software";
        }
#ifdef EXTERNAL_CODECS
        if (g_CodecsObj)
        {
            UString s;
            g_CodecsObj->GetCodecsErrorMessage(s);
            if (!s.IsEmpty())
            {
                Content.append(L"\r\n\r\n");
                Content.append(s);
            }
        }
#endif

        this->Title().Text(Title);
        this->Version().Text(Version);
        this->Content().Text(Content);
    }
}
