#include "SpiralSequencerEngine.h"

#include <cmath>
#include <cstdio>

namespace
{
juce::File engineLogFile()
{
    return juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("orbits_engine.log");
}

void appendEngineLog(const juce::String& line)
{
    engineLogFile().appendText(line + "\n", false, false, "\n");
    juce::Logger::writeToLog(line);
}

juce::String resolvePythonExecutable()
{
    const juce::String envOverride = juce::SystemStats::getEnvironmentVariable("ORBITS_PYTHON", {});
    if (envOverride.isNotEmpty() && juce::File(envOverride).existsAsFile())
        return envOverride;

    const juce::StringArray candidates {
        "/opt/homebrew/bin/python3",
        "/usr/local/bin/python3",
        "/usr/bin/python3"
    };

    for (const auto& candidate : candidates)
    {
        if (juce::File(candidate).existsAsFile())
            return candidate;
    }

    return "/usr/bin/python3";
}
}

SpiralSequencerEngine::SpiralSequencerEngine(juce::AudioDeviceManager& dm)
    : deviceManager(dm),
      renderDirectory(juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("orbits_spiral_renders"))
{
    renderDirectory.createDirectory();
    audioFormats.registerBasicFormats();

    tracks.add({});
    tracks.getReference(0).name = "Track 1";

    currentPhases.resize(tracks.size());
    lastPhases.resize(tracks.size());

    for (int i = 0; i < tracks.size(); ++i)
    {
        currentPhases.set(i, 0.0);
        lastPhases.set(i, 0.0);
    }

    deviceManager.addAudioCallback(this);
}

SpiralSequencerEngine::~SpiralSequencerEngine()
{
    deviceManager.removeAudioCallback(this);
}

void SpiralSequencerEngine::setTracks(juce::Array<SpiralTrackState> newTracks)
{
    {
        const juce::SpinLock::ScopedLockType lock(stateLock);
        tracks = std::move(newTracks);

        currentPhases.resize(tracks.size());
        lastPhases.resize(tracks.size());

        for (int i = 0; i < tracks.size(); ++i)
        {
            currentPhases.set(i, 0.0);
            lastPhases.set(i, 0.0);
        }

        globalTimeSeconds.store(0.0);
    }

    {
        const juce::SpinLock::ScopedLockType lock(triggerEventLock);
        recentTriggerEvents.clear();
    }

    const auto renderSr = static_cast<int>(std::round(sampleRate.load()));

    for (int trackIndex = 0; trackIndex < tracks.size(); ++trackIndex)
    {
        const auto& track = tracks.getReference(trackIndex);
        const auto loopSeconds = loopDurationSeconds(track);

        for (int lineIndex = 0; lineIndex < track.lines.size(); ++lineIndex)
        {
            const auto& line = track.lines.getReference(lineIndex);
            const auto renderDuration = renderDurationForLine(line, track, loopSeconds);
            ensureRendered(line, trackIndex, lineIndex, renderDuration, renderSr);
        }
    }
}

