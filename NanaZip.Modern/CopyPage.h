#pragma once

#include "CopyPage.g.h"
#include "ControlMacros.h"

#include <Mile.Helpers.CppWinRT.h>

namespace winrt::NanaZip::Modern::implementation
{
    struct CopyPage : CopyPageT<CopyPage>
    {
        CopyPage() 
        {
            // Xaml objects should not call InitializeComponent during construction.
            // See https://github.com/microsoft/cppwinrt/tree/master/nuget#initializecomponent
        }

        DEPENDENCY_PROPERTY_HEADER(
            TitleText,
            winrt::hstring);

        DEPENDENCY_PROPERTY_HEADER(
            PathLabelText,
            winrt::hstring);

        DEPENDENCY_PROPERTY_HEADER(
            PathText,
            winrt::hstring);

        Mile::WinRT::Event<
            winrt::Windows::Foundation::TypedEventHandler<
                winrt::NanaZip::Modern::CopyPage,
                winrt::Windows::Foundation::IInspectable>> OkButtonClicked;

        Mile::WinRT::Event<
            winrt::Windows::Foundation::TypedEventHandler<
            winrt::NanaZip::Modern::CopyPage,
            winrt::Windows::Foundation::IInspectable>> CancelButtonClicked;

        void OkButtonClickedHandler(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Windows::UI::Xaml::RoutedEventArgs const& args);

        void CancelButtonClickedHandler(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Windows::UI::Xaml::RoutedEventArgs const& args);
    };
}

namespace winrt::NanaZip::Modern::factory_implementation
{
    struct CopyPage : CopyPageT<
        CopyPage,
        implementation::CopyPage>
    {
    };
}
