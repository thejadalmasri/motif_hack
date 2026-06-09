#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
class MotiffLookAndFeel : public juce::LookAndFeel_V4
{
public:
    static constexpr uint32_t BG_DARK     = 0xff0f0f14;
    static constexpr uint32_t BG_PANEL    = 0xff1a1a24;
    static constexpr uint32_t BG_INPUT    = 0xff13131c;
    static constexpr uint32_t PURPLE      = 0xff8b5cf6;
    static constexpr uint32_t PURPLE_DIM  = 0xff2d1f4e;
    static constexpr uint32_t GREEN       = 0xff86efac;
    static constexpr uint32_t TEXT_DIM    = 0xff6b7280;
    static constexpr uint32_t TEXT_MID    = 0xffaaaacc;
    static constexpr uint32_t TEXT_BRIGHT = 0xffe8e8f0;
    static constexpr uint32_t BORDER      = 0xff2a2a3a;

    MotiffLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(BG_DARK));
        setColour(juce::TextEditor::backgroundColourId,      juce::Colour(BG_INPUT));
        setColour(juce::TextEditor::textColourId,            juce::Colour(TEXT_BRIGHT));
        setColour(juce::TextEditor::outlineColourId,         juce::Colour(BORDER));
        setColour(juce::TextEditor::focusedOutlineColourId,  juce::Colour(PURPLE));
        setColour(juce::Label::textColourId,                 juce::Colour(TEXT_BRIGHT));
        setColour(juce::ScrollBar::thumbColourId,            juce::Colour(PURPLE));
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& btn,
                              const juce::Colour&, bool isOver, bool isDown) override
    {
        auto b = btn.getLocalBounds().toFloat().reduced(0.5f);
        auto prop = btn.getProperties();

        if (prop.contains("generate"))
        {
            auto col = juce::Colour(PURPLE);
            if (isDown) col = col.darker(0.2f);
            else if (isOver) col = col.brighter(0.1f);
            g.setColour(col);
            g.fillRoundedRectangle(b, 10.0f);
        }
        else if (prop.contains("mode"))
        {
            bool active = prop.contains("active") && (bool)prop["active"];
            g.setColour(active ? juce::Colour(PURPLE) : juce::Colour(BG_INPUT));
            g.fillRoundedRectangle(b, 6.0f);
            if (!active)
            {
                g.setColour(juce::Colour(BORDER));
                g.drawRoundedRectangle(b, 6.0f, 1.0f);
            }
        }
        else if (prop.contains("play"))
        {
            auto col = juce::Colour(PURPLE);
            if (isDown) col = col.darker(0.2f);
            g.setColour(col);
            g.fillEllipse(b);
        }
        else if (prop.contains("print"))
        {
            g.setColour(juce::Colour(BORDER));
            g.drawRoundedRectangle(b, 8.0f, 1.5f);
            if (isOver)
            {
                g.setColour(juce::Colour(PURPLE).withAlpha(0.15f));
                g.fillRoundedRectangle(b, 8.0f);
            }
        }
        else
        {
            g.setColour(juce::Colour(BG_INPUT));
            g.fillRoundedRectangle(b, 6.0f);
        }
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& btn, bool, bool) override
    {
        auto prop = btn.getProperties();
        g.setFont(juce::Font(11.0f, juce::Font::bold));

        if (prop.contains("generate"))
            g.setColour(juce::Colours::white);
        else if (prop.contains("mode"))
        {
            bool active = prop.contains("active") && (bool)prop["active"];
            g.setColour(active ? juce::Colours::white : juce::Colour(TEXT_DIM));
        }
        else if (prop.contains("print"))
            g.setColour(juce::Colour(TEXT_MID));
        else
            g.setColour(juce::Colour(TEXT_MID));

        g.drawFittedText(btn.getButtonText(), btn.getLocalBounds(),
                         juce::Justification::centred, 1);
    }
};

//==============================================================================
// Waveform display with draggable selection handles
class WaveformDisplay : public juce::Component
{
public:
    std::function<void(float, float)> onSelectionChanged;

    WaveformDisplay()
    {
        setRepaintsOnMouseActivity(false);
    }

    void setWaveformData(const juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        waveformData.clear();
        numSamples = buffer.getNumSamples();
        this->sampleRate = sampleRate;

        if (numSamples == 0) return;

        int numBars = 120;
        int samplesPerBar = juce::jmax(1, numSamples / numBars);

        for (int bar = 0; bar < numBars; ++bar)
        {
            int start = bar * samplesPerBar;
            int end   = juce::jmin(start + samplesPerBar, numSamples);
            float peak = 0.0f;
            for (int s = start; s < end; ++s)
                peak = juce::jmax(peak, std::abs(buffer.getSample(0, s)));
            waveformData.push_back(peak);
        }
        repaint();
    }

    void clearWaveform()
    {
        waveformData.clear();
        numSamples = 0;
        repaint();
    }

    float getStartNorm() const { return startNorm; }
    float getEndNorm()   const { return endNorm; }

    void setSelectionSeconds(float startSec, float endSec)
    {
        if (sampleRate <= 0 || numSamples == 0) return;
        float duration = (float)numSamples / (float)sampleRate;
        startNorm = juce::jlimit(0.0f, 1.0f, startSec / duration);
        endNorm   = juce::jlimit(0.0f, 1.0f, endSec   / duration);
        repaint();
    }

