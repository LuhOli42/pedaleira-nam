#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace pedaleira
{

/**
    Shows the current preset's NUMBER, big and bold -- this is a stage
    instrument, not a desktop app; the player needs to read it from a few
    feet away, not squint at a toolbar (see AGENT.md's UI/UX Design
    Philosophy). Custom-painted rather than a juce::TextButton so its font
    size isn't at the mercy of LookAndFeel::getTextButtonFont()'s
    proportional-to-height cap, same idea as EffectBlockComponent/
    IOSelectorBlock elsewhere in this UI.

    Once scenes exist (a preset's own internal variations), this becomes
    number+letter ("1A") in the same slot -- not built yet, see
    MainComponent.h.
*/
class PresetBadge : public juce::Component
{
public:
    /** 0 = no preset loaded (shows a placeholder dash). */
    void setNumber (int number);

    /** The heavier display weight for the big number -- see
        PedaleiraLookAndFeel::getExtraBoldTypeface(). Falls back to the
        current LookAndFeel's bold if never set. */
    void setDisplayTypeface (juce::Typeface::Ptr typeface);

    void paint (juce::Graphics& g) override;
    void mouseUp (const juce::MouseEvent&) override;

    std::function<void()> onClicked;

private:
    int currentNumber = 0;
    juce::Typeface::Ptr displayTypeface;
};

} // namespace pedaleira
