#pragma once
#include <JuceHeader.h>

class AIAudioProcessor : public juce::AudioProcessor
{
public:
    AIAudioProcessor();
    ~AIAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    juce::AudioBuffer<float> inputBuffer;
    juce::AudioBuffer<float> resultBuffer;
    std::atomic<bool> isProcessing { false };
    std::atomic<bool> hasResult    { false };
    std::atomic<int>  playPosition { 0 };
    std::atomic<bool> isPlaying    { false };
    double fileSampleRate = 44100.0;

    std::atomic<bool>  isAppendMode    { true };
    juce::String       promptText;
    std::atomic<float> startValue      { 3.0f };
    std::atomic<float> endValue        { 11.0f };
    std::atomic<int>   creativityValue { 50 };

    void loadAudioFile(const juce::File& file);
    void sendToServer();
    void startPlayback();
    void stopPlayback();

private:
    juce::AudioFormatManager formatManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AIAudioProcessor)
};
