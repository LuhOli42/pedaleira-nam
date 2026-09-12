#pragma once

#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h> // juce::SHA256 lives here, not in juce_core

namespace openguitarmultifx::pkce
{

/** Standard base64 -> base64url (RFC 7636): -/_ instead of +//, no padding. */
inline juce::String base64Url (const void* data, size_t numBytes)
{
    auto b64 = juce::Base64::toBase64 (data, numBytes);
    b64 = b64.replaceCharacter ('+', '-').replaceCharacter ('/', '_');
    while (b64.endsWithChar ('='))
        b64 = b64.dropLastCharacters (1);
    return b64;
}

/** A random, URL-safe string suitable for a PKCE code_verifier or an OAuth state param. */
inline juce::String randomUrlSafeString (int numBytes = 32)
{
    juce::MemoryBlock block;
    block.setSize ((size_t) numBytes);
    auto* data = static_cast<uint8_t*> (block.getData());
    auto& rng = juce::Random::getSystemRandom();
    for (int i = 0; i < numBytes; ++i)
        data[i] = (uint8_t) rng.nextInt (256);
    return base64Url (block.getData(), block.getSize());
}

/** code_challenge = base64url(SHA256(code_verifier)), per RFC 7636 S256. */
inline juce::String codeChallengeFromVerifier (const juce::String& verifier)
{
    juce::SHA256 hash (verifier.toRawUTF8(), (size_t) verifier.getNumBytesAsUTF8());
    auto hex = hash.toHexString();

    juce::MemoryBlock raw;
    raw.setSize ((size_t) hex.length() / 2);
    auto* bytes = static_cast<uint8_t*> (raw.getData());
    for (int i = 0; i < hex.length() / 2; ++i)
        bytes[i] = (uint8_t) hex.substring (i * 2, i * 2 + 2).getHexValue32();

    return base64Url (raw.getData(), raw.getSize());
}

} // namespace openguitarmultifx::pkce
