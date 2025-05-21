#include "pch.h"
#include "AddressBar.h"
#include "AddressBar.g.cpp"
#include "AddressBarItem.g.cpp"
#include "AddressBarQuerySubmittedEventArgs.g.cpp"

#include <winrt/Windows.System.h>

namespace wf = winrt::Windows::Foundation;
namespace wfc = wf::Collections;

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
        (wf::IInspectable const& sender, auto&&)
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
        (wf::IInspectable const& sender, auto&&)
        {
            winrt::Windows::UI::Xaml::Controls::TextBox textBox =
                sender.as<winrt::Windows::UI::Xaml::Controls::TextBox>();
            textBox.SelectAll();
        });

    m_textBoxElement.PreviewKeyDown({ get_weak(), &AddressBar::OnTextBoxPreviewKeyDown });

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

    wf::IInspectable existingItemsSource =
        ItemsSource();

    if (existingItemsSource)
    {
        m_suggestionsList.ItemsSource(existingItemsSource);
    }

    m_suggestionsList.ItemClick(
        [weak_this{ get_weak() }]
        (auto&&, winrt::Windows::UI::Xaml::Controls::ItemClickEventArgs const& args)
        {
            if (auto strong_this{ weak_this.get() })
            {
                strong_this->m_popup.IsOpen(false);
                auto submitArgs = winrt::make<AddressBarQuerySubmittedEventArgs>(
                    L"",
                    args.ClickedItem()
                );
                strong_this->m_querySubmittedEvent(
                    *strong_this,
                    submitArgs
                );
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
                wfc::IVectorView<wf::IInspectable> source =
                    strong_this->ItemsSource()
                    .as<wfc::IVectorView<wf::IInspectable>>();
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

void AddressBar::OnTextBoxPreviewKeyDown(
    wf::IInspectable const&,
    winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const& args)
{
    switch (args.Key())
    {
        case winrt::Windows::System::VirtualKey::Enter:
        {
            if (m_popup &&
                m_suggestionsList &&
                m_popup.IsOpen() &&
                m_suggestionsList.SelectionMode()
                != winrt::Windows::UI::Xaml::Controls::ListViewSelectionMode::None)
            {
                auto selectedItem = m_suggestionsList.SelectedItem();
                auto submitArgs = winrt::make<AddressBarQuerySubmittedEventArgs>(L"", selectedItem);
                // The suggestions dropdown is opened via keyboard.
                m_querySubmittedEvent(*this, submitArgs);
            }
            else
            {
                auto submitArgs = winrt::make<AddressBarQuerySubmittedEventArgs>(Text(), nullptr);
                m_querySubmittedEvent(*this, submitArgs);
            }
            args.Handled(true);
            break;
        }
        case winrt::Windows::System::VirtualKey::Down:
        {
            if (!m_popup || !m_suggestionsList)
                break;

            wfc::IVectorView<wf::IInspectable> source =
                ItemsSource().as<wfc::IVectorView<wf::IInspectable>>();

            if (!m_popup.IsOpen())
            {
                if (!OpenSuggestionsPopup(true))
                    break;
                m_suggestionsList.SelectedIndex(0);
                args.Handled(true);
                break;
            }

            int currentIndex = m_suggestionsList.SelectedIndex();
            if (currentIndex < (int)source.Size() - 1)
            {
                m_suggestionsList.SelectedIndex(currentIndex + 1);
            }

            args.Handled(true);

            break;
        }
        case winrt::Windows::System::VirtualKey::Up:
        {
            if (!m_popup || !m_suggestionsList || !m_popup.IsOpen())
                break;

            int currentIndex = m_suggestionsList.SelectedIndex();
            if (currentIndex == 0)
                m_popup.IsOpen(false);
            else
                m_suggestionsList.SelectedIndex(currentIndex - 1);

            args.Handled(true);
            break;
        }
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
                    wf::IInspectable(nullptr),
                    &AddressBar::OnTextChanged
                )
            );
    }
    return s_textProperty;
}

void AddressBar::OnTextChanged(
    wf::IInspectable const& sender,
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
                    wf::IInspectable(nullptr),
                    &AddressBar::OnIconSourceChanged
                )
            );
    }
    return s_iconSourceProperty;
}

void AddressBar::OnIconSourceChanged(
    wf::IInspectable const& sender,
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

wf::IInspectable AddressBar::ItemsSource()
{
    return GetValue(ItemsSourceProperty());
}

void AddressBar::ItemsSource(wf::IInspectable const& itemsSource)
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
                winrt::xaml_typename<wf::IInspectable>(),
                winrt::xaml_typename<winrt::NanaZip::ModernExperience::AddressBar>(),
                winrt::Windows::UI::Xaml::PropertyMetadata(
                    wf::IInspectable(nullptr),
                    &AddressBar::OnItemsSourceChanged
                )
            );
    }
    return s_itemsSourceProperty;
}

void AddressBar::OnItemsSourceChanged(
    wf::IInspectable const& sender,
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
    wf::IInspectable const& sender,
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
    wf::TypedEventHandler<
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
    wf::TypedEventHandler<
    winrt::NanaZip::ModernExperience::AddressBar,
    wf::IInspectable>
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

bool AddressBar::OpenSuggestionsPopup(
    bool isKeyboard
)
{
    if (!m_popup || !m_textBoxElement || !m_suggestionsList)
        return false;

    m_dropDownOpenedEvent(*this, nullptr);

    wfc::IVectorView<wf::IInspectable> source =
        ItemsSource().as<wfc::IVectorView<wf::IInspectable>>();
    if (source.Size() == 0)
        return false;

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

    return true;
}

winrt::hstring AddressBarItem::Text()
{
    return m_text;
}

void AddressBarItem::Text(winrt::hstring const& text)
{
    m_text = text;
}

winrt::Windows::UI::Xaml::Media::ImageSource AddressBarItem::Icon()
{
    return m_icon;
}

void AddressBarItem::Icon(winrt::Windows::UI::Xaml::Media::ImageSource const& icon)
{
    m_icon = icon;
}

winrt::Windows::UI::Xaml::Thickness AddressBarItem::Padding()
{
    return m_padding;
}

void AddressBarItem::Padding(winrt::Windows::UI::Xaml::Thickness const& padding)
{
    m_padding = padding;
}

AddressBarQuerySubmittedEventArgs::
AddressBarQuerySubmittedEventArgs(
    winrt::hstring const& queryText,
    wf::IInspectable const& suggestionChosen
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

wf::IInspectable
AddressBarQuerySubmittedEventArgs::
ChosenSuggestion()
{
    return m_chosenSuggestion;
}