    float getStartSeconds() const
    {
        if (sampleRate <= 0 || numSamples == 0) return 0.0f;
        return startNorm * (float)numSamples / (float)sampleRate;
    }

    float getEndSeconds() const
    {
        if (sampleRate <= 0 || numSamples == 0) return 0.0f;
        return endNorm * (float)numSamples / (float)sampleRate;
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Background
        g.setColour(juce::Colour(0xff0d0d18));
        g.fillRoundedRectangle(bounds, 6.0f);

        if (waveformData.empty())
        {
            g.setColour(juce::Colour(0xff2a2a3a));
            g.setFont(juce::Font(11.0f));
            g.drawText("Load audio to see waveform", bounds, juce::Justification::centred);
            return;
        }

        int w = getWidth();
        int h = getHeight();
        float cx = h * 0.5f;
        int n = (int)waveformData.size();

        int startPx = (int)(startNorm * w);
        int endPx   = (int)(endNorm   * w);

        // Selection highlight
        g.setColour(juce::Colour(0xff8b5cf6).withAlpha(0.15f));
        g.fillRect(startPx, 0, endPx - startPx, h);

        // Bars
        float barW = (float)w / n;
        for (int i = 0; i < n; ++i)
        {
            float x   = i * barW;
            float barH = juce::jmax(2.0f, waveformData[i] * cx * 1.8f);
            float xMid = (float)i / n;
            bool inSel = xMid >= startNorm && xMid <= endNorm;

            g.setColour(inSel ? juce::Colour(0xff86efac) : juce::Colour(0xff3a4a3a));
            g.fillRect(x + 1.0f, cx - barH, barW - 1.5f, barH * 2.0f);
        }

        // Handles
        auto drawHandle = [&](int px)
        {
            g.setColour(juce::Colour(0xffddaaff));
            g.fillRect(px - 1, 0, 2, h);
            g.fillRoundedRectangle((float)px - 7.0f, (float)h * 0.35f, 14.0f, (float)h * 0.3f, 4.0f);
        };
        drawHandle(startPx);
        drawHandle(endPx);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        int px = e.x;
        int startPx = (int)(startNorm * getWidth());
        int endPx   = (int)(endNorm   * getWidth());

        draggingStart = std::abs(px - startPx) < std::abs(px - endPx);
        isDragging = true;
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!isDragging) return;
        float norm = juce::jlimit(0.0f, 1.0f, (float)e.x / getWidth());

        if (draggingStart)
            startNorm = juce::jmin(norm, endNorm - 0.01f);
        else
            endNorm = juce::jmax(norm, startNorm + 0.01f);

        repaint();
        if (onSelectionChanged)
            onSelectionChanged(getStartSeconds(), getEndSeconds());
    }

    void mouseUp(const juce::MouseEvent&) override { isDragging = false; }

private:
    std::vector<float> waveformData;
    int    numSamples  = 0;
    double sampleRate  = 44100.0;
    float  startNorm   = 0.1f;
    float  endNorm     = 0.6f;
    bool   isDragging    = false;
    bool   draggingStart = true;
};

//==============================================================================
class AIAudioProcessorEditor : public juce::AudioProcessorEditor,
                               public juce::Timer,
                               public juce::FileDragAndDropTarget
{
public:
    AIAudioProcessorEditor(AIAudioProcessor&);
    ~AIAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    void updateState();

    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int, int) override;
    void fileDragEnter(const juce::StringArray&, int, int) override { isDragging = true;  repaint(); }
    void fileDragExit(const juce::StringArray&) override           { isDragging = false; repaint(); }

private:
    AIAudioProcessor&  audioProcessor;
    MotiffLookAndFeel  laf;

    // Source section
    juce::TextButton   browseButton { "BROWSE FILES" };
    juce::Label        dropLabel;
    juce::Label        formatLabel;
    juce::Label        fileNameLabel;
    bool               isDragging = false;

    // Prompt
    juce::Label        promptHeaderLabel;
    juce::TextEditor   promptEditor;

    // Mode
    juce::Label        modeHeaderLabel;
    juce::TextButton   appendButton  { "APPEND" };
    juce::TextButton   restyleButton { "RESTYLE" };

    // Waveform + selection
    WaveformDisplay    inputWaveform;
    juce::Label        selStartLabel;
    juce::Label        selEndLabel;
    juce::Label        selStartVal;
    juce::Label        selEndVal;
    juce::Label        dragHintLabel;

    // Creativity (restyle only)
    juce::Label        creativityHeaderLabel;
    juce::Label        creativityValueLabel;
    juce::Slider       creativitySlider;

    // Generate
    juce::TextButton   generateButton { "  ⚡  GENERATE" };

    // Output section
    juce::Label        outputHeaderLabel;
    WaveformDisplay    outputWaveform;
    juce::TextButton   playButton    { "" };
    juce::Label        positionHeaderLabel;
    juce::Label        positionLabel;
    juce::TextButton   printButton   { "⬇  PRINT TO TRACK" };

    juce::File         loadedFile;
    bool               appendMode = true;
    int                playPositionDisplay = 0;

    void setMode(bool isAppend);
    void loadFile(const juce::File& f);
    void formatTime(juce::Label& lbl, float seconds);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AIAudioProcessorEditor)
};

