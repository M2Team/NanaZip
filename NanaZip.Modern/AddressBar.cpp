#include "pch.h"
#include "AddressBar.h"
#include "AddressBar.g.cpp"
#include "AddressBarItem.g.cpp"
#include "AddressBarQuerySubmittedEventArgs.g.cpp"

#include <winrt/Windows.System.h>

namespace winrt::NanaZip::Modern::implementation
{
    void AddressBar::OnApplyTemplate()
    {
        m_textBoxElement =
            GetTemplateChild(L"TextBoxElement")
            .as<winrt::TextBox>();

        // Workaround for https://github.com/microsoft/microsoft-ui-xaml/issues/5341
        winrt::TextCommandBarFlyout textFlyout;
        textFlyout.Placement(
            winrt::FlyoutPlacementMode::BottomEdgeAlignedLeft);
        m_textBoxElement.ContextFlyout(textFlyout);
        m_textBoxElement.SelectionFlyout(textFlyout);

        m_textBoxElement.GotFocus(
            []
            (winrt::IInspectable const& sender, auto&&)
            {
                winrt::TextBox textBox =
                    sender.as<winrt::TextBox>();
                textBox.SelectAll();
            });

        m_textBoxElement.PreviewKeyDown({ get_weak(), &AddressBar::OnTextBoxPreviewKeyDown });

        m_popup =
            GetTemplateChild(L"SuggestionsPopup")
            .as<winrt::Popup>();

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
            .as<winrt::ListView>();

        m_suggestionsList.ItemClick(
            [weak_this{ get_weak() }]
            (auto&&, winrt::ItemClickEventArgs const& args)
            {
                if (auto strong_this{ weak_this.get() })
                {
                    strong_this->m_popup.IsOpen(false);
                    auto submitArgs = winrt::make<AddressBarQuerySubmittedEventArgs>(
                        L"",
                        args.ClickedItem()
                    );
                    strong_this->QuerySubmitted(
                        *strong_this,
                        submitArgs
                    );
                }
            }
        );

        winrt::Button m_dropDownButton =
            GetTemplateChild(L"DropDownButton")
            .as<winrt::Button>();

        m_dropDownButton.Click(
            [weak_this{ get_weak() }]
            (
                auto&&, auto&&
                )
            {
                if (auto strong_this{ weak_this.get() })
                {
                    winrt::IVectorView<winrt::IInspectable> source =
                        strong_this->ItemsSource()
                        .as<winrt::IVectorView<winrt::IInspectable>>();
                    strong_this->OpenSuggestionsPopup(false);
                }
            });

        this->m_upButtonElement =
            GetTemplateChild(L"UpButton")
            .as<winrt::Button>();

        this->m_upButtonElement.Click(
            [weak_this{ get_weak() }]
            (
                auto&&,
                winrt::RoutedEventArgs const&
                args
                )
            {
                if (auto strong_this{ weak_this.get() })
                {
                    strong_this->UpButtonClicked(*strong_this, args);
                }
            });

        __super::OnApplyTemplate();
    }

    void AddressBar::OnTextBoxPreviewKeyDown(
        winrt::IInspectable const&,
        winrt::KeyRoutedEventArgs const& args)
    {
        switch (args.Key())
        {
        case winrt::VirtualKey::Enter:
        {
            if (m_popup &&
                m_suggestionsList &&
                m_popup.IsOpen() &&
                m_suggestionsList.SelectionMode()
                != winrt::ListViewSelectionMode::None)
            {
                auto selectedItem = m_suggestionsList.SelectedItem();
                auto submitArgs = winrt::make<AddressBarQuerySubmittedEventArgs>(L"", selectedItem);
                // The suggestions dropdown is opened via keyboard.
                QuerySubmitted(*this, submitArgs);
            }
            else
            {
                auto submitArgs = winrt::make<AddressBarQuerySubmittedEventArgs>(Text(), nullptr);
                QuerySubmitted(*this, submitArgs);
            }
            args.Handled(true);
            break;
        }
        case winrt::VirtualKey::Down:
        {
            if (!m_popup || !m_suggestionsList)
                break;

            winrt::IVectorView<winrt::IInspectable> source =
                ItemsSource().as<winrt::IVectorView<winrt::IInspectable>>();

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
        case winrt::VirtualKey::Up:
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

    DEPENDENCY_PROPERTY_SOURCE_BOX(
        Text,
        winrt::hstring,
        AddressBar,
        winrt::NanaZip::Modern::AddressBar
    );

    DEPENDENCY_PROPERTY_SOURCE_NOBOX(
        IconSource,
        winrt::ImageSource,
        AddressBar,
        winrt::NanaZip::Modern::AddressBar
    );

    DEPENDENCY_PROPERTY_SOURCE_NOBOX(
        ItemsSource,
        winrt::IInspectable,
        AddressBar,
        winrt::NanaZip::Modern::AddressBar
    );

    DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(
        IsUpButtonEnabled,
        bool,
        AddressBar,
        winrt::NanaZip::Modern::AddressBar,
        true
    );

    bool AddressBar::OpenSuggestionsPopup(
        bool isKeyboard
    )
    {
        if (!m_popup || !m_textBoxElement || !m_suggestionsList)
            return false;

        DropDownOpened(*this, nullptr);

        winrt::IVectorView<winrt::IInspectable> source =
            ItemsSource().as<winrt::IVectorView<winrt::IInspectable>>();
        if (source.Size() == 0)
            return false;

        if (!isKeyboard)
        {
            m_suggestionsList.SelectionMode(
                winrt::ListViewSelectionMode::None
            );
        }
        else {
            m_suggestionsList.SelectionMode(
                winrt::ListViewSelectionMode::Single
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
        m_textBoxElement.Focus(winrt::FocusState::Programmatic);

        return true;
    }

    AddressBarQuerySubmittedEventArgs::
        AddressBarQuerySubmittedEventArgs(
            winrt::hstring const& queryText,
            winrt::IInspectable const& suggestionChosen
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

    winrt::IInspectable
        AddressBarQuerySubmittedEventArgs::
        ChosenSuggestion()
    {
        return m_chosenSuggestion;
    }
}
