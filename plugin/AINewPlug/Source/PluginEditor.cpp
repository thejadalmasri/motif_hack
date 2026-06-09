#include "PluginEditor.h"
#include "PluginProcessor.h"

using C = MotiffLookAndFeel;

static void styleLabel(juce::Label& l, uint32_t col, float size, bool bold = false)
{
    l.setFont(juce::Font(size, bold ? juce::Font::bold : juce::Font::plain));
    l.setColour(juce::Label::textColourId, juce::Colour(col));
}

AIAudioProcessorEditor::AIAudioProcessorEditor(AIAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setLookAndFeel(&laf);

    // Browse button
    addAndMakeVisible(browseButton);
    browseButton.setColour(juce::TextButton::buttonColourId,  juce::Colours::transparentBlack);
    browseButton.setColour(juce::TextButton::textColourOffId, juce::Colour(C::PURPLE));
    browseButton.onClick = [this]() {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Select audio file",
            juce::File::getSpecialLocation(juce::File::userMusicDirectory),
            "*.wav;*.aiff;*.aif;*.flac");
        chooser->launchAsync(juce::FileBrowserComponent::openMode |
                             juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc) {
                auto r = fc.getResult();
                if (r.existsAsFile()) loadFile(r);
            });
    };

    addAndMakeVisible(dropLabel);
    dropLabel.setText("Drag & drop raw audio or", juce::dontSendNotification);
    dropLabel.setJustificationType(juce::Justification::centred);
    styleLabel(dropLabel, C::TEXT_DIM, 11.0f);

    addAndMakeVisible(formatLabel);
    formatLabel.setText("WAV, AIFF, FLAC up to 96kHz/32-bit", juce::dontSendNotification);
    formatLabel.setJustificationType(juce::Justification::centred);
    styleLabel(formatLabel, C::TEXT_DIM, 9.0f, true);

    addAndMakeVisible(fileNameLabel);
    fileNameLabel.setJustificationType(juce::Justification::centred);
    styleLabel(fileNameLabel, C::GREEN, 10.0f);

    // Prompt
    addAndMakeVisible(promptHeaderLabel);
    promptHeaderLabel.setText("PROMPT", juce::dontSendNotification);
    styleLabel(promptHeaderLabel, C::TEXT_DIM, 9.0f, true);

    addAndMakeVisible(promptEditor);
    promptEditor.setMultiLine(true);
    promptEditor.setReturnKeyStartsNewLine(true);
    promptEditor.setScrollbarsShown(false);
    promptEditor.setTextToShowWhenEmpty("Describe the sound transformation...", juce::Colour(C::TEXT_DIM));
    promptEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(C::BG_INPUT));
    promptEditor.setColour(juce::TextEditor::outlineColourId,    juce::Colour(C::BORDER));
    promptEditor.setColour(juce::TextEditor::textColourId,       juce::Colour(C::TEXT_BRIGHT));
    promptEditor.setFont(juce::Font(12.0f));

    // Mode
    addAndMakeVisible(modeHeaderLabel);
    modeHeaderLabel.setText("GENERATION MODE", juce::dontSendNotification);
    styleLabel(modeHeaderLabel, C::TEXT_DIM, 9.0f, true);

    addAndMakeVisible(appendButton);
    addAndMakeVisible(restyleButton);
    appendButton.getProperties().set("mode", true);
    restyleButton.getProperties().set("mode", true);
    appendButton.onClick  = [this]() { setMode(true);  };
    restyleButton.onClick = [this]() { setMode(false); };

    // Waveform
    addAndMakeVisible(inputWaveform);
    inputWaveform.onSelectionChanged = [this](float startSec, float endSec) {
        audioProcessor.startValue.store(startSec);
        audioProcessor.endValue.store(endSec);
        selStartVal.setText(juce::String(startSec * 1000.0f, 1) + "ms", juce::dontSendNotification);
        selEndVal.setText(juce::String(endSec * 1000.0f, 1) + "ms", juce::dontSendNotification);
    };

    addAndMakeVisible(selStartLabel);
    selStartLabel.setText("SELECTION START", juce::dontSendNotification);
    styleLabel(selStartLabel, C::TEXT_DIM, 8.0f, true);

    addAndMakeVisible(selEndLabel);
    selEndLabel.setText("SELECTION END", juce::dontSendNotification);
    selEndLabel.setJustificationType(juce::Justification::centredRight);
    styleLabel(selEndLabel, C::TEXT_DIM, 8.0f, true);

    addAndMakeVisible(selStartVal);
    selStartVal.setText("0.0ms", juce::dontSendNotification);
    styleLabel(selStartVal, C::GREEN, 10.0f, true);

    addAndMakeVisible(selEndVal);
    selEndVal.setText("450.0ms", juce::dontSendNotification);
    selEndVal.setJustificationType(juce::Justification::centredRight);
    styleLabel(selEndVal, C::GREEN, 10.0f, true);

    addAndMakeVisible(dragHintLabel);
    dragHintLabel.setText("DRAG HANDLES TO DEFINE PROCESSING RANGE", juce::dontSendNotification);
    dragHintLabel.setJustificationType(juce::Justification::centred);
    styleLabel(dragHintLabel, C::TEXT_DIM, 8.0f, true);

    // Creativity
    addAndMakeVisible(creativityHeaderLabel);
    creativityHeaderLabel.setText("CREATIVITY", juce::dontSendNotification);
    styleLabel(creativityHeaderLabel, C::TEXT_DIM, 9.0f, true);

    addAndMakeVisible(creativityValueLabel);
    creativityValueLabel.setText("0.50", juce::dontSendNotification);
    creativityValueLabel.setJustificationType(juce::Justification::centredRight);
    styleLabel(creativityValueLabel, C::GREEN, 10.0f, true);

    addAndMakeVisible(creativitySlider);
    creativitySlider.setRange(0.0, 1.0, 0.01);
    creativitySlider.setValue(0.5);
    creativitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    creativitySlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    creativitySlider.setColour(juce::Slider::trackColourId, juce::Colour(C::BORDER));
    creativitySlider.setColour(juce::Slider::thumbColourId, juce::Colour(C::PURPLE));
    creativitySlider.onValueChange = [this]() {
        audioProcessor.creativityValue.store((int)(creativitySlider.getValue() * 100));
        creativityValueLabel.setText(juce::String(creativitySlider.getValue(), 2),
                                     juce::dontSendNotification);
    };

    // Generate
    addAndMakeVisible(generateButton);
    generateButton.getProperties().set("generate", true);
    generateButton.setEnabled(false);
    generateButton.onClick = [this]() {
        audioProcessor.promptText = promptEditor.getText();
        generateButton.setEnabled(false);
        audioProcessor.sendToServer();
        startTimer(200);
    };

    // Output
    addAndMakeVisible(outputHeaderLabel);
    outputHeaderLabel.setText("OUTPUT", juce::dontSendNotification);
    styleLabel(outputHeaderLabel, C::TEXT_DIM, 9.0f, true);

    addAndMakeVisible(outputWaveform);

    addAndMakeVisible(playButton);
    playButton.getProperties().set("play", true);
    playButton.setEnabled(false);
    playButton.onClick = [this]() { audioProcessor.startPlayback(); };

    addAndMakeVisible(positionHeaderLabel);
    positionHeaderLabel.setText("CURRENT POSITION", juce::dontSendNotification);
    styleLabel(positionHeaderLabel, C::TEXT_DIM, 8.0f, true);

    addAndMakeVisible(positionLabel);
    positionLabel.setText("00:00.00", juce::dontSendNotification);
    styleLabel(positionLabel, C::GREEN, 12.0f, true);

    addAndMakeVisible(printButton);
    printButton.getProperties().set("print", true);
    printButton.setEnabled(false);
    printButton.onClick = [this]() {
        auto triggerFile = juce::File::getSpecialLocation(
            juce::File::tempDirectory).getChildFile("AINewPlug/ai_output_ready.txt");
        triggerFile.replaceWithText("ready");
    };

    setMode(true);
    setSize(390, 680);
}

