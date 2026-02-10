#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <vector>

struct TriggerLineData
{
    enum class ScriptType
    {
        singleHit = 0,
        bar = 1
    };

    juce::String lineId;
    juce::Line<float> screenLine;
    juce::String pythonScriptPath;
    ScriptType scriptType { ScriptType::singleHit };
    bool cutToBarEnd { false };
    float volume { 1.0f };
    float fadeInMs { 0.0f };
    float fadeOutMs { 0.0f };
    juce::Array<double> triggerPhases;
    juce::Array<float> waveformPreview;
    double waveformDurationSeconds { 0.0 };
};

struct SpiralTrackState
{
    juce::String name { "Track" };
    int colourIndex { -1 };
    double bpm { 120.0 };
    int timeSigNumerator { 4 };
    int timeSigDenominator { 4 };
    double loopBars { 2.0 };
    double thickness { 1.0 };
    double orbitWarpAmount { 0.0 };
    double spiralTwistAmount { 0.0 };
    double phaseOffsetDegrees { 0.0 };
    double xRotationDegrees { 0.0 };
    double yRotationDegrees { 0.0 };
    double xOffset { 0.0 };
    double yOffset { 0.0 };
    bool hidden { false };
    bool muted { false };
    bool syncToMidiClock { false };
    int midiSyncDivision { 1 };
    juce::Array<TriggerLineData> lines;
};

class SpiralSequencerEngine : public juce::AudioIODeviceCallback
{
public:
    struct LineWaveformPreview
    {
        juce::String lineId;
        juce::Array<float> samples;
        double durationSeconds { 0.0 };
    };
    struct TriggerEvent
    {
        int trackIndex { -1 };
        int lineIndex { -1 };
        juce::String lineId;
        double timeSeconds { 0.0 };
    };

    explicit SpiralSequencerEngine(juce::AudioDeviceManager& dm);
    ~SpiralSequencerEngine() override;

    void setTracks(juce::Array<SpiralTrackState> newTracks);
    void setTrackTempo(int trackIndex, double bpm);
    juce::Array<SpiralTrackState> getTracks() const;

    juce::Array<double> getCurrentPhases() const;
    double getGlobalTimeSeconds() const;
    std::vector<LineWaveformPreview> getWaveformPreviews() const;
    std::vector<TriggerEvent> consumeRecentTriggerEvents();

    void resetTransport();
    void setPlaying(bool shouldPlay);
    bool isPlaying() const;

    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

private:
    struct RenderedClip
    {
        std::shared_ptr<juce::AudioBuffer<float>> buffer;
        double sourceSampleRate { 44100.0 };
        double durationSeconds { 0.0 };
        juce::String signature;
        juce::Array<float> waveformPreview;
        bool rendering { false };
        bool ready { false };
        bool warnedMissing { false };
    };

    struct ActiveVoice
    {
        juce::String lineId;
        std::shared_ptr<juce::AudioBuffer<float>> buffer;
        double position { 0.0 };
        double ratio { 1.0 };
        float gain { 1.0f };
    };

    double loopDurationSeconds(const SpiralTrackState& track) const;
    bool crossedPhase(double from, double to, double target) const;

    juce::String normalizedLineId(const TriggerLineData& line, int trackIndex, int lineIndex) const;
    juce::String renderSignature(const TriggerLineData& line,
                                 double durationSeconds,
                                 int renderSampleRate) const;
    double renderDurationForLine(const TriggerLineData& line,
                                 const SpiralTrackState& track,
                                 double loopSeconds) const;

    void ensureRendered(const TriggerLineData& line,
                        int trackIndex,
                        int lineIndex,
                        double durationSeconds,
                        int renderSampleRate);

    void renderClipJob(juce::String lineId,
                       juce::String signature,
                       juce::String scriptPath,
                       int trackIndex,
                       double durationSeconds,
                       int renderSampleRate,
                       juce::String outputPath);

    std::shared_ptr<juce::AudioBuffer<float>> loadAudioFile(const juce::File& file,
                                                            double& outSampleRate);

    void triggerLine(const TriggerLineData& line,
                     int trackIndex,
                     int lineIndex,
                     double triggerPhase,
                     const SpiralTrackState& track,
                     double loopSeconds,
                     double hostSampleRate);

    juce::AudioDeviceManager& deviceManager;

    mutable juce::SpinLock stateLock;
    mutable juce::SpinLock cacheLock;
    mutable juce::SpinLock voiceLock;
    mutable juce::SpinLock formatLock;
    mutable juce::SpinLock triggerEventLock;

    juce::Array<SpiralTrackState> tracks;
    juce::Array<double> currentPhases;
    juce::Array<double> lastPhases;

    std::unordered_map<std::string, std::shared_ptr<RenderedClip>> clipByLineId;
    std::vector<ActiveVoice> activeVoices;
    std::vector<TriggerEvent> recentTriggerEvents;

    juce::AudioFormatManager audioFormats;
    juce::File renderDirectory;

    std::atomic<double> sampleRate { 44100.0 };
    std::atomic<double> globalTimeSeconds { 0.0 };
    std::atomic<bool> playing { false };
};
