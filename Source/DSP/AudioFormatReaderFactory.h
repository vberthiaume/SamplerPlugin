#pragma once

#include "juceHeader.h"
using namespace juce;

#include "../Helper.h"

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
struct VariantConverter<std::shared_ptr<AudioFormatReaderFactory>> final
    : GenericVariantConverter<std::shared_ptr<AudioFormatReaderFactory>>
{
};

} // namespace juce