AIAudioProcessorEditor::~AIAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
    stopTimer();
}

void AIAudioProcessorEditor::loadFile(const juce::File& f)
{
    loadedFile = f;
    audioProcessor.loadAudioFile(f);
    fileNameLabel.setText(f.getFileName(), juce::dontSendNotification);
    dropLabel.setVisible(false);
    formatLabel.setVisible(false);

    inputWaveform.setWaveformData(audioProcessor.inputBuffer, audioProcessor.fileSampleRate);
    float dur = (float)audioProcessor.inputBuffer.getNumSamples() / (float)audioProcessor.fileSampleRate;
    inputWaveform.setSelectionSeconds(0.0f, juce::jmin(dur, 10.0f));
    selStartVal.setText("0.0ms", juce::dontSendNotification);
    selEndVal.setText(juce::String(juce::jmin(dur, 10.0f) * 1000.0f, 1) + "ms", juce::dontSendNotification);

    audioProcessor.startValue.store(0.0f);
    audioProcessor.endValue.store(juce::jmin(dur, 10.0f));

    generateButton.setEnabled(true);
    repaint();
}

bool AIAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (auto& f : files)
        if (f.endsWithIgnoreCase(".wav") || f.endsWithIgnoreCase(".aif") ||
            f.endsWithIgnoreCase(".aiff") || f.endsWithIgnoreCase(".flac"))
            return true;
    return false;
}

