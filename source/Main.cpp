#include "BungeeAudioSource.h"

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

class MainComponent :
public juce::AudioAppComponent,
public juce::FileDragAndDropTarget,
public juce::Timer
{
public:
    MainComponent()
    {
        setAudioChannels (0, 2);
        DBG ("MainComponent::MainComponent");

        // Filename label
        addAndMakeVisible(filenameLabel);
        filenameLabel.setText("No file loaded", juce::dontSendNotification);
        filenameLabel.setJustificationType(juce::Justification::centredLeft);

        // Playback controls
        addAndMakeVisible(playButton);
        playButton.setButtonText("Play");
        playButton.onClick = [this]() { playAudio(); };

        addAndMakeVisible(stopButton);
        stopButton.setButtonText("Stop");
        stopButton.onClick = [this]() { stopAudio(true); };

        // Speed slider
        addAndMakeVisible(speedSlider);
        speedSlider.setRange(0.125, 4.0, 0.01);
        speedSlider.setValue(1.0);
        speedSlider.onValueChange = [this]() {
            speed = speedSlider.getValue();
            if (audioSource) audioSource->setSpeed(speed);
        };

        addAndMakeVisible(speedLabel);
        speedLabel.setText("Speed", juce::dontSendNotification);

        // Pitch slider
        addAndMakeVisible(pitchSlider);
        pitchSlider.setRange(-12.0, 12.0, 0.1);
        pitchSlider.setValue(0.0);
        pitchSlider.onValueChange = [this]() {
            pitch = pitchSlider.getValue();
            if (audioSource) audioSource->setPitch(pitch);
        };

        addAndMakeVisible(pitchLabel);
        pitchLabel.setText("Pitch", juce::dontSendNotification);

        // Playback position slider
        addAndMakeVisible(positionSlider);
        positionSlider.setRange(0.0, 1.0, 0.001);
        positionSlider.setValue(0.0, juce::dontSendNotification);
        positionSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
        positionSlider.onValueChange = [this]() {
            audioSource->setPlaybackPosition(positionSlider.getValue());
        };

        addAndMakeVisible(positionLabel);
        positionLabel.setText("Position", juce::dontSendNotification);

        setSize(600, 400);
    }

    ~MainComponent() override
    {
        stopTimer();
        shutdownAudio();
    }

    void resized() override
    {
        // Define the layout using FlexBox
        juce::FlexBox flexBox;
        flexBox.flexDirection = juce::FlexBox::Direction::column;
        flexBox.alignItems = juce::FlexBox::AlignItems::stretch;
        flexBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround;

        // Add components to FlexBox
        flexBox.items.add(juce::FlexItem(filenameLabel).withHeight(40.0f));

        // Add buttons in a row
        juce::FlexBox buttonBox;
        buttonBox.flexDirection = juce::FlexBox::Direction::row;
        buttonBox.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
        buttonBox.items.add(juce::FlexItem(playButton).withFlex(1).withMargin(5.0f));
        buttonBox.items.add(juce::FlexItem(stopButton).withFlex(1).withMargin(5.0f));
        flexBox.items.add(juce::FlexItem(buttonBox).withHeight(40.0f));

        // Add sliders and labels
        flexBox.items.add(juce::FlexItem(speedLabel).withHeight(20.0f));
        flexBox.items.add(juce::FlexItem(speedSlider).withHeight(40.0f));
        flexBox.items.add(juce::FlexItem(pitchLabel).withHeight(20.0f));
        flexBox.items.add(juce::FlexItem(pitchSlider).withHeight(40.0f));
        flexBox.items.add(juce::FlexItem(positionLabel).withHeight(20.0f));
        flexBox.items.add(juce::FlexItem(positionSlider).withHeight(40.0f));

        // Apply the layout
        flexBox.performLayout(getLocalBounds().reduced(10));
    }

    // File Drag-and-Drop
    bool isInterestedInFileDrag(const juce::StringArray& files) override
    {
        return true; // Accept all files (filtering can be added if needed)
    }

    void filesDropped(const juce::StringArray& files, int, int) override
    {
        if (!files.isEmpty())
        {
            currentFile = juce::File(files[0]);
            filenameLabel.setText("Loaded: " + currentFile.getFileName(), juce::dontSendNotification);

            try {
                audioSource = std::make_unique<BungeeAudioSource>(currentFile);
                audioSource->prepareToPlay(deviceSamplesPerBlock, deviceSampleRate);
                startTimerHz(30);
            }
            catch (const std::runtime_error& e) {
                filenameLabel.setText("Error loading file: " + juce::String(e.what()), juce::dontSendNotification);
            }
        }
    }

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override
    {
        deviceSampleRate = sampleRate;
        deviceSamplesPerBlock = samplesPerBlockExpected;

        if (audioSource)
        {
            audioSource->prepareToPlay(samplesPerBlockExpected, deviceSampleRate);
        }
    }
    void releaseResources() override
    {
        if (audioSource)
            audioSource->releaseResources();
    }
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        if (!tryingToPlay) return;

        if (audioSource)
        {
            audioSource->getNextAudioBlock(bufferToFill);
            // outputTestNoise(bufferToFill);
        }
        else
        {
            bufferToFill.clearActiveBufferRegion();
        }
    }

    void timerCallback() override
    {
        if (!audioSource) return;
        positionSlider.setValue(audioSource->getPlaybackPosition(), juce::dontSendNotification);
    }

