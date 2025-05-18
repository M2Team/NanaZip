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

    m_textBoxElement.GotFocus(
        []
        (winrt::Windows::Foundation::IInspectable const& sender, auto&&)
        {
            winrt::Windows::UI::Xaml::Controls::TextBox textBox =
                sender.as<winrt::Windows::UI::Xaml::Controls::TextBox>();
            textBox.SelectAll();
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

    m_popup =
        GetTemplateChild(L"SuggestionsPopup")
        .as<winrt::Windows::UI::Xaml::Controls::Primitives::Popup>();

    m_popup.Closed(
        [weak_this{ get_weak() }]
        (auto&&, auto&&)
        {
            if (auto strong_this{ weak_this.get() })
            {
                if (!strong_this->m_textBoxElement)
                    return;
                auto tbCorner = strong_this->m_textBoxElement.CornerRadius();
                tbCorner.BottomLeft = tbCorner.TopLeft;
                tbCorner.BottomRight = tbCorner.TopRight;
                strong_this->m_textBoxElement.CornerRadius(tbCorner);
            }
        });

    m_suggestionsList =
        GetTemplateChild(L"SuggestionsList")
        .as<winrt::Windows::UI::Xaml::Controls::ListView>();

    winrt::Windows::Foundation::IInspectable existingItemsSource =
        ItemsSource();

    if (existingItemsSource)
    {
        m_suggestionsList.ItemsSource(existingItemsSource);
    }

    m_suggestionsList.ItemClick(
        [weak_this{ get_weak() }]
        (auto&&, winrt::Windows::UI::Xaml::Controls::ItemClickEventArgs const& args)
        {
            UNREFERENCED_PARAMETER(args);
            if (auto strong_this{ weak_this.get() })
            {
                strong_this->m_popup.IsOpen(false);
            }
        }
    );

    winrt::Windows::UI::Xaml::Controls::Button m_dropDownButton =
        GetTemplateChild(L"DropDownButton")
        .as<winrt::Windows::UI::Xaml::Controls::Button>();

    m_dropDownButton.Click(
        [ weak_this{ get_weak() }]
        (
            auto&&, auto&&
        )
        {
            if (auto strong_this{ weak_this.get() })
            {
                strong_this->OpenSuggestionsPopup(false);
            }
        });

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
        auto submitArgs = winrt::make<AddressBarQuerySubmittedEventArgs>(Text(), nullptr);
        m_querySubmittedEvent(*this, submitArgs);
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

winrt::Windows::Foundation::IInspectable AddressBar::ItemsSource()
{
    return GetValue(ItemsSourceProperty());
}

void AddressBar::ItemsSource(winrt::Windows::Foundation::IInspectable const& itemsSource)
{
    SetValue(
        ItemsSourceProperty(),
        itemsSource
    );
}

winrt::Windows::UI::Xaml::DependencyProperty AddressBar::ItemsSourceProperty()
{
    if (!s_itemsSourceProperty)
    {
        s_itemsSourceProperty =
            winrt::Windows::UI::Xaml::DependencyProperty::Register(
                L"ItemsSource",
                winrt::xaml_typename<winrt::Windows::Foundation::IInspectable>(),
                winrt::xaml_typename<winrt::NanaZip::ModernExperience::AddressBar>(),
                winrt::Windows::UI::Xaml::PropertyMetadata(
                    winrt::Windows::Foundation::IInspectable(nullptr),
                    &AddressBar::OnItemsSourceChanged
                )
            );
    }
    return s_itemsSourceProperty;
}

void AddressBar::OnItemsSourceChanged(
    winrt::Windows::Foundation::IInspectable const& sender,
    winrt::Windows::UI::Xaml::DependencyPropertyChangedEventArgs const& args)
{
    winrt::NanaZip::ModernExperience::AddressBar addressBar =
        sender.as<winrt::NanaZip::ModernExperience::AddressBar>();

    winrt::NanaZip::ModernExperience::implementation::AddressBar* addressBarImpl =
        winrt::get_self<winrt::NanaZip::ModernExperience::implementation::AddressBar>(
            addressBar
        );

    if (addressBarImpl->m_suggestionsList)
    {
        addressBarImpl->m_suggestionsList.ItemsSource(args.NewValue());
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

winrt::event_token
AddressBar::DropDownOpened(
    winrt::Windows::Foundation::TypedEventHandler<
    winrt::NanaZip::ModernExperience::AddressBar,
    winrt::Windows::Foundation::IInspectable>
    const& handler
)
{
    return m_dropDownOpenedEvent.add(handler);
}

void AddressBar::DropDownOpened(
    winrt::event_token const& token
) noexcept
{
    m_dropDownOpenedEvent.remove(token);
}

void AddressBar::OpenSuggestionsPopup(
    bool isKeyboard
)
{
    if (!m_popup || !m_textBoxElement || !m_suggestionsList)
        return;

    m_dropDownOpenedEvent(*this, nullptr);

    if (!isKeyboard)
    {
        m_suggestionsList.SelectionMode(
            winrt::Windows::UI::Xaml::Controls::ListViewSelectionMode::None
        );
    }
    else {
        m_suggestionsList.SelectionMode(
            winrt::Windows::UI::Xaml::Controls::ListViewSelectionMode::Single
        );
    }

    m_suggestionsList.Width(m_textBoxElement.ActualWidth());
    m_popup.VerticalOffset(m_textBoxElement.ActualHeight());

    auto tbCorner = m_textBoxElement.CornerRadius();
    tbCorner.BottomLeft = tbCorner.BottomRight = 0;
    m_textBoxElement.CornerRadius(tbCorner);

    auto slCorner = m_suggestionsList.CornerRadius();
    slCorner.TopLeft = slCorner.TopRight = 0;
    m_suggestionsList.CornerRadius(slCorner);

    m_popup.IsOpen(true);
    m_textBoxElement.Focus(winrt::Windows::UI::Xaml::FocusState::Programmatic);
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