void AIAudioProcessorEditor::filesDropped(const juce::StringArray& files, int, int)
{
    isDragging = false;
    if (files.size() > 0) loadFile(juce::File(files[0]));
}

void AIAudioProcessorEditor::setMode(bool isAppend)
{
    appendMode = isAppend;
    audioProcessor.isAppendMode.store(isAppend);

    appendButton.getProperties().set("active", isAppend);
    restyleButton.getProperties().set("active", !isAppend);
    appendButton.repaint();
    restyleButton.repaint();

    inputWaveform.setVisible(isAppend);
    selStartLabel.setVisible(isAppend);
    selEndLabel.setVisible(isAppend);
    selStartVal.setVisible(isAppend);
    selEndVal.setVisible(isAppend);
    dragHintLabel.setVisible(isAppend);

    creativityHeaderLabel.setVisible(!isAppend);
    creativityValueLabel.setVisible(!isAppend);
    creativitySlider.setVisible(!isAppend);

    resized();
    repaint();
}

void AIAudioProcessorEditor::formatTime(juce::Label& lbl, float seconds)
{
    int mins = (int)(seconds / 60);
    float secs = seconds - mins * 60;
    lbl.setText(juce::String::formatted("%02d:%05.2f", mins, secs), juce::dontSendNotification);
}

void AIAudioProcessorEditor::paint(juce::Graphics& g)
{
    int W = getWidth(), H = getHeight();

    // Background
    g.fillAll(juce::Colour(C::BG_DARK));

    // Header
    g.setColour(juce::Colour(C::BG_PANEL));
    g.fillRect(0, 0, W, 44);
    g.setColour(juce::Colour(C::BORDER));
    g.drawLine(0.0f, 44.0f, (float)W, 44.0f, 1.0f);

    // Grid icon
    g.setColour(juce::Colour(C::GREEN));
    for (int i = 0; i < 3; ++i)
        g.fillRect(14 + i * 7, 14, 4, 16);

    // Title
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.setColour(juce::Colour(C::TEXT_BRIGHT));
    g.drawText("MOTIFF", 44, 14, 150, 16, juce::Justification::centredLeft);

    // Source panel
    g.setColour(juce::Colour(C::BG_PANEL));
    g.fillRoundedRectangle(juce::Rectangle<int>(10, 52, W - 20, 148).toFloat(), 8.0f);

    // SOURCE AUDIO dot + label
    g.setColour(juce::Colour(C::GREEN));
    g.fillEllipse(22.0f, 64.0f, 6.0f, 6.0f);
    g.setFont(juce::Font(9.0f, juce::Font::bold));
    g.setColour(juce::Colour(C::TEXT_DIM));
    g.drawText("SOURCE AUDIO", 34, 60, 180, 16, juce::Justification::centredLeft);

    // Dashed drop zone
    auto dz = juce::Rectangle<int>(20, 80, W - 40, 110);
    juce::Path dp;
    dp.addRoundedRectangle(dz.toFloat(), 5.0f);
    float dashes[] = { 5.0f, 4.0f };
    juce::Path ds;
    juce::PathStrokeType(1.2f).createDashedStroke(ds, dp, dashes, 2);
    g.setColour(isDragging ? juce::Colour(C::PURPLE) : juce::Colour(C::BORDER));
    g.fillPath(ds);

    // Upload icon
    if (!loadedFile.existsAsFile())
    {
        int cx = W / 2, cy = dz.getCentreY() - 10;
        g.setColour(juce::Colour(C::BORDER));
        g.drawRoundedRectangle((float)cx - 14, (float)cy - 14, 28, 28, 3.0f, 1.2f);
        g.setColour(juce::Colour(C::TEXT_DIM));
        g.fillRect(cx - 1, cy - 8, 2, 12);
        juce::Path arr;
        arr.addTriangle((float)cx, (float)cy - 11, (float)cx - 5, (float)cy - 5, (float)cx + 5, (float)cy - 5);
        g.fillPath(arr);
    }

    // Main content panel
    int mainTop = 208;
    int mainH = appendMode ? 310 : 236;
    g.setColour(juce::Colour(C::BG_PANEL));
    g.fillRoundedRectangle(juce::Rectangle<int>(10, mainTop, W - 20, mainH).toFloat(), 8.0f);

    // Output panel
    int outTop = mainTop + mainH + 8;
    int outH = H - outTop - 8;
    g.setColour(juce::Colour(C::BG_PANEL));
    g.fillRoundedRectangle(juce::Rectangle<int>(10, outTop, W - 20, outH).toFloat(), 8.0f);
}