void SpiralSequencerEngine::setTrackTempo(int trackIndex, double bpm)
{
    const juce::SpinLock::ScopedLockType lock(stateLock);
    if (!juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return;

    tracks.getReference(trackIndex).bpm = juce::jlimit(1.0, 500.0, bpm);
}

juce::Array<SpiralTrackState> SpiralSequencerEngine::getTracks() const
{
    const juce::SpinLock::ScopedLockType lock(stateLock);
    return tracks;
}

juce::Array<double> SpiralSequencerEngine::getCurrentPhases() const
{
    const juce::SpinLock::ScopedLockType lock(stateLock);
    return currentPhases;
}

double SpiralSequencerEngine::getGlobalTimeSeconds() const
{
    return globalTimeSeconds.load();
}

std::vector<SpiralSequencerEngine::LineWaveformPreview> SpiralSequencerEngine::getWaveformPreviews() const
{
    std::vector<LineWaveformPreview> previews;
    const juce::SpinLock::ScopedLockType lock(cacheLock);
    previews.reserve(clipByLineId.size());

    for (const auto& [lineId, clip] : clipByLineId)
    {
        if (clip == nullptr || !clip->ready || clip->waveformPreview.isEmpty())
            continue;

        LineWaveformPreview out;
        out.lineId = juce::String(lineId);
        out.samples = clip->waveformPreview;
        out.durationSeconds = clip->durationSeconds;
        previews.push_back(std::move(out));
    }

    return previews;
}

std::vector<SpiralSequencerEngine::TriggerEvent> SpiralSequencerEngine::consumeRecentTriggerEvents()
{
    const juce::SpinLock::ScopedLockType lock(triggerEventLock);
    auto out = std::move(recentTriggerEvents);
    recentTriggerEvents.clear();
    return out;
}

void SpiralSequencerEngine::resetTransport()
{
    const juce::SpinLock::ScopedLockType lock(stateLock);

    globalTimeSeconds.store(0.0);

    for (int i = 0; i < tracks.size(); ++i)
    {
        currentPhases.set(i, 0.0);
        lastPhases.set(i, 0.0);
    }
}

void SpiralSequencerEngine::setPlaying(bool shouldPlay)
{
    playing.store(shouldPlay);

    if (!shouldPlay)
    {
        const juce::SpinLock::ScopedLockType lock(voiceLock);
        activeVoices.clear();
    }
}

bool SpiralSequencerEngine::isPlaying() const
{
    return playing.load();
}

void SpiralSequencerEngine::audioDeviceIOCallbackWithContext(const float* const* /*inputChannelData*/,
                                                             int /*numInputChannels*/,
                                                             float* const* outputChannelData,
                                                             int numOutputChannels,
                                                             int numSamples,
                                                             const juce::AudioIODeviceCallbackContext& /*context*/)
{
    for (int ch = 0; ch < numOutputChannels; ++ch)
        juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);

    if (!playing.load())
        return;

    const auto hostSr = sampleRate.load();
    const auto dt = numSamples / hostSr;

    juce::Array<SpiralTrackState> localTracks;

    {
        const juce::SpinLock::ScopedLockType lock(stateLock);
        localTracks = tracks;

        const auto newGlobalTime = globalTimeSeconds.load() + dt;
        globalTimeSeconds.store(newGlobalTime);

        for (int trackIndex = 0; trackIndex < localTracks.size(); ++trackIndex)
        {
            const auto& track = localTracks.getReference(trackIndex);
            const auto loopSeconds = juce::jmax(0.001, loopDurationSeconds(track));

            const auto previousPhase = currentPhases[trackIndex];
            const auto phaseAdvance = dt / loopSeconds;
            const auto nextPhase = std::fmod(previousPhase + phaseAdvance, 1.0);

            if (!track.muted)
            {
                for (int lineIndex = 0; lineIndex < track.lines.size(); ++lineIndex)
                {
                    const auto& line = track.lines.getReference(lineIndex);
                    for (const auto triggerPhase : line.triggerPhases)
                    {
                        if (crossedPhase(previousPhase, nextPhase, triggerPhase))
                        {
                            triggerLine(line, trackIndex, lineIndex, triggerPhase, track, loopSeconds, hostSr);
                        }
                    }
                }
            }

            lastPhases.set(trackIndex, previousPhase);
            currentPhases.set(trackIndex, nextPhase);
        }
    }

    {
        const juce::SpinLock::ScopedLockType lock(voiceLock);

        for (size_t i = 0; i < activeVoices.size();)
        {
            auto& voice = activeVoices[i];

            if (voice.buffer == nullptr)
            {
                activeVoices.erase(activeVoices.begin() + static_cast<std::ptrdiff_t>(i));
                continue;
            }

            const int srcChannels = voice.buffer->getNumChannels();
            const int srcSamples = voice.buffer->getNumSamples();
            bool finished = false;

            for (int s = 0; s < numSamples; ++s)
            {
                const int idx = static_cast<int>(voice.position);
                if (idx >= srcSamples)
                {
                    finished = true;
                    break;
                }

                const int idx2 = juce::jmin(idx + 1, srcSamples - 1);
                const float frac = static_cast<float>(voice.position - static_cast<double>(idx));

                for (int ch = 0; ch < numOutputChannels; ++ch)
                {
                    const int srcCh = juce::jmin(ch, srcChannels - 1);
                    const float a = voice.buffer->getSample(srcCh, idx);
                    const float b = voice.buffer->getSample(srcCh, idx2);
                    outputChannelData[ch][s] += (a + (b - a) * frac) * voice.gain;
                }

                voice.position += voice.ratio;
            }

            if (finished)
                activeVoices.erase(activeVoices.begin() + static_cast<std::ptrdiff_t>(i));
            else
                ++i;
        }
    }
    for (int ch = 0; ch < numOutputChannels; ++ch)
    {
        auto* out = outputChannelData[ch];
        for (int s = 0; s < numSamples; ++s)
            out[s] = std::tanh(out[s] * 0.75f);
    }
}

void SpiralSequencerEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    sampleRate.store(device != nullptr ? device->getCurrentSampleRate() : 44100.0);
    resetTransport();
    setPlaying(false);
}

void SpiralSequencerEngine::audioDeviceStopped()
{
    sampleRate.store(44100.0);
}

double SpiralSequencerEngine::loopDurationSeconds(const SpiralTrackState& track) const
{
    const auto beatsPerBar = track.timeSigNumerator * (4.0 / juce::jmax(1, track.timeSigDenominator));
    const auto totalBeats = beatsPerBar * juce::jmax(0.25, track.loopBars);
    return totalBeats * (60.0 / juce::jmax(1.0, track.bpm));
}

bool SpiralSequencerEngine::crossedPhase(double from, double to, double target) const
{
    if (from <= to)
        return target >= from && target < to;

    return target >= from || target < to;
}

juce::String SpiralSequencerEngine::normalizedLineId(const TriggerLineData& line, int trackIndex, int lineIndex) const
{
    if (line.lineId.isNotEmpty())
        return line.lineId;

    return juce::String(trackIndex) + ":" + juce::String(lineIndex) + ":" + line.pythonScriptPath;
}

juce::String SpiralSequencerEngine::renderSignature(const TriggerLineData& line,
                                                    double durationSeconds,
                                                    int renderSampleRate) const
{
    const auto durationMs = static_cast<int>(std::round(durationSeconds * 1000.0));
    return line.pythonScriptPath + "|"
           + juce::String(durationMs) + "|"
           + juce::String(renderSampleRate) + "|"
           + juce::String(static_cast<int>(line.scriptType));
}

double SpiralSequencerEngine::renderDurationForLine(const TriggerLineData& line,
                                                    const SpiralTrackState& track,
                                                    double loopSeconds) const
{
    if (line.scriptType == TriggerLineData::ScriptType::bar)
    {
        const auto bars = juce::jmax(0.25, track.loopBars);
        return juce::jmax(0.05, loopSeconds / bars);
    }

    // Single-hit clips get rendered with a longer window and then play back at their natural clip length.
    return 3.0;
}