private:
    void playAudio()
    {
        if (!audioSource) return;
        if (tryingToPlay)
        {
            playButton.setButtonText("Play");
            return stopAudio();
        }
        audioSource->play();
        playButton.setButtonText("Pause");
        tryingToPlay = true;
    }

    void stopAudio(bool restart = false)
    {
        if (!audioSource) return;
        audioSource->stop();
        if (restart) audioSource->resetPlayback();
        playButton.setButtonText("Play");
        tryingToPlay = false;
    }

    void outputTestNoise (const juce::AudioSourceChannelInfo& bufferToFill)
    {
        for (auto channel = 0; channel < bufferToFill.buffer->getNumChannels(); ++channel)
        {
            // Get a pointer to the start sample in the buffer for this audio output channel
            auto* buffer = bufferToFill.buffer->getWritePointer (channel, bufferToFill.startSample);

            // Fill the required number of samples with noise between -0.125 and +0.125
            for (auto sample = 0; sample < bufferToFill.numSamples; ++sample)
                buffer[sample] = random.nextFloat() * 0.25f - 0.125f;
        }
    }

    // UI components
    juce::Label filenameLabel;
    juce::TextButton playButton, stopButton;
    juce::Slider speedSlider, pitchSlider, positionSlider;
    juce::Label speedLabel, pitchLabel, positionLabel;

    bool tryingToPlay = false;
    juce::Random random;

    std::unique_ptr<BungeeAudioSource> audioSource;
    int deviceSamplesPerBlock;
    double deviceSampleRate;

    // Playback parameters
    double speed = 1.0;
    double pitch = 0.0;

    // Current file
    juce::File currentFile;
};

class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow(juce::String name)
        : DocumentWindow(name,
                         juce::Desktop::getInstance().getDefaultLookAndFeel()
                             .findColour(juce::ResizableWindow::backgroundColourId),
                         allButtons)
    {
        setUsingNativeTitleBar(false);
        setContentOwned(new MainComponent(), true);
        setResizable(true, true);
        centreWithSize(600, 300);
        setVisible(true);

    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
};

class BungeeAudioPlayerApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Bungee Audio Player"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }

    void initialise(const juce::String&) override
    {
        mainWindow.reset(new MainWindow(getApplicationName()));
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(BungeeAudioPlayerApplication)
