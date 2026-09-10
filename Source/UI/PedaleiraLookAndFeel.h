#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace pedaleira
{

/**
    Inter (SIL OFL, embedded -- see Assets/Fonts/INTER-LICENSE.txt)
    everywhere instead of whatever generic sans the OS happens to have --
    the final target is a from-scratch embedded Linux build with no
    guaranteed font at all, and the default JUCE font reads as generic
    "desktop app", not a real product.

    Also bumps the popup menu font, which is what makes
    juce::PopupMenu AND juce::ComboBox's dropdown touch-sized:
    ComboBox has no per-instance "row height" hook the way
    PopupMenu::Options::withStandardItemHeight() does (used directly at
    each PopupMenu call site -- see Source/UI/TouchSizing.h), so this is
    the one place that also covers architectureBox
    (Tone3000SearchDialog's A1/A2/Custom picker) and any future
    juce::ComboBox.
*/
class PedaleiraLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PedaleiraLookAndFeel();

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& font) override;
    juce::Font getPopupMenuFont() override;

    /** For the handful of places (the big preset number/name -- see
        MainComponent) that specifically want the heavier display weight,
        not just "bold" (juce::Font's styleFlags only distinguish
        plain/bold/italic, not weight beyond that -- getTypefaceForFont()
        above already maps `bold` to Inter Bold; this is the one step
        further, applied by hand where it matters). */
    juce::Typeface::Ptr getExtraBoldTypeface() const noexcept { return extraBold; }

private:
    juce::Typeface::Ptr regular, bold, extraBold;
};

} // namespace pedaleira
