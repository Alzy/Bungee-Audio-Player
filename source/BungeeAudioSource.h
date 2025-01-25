#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <bungee/Bungee.h>
#include <bungee/Push.h>
#include "FifoAudioBuffer.h"

class BungeeAudioSource : public juce::AudioSource {
public:
    BungeeAudioSource(const juce::File& inputFile);
    ~BungeeAudioSource() override = default;

    void prepareToPlay(int samplesPerBlock, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

    void play();
    void stop();
    void setPlaybackPosition(double newPosition);
    double getPlaybackPosition() const;
    void setSpeed(double speed);
    double getSpeed() const;
    void setPitch(double semitones);
    double getPitch() const;
    void resetPlayback();

private:
    void processNextBlock();
    void resetBuffers();

    std::unique_ptr<juce::AudioFormatReader> reader;
    std::unique_ptr<Bungee::Stretcher<Bungee::Basic>> stretcher;
    std::unique_ptr<Bungee::Push::InputBuffer> pushInputBuffer;
    std::unique_ptr<FifoAudioBuffer> outputFifo;
    juce::AudioBuffer<float> scratchBuffer;

    int samplesPerBlockExpected;
    int64_t inputPosition = 0;
    double playbackPosition = 0.0;
    bool isPlaying;
    double playbackSpeed;
    double pitchShift;
    double deviceSampleRate = 0.0;

    Bungee::Request request{};
};

inline BungeeAudioSource::BungeeAudioSource(const juce::File& inputFile)
    : reader(nullptr),
      stretcher(nullptr),
      pushInputBuffer(nullptr),
      outputFifo(nullptr),
      isPlaying(false),
      playbackSpeed(1.0),
      pitchShift(0.0),
      samplesPerBlockExpected(0) {

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    auto fileInputStream = inputFile.createInputStream();
    if (!fileInputStream)
        throw std::runtime_error("Failed to open input file.");

    reader.reset(formatManager.createReaderFor(std::move(fileInputStream)));
    if (!reader)
        throw std::runtime_error("Unsupported file format.");
}

inline void BungeeAudioSource::prepareToPlay(int samplesPerBlock, double sampleRate) {
    samplesPerBlockExpected = samplesPerBlock;
    deviceSampleRate = sampleRate;

    Bungee::SampleRates sampleRates = {
        juce::roundToIntAccurate(reader->sampleRate),
        juce::roundToIntAccurate(deviceSampleRate)
    };

    stretcher = std::make_unique<Bungee::Stretcher<Bungee::Basic>>(sampleRates, (int)reader->numChannels);
    pushInputBuffer = std::make_unique<Bungee::Push::InputBuffer>(stretcher->maxInputFrameCount() + samplesPerBlock, reader->numChannels);
    outputFifo = std::make_unique<FifoAudioBuffer>(reader->numChannels, reader->sampleRate, 2.0); // 2 second buffer
    scratchBuffer.setSize((int)reader->numChannels, stretcher->maxInputFrameCount() + samplesPerBlock);

    request.speed = playbackSpeed;
    request.pitch = std::pow(2.0, pitchShift / 12.0);
    request.position = 0.0;
    stretcher->preroll(request);

    resetPlayback();
    Bungee::InputChunk inputChunk = stretcher->specifyGrain(request);
    pushInputBuffer->grain(inputChunk);
}

inline void BungeeAudioSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) {
    if (!isPlaying || !reader || !stretcher || !pushInputBuffer || !outputFifo) {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    auto* outputBuffer = bufferToFill.buffer;
    const int numSamples = bufferToFill.numSamples;

    // Check if we need to process more audio
    while (outputFifo->getNumReady() < numSamples) {
        processNextBlock();
    }

    // Read from FIFO into output buffer

    outputFifo->read(*outputBuffer);
    playbackPosition = static_cast<double>(inputPosition) / static_cast<double>(reader->lengthInSamples);
}

inline void BungeeAudioSource::releaseResources() {
    stretcher.reset();
    pushInputBuffer.reset();
    outputFifo.reset();
}

inline void BungeeAudioSource::processNextBlock() {
    const int numChannels = (int)reader->numChannels;
    int required = pushInputBuffer->inputFrameCountRequired();

    if (required > 0) {
        if (inputPosition >= reader->lengthInSamples) {
            isPlaying = false;
            return;
        }

        reader->read(&scratchBuffer, 0, required, inputPosition, true, true);

        for (int channel = 0; channel < numChannels; ++channel) {
            const float* sourceData = scratchBuffer.getReadPointer(channel);
            float* destData = pushInputBuffer->inputData() + channel * pushInputBuffer->stride();
            memcpy(destData, sourceData, required * sizeof(float));
        }

        pushInputBuffer->deliver(required);
        inputPosition += required;
    }

    scratchBuffer.clear();
    while (pushInputBuffer->inputFrameCountRequired() <= 0) {
        stretcher->analyseGrain(pushInputBuffer->outputData(), pushInputBuffer->stride());

        Bungee::OutputChunk outputChunk{};
        stretcher->synthesiseGrain(outputChunk);

        // Write entire output chunk to FIFO
        for (int channel = 0; channel < numChannels; ++channel) {
            float* writePointer = scratchBuffer.getWritePointer(channel);
            const float* readPointer = outputChunk.data + channel * outputChunk.channelStride;
            memcpy(writePointer, readPointer, outputChunk.frameCount * sizeof(float));
        }
        outputFifo->write(scratchBuffer, outputChunk.frameCount);

        stretcher->next(request);
        Bungee::InputChunk inputChunk = stretcher->specifyGrain(request);
        pushInputBuffer->grain(inputChunk);
    }
}

inline void BungeeAudioSource::play() {
    isPlaying = true;
}

inline void BungeeAudioSource::stop() {
    isPlaying = false;
}

inline void BungeeAudioSource::setPlaybackPosition(double newPosition) {
    jassert(deviceSampleRate > 0);
    playbackPosition = newPosition;
    inputPosition = juce::roundToIntAccurate (static_cast<double>(reader->lengthInSamples) * newPosition);
    resetBuffers();
}

inline double BungeeAudioSource::getPlaybackPosition() const {
    return playbackPosition;
}

inline void BungeeAudioSource::setSpeed(double speed) {
    playbackSpeed = speed;
    request.speed = speed;
}

inline double BungeeAudioSource::getSpeed() const {
    return playbackSpeed;
}

inline void BungeeAudioSource::setPitch(double semitones) {
    semitones = std::clamp(semitones, -24.0, 24.0);
    pitchShift = semitones;
    request.pitch = std::pow(2.0, semitones / 12.0);
}

inline double BungeeAudioSource::getPitch() const {
    return pitchShift;
}

inline void BungeeAudioSource::resetPlayback() {
    playbackPosition = 0.0;
    inputPosition = 0;
    isPlaying = false;
    request.reset = true;
    resetBuffers();
}

inline void BungeeAudioSource::resetBuffers() {
    if (outputFifo) {
        outputFifo->reset();
        outputFifo->clear();
    }
}