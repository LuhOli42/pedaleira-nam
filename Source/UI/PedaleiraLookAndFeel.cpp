#include "PedaleiraLookAndFeel.h"

#include <BinaryData.h>

namespace pedaleira
{

PedaleiraLookAndFeel::PedaleiraLookAndFeel()
{
    regular   = juce::Typeface::createSystemTypefaceFor (BinaryData::InterRegular_ttf,
                                                           (size_t) BinaryData::InterRegular_ttfSize);
    bold      = juce::Typeface::createSystemTypefaceFor (BinaryData::InterBold_ttf,
                                                           (size_t) BinaryData::InterBold_ttfSize);
    extraBold = juce::Typeface::createSystemTypefaceFor (BinaryData::InterExtraBold_ttf,
                                                           (size_t) BinaryData::InterExtraBold_ttfSize);
}

juce::Typeface::Ptr PedaleiraLookAndFeel::getTypefaceForFont (const juce::Font& font)
{
    // A real bold file, not JUCE's algorithmic embolden of the regular
    // weight -- looks meaningfully better, especially at the large sizes
    // the preset name/number use (see MainComponent).
    return font.isBold() ? bold : regular;
}

juce::Font PedaleiraLookAndFeel::getPopupMenuFont()
{
    return juce::Font (juce::FontOptions (18.0f));
}

} // namespace pedaleira
