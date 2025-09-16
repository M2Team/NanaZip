#include "pch.h"
#include "CopyPage.h"
#include "CopyPage.g.cpp"

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::NanaZip::Modern::implementation
{
    DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(
        TitleText,
        winrt::hstring,
        winrt::NanaZip::Modern::implementation::CopyPage,
        winrt::NanaZip::Modern::CopyPage,
        L"");

    DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(
        PathLabelText,
        winrt::hstring,
        winrt::NanaZip::Modern::implementation::CopyPage,
        winrt::NanaZip::Modern::CopyPage,
        L"");

    DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(
        PathText,
        winrt::hstring,
        winrt::NanaZip::Modern::implementation::CopyPage,
        winrt::NanaZip::Modern::CopyPage,
        L"");

    DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(
        InfoText,
        winrt::hstring,
        winrt::NanaZip::Modern::implementation::CopyPage,
        winrt::NanaZip::Modern::CopyPage,
        L"");

    void CopyPage::OkButtonClickedHandler(
        winrt::Windows::Foundation::IInspectable const& sender,
        winrt::Windows::UI::Xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        this->OkButtonClicked(*this, args);
    }

    void CopyPage::CancelButtonClickedHandler(
        winrt::Windows::Foundation::IInspectable const& sender,
        winrt::Windows::UI::Xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        this->CancelButtonClicked(*this, args);
    }

    void CopyPage::PickerButtonClickedHandler(
        winrt::Windows::Foundation::IInspectable const& sender,
        winrt::Windows::UI::Xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        this->PickerButtonClicked(*this, args);
    }
}