void SpiralSequencerEngine::ensureRendered(const TriggerLineData& line,
                                           int trackIndex,
                                           int lineIndex,
                                           double durationSeconds,
                                           int renderSampleRate)
{
    if (line.pythonScriptPath.isEmpty())
        return;

    const auto lineId = normalizedLineId(line, trackIndex, lineIndex);
    const auto signature = renderSignature(line, durationSeconds, renderSampleRate);
    const auto lineKey = lineId.toStdString();

    {
        const juce::SpinLock::ScopedLockType lock(cacheLock);
        auto it = clipByLineId.find(lineKey);

        if (it != clipByLineId.end())
        {
            if (it->second->rendering)
                return;

            if (it->second->ready && it->second->signature == signature)
                return;
        }
    }

    const auto outputPath = renderDirectory.getChildFile(juce::String::toHexString(lineId.hashCode64()) + ".wav").getFullPathName();

    {
        const juce::SpinLock::ScopedLockType lock(cacheLock);
        auto& clip = clipByLineId[lineKey];

        if (clip == nullptr)
            clip = std::make_shared<RenderedClip>();

        clip->rendering = true;
        clip->ready = false;
        clip->warnedMissing = false;
        clip->signature = signature;
        clip->buffer.reset();
    }

    renderClipJob(lineId, signature, line.pythonScriptPath, trackIndex, durationSeconds, renderSampleRate, outputPath);
}

void SpiralSequencerEngine::renderClipJob(juce::String lineId,
                                          juce::String signature,
                                          juce::String scriptPath,
                                          int trackIndex,
                                          double durationSeconds,
                                          int renderSampleRate,
                                          juce::String outputPath)
{
    juce::File output(outputPath);
    output.getParentDirectory().createDirectory();

    const auto pythonExe = resolvePythonExecutable();

    const juce::StringArray args {
        pythonExe,
        scriptPath,
        "--track", juce::String(trackIndex),
        "--time", "0.0",
        "--duration", juce::String(durationSeconds, 6),
        "--sample-rate", juce::String(renderSampleRate),
        "--output", output.getFullPathName(),
        "--no-play"
    };

    bool ready = false;
    std::shared_ptr<juce::AudioBuffer<float>> buffer;
    double sourceSr = static_cast<double>(renderSampleRate);
    int exitCode = -1;

    appendEngineLog("[render] start python=" + pythonExe
                    + " script=" + scriptPath
                    + " duration=" + juce::String(durationSeconds, 4)
                    + " sr=" + juce::String(renderSampleRate));

    auto runProcess = [&](const juce::StringArray& command, const juce::String& modeLabel)
    {
        juce::ChildProcess localProcess;
        if (!localProcess.start(command))
        {
            appendEngineLog("[render] failed to start python for " + scriptPath + " mode=" + modeLabel);
            return false;
        }

        const auto ok = localProcess.waitForProcessToFinish(10 * 60 * 1000);
        const auto logs = localProcess.readAllProcessOutput();
        exitCode = localProcess.getExitCode();

        if (logs.isNotEmpty())
            appendEngineLog("[render] output(" + modeLabel + ") " + logs.trim());

        if (!ok)
        {
            appendEngineLog("[render] timeout for " + scriptPath + " mode=" + modeLabel);
            return false;
        }

        buffer = loadAudioFile(output, sourceSr);
        ready = (buffer != nullptr && buffer->getNumSamples() > 0);
        return ready;
    };

    const bool renderedWithFlags = runProcess(args, "flags");

    if (!renderedWithFlags)
    {
        const juce::StringArray legacyArgs {
            pythonExe,
            scriptPath,
            output.getFullPathName(),
            juce::String(renderSampleRate),
            juce::String(durationSeconds, 6)
        };

        appendEngineLog("[render] retry legacy positional CLI for " + scriptPath);
        runProcess(legacyArgs, "legacy");
    }

    const juce::SpinLock::ScopedLockType lock(cacheLock);
    auto it = clipByLineId.find(lineId.toStdString());
    if (it != clipByLineId.end())
    {
        it->second->rendering = false;
        it->second->ready = ready;
        it->second->sourceSampleRate = sourceSr;
        it->second->durationSeconds = (buffer != nullptr && sourceSr > 0.0)
            ? (static_cast<double>(buffer->getNumSamples()) / sourceSr)
            : 0.0;

        if (it->second->signature == signature)
        {
            it->second->buffer = buffer;
            it->second->warnedMissing = false;
            it->second->waveformPreview.clearQuick();

            if (buffer != nullptr && buffer->getNumSamples() > 0)
            {
                constexpr int bins = 128;
                it->second->waveformPreview.resize(bins);
                const auto* data = buffer->getReadPointer(0);
                const int n = buffer->getNumSamples();

                for (int i = 0; i < bins; ++i)
                {
                    const int idx = juce::jlimit(0, n - 1, (i * n) / bins);
                    it->second->waveformPreview.set(i, data[idx]);
                }
            }
        }

        if (!ready)
            appendEngineLog("[render] failed/empty clip for " + scriptPath + " exit=" + juce::String(exitCode));
        else
            appendEngineLog("[render] ready " + scriptPath
                            + " samples=" + juce::String(buffer->getNumSamples())
                            + " sr=" + juce::String(sourceSr, 2)
                            + " exit=" + juce::String(exitCode));
    }

    if (ready && buffer != nullptr)
    {
        ActiveVoice voice;
        voice.lineId = lineId;
        voice.buffer = buffer;
        voice.position = 0.0;
        voice.ratio = juce::jmax(0.001, sourceSr / juce::jmax(1.0, sampleRate.load()));
        voice.gain = 0.45f;

        const juce::SpinLock::ScopedLockType voiceGuard(voiceLock);
        bool replaced = false;
        for (auto& existing : activeVoices)
        {
            if (existing.lineId == voice.lineId)
            {
                existing = voice;
                replaced = true;
                break;
            }
        }

        if (!replaced)
            activeVoices.push_back(std::move(voice));
    }
}

