#include "pch.h"
#include "AddressBar.h"
#include "AddressBar.g.cpp"
#include "AddressBarQuerySubmittedEventArgs.g.cpp"

#include <winrt/Windows.System.h>

using namespace winrt::NanaZip::ModernExperience::implementation;

void AddressBar::OnApplyTemplate()
{
    m_textBoxElement =
        GetTemplateChild(L"TextBoxElement")
        .as<winrt::Windows::UI::Xaml::Controls::TextBox>();

    // Workaround for https://github.com/microsoft/microsoft-ui-xaml/issues/5341
    winrt::Windows::UI::Xaml::Controls::TextCommandBarFlyout textFlyout;
    textFlyout.Placement(
        winrt::Windows::UI::Xaml::Controls::Primitives::
        FlyoutPlacementMode::
        BottomEdgeAlignedLeft);
    m_textBoxElement.ContextFlyout(textFlyout);
    m_textBoxElement.SelectionFlyout(textFlyout);

    winrt::hstring currentText = Text();
    m_textBoxElement.Text(currentText);

    m_textBoxElement.TextChanged(
        [weak_this{ get_weak() }]
        (winrt::Windows::Foundation::IInspectable const& sender, auto&&)
        {
            if (auto strong_this{ weak_this.get() })
            {
                winrt::Windows::UI::Xaml::Controls::TextBox textBox =
                    sender.as<winrt::Windows::UI::Xaml::Controls::TextBox>();

                if (textBox.Text() != strong_this->Text())
                {
                    strong_this->Text(textBox.Text());
                }
            }
        });

    m_iconElement =
        GetTemplateChild(L"IconElement")
        .as<winrt::Windows::UI::Xaml::Controls::Image>();

    winrt::Windows::UI::Xaml::Media::ImageSource currentSource =
        IconSource();

    if (currentSource)
    {
        m_iconElement.Source(currentSource);
    }

    winrt::Windows::UI::Xaml::Controls::Button m_dropDownButton =
        GetTemplateChild(L"DropDownButton")
        .as<winrt::Windows::UI::Xaml::Controls::Button>();

    m_upButtonElement =
        GetTemplateChild(L"UpButton")
        .as<winrt::Windows::UI::Xaml::Controls::Button>();

    m_upButtonElement.Click(
        [weak_this{ get_weak() }]
        (
            auto&&,
            winrt::Windows::UI::Xaml::RoutedEventArgs const&
            args
        )
        {
            if (auto strong_this{ weak_this.get() })
            {
                strong_this->m_upButtonClickedEvent(*strong_this, args);
            }
        });

    m_upButtonElement.IsEnabled(IsUpButtonEnabled());

    __super::OnApplyTemplate();
}

void AddressBar::OnKeyUp(winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const& args)
{
    if (args.Key() == winrt::Windows::System::VirtualKey::Enter)
    {
        auto submitArgs = winrt::make_self<AddressBarQuerySubmittedEventArgs>(Text(), nullptr);
        m_querySubmittedEvent(*this, *submitArgs);
    }
    __super::OnKeyUp(args);
}

winrt::hstring AddressBar::Text()
{
    auto value = GetValue(TextProperty());
    if (!value)
        return L"";
    return winrt::unbox_value<winrt::hstring>(
        value
    );
}

void AddressBar::Text(winrt::hstring const& string)
{
    SetValue(
        TextProperty(),
        winrt::box_value(string)
    );
}

winrt::Windows::UI::Xaml::DependencyProperty AddressBar::TextProperty()
{
    if (!s_textProperty)
    {
        s_textProperty =
            winrt::Windows::UI::Xaml::DependencyProperty::Register(
                L"Text",
                winrt::xaml_typename<winrt::hstring>(),
                winrt::xaml_typename<winrt::NanaZip::ModernExperience::AddressBar>(),
                winrt::Windows::UI::Xaml::PropertyMetadata(
                    winrt::Windows::Foundation::IInspectable(nullptr),
                    &AddressBar::OnTextChanged
                )
            );
    }
    return s_textProperty;
}

void AddressBar::OnTextChanged(
    winrt::Windows::Foundation::IInspectable const& sender,
    winrt::Windows::UI::Xaml::DependencyPropertyChangedEventArgs const& args)
{
    winrt::NanaZip::ModernExperience::AddressBar addressBar =
        sender.as<winrt::NanaZip::ModernExperience::AddressBar>();

    winrt::NanaZip::ModernExperience::implementation::AddressBar* addressBarImpl =
        winrt::get_self<winrt::NanaZip::ModernExperience::implementation::AddressBar>(
            addressBar
        );

    if (winrt::Windows::UI::Xaml::Controls::TextBox textBoxElement =
        addressBarImpl->m_textBoxElement)
    {
        winrt::hstring newText =
            winrt::unbox_value<winrt::hstring>(
                args.NewValue()
            );

        if (textBoxElement.Text() != newText)
            textBoxElement.Text(newText);
    }
}

winrt::Windows::UI::Xaml::Media::ImageSource AddressBar::IconSource()
{
    return GetValue(IconSourceProperty())
        .as<winrt::Windows::UI::Xaml::Media::ImageSource>();
}

