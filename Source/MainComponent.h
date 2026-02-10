#pragma once

#include <JuceHeader.h>
#include <memory>
#include "SpiralSequencerEngine.h"

class SpiralTrackComponent : public juce::Component
{
public:
    using ChangeCallback = std::function<void()>;
    using SelectCallback = std::function<void()>;
    using ViewChangedCallback = std::function<void(float, juce::Point<float>)>;

    explicit SpiralTrackComponent(SpiralTrackState initialState);

    const SpiralTrackState& getState() const;
    void setState(const SpiralTrackState& newState);

    void setPlayheadPhase(double newPhase);
    void setSelected(bool isSelected);

    void setChangeCallback(ChangeCallback cb);
    void setSelectCallback(SelectCallback cb);
    void setViewChangedCallback(ViewChangedCallback cb);
    void setViewTransform(float scale, juce::Point<float> offset);
    void recomputeLineTriggers();
    void applyWaveformPreview(const SpiralSequencerEngine::LineWaveformPreview& preview);
    void flashLine(const juce::String& lineId);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    struct SpiralSample
    {
        juce::Point<float> point;
        double phase { 0.0 };
    };

    juce::Rectangle<float> spiralBounds() const;
    juce::Array<SpiralSample> buildSpiralSamples(int sampleCount) const;
    juce::Point<float> pointAtPhase(double phase) const;
    juce::Array<double> computeIntersectionsForLine(const juce::Line<float>& line) const;
    double nearestPhaseToPoint(juce::Point<float> point) const;
    int findLineAtPoint(juce::Point<float> point, float tolerance) const;
    int findLineHandleAtPoint(juce::Point<float> point, float radius, bool& isStartHandle) const;
    int findLineVolumeHandleAtPoint(juce::Point<float> point, float radius) const;
    void updateLineTriggerPhases(int lineIndex);
    float flashAmountForLineId(const juce::String& lineId) const;
    void decayLineFlashes();
    juce::Point<float> toWorldPoint(juce::Point<float> screenPoint) const;
    void notifyViewChanged();

    struct LineFlashState
    {
        juce::String lineId;
        float amount { 0.0f };
    };

    SpiralTrackState state;
    double playheadPhase { 0.0 };
    bool selected { false };
    float viewScale { 1.0f };
    juce::Point<float> viewOffset { 0.0f, 0.0f };

    enum class DragMode
    {
        none,
        drawingNew,
        movingLine,
        draggingStartHandle,
        draggingEndHandle,
        draggingVolumeHandle,
        panningView
    };

    DragMode dragMode { DragMode::none };
    int selectedLineIndex { -1 };
    juce::Point<float> dragStart;
    juce::Point<float> dragCurrent;
    juce::Point<float> lastDragPoint;
    juce::Array<LineFlashState> lineFlashes;
    std::unique_ptr<juce::FileChooser> pendingFileChooser;

    ChangeCallback onChanged;
    SelectCallback onSelected;
    ViewChangedCallback onViewChanged;
};

class MainComponent : public juce::Component,
                      private juce::Timer,
                      private juce::MidiInputCallback
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    void timerCallback() override;
    void mouseUp(const juce::MouseEvent& event) override;
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;

    void addTrack();
    void deleteTrack(int index);
    void duplicateTrack(int index);
    void rebindTrackCallbacks();
    void syncEngineTracks();
    void saveProject();
    void loadProject();
    void exportWav();
    void runExportWav(double exportSeconds);
    void openAudioSettings();
    void openOscSettings();
    void openMidiSettings();
    double loopDurationSeconds(const SpiralTrackState& track) const;
    juce::var trackToVar(const SpiralTrackState& track) const;
    bool trackFromVar(const juce::var& v, SpiralTrackState& out) const;

    void updateInspectorFromSelectedTrack();
    void updateSelectedTrackFromInspector();
    void setSelectedTrackIndex(int newIndex);
    void refreshTrackControlStates();
    void applyViewToAllTracks();
    void setTrackSettingsCollapsed(bool shouldCollapse);

    SpiralTrackComponent* selectedTrackComponent() const;

    juce::AudioDeviceManager audioDeviceManager;
    std::unique_ptr<SpiralSequencerEngine> engine;
    std::unique_ptr<juce::DatagramSocket> triggerOscSocket;
    bool triggerOscEnabled { false };
    juce::String oscHost { "127.0.0.1" };
    int oscPort { 9000 };
    juce::CriticalSection midiClockLock;
    double lastMidiClockTimeSec { -1.0 };
    double smoothedMidiClockIntervalSec { 0.0 };
    double lastMidiClockArrivalSec { 0.0 };
    double midiClockBpm { 0.0 };
    bool midiClockRunning { false };

    juce::OwnedArray<SpiralTrackComponent> trackComponents;

    int selectedTrackIndex { 0 };
    float viewScale { 1.0f };
    juce::Point<float> viewOffset { 0.0f, 0.0f };

    juce::TextButton addTrackButton { "Add Track" };
    juce::TextButton resetButton { "Play" };
    juce::TextButton saveProjectButton { "Save Project" };
    juce::TextButton loadProjectButton { "Load Project" };
    juce::TextButton exportWavButton { "Export WAV" };
    juce::TextButton audioSettingsButton { "Audio Settings" };
    juce::TextButton collapseSettingsButton { "Collapse Settings" };

    juce::Label selectedTrackLabel;
    juce::Label tracksLabel;
    juce::Viewport tracksViewport;
    juce::Component tracksListContent;
    juce::OwnedArray<juce::TextButton> selectTrackButtons;
    juce::OwnedArray<juce::ToggleButton> hideButtons;
    juce::OwnedArray<juce::ToggleButton> muteButtons;
    juce::OwnedArray<juce::ToggleButton> syncButtons;
    juce::OwnedArray<juce::ComboBox> syncDivisionBoxes;

    juce::Slider bpmSlider;
    juce::Slider numeratorSlider;
    juce::Slider denominatorSlider;
    juce::Slider loopBarsSlider;
    juce::Slider thicknessSlider;
    juce::Slider orbitWarpSlider;
    juce::Slider spiralTwistSlider;
    juce::Slider phaseSlider;
    juce::Slider xRotationSlider;
    juce::Slider yRotationSlider;
    juce::Slider xOffsetSlider;
    juce::Slider yOffsetSlider;

    juce::Label bpmLabel;
    juce::Label numeratorLabel;
    juce::Label denominatorLabel;
    juce::Label loopBarsLabel;
    juce::Label thicknessLabel;
    juce::Label orbitWarpLabel;
    juce::Label spiralTwistLabel;
    juce::Label phaseLabel;
    juce::Label xRotationLabel;
    juce::Label yRotationLabel;
    juce::Label xOffsetLabel;
    juce::Label yOffsetLabel;

    juce::Label infoLabel;
    std::unique_ptr<juce::FileChooser> pendingProjectChooser;
    bool trackSettingsCollapsed { false };
};
