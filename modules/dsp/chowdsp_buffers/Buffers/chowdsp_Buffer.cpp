#include "chowdsp_Buffer.h"

namespace chowdsp
{
template <typename SampleType>
Buffer<SampleType>::Buffer (int numChannels, int numSamples)
{
    setMaxSize (numChannels, numSamples);
}

template <typename SampleType>
void Buffer<SampleType>::setMaxSize (int numChannels, int numSamples)
{
    // Make sure we don't have any null internal buffers
    jassert (juce::isPositiveAndNotGreaterThan (numChannels, maxNumChannels));

    numChannels = juce::jmax (numChannels, 1);
    numSamples = juce::jmax (numSamples, 0);

    int numSamplesPadded = numSamples;
#if ! CHOWDSP_NO_XSIMD
    static constexpr auto vec_size = (int) xsimd::batch<SampleType>::size;
    if constexpr (std::is_floating_point_v<SampleType>)
        numSamplesPadded = buffers_detail::ceiling_divide (numSamples, vec_size) * vec_size;
#endif

    rawData.clear();
    hasBeenCleared = true;
    currentNumChannels = 0;
    currentNumSamples = 0;

    rawData.resize ((size_t) numChannels * (size_t) numSamplesPadded, SampleType {});
    std::fill (channelPointers.begin(), channelPointers.end(), nullptr);
    for (int ch = 0; ch < numChannels; ++ch)
        channelPointers[(size_t) ch] = rawData.data() + ch * numSamplesPadded;

    setCurrentSize (numChannels, numSamples);
}

template <typename SampleType>
void Buffer<SampleType>::setCurrentSize (int numChannels, int numSamples) noexcept
{
    // trying to set a current size, but we don't have enough memory allocated!
    jassert (numSamples * numChannels <= (int) rawData.size());

    const auto increasingNumChannels = numChannels > currentNumChannels;
    const auto increasingNumSamples = numSamples > currentNumSamples;

    if (increasingNumSamples)
        buffer_detail::clear (channelPointers.data(), 0, currentNumChannels, currentNumSamples, numSamples);

    if (increasingNumChannels)
        buffer_detail::clear (channelPointers.data(), currentNumChannels, numChannels, 0, numSamples);

    currentNumChannels = numChannels;
    currentNumSamples = numSamples;
}

template <typename SampleType>
SampleType* Buffer<SampleType>::getWritePointer (int channel) noexcept
{
    hasBeenCleared = false;
    return channelPointers[(size_t) channel];
}

template <typename SampleType>
const SampleType* Buffer<SampleType>::getReadPointer (int channel) const noexcept
{
    return channelPointers[(size_t) channel];
}

template <typename SampleType>
nonstd::span<SampleType> Buffer<SampleType>::getWriteSpan (int channel) noexcept
{
    hasBeenCleared = false;
    return { channelPointers[(size_t) channel], (size_t) currentNumSamples };
}

template <typename SampleType>
nonstd::span<const SampleType> Buffer<SampleType>::getReadSpan (int channel) const noexcept
{
    return { channelPointers[(size_t) channel], (size_t) currentNumSamples };
}

template <typename SampleType>
SampleType** Buffer<SampleType>::getArrayOfWritePointers() noexcept
{
    hasBeenCleared = false;
    return channelPointers.data();
}

template <typename SampleType>
const SampleType** Buffer<SampleType>::getArrayOfReadPointers() const noexcept
{
    return const_cast<const SampleType**> (channelPointers.data()); // NOSONAR (using const_cast to be more strict)
}

#if CHOWDSP_USING_JUCE
template <typename SampleType>
juce::AudioBuffer<SampleType> Buffer<SampleType>::toAudioBuffer()
{
    return { getArrayOfWritePointers(), currentNumChannels, currentNumSamples };
}

template <typename SampleType>
juce::AudioBuffer<SampleType> Buffer<SampleType>::toAudioBuffer() const
{
    return { const_cast<SampleType* const*> (getArrayOfReadPointers()), currentNumChannels, currentNumSamples }; // NOSONAR
}

#if JUCE_MODULE_AVAILABLE_juce_dsp
template <typename SampleType>
AudioBlock<SampleType> Buffer<SampleType>::toAudioBlock()
{
    return { getArrayOfWritePointers(), (size_t) currentNumChannels, (size_t) currentNumSamples };
}

template <typename SampleType>
AudioBlock<const SampleType> Buffer<SampleType>::toAudioBlock() const
{
    return { getArrayOfReadPointers(), (size_t) currentNumChannels, (size_t) currentNumSamples };
}
#endif
#endif

template <typename SampleType>
void Buffer<SampleType>::clear() noexcept
{
    if (hasBeenCleared)
        return;

    buffer_detail::clear (channelPointers.data(), 0, currentNumChannels, 0, currentNumSamples);
    hasBeenCleared = true;
}

template class Buffer<float>;
template class Buffer<double>;
#if ! CHOWDSP_NO_XSIMD
template class Buffer<xsimd::batch<float>>;
template class Buffer<xsimd::batch<double>>;
#endif
} // namespace chowdsp