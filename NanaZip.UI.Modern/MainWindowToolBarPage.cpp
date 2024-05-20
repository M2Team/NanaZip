#include "pch.h"
#include "MainWindowToolBarPage.h"
#if __has_include("MainWindowToolBarPage.g.cpp")
#include "MainWindowToolBarPage.g.cpp"
#endif

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::NanaZip::Modern::implementation
{
    MainWindowToolBarPage::MainWindowToolBarPage(
        _In_ HWND WindowHandle) :
        m_WindowHandle(WindowHandle)
    {

    }

    void MainWindowToolBarPage::InitializeComponent()
    {
        MainWindowToolBarPageT::InitializeComponent();
    }
}
