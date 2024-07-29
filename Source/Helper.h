#pragma once

#include "juceHeader.h"
using namespace juce;

#include "DSP/Sampler.h"

namespace IDs
{
#define DECLARE_ID(name) const juce::Identifier name (#name);

DECLARE_ID (DATA_MODEL)
DECLARE_ID (sampleReader)
DECLARE_ID (centreFrequencyHz)

DECLARE_ID (MPE_SETTINGS)
DECLARE_ID (synthVoices)
DECLARE_ID (voiceStealingEnabled)
DECLARE_ID (legacyModeEnabled)
DECLARE_ID (mpeZoneLayout)
DECLARE_ID (legacyFirstChannel)
DECLARE_ID (legacyLastChannel)
DECLARE_ID (legacyPitchbendRange)

DECLARE_ID (VISIBLE_RANGE)
DECLARE_ID (totalRange)
DECLARE_ID (visibleRange)

#undef DECLARE_ID

} // namespace IDs

template <typename Contents>
class ReferenceCountingAdapter final : public ReferenceCountedObject
{
public:
    template <typename... Args>
    explicit ReferenceCountingAdapter (Args&&... args)
        : contents (std::forward<Args> (args)...)
    {
    }

    const Contents& get () const
    {
        return contents;
    }

    Contents& get ()
    {
        return contents;
    }

private:
    Contents contents;
};

template <typename Contents, typename... Args>
std::unique_ptr<ReferenceCountingAdapter<Contents>>
make_reference_counted (Args&&... args)
{
    auto adapter = new ReferenceCountingAdapter<Contents> (std::forward<Args> (args)...);
    return std::unique_ptr<ReferenceCountingAdapter<Contents>> (adapter);
}

//==============================================================================

namespace juce
{

template <typename Wrapped>
struct GenericVariantConverter
{
    static Wrapped fromVar (const var& v)
    {
        auto cast = dynamic_cast<ReferenceCountingAdapter<Wrapped>*> (v.getObject ());
        jassert (cast != nullptr);
        return cast->get ();
    }

    static var toVar (Wrapped range)
    {
        return { make_reference_counted<Wrapped> (std::move (range)).release () };
    }
};

template <typename Numeric>
struct VariantConverter<Range<Numeric>> final : GenericVariantConverter<Range<Numeric>> {};

template<>
struct VariantConverter<MPEZoneLayout> final : GenericVariantConverter<MPEZoneLayout> {};

} // namespace juce
