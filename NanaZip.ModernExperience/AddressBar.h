#pragma once

#include "AddressBar.g.h"
#include "AddressBarQuerySubmittedEventArgs.g.h"

#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Input.h>

namespace winrt::NanaZip::ModernExperience::implementation
{
    struct AddressBar : AddressBarT<AddressBar>
    {
        AddressBar() = default;

        void OnApplyTemplate();
        void OnKeyUp(winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const&);

        winrt::hstring Text();
        void Text(winrt::hstring const&);

        static winrt::Windows::UI::Xaml::DependencyProperty TextProperty();

        winrt::Windows::UI::Xaml::Media::ImageSource IconSource();
        void IconSource(winrt::Windows::UI::Xaml::Media::ImageSource const&);

        static winrt::Windows::UI::Xaml::DependencyProperty IconSourceProperty();

        bool IsUpButtonEnabled();
        void IsUpButtonEnabled(bool);

        static winrt::Windows::UI::Xaml::DependencyProperty
            IsUpButtonEnabledProperty();

        winrt::event_token QuerySubmitted(
            winrt::Windows::Foundation::TypedEventHandler<
            winrt::NanaZip::ModernExperience::AddressBar,
            winrt::NanaZip::ModernExperience::AddressBarQuerySubmittedEventArgs>
            const&
        );
        void QuerySubmitted(winrt::event_token const& token) noexcept;

        winrt::event_token UpButtonClicked(
            winrt::Windows::UI::Xaml::RoutedEventHandler const&
        );
        void UpButtonClicked(winrt::event_token const& token) noexcept;


    private:
        static void OnTextChanged(
            winrt::Windows::Foundation::IInspectable const&,
            winrt::Windows::UI::Xaml::DependencyPropertyChangedEventArgs const&
        );
        static void OnIconSourceChanged(
            winrt::Windows::Foundation::IInspectable const&,
            winrt::Windows::UI::Xaml::DependencyPropertyChangedEventArgs const&
        );
        static void OnUpButtonEnabledChanged(
            winrt::Windows::Foundation::IInspectable const&,
            winrt::Windows::UI::Xaml::DependencyPropertyChangedEventArgs const&
        );

        inline static winrt::Windows::UI::Xaml::DependencyProperty s_textProperty{ nullptr };
        inline static winrt::Windows::UI::Xaml::DependencyProperty s_iconSourceProperty{ nullptr };
        inline static winrt::Windows::UI::Xaml::DependencyProperty s_upButtonEnabledProperty{ nullptr };

        winrt::Windows::UI::Xaml::Controls::TextBox m_textBoxElement{ nullptr };
        winrt::Windows::UI::Xaml::Controls::Image m_iconElement{ nullptr };
        winrt::Windows::UI::Xaml::Controls::Primitives::Popup
            m_popup{ nullptr };
        winrt::Windows::UI::Xaml::Controls::Button m_upButtonElement{ nullptr };

        winrt::event<
            winrt::Windows::Foundation::TypedEventHandler<
            winrt::NanaZip::ModernExperience::AddressBar,
            winrt::NanaZip::ModernExperience::AddressBarQuerySubmittedEventArgs>>
            m_querySubmittedEvent;

        winrt::event<winrt::Windows::UI::Xaml::RoutedEventHandler>
            m_upButtonClickedEvent;
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

namespace winrt::NanaZip::ModernExperience::factory_implementation
{
    struct AddressBar : AddressBarT<AddressBar, implementation::AddressBar>
    { };
}