void AddressBar::IconSource(winrt::Windows::UI::Xaml::Media::ImageSource const& iconSource)
{
    SetValue(
        IconSourceProperty(),
        iconSource
    );
}

winrt::Windows::UI::Xaml::DependencyProperty AddressBar::IconSourceProperty()
{
    if (!s_iconSourceProperty)
    {
        s_iconSourceProperty =
            winrt::Windows::UI::Xaml::DependencyProperty::Register(
                L"IconSource",
                winrt::xaml_typename<winrt::Windows::UI::Xaml::Media::ImageSource>(),
                winrt::xaml_typename<winrt::NanaZip::ModernExperience::AddressBar>(),
                winrt::Windows::UI::Xaml::PropertyMetadata(
                    winrt::Windows::Foundation::IInspectable(nullptr),
                    &AddressBar::OnIconSourceChanged
                )
            );
    }
    return s_iconSourceProperty;
}

void AddressBar::OnIconSourceChanged(
    winrt::Windows::Foundation::IInspectable const& sender,
    winrt::Windows::UI::Xaml::DependencyPropertyChangedEventArgs const& args)
{
    winrt::NanaZip::ModernExperience::AddressBar addressBar =
        sender.as<winrt::NanaZip::ModernExperience::AddressBar>();

    winrt::NanaZip::ModernExperience::implementation::AddressBar* addressBarImpl =
        winrt::get_self<winrt::NanaZip::ModernExperience::implementation::AddressBar>(
            addressBar
        );

    if (winrt::Windows::UI::Xaml::Controls::Image iconElement =
        addressBarImpl->m_iconElement)
    {
        winrt::Windows::UI::Xaml::Media::ImageSource newSource =
            args.NewValue()
            .as<winrt::Windows::UI::Xaml::Media::ImageSource>();

        iconElement.Source(newSource);
    }
}

bool AddressBar::IsUpButtonEnabled()
{
    return winrt::unbox_value<bool>(
        GetValue(IsUpButtonEnabledProperty())
    );
}

void AddressBar::IsUpButtonEnabled(bool isUpButtonEnabled)
{
    SetValue(
        IsUpButtonEnabledProperty(),
        winrt::box_value(isUpButtonEnabled)
    );
}

winrt::Windows::UI::Xaml::DependencyProperty AddressBar::IsUpButtonEnabledProperty()
{
    if (!s_upButtonEnabledProperty)
    {
        s_upButtonEnabledProperty =
            winrt::Windows::UI::Xaml::DependencyProperty::Register(
                L"IsUpButtonEnabled",
                winrt::xaml_typename<bool>(),
                winrt::xaml_typename<winrt::NanaZip::ModernExperience::AddressBar>(),
                winrt::Windows::UI::Xaml::PropertyMetadata(
                    winrt::box_value(true),
                    &AddressBar::OnUpButtonEnabledChanged
                )
            );
    }
    return s_upButtonEnabledProperty;
}

void AddressBar::OnUpButtonEnabledChanged(
    winrt::Windows::Foundation::IInspectable const& sender,
    winrt::Windows::UI::Xaml::DependencyPropertyChangedEventArgs const& args)
{
    winrt::NanaZip::ModernExperience::AddressBar addressBar =
        sender.as<winrt::NanaZip::ModernExperience::AddressBar>();

    winrt::NanaZip::ModernExperience::implementation::AddressBar* addressBarImpl =
        winrt::get_self<winrt::NanaZip::ModernExperience::implementation::AddressBar>(
            addressBar
        );

    if (winrt::Windows::UI::Xaml::Controls::Button upButtonElement =
        addressBarImpl->m_upButtonElement)
    {
        bool newValue =
            winrt::unbox_value<bool>(
                args.NewValue()
            );

        upButtonElement.IsEnabled(newValue);
    }
}

winrt::event_token
AddressBar::QuerySubmitted(
    winrt::Windows::Foundation::TypedEventHandler<
    winrt::NanaZip::ModernExperience::AddressBar,
    winrt::NanaZip::ModernExperience::AddressBarQuerySubmittedEventArgs>
    const& handler
)
{
    return m_querySubmittedEvent.add(handler);
}

void AddressBar::QuerySubmitted(
    winrt::event_token const& token
) noexcept
{
    m_querySubmittedEvent.remove(token);
}

winrt::event_token
AddressBar::UpButtonClicked(
    winrt::Windows::UI::Xaml::RoutedEventHandler const&
    handler
)
{
    return m_upButtonClickedEvent.add(handler);
}

void AddressBar::UpButtonClicked(
    winrt::event_token const& token
) noexcept
{
    m_upButtonClickedEvent.remove(token);
}

AddressBarQuerySubmittedEventArgs::
AddressBarQuerySubmittedEventArgs(
    winrt::hstring const& queryText,
    winrt::Windows::Foundation::IInspectable const& suggestionChosen
)
{
    m_queryText = queryText;
    m_chosenSuggestion = suggestionChosen;
}

winrt::hstring
AddressBarQuerySubmittedEventArgs::
QueryText()
{
    return m_queryText;
}

winrt::Windows::Foundation::IInspectable
AddressBarQuerySubmittedEventArgs::
ChosenSuggestion()
{
    return m_chosenSuggestion;
}
