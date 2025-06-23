#pragma once

#include "AddressBar.g.h"
#include "AddressBarQuerySubmittedEventArgs.g.h"
#include "AddressBarItem.g.h"

#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Input.h>

#include <Mile.Helpers.CppWinRT.h>
#include "ControlMacros.h"

namespace winrt
{
    using namespace Windows::Foundation;
    using namespace Windows::Foundation::Collections;
    using namespace Windows::UI::Xaml;
    using namespace Windows::UI::Xaml::Controls;
    using namespace Windows::UI::Xaml::Controls::Primitives;
    using namespace Windows::UI::Xaml::Input;
    using namespace Windows::UI::Xaml::Media;
    using namespace Windows::System;
}

namespace winrt::NanaZip::Modern::implementation
{
    struct AddressBar : AddressBarT<AddressBar>
    {
        AddressBar() = default;

        void OnApplyTemplate();
        void OnTextBoxPreviewKeyDown(
            winrt::IInspectable const&,
            winrt::KeyRoutedEventArgs const&);

        DEPENDENCY_PROPERTY_HEADER(Text, winrt::hstring);
        DEPENDENCY_PROPERTY_HEADER(
            IconSource,
            winrt::ImageSource
        );
        DEPENDENCY_PROPERTY_HEADER(
            ItemsSource,
            winrt::IInspectable
        );
        DEPENDENCY_PROPERTY_HEADER(
            IsUpButtonEnabled,
            bool
        );

        Mile::WinRT::Event<
            winrt::TypedEventHandler<
            winrt::NanaZip::Modern::AddressBar,
            winrt::NanaZip::Modern::AddressBarQuerySubmittedEventArgs>>
            QuerySubmitted;

        Mile::WinRT::Event<winrt::Windows::UI::Xaml::RoutedEventHandler>
            UpButtonClicked;

        Mile::WinRT::Event<
            winrt::TypedEventHandler<
            winrt::NanaZip::Modern::AddressBar,
            winrt::IInspectable>>
            DropDownOpened;

    private:
        bool OpenSuggestionsPopup(bool isKeyboard);

        winrt::TextBox m_textBoxElement{ nullptr };
        winrt::Popup
            m_popup{ nullptr };
        winrt::Button m_upButtonElement{ nullptr };
        winrt::ListView m_suggestionsList{ nullptr };
    };

    struct AddressBarItem : AddressBarItemT<AddressBarItem>
    {
        AddressBarItem() = default;

        Mile::WinRT::Property<winrt::hstring> Text;
        Mile::WinRT::Property<winrt::ImageSource> Icon =
            Mile::WinRT::Property<winrt::ImageSource>(nullptr);
        Mile::WinRT::Property<winrt::Thickness> Padding;
    };

    struct AddressBarQuerySubmittedEventArgs :
        AddressBarQuerySubmittedEventArgsT<
        AddressBarQuerySubmittedEventArgs>
    {
        AddressBarQuerySubmittedEventArgs(
            winrt::hstring const&,
            winrt::Windows::Foundation::IInspectable const&);

        winrt::hstring QueryText();
        winrt::Windows::Foundation::IInspectable ChosenSuggestion();

    private:
        winrt::hstring m_queryText;
        winrt::Windows::Foundation::IInspectable m_chosenSuggestion{ nullptr };
    };
}

namespace winrt::NanaZip::Modern::factory_implementation
{
    struct AddressBar : AddressBarT<AddressBar, implementation::AddressBar>
    { };

    struct AddressBarItem : AddressBarItemT<AddressBarItem, implementation::AddressBarItem>
    { };
}
