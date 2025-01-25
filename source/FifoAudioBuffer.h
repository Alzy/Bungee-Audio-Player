#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

class FifoAudioBuffer
{
public:
    FifoAudioBuffer(int numChannels, double sampleRate, double maxBufferLengthInSeconds)
        : fifo(static_cast<int>(sampleRate* maxBufferLengthInSeconds)),
        buffer(numChannels, static_cast<int>(sampleRate* maxBufferLengthInSeconds)),
        numChannels(numChannels),
        sampleRate(sampleRate),
        maxBufferLengthInSeconds(maxBufferLengthInSeconds)
    {
        buffer.clear();
    }

    int write(const juce::AudioBuffer<float>& source, int numSamples)
    {
        int start1, size1, start2, size2;
        fifo.prepareToWrite(numSamples, start1, size1, start2, size2);

        if (size1 > 0)
            copyBufferSection(source, buffer, 0, start1, size1);
        if (size2 > 0)
            copyBufferSection(source, buffer, size1, start2, size2);

        int totalWritten = size1 + size2;
        fifo.finishedWrite(totalWritten);
        return totalWritten;
    }

    int read(juce::AudioBuffer<float>& destination, int numSamplesToRead)
    {
        int availableToRead = juce::jmin(numSamplesToRead, fifo.getNumReady());
        jassert(numSamplesToRead <= fifo.getNumReady());

        int start1, size1, start2, size2;
        fifo.prepareToRead(availableToRead, start1, size1, start2, size2);

        if (size1 > 0)
            copyBufferSection(buffer, destination, start1, 0, size1);
        if (size2 > 0)
            copyBufferSection(buffer, destination, start2, size1, size2);

        int totalRead = size1 + size2;
        fifo.finishedRead(totalRead);
        return totalRead;
    }

    int read(juce::AudioBuffer<float>& destination)
    {
        return read(destination, destination.getNumSamples());
    }

    double getSampleRate() const
    {
        return sampleRate;
    }

    void reset()
    {
        fifo.reset();
    }

    void clear()
    {
        buffer.clear();
    }

    int getNumReady() const
    {
        return fifo.getNumReady();
    }

    int getFreeSpace() const
    {
        return fifo.getFreeSpace();
    }

    int getSize() const
    {
        return fifo.getTotalSize();
    }

    double getMaxLengthInSeconds() const
    {
        return maxBufferLengthInSeconds;
    }

private:
    juce::AbstractFifo fifo;
    juce::AudioBuffer<float> buffer;
    const int numChannels;
    const double sampleRate;
    const double maxBufferLengthInSeconds;

    static void copyBufferSection(const juce::AudioBuffer<float>& source,
        juce::AudioBuffer<float>& destination,
        int sourceStart, int destStart, int numSamples)
    {
        int channelsToCopy = juce::jmin(source.getNumChannels(), destination.getNumChannels());
        for (int channel = 0; channel < channelsToCopy; ++channel)
        {
            destination.copyFrom(channel, destStart, source, channel, sourceStart, numSamples);
        }
    }
};
