#pragma once

#include <juce_core/juce_core.h>

namespace daw
{
// MSVC may read plain narrow literals in the active Windows code page; make
// German UI text unambiguously UTF-8 (same approach as MainComponent::uiText).
inline juce::String text(const char* utf8Text)
{
    return juce::String(juce::CharPointer_UTF8(utf8Text));
}
} // namespace daw
