#include "PluginProcessor.h"
#include "PluginEditor.h"

AIAudioProcessor::AIAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input",   juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    DBG("=== Constructor called ===");
    formatManager.registerBasicFormats();
}

AIAudioProcessor::~AIAudioProcessor() {}

void AIAudioProcessor::prepareToPlay(double, int)
{
    DBG("=== prepareToPlay called ===");
}

void AIAudioProcessor::releaseResources() {}

void AIAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    buffer.clear();

    if (isPlaying.load() && hasResult.load())
    {
        int pos    = playPosition.load();
        int frames = buffer.getNumSamples();
        int avail  = resultBuffer.getNumSamples() - pos;
        int tocopy = juce::jmin(frames, avail);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            int srcCh = ch % resultBuffer.getNumChannels();
            buffer.copyFrom(ch, 0, resultBuffer, srcCh, pos, tocopy);
        }

        playPosition.store(pos + tocopy);
        if (tocopy < frames) { isPlaying.store(false); playPosition.store(0); }
    }
}

void AIAudioProcessor::loadAudioFile(const juce::File& file)
{
    DBG("=== loadAudioFile called ===");
    auto* reader = formatManager.createReaderFor(file);
    if (reader == nullptr) { DBG("ERROR: reader is null"); return; }
    fileSampleRate = reader->sampleRate;
    inputBuffer.setSize((int)reader->numChannels, (int)reader->lengthInSamples);
    reader->read(&inputBuffer, 0, (int)reader->lengthInSamples, 0, true, true);
    delete reader;
    DBG("loadAudioFile done, samples: " + juce::String(inputBuffer.getNumSamples()));
}

void AIAudioProcessor::sendToServer()
{
    DBG("=== sendToServer called ===");
    isProcessing.store(true);
    hasResult.store(false);

    juce::Thread::launch([this]()
    {
        DBG("=== thread started ===");

        auto tempFile = juce::File::getSpecialLocation(
            juce::File::tempDirectory).getChildFile("ai_input.wav");
        tempFile.deleteFile();

        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatWriter> writer(
            wavFormat.createWriterFor(
                new juce::FileOutputStream(tempFile),
                fileSampleRate, inputBuffer.getNumChannels(), 16, {}, 0));

        if (writer != nullptr)
        {
            writer->writeFromAudioSampleBuffer(inputBuffer, 0, inputBuffer.getNumSamples());
            DBG("WAV written: " + tempFile.getFullPathName());
        }
        else
        {
            DBG("ERROR: Failed to write WAV");
            isProcessing.store(false);
            return;
        }
        writer.reset();

        juce::String mode = isAppendMode.load() ? "append" : "restyle";
        DBG("Params: mode=" + mode
            + " start=" + juce::String(startValue.load(), 2)
            + " end="   + juce::String(endValue.load(), 2)
            + " creativity=" + juce::String(creativityValue.load()));

        juce::URL uploadURL("http://127.0.0.1:8080/process");
        uploadURL = uploadURL
            .withFileToUpload("audio", tempFile, "audio/wav")
            .withParameter("mode",       mode)
            .withParameter("prompt",     promptText)
            .withParameter("start",      juce::String(startValue.load(), 2))
            .withParameter("end",        juce::String(endValue.load(), 2))
            .withParameter("creativity", juce::String(creativityValue.load()));

        DBG("Sending to server...");
        auto response = uploadURL.createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData));

        DBG("Response: " + juce::String(response != nullptr ? "yes" : "no"));

        if (response != nullptr)
        {
            juce::MemoryBlock data;
            response->readIntoMemoryBlock(data);
            DBG("Response size: " + juce::String((int)data.getSize()));

            if (data.getSize() == 0)
            {
                DBG("ERROR: Empty response");
                isProcessing.store(false);
                return;
            }

            // --- Write the AI OUTPUT (response bytes) to disk BEFORE parsing ---
            // Use the response WAV bytes, NOT the input tempFile.
            auto outputFile = juce::File::getSpecialLocation(
                juce::File::tempDirectory).getChildFile("ai_output.wav");
            outputFile.deleteFile();

            {
                juce::FileOutputStream outStream(outputFile);
                if (outStream.openedOk())
                {
                    outStream.write(data.getData(), data.getSize());
                    outStream.flush();
                    DBG("Output saved: " + outputFile.getFullPathName()
                        + " (" + juce::String((int)data.getSize()) + " bytes)");
                }
                else
                {
                    DBG("ERROR: could not open ai_output.wav for writing");
                }
            }

            // Now parse the WAV in memory for in-plugin playback
            auto* mis = new juce::MemoryInputStream(data.getData(), data.getSize(), false);
            juce::WavAudioFormat fmt;
            auto* reader = fmt.createReaderFor(mis, true);
            DBG("WAV reader: " + juce::String(reader != nullptr ? "OK" : "FAILED"));

            if (reader != nullptr)
            {
                resultBuffer.setSize((int)reader->numChannels, (int)reader->lengthInSamples);
                reader->read(&resultBuffer, 0, (int)reader->lengthInSamples, 0, true, true);
                delete reader;
                hasResult.store(true);
                DBG("Result loaded into resultBuffer");

                auto triggerFile = juce::File::getSpecialLocation(
                    juce::File::tempDirectory).getChildFile("ai_output_ready.txt");
                triggerFile.replaceWithText("ready");
                DBG("Trigger written");
            }
            else
            {
                DBG("ERROR: Could not parse response as WAV");
            }
        }
        else
        {
            DBG("ERROR: No response from server");
        }

        isProcessing.store(false);
        juce::MessageManager::callAsync([this]() {
            if (auto* ed = dynamic_cast<AIAudioProcessorEditor*>(getActiveEditor()))
                ed->updateState();
        });

        DBG("=== thread done ===");
    });
}

void AIAudioProcessor::startPlayback() { playPosition.store(0); isPlaying.store(true); }
void AIAudioProcessor::stopPlayback()  { isPlaying.store(false); playPosition.store(0); }

juce::AudioProcessorEditor* AIAudioProcessor::createEditor()
{ return new AIAudioProcessorEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{ return new AIAudioProcessor(); }
