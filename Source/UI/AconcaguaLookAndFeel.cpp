#include "AconcaguaLookAndFeel.h"

AconcaguaLookAndFeel::AconcaguaLookAndFeel()
{
    // Dark colour scheme
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff1e1e2e));
    setColour(juce::TextButton::buttonColourId, juce::Colour(0xff313244));
    setColour(juce::TextButton::textColourOffId, juce::Colour(0xffcdd6f4));
    setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff585b70));
    setColour(juce::TextButton::textColourOnId, juce::Colour(0xffcdd6f4));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff313244));
    setColour(juce::ComboBox::textColourId, juce::Colour(0xffcdd6f4));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff585b70));
    setColour(juce::Label::textColourId, juce::Colour(0xffcdd6f4));
    setColour(juce::Slider::backgroundColourId, juce::Colour(0xff313244));
    setColour(juce::Slider::thumbColourId, juce::Colour(0xff89b4fa));
    setColour(juce::Slider::trackColourId, juce::Colour(0xff585b70));
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffcdd6f4));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff313244));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff585b70));
    setColour(juce::TabbedComponent::backgroundColourId, juce::Colour(0xff1e1e2e));
    setColour(juce::TabbedComponent::outlineColourId, juce::Colour(0xff313244));
    setColour(juce::TabbedButtonBar::tabOutlineColourId, juce::Colour(0xff313244));
    setColour(juce::TabbedButtonBar::tabTextColourId, juce::Colour(0xffa6adc8));
    setColour(juce::TabbedButtonBar::frontTextColourId, juce::Colour(0xffcdd6f4));
    setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff1e1e2e));
    setColour(juce::ListBox::textColourId, juce::Colour(0xffcdd6f4));
    setColour(juce::TableHeaderComponent::backgroundColourId, juce::Colour(0xff313244));
    setColour(juce::TableHeaderComponent::textColourId, juce::Colour(0xffcdd6f4));
    setColour(juce::TableHeaderComponent::outlineColourId, juce::Colour(0xff585b70));
}
