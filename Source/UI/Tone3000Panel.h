#pragma once

#include "OAuthLoginDialog.h"
#include "../Tone3000/Tone3000Manager.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

namespace pedaleira
{

/**
    TONE3000 account surface: paste your publishable client_id, log in.
    That's all this does now -- searching and downloading models moved
    into ParameterPanel itself, contextual to whichever block is selected
    (a Neural Amp block searches amps, a Cab block searches cabs, etc.),
    rather than a separate generic dialog you'd have to cross-reference by
    hand. See GearRouting.h::gearFilterForProcessorName.
*/
class Tone3000Panel : public juce::Component
{
public:
    explicit Tone3000Panel (Tone3000Manager& managerToUse);

    void resized() override;
    void paint (juce::Graphics& g) override;

    std::function<void()> onRequestClose;

private:
    void refreshLoginState();
    void doLogin();

    Tone3000Manager& manager;
    std::unique_ptr<OAuthLoginDialog> loginDialog;

    juce::Label clientIdLabel { {}, "client_id" };
    juce::TextEditor clientIdField;
    juce::TextButton saveClientIdButton { "Save key" };

    juce::TextButton loginButton { "Log in" };
    juce::TextButton logoutButton { "Log out" };
    juce::Label statusLabel;
    juce::TextButton closeButton { "Close" };
};

} // namespace pedaleira