void AIAudioProcessorEditor::resized()
{
    int W = getWidth();

    // Source panel internals
    int dzCy = 80 + 55; // centre of drop zone
    dropLabel.setBounds(10, dzCy - 18, W - 20, 16);
    browseButton.setBounds(W / 2 - 52, dzCy, 104, 18);
    formatLabel.setBounds(10, dzCy + 22, W - 20, 14);
    fileNameLabel.setBounds(20, 80, W - 40, 110);

    // Main panel — starts at 208
    int y = 216;
    int pad = 20;
    int inner = W - pad * 2;

    // Prompt
    promptHeaderLabel.setBounds(pad, y, inner, 13);
    y += 15;
    promptEditor.setBounds(pad, y, inner, 52);
    y += 58;

    // Mode
    modeHeaderLabel.setBounds(pad, y, inner, 13);
    y += 15;
    int halfW = inner / 2 - 3;
    appendButton.setBounds(pad, y, halfW, 30);
    restyleButton.setBounds(pad + halfW + 6, y, halfW, 30);
    y += 36;

    if (appendMode)
    {
        // Selection labels row
        selStartLabel.setBounds(pad, y, 130, 12);
        selEndLabel.setBounds(W - pad - 130, y, 130, 12);
        selStartVal.setBounds(pad, y + 13, 130, 13);
        selEndVal.setBounds(W - pad - 130, y + 13, 130, 13);
        y += 30;

        // Waveform
        inputWaveform.setBounds(pad, y, inner, 90);
        y += 94;

        // Drag hint
        dragHintLabel.setBounds(pad, y, inner, 12);
    }
    else
    {
        // Creativity
        creativityHeaderLabel.setBounds(pad, y, 120, 13);
        creativityValueLabel.setBounds(W - pad - 60, y, 60, 13);
        y += 16;
        creativitySlider.setBounds(pad, y, inner, 22);
    }

    // Generate button — always at a fixed distance from bottom of main panel
    int mainTop = 208;
    int mainH   = appendMode ? 310 : 236;
    int genY    = mainTop + mainH - 52;
    generateButton.setBounds(pad, genY, inner, 44);

    // Output panel
    int outTop = mainTop + mainH + 8;
    int oy = outTop + 10;

    outputHeaderLabel.setBounds(pad, oy, 80, 13);
    oy += 16;

    outputWaveform.setBounds(pad, oy, inner, 64);
    oy += 70;

    playButton.setBounds(pad, oy, 44, 44);
    positionHeaderLabel.setBounds(pad + 52, oy + 2, 160, 13);
    positionLabel.setBounds(pad + 52, oy + 17, 160, 18);
    oy += 50;
    printButton.setBounds(W / 2 - 88, oy, 176, 32);
}

void AIAudioProcessorEditor::timerCallback()
{
    if (!audioProcessor.isProcessing.load())
    {
        stopTimer();
        updateState();
    }
    else
    {
        static int dots = 0;
        dots = (dots + 1) % 4;
        juce::String s = "GENERATING";
        for (int i = 0; i < dots; ++i) s += ".";
        generateButton.setButtonText(s);
    }

    if (audioProcessor.isPlaying.load() && audioProcessor.hasResult.load())
    {
        float pos = (float)audioProcessor.playPosition.load() / (float)audioProcessor.fileSampleRate;
        formatTime(positionLabel, pos);
    }
}

void AIAudioProcessorEditor::updateState()
{
    bool ready = audioProcessor.hasResult.load();
    generateButton.setEnabled(loadedFile.existsAsFile());
    generateButton.setButtonText("GENERATE");
    playButton.setEnabled(ready);
    printButton.setEnabled(ready);

    if (ready)
    {
        outputWaveform.setWaveformData(audioProcessor.resultBuffer, audioProcessor.fileSampleRate);
        formatTime(positionLabel, 0.0f);
    }
}

