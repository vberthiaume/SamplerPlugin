#pragma once

#include "juceHeader.h"
using namespace juce;

#include "Sampler.h"

namespace IDs
{
#define DECLARE_ID(name) const juce::Identifier name (#name);

DECLARE_ID (DATA_MODEL)
DECLARE_ID (sampleReader)
DECLARE_ID (centreFrequencyHz)
DECLARE_ID (loopMode)
DECLARE_ID (loopPointsSeconds)

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
inline std::unique_ptr<AudioFormatReader> makeAudioFormatReader (AudioFormatManager& manager,
                                                                 const void* sampleData,
                                                                 size_t dataSize)
{
    return std::unique_ptr<AudioFormatReader> (manager.createReaderFor (std::make_unique<MemoryInputStream> (sampleData,
                                                                                                             dataSize,
                                                                                                             false)));
}

inline std::unique_ptr<AudioFormatReader> makeAudioFormatReader (AudioFormatManager& manager,
                                                                 const File& file)
{
    return std::unique_ptr<AudioFormatReader> (manager.createReaderFor (file));
}

//==============================================================================
class AudioFormatReaderFactory
{
public:
    AudioFormatReaderFactory () = default;
    AudioFormatReaderFactory (const AudioFormatReaderFactory&) = default;
    AudioFormatReaderFactory (AudioFormatReaderFactory&&) = default;
    AudioFormatReaderFactory& operator= (const AudioFormatReaderFactory&) = default;
    AudioFormatReaderFactory& operator= (AudioFormatReaderFactory&&) = default;

    virtual ~AudioFormatReaderFactory () noexcept = default;

    virtual std::unique_ptr<AudioFormatReader> make (AudioFormatManager&) const = 0;
    virtual std::unique_ptr<AudioFormatReaderFactory> clone () const = 0;
};

//==============================================================================
class MemoryAudioFormatReaderFactory final : public AudioFormatReaderFactory
{
public:
    MemoryAudioFormatReaderFactory (const void* sampleDataIn, size_t dataSizeIn)
        : sampleData (sampleDataIn),
        dataSize (dataSizeIn)
    {
    }

    std::unique_ptr<AudioFormatReader> make (AudioFormatManager& manager) const override
    {
        return makeAudioFormatReader (manager, sampleData, dataSize);
    }

    std::unique_ptr<AudioFormatReaderFactory> clone () const override
    {
        return std::unique_ptr<AudioFormatReaderFactory> (new MemoryAudioFormatReaderFactory (*this));
    }

private:
    const void* sampleData;
    size_t dataSize;
};

//==============================================================================
class FileAudioFormatReaderFactory final : public AudioFormatReaderFactory
{
public:
    explicit FileAudioFormatReaderFactory (File fileIn)
        : file (std::move (fileIn))
    {
    }

    std::unique_ptr<AudioFormatReader> make (AudioFormatManager& manager) const override
    {
        return makeAudioFormatReader (manager, file);
    }

    std::unique_ptr<AudioFormatReaderFactory> clone () const override
    {
        return std::unique_ptr<AudioFormatReaderFactory> (new FileAudioFormatReaderFactory (*this));
    }

private:
    File file;
};

namespace juce
{

template<>
struct VariantConverter<LoopMode>
{
    static LoopMode fromVar (const var& v)
    {
        return static_cast<LoopMode> (int (v));
    }

    static var toVar (LoopMode loopMode)
    {
        return static_cast<int> (loopMode);
    }
};

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

template<>
struct VariantConverter<std::shared_ptr<AudioFormatReaderFactory>> final
    : GenericVariantConverter<std::shared_ptr<AudioFormatReaderFactory>>
{
};

} // namespace juce
