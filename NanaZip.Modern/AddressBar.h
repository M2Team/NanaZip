#pragma once

#include "AddressBar.g.h"
#include "AddressBarQuerySubmittedEventArgs.g.h"
#include "AddressBarItem.g.h"

#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Input.h>

#include "ControlMacros.h"

namespace winrt::NanaZip::Modern::implementation
{
    struct AddressBar : AddressBarT<AddressBar>
    {
        AddressBar() = default;

        void OnApplyTemplate();
        void OnTextBoxPreviewKeyDown(
            winrt::Windows::Foundation::IInspectable const&,
            winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const&);

        DEPENDENCY_PROPERTY_HEADER(Text, winrt::hstring);
        DEPENDENCY_PROPERTY_HEADER(
            IconSource,
            winrt::Windows::UI::Xaml::Media::ImageSource
        );
        DEPENDENCY_PROPERTY_HEADER(
            ItemsSource,
            winrt::Windows::Foundation::IInspectable
        );
        DEPENDENCY_PROPERTY_HEADER(
            IsUpButtonEnabled,
            bool
        );

        winrt::event_helper<
            winrt::Windows::Foundation::TypedEventHandler<
            winrt::NanaZip::Modern::AddressBar,
            winrt::NanaZip::Modern::AddressBarQuerySubmittedEventArgs>>
            QuerySubmitted;

        winrt::event_helper<winrt::Windows::UI::Xaml::RoutedEventHandler>
            UpButtonClicked;

        winrt::event_helper<
            winrt::Windows::Foundation::TypedEventHandler<
            winrt::NanaZip::Modern::AddressBar,
            winrt::Windows::Foundation::IInspectable>>
            DropDownOpened;

    private:
        bool OpenSuggestionsPopup(bool isKeyboard);

        winrt::Windows::UI::Xaml::Controls::TextBox m_textBoxElement{ nullptr };
        winrt::Windows::UI::Xaml::Controls::Primitives::Popup
            m_popup{ nullptr };
        winrt::Windows::UI::Xaml::Controls::Button m_upButtonElement{ nullptr };
        winrt::Windows::UI::Xaml::Controls::ListView m_suggestionsList{ nullptr };
    };

    struct AddressBarItem : AddressBarItemT<AddressBarItem>
    {
        AddressBarItem() = default;

        winrt::hstring Text();
        void Text(winrt::hstring const&);

        winrt::Windows::UI::Xaml::Media::ImageSource Icon();
        void Icon(winrt::Windows::UI::Xaml::Media::ImageSource const&);

        winrt::Windows::UI::Xaml::Thickness Padding();
        void Padding(winrt::Windows::UI::Xaml::Thickness const&);

    private:
        winrt::hstring m_text;
        winrt::Windows::UI::Xaml::Media::ImageSource m_icon{ nullptr };
        winrt::Windows::UI::Xaml::Thickness m_padding = { 0, 0, 0, 0 };
    };

    struct AddressBarQuerySubmittedEventArgs : AddressBarQuerySubmittedEventArgsT<AddressBarQuerySubmittedEventArgs>
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