std::shared_ptr<juce::AudioBuffer<float>> SpiralSequencerEngine::loadAudioFile(const juce::File& file,
                                                                                double& outSampleRate)
{
    outSampleRate = 0.0;

    if (!file.existsAsFile())
        return nullptr;

    std::unique_ptr<juce::AudioFormatReader> reader;
    {
        const juce::SpinLock::ScopedLockType lock(formatLock);
        reader.reset(audioFormats.createReaderFor(file));
    }

    if (reader == nullptr)
        return nullptr;

    outSampleRate = reader->sampleRate;

    const auto numChannels = static_cast<int>(reader->numChannels);
    const auto numSamples = static_cast<int>(reader->lengthInSamples);
    if (numChannels <= 0 || numSamples <= 0)
        return nullptr;

    auto buffer = std::make_shared<juce::AudioBuffer<float>>(juce::jmax(1, numChannels), numSamples);
    if (!reader->read(buffer.get(), 0, numSamples, 0, true, true))
        return nullptr;

    return buffer;
}

void SpiralSequencerEngine::triggerLine(const TriggerLineData& line,
                                        int trackIndex,
                                        int lineIndex,
                                        double triggerPhase,
                                        const SpiralTrackState& track,
                                        double loopSeconds,
                                        double hostSampleRate)
{
    const auto lineId = normalizedLineId(line, trackIndex, lineIndex).toStdString();

    std::shared_ptr<RenderedClip> clip;

    {
        const juce::SpinLock::ScopedLockType lock(cacheLock);
        auto it = clipByLineId.find(lineId);
        if (it != clipByLineId.end())
            clip = it->second;
    }

    if (clip == nullptr || !clip->ready || clip->buffer == nullptr)
    {
        const juce::SpinLock::ScopedLockType lock(cacheLock);
        auto it = clipByLineId.find(lineId);
        if (it != clipByLineId.end() && !it->second->warnedMissing)
        {
            it->second->warnedMissing = true;
            juce::Logger::writeToLog("[trigger] clip not ready for line " + juce::String(lineId));
        }
        return;
    }

    ActiveVoice voice;
    voice.lineId = juce::String(lineId);
    voice.buffer = clip->buffer;
    voice.position = 0.0;
    voice.ratio = juce::jmax(0.001, clip->sourceSampleRate / juce::jmax(1.0, hostSampleRate));
    voice.gain = 0.55f * juce::jlimit(0.0f, 1.6f, line.volume);

    auto workingBuffer = voice.buffer;

    if (line.scriptType == TriggerLineData::ScriptType::bar && line.cutToBarEnd)
    {
        const auto bars = juce::jmax(0.25, track.loopBars);
        const auto barSeconds = juce::jmax(0.01, loopSeconds / bars);
        const auto barPos = triggerPhase * bars;
        const auto fracInBar = barPos - std::floor(barPos);
        const auto remainSeconds = juce::jmax(0.0, (1.0 - fracInBar) * barSeconds);
        const auto maxOutSamples = static_cast<int>(remainSeconds * hostSampleRate);
        if (maxOutSamples > 0)
        {
            const auto maxSrcSamples = static_cast<int>(std::floor(maxOutSamples * voice.ratio));
            if (maxSrcSamples > 0 && maxSrcSamples < workingBuffer->getNumSamples())
            {
                auto trimmed = std::make_shared<juce::AudioBuffer<float>>(workingBuffer->getNumChannels(), maxSrcSamples);
                for (int ch = 0; ch < trimmed->getNumChannels(); ++ch)
                    trimmed->copyFrom(ch, 0, *workingBuffer, ch, 0, maxSrcSamples);
                workingBuffer = trimmed;
            }
        }
    }

    const auto fadeInSamples = static_cast<int>(std::round(line.fadeInMs * 0.001 * clip->sourceSampleRate));
    const auto fadeOutSamples = static_cast<int>(std::round(line.fadeOutMs * 0.001 * clip->sourceSampleRate));
    if ((fadeInSamples > 0 || fadeOutSamples > 0) && workingBuffer != nullptr)
    {
        if (workingBuffer == voice.buffer)
        {
            auto copied = std::make_shared<juce::AudioBuffer<float>>(workingBuffer->getNumChannels(), workingBuffer->getNumSamples());
            for (int ch = 0; ch < copied->getNumChannels(); ++ch)
                copied->copyFrom(ch, 0, *workingBuffer, ch, 0, workingBuffer->getNumSamples());
            workingBuffer = copied;
        }

        const int n = workingBuffer->getNumSamples();
        const int fi = juce::jlimit(0, n, fadeInSamples);
        const int fo = juce::jlimit(0, n, fadeOutSamples);

        for (int ch = 0; ch < workingBuffer->getNumChannels(); ++ch)
        {
            auto* data = workingBuffer->getWritePointer(ch);
            for (int i = 0; i < fi; ++i)
            {
                const auto g = static_cast<float>(i) / static_cast<float>(juce::jmax(1, fi));
                data[i] *= g;
            }
            for (int i = 0; i < fo; ++i)
            {
                const int idx = n - 1 - i;
                if (idx < 0)
                    break;
                const auto g = static_cast<float>(i) / static_cast<float>(juce::jmax(1, fo));
                data[idx] *= g;
            }
        }
    }

    voice.buffer = workingBuffer;

    const juce::SpinLock::ScopedLockType lock(voiceLock);
    bool replaced = false;
    for (auto& existing : activeVoices)
    {
        if (existing.lineId == voice.lineId)
        {
            existing = voice;
            replaced = true;
            break;
        }
    }

    if (!replaced)
        activeVoices.push_back(std::move(voice));

    {
        const juce::SpinLock::ScopedLockType eventLock(triggerEventLock);
        TriggerEvent ev;
        ev.trackIndex = trackIndex;
        ev.lineIndex = lineIndex;
        ev.lineId = juce::String(lineId);
        ev.timeSeconds = globalTimeSeconds.load();
        recentTriggerEvents.push_back(std::move(ev));
    }
}
