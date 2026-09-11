#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace pedaleira::icon
{

/**
    Draws one of the embedded Assets/Icons SVG files into `bounds`.

    Each drawIcon() override calls this with a `static const std::unique_ptr
    <juce::Drawable> icon = ...` built once from the matching IconData::
    binary resource (see CMakeLists.txt's PedaleiraNAM_Icons target) --
    this function just parses+caches nothing itself, it only draws.

    Why SVG assets instead of juce::Path calls: an earlier version of every
    drawIcon() hand-transcribed the approved reference-sheet prototype's
    geometry by eye, and drifted from it in several independent ways per
    icon (stroke width not scaled to the icon's actual on-screen size,
    juce::Graphics::drawLine()'s flat caps vs. the prototype's rounded
    ones, wrong inset/offset ratios, filled vs. outline circles). Embedding
    the literal approved SVG and letting juce::Drawable render it removes
    that whole class of transcription bug -- see
    docs/icons/AGENT-icon-notes.md.
*/
inline void drawSvg (juce::Graphics& g, juce::Rectangle<float> bounds, const juce::Drawable* svgIcon)
{
    if (svgIcon != nullptr)
        svgIcon->drawWithin (g, bounds, juce::RectanglePlacement::centred, 1.0f);
}

inline std::unique_ptr<juce::Drawable> loadSvg (const void* data, int dataSize)
{
    return juce::Drawable::createFromImageData (data, (size_t) dataSize);
}

} // namespace pedaleira::icon
