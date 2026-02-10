#include "MainComponent.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace
{
constexpr float twoPi = 6.28318530717958647692f;
constexpr float orbitWarpHarmonic = 3.0f;
constexpr const char* triggerOscAddress = "/orbits/trigger";

void appendOscPaddedString(juce::MemoryOutputStream& out, const juce::String& text)
{
    const auto utf8 = text.toUTF8();
    out.write(utf8.getAddress(), static_cast<size_t>(utf8.sizeInBytes() - 1));
    out.writeByte(0);
    while ((out.getDataSize() % 4) != 0)
        out.writeByte(0);
}

void appendOscInt32BE(juce::MemoryOutputStream& out, int value)
{
    out.writeByte((value >> 24) & 0xff);
    out.writeByte((value >> 16) & 0xff);
    out.writeByte((value >> 8) & 0xff);
    out.writeByte(value & 0xff);
}

void buildOscTriggerMessage(juce::MemoryOutputStream& out, int trackNumber, int lineNumber)
{
    appendOscPaddedString(out, triggerOscAddress);
    appendOscPaddedString(out, ",ii");
    appendOscInt32BE(out, trackNumber);
    appendOscInt32BE(out, lineNumber);
}

struct SpiralGeometry
{
    int turns { 4 };
    float outerRadiusScale { 0.42f };
    float innerRadiusScale { 0.12f };
    float shapeExponent { 1.0f };
};

SpiralGeometry geometryForTrack(const SpiralTrackState& state)
{
    const auto beatsPerBar = state.timeSigNumerator * (4.0 / juce::jmax(1, state.timeSigDenominator));
    const auto totalBeats = beatsPerBar * juce::jmax(0.25, state.loopBars);

    SpiralGeometry g;
    g.turns = juce::jlimit(2, 18, static_cast<int>(std::round(totalBeats * 0.5)));

    const auto bpmNorm = juce::jlimit(0.0f, 1.0f, static_cast<float>((state.bpm - 40.0) / 240.0));
    g.outerRadiusScale = juce::jmap(bpmNorm, 0.48f, 0.30f);

    const auto denomNorm = juce::jlimit(0.0f, 1.0f, static_cast<float>((state.timeSigDenominator - 1) / 15.0));
    g.innerRadiusScale = juce::jmap(denomNorm, 0.08f, 0.22f);

    const auto numNorm = juce::jlimit(0.0f, 1.0f, static_cast<float>((state.timeSigNumerator - 1) / 14.0));
    g.shapeExponent = juce::jmap(numNorm, 0.72f, 1.65f);

    return g;
}

juce::Colour colourForTrack(const SpiralTrackState& state)
{
    static const std::array<juce::Colour, 10> palette {
        juce::Colour::fromRGB(255, 99, 71),   // tomato
        juce::Colour::fromRGB(64, 224, 208),  // turquoise
        juce::Colour::fromRGB(255, 215, 0),   // gold
        juce::Colour::fromRGB(255, 105, 180), // hot pink
        juce::Colour::fromRGB(124, 252, 0),   // lawn green
        juce::Colour::fromRGB(135, 206, 250), // light sky blue
        juce::Colour::fromRGB(255, 140, 0),   // dark orange
        juce::Colour::fromRGB(186, 85, 211),  // medium orchid
        juce::Colour::fromRGB(0, 255, 127),   // spring green
        juce::Colour::fromRGB(255, 160, 122)  // light salmon
    };

    if (state.colourIndex >= 0)
        return palette[static_cast<size_t>(state.colourIndex) % palette.size()];

    const auto hash = static_cast<uint32_t>(state.name.hashCode64());
    const auto hue = static_cast<float>(hash % 1024u) / 1024.0f;
    return juce::Colour::fromHSV(hue, 0.72f, 0.95f, 1.0f);
}
}

SpiralTrackComponent::SpiralTrackComponent(SpiralTrackState initialState)
    : state(std::move(initialState))
{
    setWantsKeyboardFocus(true);
    setInterceptsMouseClicks(false, false);
}

const SpiralTrackState& SpiralTrackComponent::getState() const
{
    return state;
}

void SpiralTrackComponent::setState(const SpiralTrackState& newState)
{
    state = newState;
    if (!juce::isPositiveAndBelow(selectedLineIndex, state.lines.size()))
        selectedLineIndex = -1;
    repaint();
}

void SpiralTrackComponent::setPlayheadPhase(double newPhase)
{
    playheadPhase = juce::jlimit(0.0, 1.0, newPhase);
    decayLineFlashes();
    repaint();
}

void SpiralTrackComponent::setSelected(bool isSelected)
{
    selected = isSelected;
    setInterceptsMouseClicks(selected && !state.hidden, selected && !state.hidden);
    repaint();
}

void SpiralTrackComponent::setChangeCallback(ChangeCallback cb)
{
    onChanged = std::move(cb);
}

void SpiralTrackComponent::setSelectCallback(SelectCallback cb)
{
    onSelected = std::move(cb);
}

void SpiralTrackComponent::setViewChangedCallback(ViewChangedCallback cb)
{
    onViewChanged = std::move(cb);
}

void SpiralTrackComponent::setViewTransform(float scale, juce::Point<float> offset)
{
    viewScale = juce::jlimit(0.2f, 5.0f, scale);
    viewOffset = offset;
    repaint();
}

void SpiralTrackComponent::recomputeLineTriggers()
{
    for (int i = 0; i < state.lines.size(); ++i)
        updateLineTriggerPhases(i);
    repaint();
}

void SpiralTrackComponent::paint(juce::Graphics& g)
{
    if (state.hidden)
        return;

    juce::Graphics::ScopedSaveState scope(g);
    g.addTransform(juce::AffineTransform::scale(viewScale).translated(viewOffset.x, viewOffset.y));

    const auto trackColour = colourForTrack(state);
    const auto oppositeHue = std::fmod(trackColour.getHue() + 0.5f, 1.0f);
    const auto playheadBase = trackColour.getPerceivedBrightness() > 0.55f
        ? juce::Colour::fromHSV(oppositeHue, 0.82f, 0.20f, 1.0f)
        : juce::Colour::fromHSV(oppositeHue, 0.55f, 0.98f, 1.0f);
    const auto playheadColour = state.muted
        ? juce::Colours::grey
        : playheadBase;
    const auto markerColour = juce::Colour::fromRGB(240, 246, 255).interpolatedWith(trackColour, 0.18f);
    const auto snap = [](float v) { return std::floor(v) + 0.5f; };
    const auto snapPoint = [&](juce::Point<float> p) { return juce::Point<float>(snap(p.x), snap(p.y)); };

    if (selected)
    {
        g.setColour(trackColour.withAlpha(0.16f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(4.0f), 12.0f, 1.4f);
    }

    const auto samples = buildSpiralSamples(1500);
    if (samples.size() < 2)
        return;

    juce::Path spiralPath;
    spiralPath.startNewSubPath(samples[0].point);

    for (int i = 1; i < samples.size(); ++i)
        spiralPath.lineTo(samples[i].point);

    spiralPath.closeSubPath();

    const auto thickness = juce::jlimit(0.25, 4.0, state.thickness);
    const float tubeOuter = static_cast<float>((selected ? 14.0 : 11.0) * thickness);
    const float tubeInner = static_cast<float>((selected ? 8.0 : 6.2) * thickness);
    g.setColour(trackColour.darker(0.45f).withAlpha(selected ? 0.86f : 0.74f));
    g.strokePath(spiralPath, juce::PathStrokeType(tubeOuter, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(trackColour.withAlpha(selected ? 1.0f : 0.92f));
    g.strokePath(spiralPath, juce::PathStrokeType(tubeInner, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(juce::Colours::black.withAlpha(selected ? 0.30f : 0.24f));
    g.strokePath(spiralPath, juce::PathStrokeType(1.3f));

    const auto totalBars = juce::jmax(0.25, state.loopBars);
    const auto beatsPerBar = juce::jmax(1, state.timeSigNumerator);
    const auto divisionsPerBeat = 4;
    const auto totalDivisions = juce::jmax(1, static_cast<int>(std::floor(totalBars * beatsPerBar * divisionsPerBeat)));

    for (int div = 0; div < totalDivisions; ++div)
    {
        const bool isBar = (div % (beatsPerBar * divisionsPerBeat)) == 0;
        const bool isBeat = !isBar && (div % divisionsPerBeat) == 0;

        const auto phase = juce::jlimit(0.0, 0.999999, static_cast<double>(div) / static_cast<double>(totalDivisions));
        const auto markerPoint = pointAtPhase(phase);
        const auto nextPoint = pointAtPhase(std::min(0.999999, phase + 0.0025));
        const auto tangent = nextPoint - markerPoint;
        const auto tangentLen = std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y);

        const auto tickLen = isBar ? (selected ? 23.0f : 18.0f)
                                   : (isBeat ? (selected ? 15.0f : 11.0f)
                                             : (selected ? 9.0f : 6.5f));
        const auto tickThickness = isBar ? (selected ? 4.2f : 3.0f)
                                         : (isBeat ? (selected ? 1.5f : 1.0f)
                                                   : (selected ? 0.95f : 0.7f));
        const auto alpha = isBar ? (selected ? 1.0f : 0.88f)
                                 : (isBeat ? (selected ? 0.82f : 0.62f)
                                           : (selected ? 0.55f : 0.38f));

        const auto nx = tangentLen > 0.0001f ? (-tangent.y / tangentLen) : 0.0f;
        const auto ny = tangentLen > 0.0001f ? (tangent.x / tangentLen) : -1.0f;
        const auto innerHalf = tubeInner * 0.55f;
        const auto startX = markerPoint.x - nx * innerHalf;
        const auto startY = markerPoint.y - ny * innerHalf;
        const auto endX = markerPoint.x + nx * (innerHalf + tickLen);
        const auto endY = markerPoint.y + ny * (innerHalf + tickLen);

        const auto sx = snap(startX);
        const auto sy = snap(startY);
        const auto ex = snap(endX);
        const auto ey = snap(endY);

        g.setColour(juce::Colours::black.withAlpha(alpha * 0.85f));
        g.drawLine(sx, sy, ex, ey, tickThickness + 1.1f);
        g.setColour(markerColour.withAlpha(alpha));
        g.drawLine(sx, sy, ex, ey, tickThickness);

        if (isBar)
        {
            const int barIndex = div / (beatsPerBar * divisionsPerBeat);
            const auto labelX = static_cast<int>(markerPoint.x + nx * (innerHalf + tickLen + 6.0f));
            const auto labelY = static_cast<int>(markerPoint.y + ny * (innerHalf + tickLen + 6.0f) - 8.0f);
            g.setColour(juce::Colours::black.withAlpha(selected ? 0.72f : 0.58f));
            g.fillRoundedRectangle(juce::Rectangle<float>(static_cast<float>(labelX - 2),
                                                          static_cast<float>(labelY - 1),
                                                          22.0f,
                                                          16.0f), 4.0f);
            g.setColour(markerColour.withAlpha(selected ? 1.0f : 0.9f));
            g.setFont(selected ? 12.0f : 10.0f);
            g.drawText(juce::String(barIndex + 1),
                       juce::Rectangle<int>(labelX, labelY, 20, 14),
                       juce::Justification::centredLeft,
                       false);
        }
    }

    for (int lineIndex = 0; lineIndex < state.lines.size(); ++lineIndex)
    {
        const auto& line = state.lines.getReference(lineIndex);
        const bool isLineSelected = selected && lineIndex == selectedLineIndex;
        const auto flashAmount = flashAmountForLineId(line.lineId);
        const auto lineStart = snapPoint(line.screenLine.getStart());
        const auto lineEnd = snapPoint(line.screenLine.getEnd());
        g.setColour((isLineSelected ? juce::Colours::white : juce::Colours::orange).withAlpha(selected ? 0.98f : 0.68f));
        g.drawLine(lineStart.x, lineStart.y, lineEnd.x, lineEnd.y, isLineSelected ? 2.7f : (selected ? 1.9f : 1.3f));

        for (auto phase : line.triggerPhases)
        {
            const auto p = pointAtPhase(phase);
            g.setColour(juce::Colours::gold.withAlpha(selected ? 0.95f : 0.45f));
            const auto ps = snapPoint(p);
            g.fillEllipse(ps.x - 2.4f, ps.y - 2.4f, 4.8f, 4.8f);

            if (flashAmount > 0.001f)
            {
                const auto flashCol = markerColour.brighter(0.35f);
                const auto radius = (tubeInner * 0.55f) + flashAmount * (selected ? 8.0f : 6.0f);
                g.setColour(flashCol.withAlpha(0.22f + flashAmount * 0.34f));
                g.drawEllipse(ps.x - radius, ps.y - radius, radius * 2.0f, radius * 2.0f, 1.6f);
            }

            if (!line.waveformPreview.isEmpty() && line.waveformDurationSeconds > 0.0)
            {
                const auto bars = juce::jmax(0.25, state.loopBars);
                const auto beatsPerBar = state.timeSigNumerator * (4.0 / juce::jmax(1, state.timeSigDenominator));
                const auto loopSeconds = juce::jmax(0.05, bars * beatsPerBar * (60.0 / juce::jmax(1.0, state.bpm)));
                const auto spanPhase = juce::jlimit(0.0, 1.0, line.waveformDurationSeconds / loopSeconds);
                const auto thickness = static_cast<float>(juce::jlimit(0.25, 4.0, state.thickness));
                const auto tubeInner = (selected ? 8.0f : 6.2f) * thickness;
                const auto ampScale = tubeInner * (selected ? 0.95f : 0.85f);

                juce::Path waveformPath;
                bool started = false;
                for (int i = 0; i < line.waveformPreview.size(); ++i)
                {
                    const auto t = static_cast<double>(i) / static_cast<double>(juce::jmax(1, line.waveformPreview.size() - 1));
                    const auto phasePos = std::fmod(phase + spanPhase * t, 1.0);
                    const auto basePoint = pointAtPhase(phasePos);
                    const auto nextPoint = pointAtPhase(std::fmod(phasePos + 0.002, 1.0));
                    const auto tangent = nextPoint - basePoint;
                    const auto len = std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y);
                    const auto nx = len > 0.0001f ? -tangent.y / len : 0.0f;
                    const auto ny = len > 0.0001f ? tangent.x / len : -1.0f;
                    const auto y = juce::jlimit(-1.0f, 1.0f, line.waveformPreview[i]);
                    const auto wp = juce::Point<float>(basePoint.x + nx * y * ampScale,
                                                       basePoint.y + ny * y * ampScale);

                    if (!started)
                    {
                        waveformPath.startNewSubPath(wp);
                        started = true;
                    }
                    else
                    {
                        waveformPath.lineTo(wp);
                    }
                }

                const auto oppositeHue = std::fmod(trackColour.getHue() + 0.5f, 1.0f);
                const auto waveformCore = juce::Colour::fromHSV(oppositeHue, 0.18f, 0.99f, 1.0f);
                const auto waveformMid = juce::Colour::fromHSV(oppositeHue, 0.55f, 0.96f, 1.0f);
                const auto waveformOutline = juce::Colours::black;

                g.setColour(waveformOutline.withAlpha(selected ? 0.82f : 0.68f));
                g.strokePath(waveformPath, juce::PathStrokeType(selected ? 2.8f : 2.2f));
                g.setColour(waveformMid.withAlpha(selected ? 0.95f : 0.82f));
                g.strokePath(waveformPath, juce::PathStrokeType(selected ? 1.8f : 1.4f));
                g.setColour(waveformCore.withAlpha(selected ? 1.0f : 0.92f));
                g.strokePath(waveformPath, juce::PathStrokeType(1.0f));
            }
        }

        if (isLineSelected)
        {
            const auto start = line.screenLine.getStart();
            const auto end = line.screenLine.getEnd();
            g.setColour(juce::Colours::black.withAlpha(0.85f));
            g.fillEllipse(start.x - 6.5f, start.y - 6.5f, 13.0f, 13.0f);
            g.fillEllipse(end.x - 6.5f, end.y - 6.5f, 13.0f, 13.0f);
            g.setColour(juce::Colours::white.withAlpha(0.98f));
            g.fillEllipse(start.x - 4.2f, start.y - 4.2f, 8.4f, 8.4f);
            g.fillEllipse(end.x - 4.2f, end.y - 4.2f, 8.4f, 8.4f);

            const auto mid = (start + end) * 0.5f;
            const auto tangent = end - start;
            const auto len = std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y);
            const auto nx = len > 0.0001f ? -tangent.y / len : 0.0f;
            const auto ny = len > 0.0001f ? tangent.x / len : -1.0f;

            const auto trackCol = colourForTrack(state);
            const auto highContrast = trackCol.getPerceivedBrightness() > 0.55f
                ? juce::Colour::fromRGB(20, 26, 34)
                : juce::Colour::fromRGB(244, 248, 255);
            const auto accent = trackCol.getPerceivedBrightness() > 0.55f
                ? juce::Colour::fromRGB(0, 190, 255)
                : juce::Colour::fromRGB(255, 210, 40);
            const auto base = mid + juce::Point<float>(nx * 10.0f, ny * 10.0f);
            const auto dist = 36.0f + juce::jlimit(0.0f, 1.6f, line.volume) * 68.0f;
            const auto knob = mid + juce::Point<float>(nx * dist, ny * dist);

            g.setColour(juce::Colours::black.withAlpha(0.65f));
            g.drawLine(mid.x, mid.y, knob.x, knob.y, 4.0f);
            g.setColour(accent.withAlpha(0.98f));
            g.drawLine(base.x, base.y, knob.x, knob.y, 2.0f);
            g.setColour(juce::Colours::black.withAlpha(0.9f));
            g.fillEllipse(knob.x - 8.5f, knob.y - 8.5f, 17.0f, 17.0f);
            g.setColour(highContrast.withAlpha(1.0f));
            g.fillEllipse(knob.x - 6.2f, knob.y - 6.2f, 12.4f, 12.4f);
            g.setColour(accent.withAlpha(1.0f));
            g.drawEllipse(knob.x - 6.2f, knob.y - 6.2f, 12.4f, 12.4f, 1.6f);
            g.setColour(highContrast.contrasting(1.0f).withAlpha(0.95f));
            g.drawFittedText(juce::String(line.volume, 2) + "x",
                             juce::Rectangle<int>(static_cast<int>(knob.x + 8.0f),
                                                  static_cast<int>(knob.y - 8.0f),
                                                  46,
                                                  16),
                             juce::Justification::centredLeft,
                             1);
        }
    }

    if (dragMode == DragMode::drawingNew && selected)
    {
        g.setColour(juce::Colours::white.withAlpha(0.9f));
        const auto ds = snapPoint(dragStart);
        const auto dc = snapPoint(dragCurrent);
        g.drawLine(ds.x, ds.y, dc.x, dc.y, 1.2f);
    }

    const auto playhead = snapPoint(pointAtPhase(playheadPhase));
    g.setColour(playheadColour.withAlpha(selected ? 0.98f : 0.86f));
    g.fillEllipse(playhead.x - 7.0f, playhead.y - 7.0f, 14.0f, 14.0f);
    g.setColour(juce::Colours::black.withAlpha(0.96f));
    g.fillEllipse(playhead.x - 3.2f, playhead.y - 3.2f, 6.4f, 6.4f);
    g.setColour(juce::Colours::white.withAlpha(selected ? 0.86f : 0.58f));
    g.drawEllipse(playhead.x - 7.8f, playhead.y - 7.8f, 15.6f, 15.6f, 1.1f);
}

void SpiralTrackComponent::resized()
{
}

void SpiralTrackComponent::mouseDown(const juce::MouseEvent& event)
{
    if (state.hidden)
        return;

    if (onSelected)
        onSelected();

    if (!selected)
        return;

    grabKeyboardFocus();

    if (event.mods.isRightButtonDown())
    {
        dragMode = DragMode::panningView;
        lastDragPoint = event.position;
        return;
    }

    const auto worldPos = toWorldPoint(event.position);

    const auto volumeHandleIndex = findLineVolumeHandleAtPoint(worldPos, 11.0f / viewScale);
    if (volumeHandleIndex >= 0)
    {
        selectedLineIndex = volumeHandleIndex;
        dragMode = DragMode::draggingVolumeHandle;
        lastDragPoint = worldPos;
        repaint();
        return;
    }

    bool startHandle = false;
    const auto handleIndex = findLineHandleAtPoint(worldPos, 10.0f / viewScale, startHandle);
    if (handleIndex >= 0)
    {
        selectedLineIndex = handleIndex;
        dragMode = startHandle ? DragMode::draggingStartHandle : DragMode::draggingEndHandle;
        lastDragPoint = worldPos;
        repaint();
        return;
    }

    const auto lineIndex = findLineAtPoint(worldPos, 8.0f / viewScale);
    if (lineIndex >= 0)
    {
        selectedLineIndex = lineIndex;
        dragMode = DragMode::movingLine;
        lastDragPoint = worldPos;
        repaint();
        return;
    }

    selectedLineIndex = -1;

    if (event.mods.isLeftButtonDown() && event.getNumberOfClicks() >= 2)
    {
        dragMode = DragMode::drawingNew;
        dragStart = worldPos;
        dragCurrent = worldPos;
    }
    else
    {
        dragMode = DragMode::none;
    }

    repaint();
}

void SpiralTrackComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (dragMode == DragMode::panningView)
    {
        const auto deltaScreen = event.position - lastDragPoint;
        viewOffset += deltaScreen;
        lastDragPoint = event.position;
        notifyViewChanged();
        repaint();
        return;
    }

    const auto worldPos = toWorldPoint(event.position);

    if (dragMode == DragMode::drawingNew)
    {
        dragCurrent = worldPos;
        repaint();
        return;
    }

    if (!juce::isPositiveAndBelow(selectedLineIndex, state.lines.size()))
        return;

    auto& line = state.lines.getReference(selectedLineIndex);

    if (dragMode == DragMode::movingLine)
    {
        const auto delta = worldPos - lastDragPoint;
        line.screenLine = juce::Line<float>(line.screenLine.getStart() + delta, line.screenLine.getEnd() + delta);
        lastDragPoint = worldPos;
        updateLineTriggerPhases(selectedLineIndex);
        if (onChanged)
            onChanged();
        repaint();
        return;
    }

    if (dragMode == DragMode::draggingStartHandle)
    {
        line.screenLine = juce::Line<float>(worldPos, line.screenLine.getEnd());
        updateLineTriggerPhases(selectedLineIndex);
        if (onChanged)
            onChanged();
        repaint();
        return;
    }

    if (dragMode == DragMode::draggingEndHandle)
    {
        line.screenLine = juce::Line<float>(line.screenLine.getStart(), worldPos);
        updateLineTriggerPhases(selectedLineIndex);
        if (onChanged)
            onChanged();
        repaint();
        return;
    }

    if (dragMode == DragMode::draggingVolumeHandle)
    {
        const auto start = line.screenLine.getStart();
        const auto end = line.screenLine.getEnd();
        const auto mid = (start + end) * 0.5f;
        const auto tangent = end - start;
        const auto len = std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y);
        const auto nx = len > 0.0001f ? -tangent.y / len : 0.0f;
        const auto ny = len > 0.0001f ? tangent.x / len : -1.0f;
        const auto rel = worldPos - mid;
        const auto projected = rel.x * nx + rel.y * ny;
        line.volume = juce::jlimit(0.0f, 1.6f, (projected - 36.0f) / 68.0f);
        if (onChanged)
            onChanged();
        repaint();
    }
}

void SpiralTrackComponent::mouseUp(const juce::MouseEvent& event)
{
    if (state.hidden)
        return;

    if (dragMode == DragMode::movingLine
        || dragMode == DragMode::draggingStartHandle
        || dragMode == DragMode::draggingEndHandle
        || dragMode == DragMode::draggingVolumeHandle)
    {
        dragMode = DragMode::none;
        return;
    }

    if (dragMode == DragMode::panningView)
    {
        dragMode = DragMode::none;
        return;
    }

    if (dragMode != DragMode::drawingNew)
        return;

    dragMode = DragMode::none;
    dragCurrent = toWorldPoint(event.position);

    const auto drawnLine = juce::Line<float>(dragStart, dragCurrent);
    const auto pyFolder = juce::File::getCurrentWorkingDirectory().getChildFile("py");
    const auto initialBrowsePath = pyFolder.isDirectory() ? pyFolder : juce::File();
    pendingFileChooser = std::make_unique<juce::FileChooser>("Choose a Python synth script", initialBrowsePath, "*.py");
    pendingFileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                    [this, drawnLine](const juce::FileChooser& chooser)
                                    {
                                        const auto selectedFile = chooser.getResult();

                                        if (!selectedFile.existsAsFile())
                                        {
                                            pendingFileChooser.reset();
                                            repaint();
                                            return;
                                        }

                                        juce::PopupMenu menu;
                                        menu.addItem(1, "Single Hit (natural length)");
                                        menu.addItem(2, "Bar (1 bar)");
                                        menu.addItem(3, "Bar (cut to next bar end)");

                                        menu.showMenuAsync(juce::PopupMenu::Options(),
                                                           [this, drawnLine, selectedPath = selectedFile.getFullPathName()](int choice)
                                                           {
                                                               if (choice <= 0)
                                                               {
                                                                   pendingFileChooser.reset();
                                                                   repaint();
                                                                   return;
                                                               }
                                                               auto* fadeAlert = new juce::AlertWindow("Line Fades",
                                                                                                       "Set fade times in ms (0 disables).",
                                                                                                       juce::AlertWindow::NoIcon);
                                                               fadeAlert->addTextEditor("fadeInMs", "0", "Fade In (ms)");
                                                               fadeAlert->addTextEditor("fadeOutMs", "0", "Fade Out (ms)");
                                                               fadeAlert->addButton("Place", 1);
                                                               fadeAlert->addButton("Cancel", 0);

                                                               fadeAlert->enterModalState(true,
                                                                                          juce::ModalCallbackFunction::create(
                                                                                              [this, drawnLine, selectedPath, choice, fadeAlert](int result)
                                                                                              {
                                                                                                  if (result != 1)
                                                                                                  {
                                                                                                      pendingFileChooser.reset();
                                                                                                      repaint();
                                                                                                      return;
                                                                                                  }

                                                                                                  const auto fadeInMs = juce::jmax(0.0f, static_cast<float>(fadeAlert->getTextEditorContents("fadeInMs").getDoubleValue()));
                                                                                                  const auto fadeOutMs = juce::jmax(0.0f, static_cast<float>(fadeAlert->getTextEditorContents("fadeOutMs").getDoubleValue()));

                                                                                                  TriggerLineData line;
                                                                                                  line.lineId = juce::Uuid().toString();
                                                                                                  line.screenLine = drawnLine;
                                                                                                  line.pythonScriptPath = selectedPath;
                                                                                                  line.scriptType = (choice == 1)
                                                                                                      ? TriggerLineData::ScriptType::singleHit
                                                                                                      : TriggerLineData::ScriptType::bar;
                                                                                                  line.cutToBarEnd = (choice == 3);
                                                                                                  line.volume = 1.0f;
                                                                                                  line.fadeInMs = fadeInMs;
                                                                                                  line.fadeOutMs = fadeOutMs;
                                                                                                  line.triggerPhases = computeIntersectionsForLine(line.screenLine);

                                                                                                  if (line.triggerPhases.isEmpty())
                                                                                                  {
                                                                                                      const auto midpoint = (drawnLine.getStart() + drawnLine.getEnd()) * 0.5f;
                                                                                                      line.triggerPhases.add(nearestPhaseToPoint(midpoint));
                                                                                                  }

                                                                                                  state.lines.add(std::move(line));
                                                                                                  selectedLineIndex = state.lines.size() - 1;

                                                                                                  if (onChanged)
                                                                                                      onChanged();

                                                                                                  pendingFileChooser.reset();
                                                                                                  repaint();
                                                                                              }),
                                                                                          true);
                                                           });
                                    });
    repaint();
}

void SpiralTrackComponent::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (state.hidden || !selected)
        return;

    const auto oldScale = viewScale;
    const auto factor = wheel.deltaY > 0.0f ? 1.1f : 0.9f;
    viewScale = juce::jlimit(0.2f, 5.0f, oldScale * factor);

    const auto mouse = event.position;
    const auto worldBefore = (mouse - viewOffset) / oldScale;
    viewOffset = mouse - worldBefore * viewScale;

    notifyViewChanged();
    repaint();
}

bool SpiralTrackComponent::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::backspaceKey || key == juce::KeyPress::deleteKey)
    {
        if (juce::isPositiveAndBelow(selectedLineIndex, state.lines.size()))
        {
            state.lines.remove(selectedLineIndex);
            selectedLineIndex = -1;
            if (onChanged)
                onChanged();
            repaint();
            return true;
        }
    }

    return false;
}

juce::Rectangle<float> SpiralTrackComponent::spiralBounds() const
{
    return getLocalBounds().toFloat().reduced(20.0f);
}

juce::Array<SpiralTrackComponent::SpiralSample> SpiralTrackComponent::buildSpiralSamples(int sampleCount) const
{
    juce::Array<SpiralSample> out;
    out.ensureStorageAllocated(sampleCount + 1);

    const auto sb = spiralBounds();
    const auto center = sb.getCentre() + juce::Point<float>(static_cast<float>(state.xOffset),
                                                             static_cast<float>(state.yOffset));

    const auto geometry = geometryForTrack(state);

    const auto maxRadius = juce::jmin(sb.getWidth(), sb.getHeight()) * geometry.outerRadiusScale;
    const auto innerRadius = maxRadius * geometry.innerRadiusScale;
    const auto warpAmount = static_cast<float>(juce::jlimit(0.0, 1.0, state.orbitWarpAmount));
    const auto twistAmount = static_cast<float>(juce::jlimit(-1.0, 1.0, state.spiralTwistAmount));
    const auto phaseOffsetTurns = state.phaseOffsetDegrees / 360.0;
    const auto xRadians = static_cast<float>(state.xRotationDegrees * juce::MathConstants<double>::pi / 180.0);
    const auto yRadians = static_cast<float>(state.yRotationDegrees * juce::MathConstants<double>::pi / 180.0);
    const auto yScale = std::cos(xRadians);
    const auto xScale = std::cos(yRadians);

    for (int i = 0; i <= sampleCount; ++i)
    {
        const auto phase = static_cast<double>(i) / static_cast<double>(sampleCount);
        const auto theta = static_cast<float>((phase + phaseOffsetTurns) * static_cast<double>(geometry.turns) * twoPi);

        const auto shaped = std::pow(static_cast<float>(phase), geometry.shapeExponent);
        const auto thetaTwisted = theta;
        const auto radiusBase = innerRadius + shaped * (maxRadius - innerRadius);
        const auto warp = 1.0f + warpAmount * std::sin(orbitWarpHarmonic * thetaTwisted);
        const auto radius = juce::jmax(innerRadius * 0.15f, radiusBase * warp);

        auto local = juce::Point<float>(radius * std::cos(thetaTwisted) * xScale,
                                        radius * std::sin(thetaTwisted) * yScale);
        const auto bend = twistAmount * (0.35f + 0.75f * shaped);
        const auto yNorm = local.y / juce::jmax(1.0f, maxRadius);
        const auto xNorm = local.x / juce::jmax(1.0f, maxRadius);
        local.x += bend * yNorm * std::abs(yNorm) * maxRadius;
        local.y *= 1.0f - 0.22f * std::abs(bend) * xNorm * xNorm;
        juce::Point<float> p(center.x + local.x, center.y + local.y);

        out.add({ p, phase });
    }

    return out;
}

juce::Point<float> SpiralTrackComponent::pointAtPhase(double phase) const
{
    const auto sb = spiralBounds();
    const auto center = sb.getCentre() + juce::Point<float>(static_cast<float>(state.xOffset),
                                                             static_cast<float>(state.yOffset));

    const auto geometry = geometryForTrack(state);

    const auto maxRadius = juce::jmin(sb.getWidth(), sb.getHeight()) * geometry.outerRadiusScale;
    const auto innerRadius = maxRadius * geometry.innerRadiusScale;
    const auto warpAmount = static_cast<float>(juce::jlimit(0.0, 1.0, state.orbitWarpAmount));
    const auto twistAmount = static_cast<float>(juce::jlimit(-1.0, 1.0, state.spiralTwistAmount));

    const auto wrappedBase = std::fmod(phase, 1.0);
    const auto base = wrappedBase < 0.0 ? wrappedBase + 1.0 : wrappedBase;

    const auto wrappedAngle = std::fmod(phase + (state.phaseOffsetDegrees / 360.0), 1.0);
    const auto anglePhase = wrappedAngle < 0.0 ? wrappedAngle + 1.0 : wrappedAngle;

    const auto theta = static_cast<float>(anglePhase * static_cast<double>(geometry.turns) * twoPi);
    const auto shaped = std::pow(static_cast<float>(base), geometry.shapeExponent);
    const auto thetaTwisted = theta;
    const auto radiusBase = innerRadius + shaped * (maxRadius - innerRadius);
    const auto warp = 1.0f + warpAmount * std::sin(orbitWarpHarmonic * thetaTwisted);
    const auto radius = juce::jmax(innerRadius * 0.15f, radiusBase * warp);
    const auto xRadians = static_cast<float>(state.xRotationDegrees * juce::MathConstants<double>::pi / 180.0);
    const auto yRadians = static_cast<float>(state.yRotationDegrees * juce::MathConstants<double>::pi / 180.0);
    const auto yScale = std::cos(xRadians);
    const auto xScale = std::cos(yRadians);
    auto local = juce::Point<float>(radius * std::cos(thetaTwisted) * xScale,
                                    radius * std::sin(thetaTwisted) * yScale);
    const auto bend = twistAmount * (0.35f + 0.75f * shaped);
    const auto yNorm = local.y / juce::jmax(1.0f, maxRadius);
    const auto xNorm = local.x / juce::jmax(1.0f, maxRadius);
    local.x += bend * yNorm * std::abs(yNorm) * maxRadius;
    local.y *= 1.0f - 0.22f * std::abs(bend) * xNorm * xNorm;
    return { center.x + local.x, center.y + local.y };
}

juce::Array<double> SpiralTrackComponent::computeIntersectionsForLine(const juce::Line<float>& line) const
{
    auto samples = buildSpiralSamples(1800);
    juce::Array<double> intersections;

    for (int i = 1; i < samples.size(); ++i)
    {
        juce::Point<float> hit;
        juce::Line<float> segment(samples.getReference(i - 1).point, samples.getReference(i).point);

        if (!segment.intersects(line, hit))
            continue;

        const auto segmentLength = segment.getLength();

        double frac = 0.0;
        if (segmentLength > 0.0001f)
            frac = hit.getDistanceFrom(segment.getStart()) / segmentLength;

        const auto hitPhase = juce::jlimit(0.0, 1.0,
            juce::jmap(frac, samples.getReference(i - 1).phase, samples.getReference(i).phase));

        bool duplicate = false;
        for (auto p : intersections)
        {
            if (std::abs(p - hitPhase) < 0.002)
            {
                duplicate = true;
                break;
            }
        }

        if (!duplicate)
            intersections.add(hitPhase);
    }

    std::sort(intersections.begin(), intersections.end());
    return intersections;
}

double SpiralTrackComponent::nearestPhaseToPoint(juce::Point<float> point) const
{
    const auto samples = buildSpiralSamples(1200);
    float bestDistance = std::numeric_limits<float>::max();
    double bestPhase = 0.0;

    for (const auto& sample : samples)
    {
        const auto d = sample.point.getDistanceFrom(point);
        if (d < bestDistance)
        {
            bestDistance = d;
            bestPhase = sample.phase;
        }
    }

    return bestPhase;
}

int SpiralTrackComponent::findLineAtPoint(juce::Point<float> point, float tolerance) const
{
    for (int i = state.lines.size(); --i >= 0;)
    {
        const auto& line = state.lines.getReference(i).screenLine;
        const auto nearest = line.findNearestPointTo(point);
        if (nearest.getDistanceFrom(point) <= tolerance)
            return i;
    }
    return -1;
}

int SpiralTrackComponent::findLineHandleAtPoint(juce::Point<float> point, float radius, bool& isStartHandle) const
{
    for (int i = state.lines.size(); --i >= 0;)
    {
        const auto& line = state.lines.getReference(i).screenLine;
        if (line.getStart().getDistanceFrom(point) <= radius)
        {
            isStartHandle = true;
            return i;
        }
        if (line.getEnd().getDistanceFrom(point) <= radius)
        {
            isStartHandle = false;
            return i;
        }
    }
    return -1;
}

int SpiralTrackComponent::findLineVolumeHandleAtPoint(juce::Point<float> point, float radius) const
{
    for (int i = state.lines.size(); --i >= 0;)
    {
        const auto& l = state.lines.getReference(i);
        const auto start = l.screenLine.getStart();
        const auto end = l.screenLine.getEnd();
        const auto mid = (start + end) * 0.5f;
        const auto tangent = end - start;
        const auto len = std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y);
        const auto nx = len > 0.0001f ? -tangent.y / len : 0.0f;
        const auto ny = len > 0.0001f ? tangent.x / len : -1.0f;
        const auto dist = 36.0f + juce::jlimit(0.0f, 1.6f, l.volume) * 68.0f;
        const auto knob = mid + juce::Point<float>(nx * dist, ny * dist);
        if (knob.getDistanceFrom(point) <= radius)
            return i;
    }
    return -1;
}

void SpiralTrackComponent::updateLineTriggerPhases(int lineIndex)
{
    if (!juce::isPositiveAndBelow(lineIndex, state.lines.size()))
        return;

    auto& line = state.lines.getReference(lineIndex);
    line.triggerPhases = computeIntersectionsForLine(line.screenLine);
    if (line.triggerPhases.isEmpty())
        line.triggerPhases.add(nearestPhaseToPoint((line.screenLine.getStart() + line.screenLine.getEnd()) * 0.5f));
}

void SpiralTrackComponent::applyWaveformPreview(const SpiralSequencerEngine::LineWaveformPreview& preview)
{
    for (auto& line : state.lines)
    {
        if (line.lineId == preview.lineId)
        {
            line.waveformPreview = preview.samples;
            line.waveformDurationSeconds = preview.durationSeconds;
            repaint();
            return;
        }
    }
}

void SpiralTrackComponent::flashLine(const juce::String& lineId)
{
    if (lineId.isEmpty())
        return;

    for (auto& flash : lineFlashes)
    {
        if (flash.lineId == lineId)
        {
            flash.amount = 1.0f;
            repaint();
            return;
        }
    }

    LineFlashState flash;
    flash.lineId = lineId;
    flash.amount = 1.0f;
    lineFlashes.add(std::move(flash));
    repaint();
}

float SpiralTrackComponent::flashAmountForLineId(const juce::String& lineId) const
{
    for (const auto& flash : lineFlashes)
    {
        if (flash.lineId == lineId)
            return flash.amount;
    }

    return 0.0f;
}

void SpiralTrackComponent::decayLineFlashes()
{
    for (int i = lineFlashes.size(); --i >= 0;)
    {
        auto& flash = lineFlashes.getReference(i);
        flash.amount = juce::jmax(0.0f, flash.amount - 0.14f);
        if (flash.amount <= 0.0001f)
            lineFlashes.remove(i);
    }
}

juce::Point<float> SpiralTrackComponent::toWorldPoint(juce::Point<float> screenPoint) const
{
    return (screenPoint - viewOffset) / viewScale;
}

void SpiralTrackComponent::notifyViewChanged()
{
    if (onViewChanged)
        onViewChanged(viewScale, viewOffset);
}

MainComponent::MainComponent()
{
    setSize(1300, 840);
    setWantsKeyboardFocus(true);

    auto setupError = audioDeviceManager.initialise(0, 2, nullptr, true);
    if (setupError.isNotEmpty())
        juce::Logger::writeToLog("Audio init error: " + setupError);

    for (const auto& midiDevice : juce::MidiInput::getAvailableDevices())
    {
        audioDeviceManager.setMidiInputDeviceEnabled(midiDevice.identifier, true);
        audioDeviceManager.addMidiInputDeviceCallback(midiDevice.identifier, this);
    }

    engine = std::make_unique<SpiralSequencerEngine>(audioDeviceManager);
    triggerOscSocket = std::make_unique<juce::DatagramSocket>();
    triggerOscEnabled = triggerOscSocket->bindToPort(0);
    if (!triggerOscEnabled)
        juce::Logger::writeToLog("OSC: failed to open UDP socket");

    const auto panelText = juce::Colour::fromRGB(228, 236, 248);
    const auto panelSubtleText = juce::Colour::fromRGB(168, 182, 202);
    const auto controlBg = juce::Colour::fromRGB(22, 30, 43);
    const auto controlOutline = juce::Colour::fromRGB(58, 76, 104);
    const auto accent = juce::Colour::fromRGB(84, 194, 255);

    addAndMakeVisible(addTrackButton);
    addTrackButton.onClick = [this]() { addTrack(); };

    addAndMakeVisible(resetButton);
    resetButton.onClick = [this]()
    {
        if (engine != nullptr)
        {
            const bool nextPlaying = !engine->isPlaying();
            engine->setPlaying(nextPlaying);

            if (!nextPlaying)
                engine->resetTransport();

            resetButton.setButtonText(nextPlaying ? "Stop" : "Play");
        }
    };

    addAndMakeVisible(saveProjectButton);
    saveProjectButton.onClick = [this]() { saveProject(); };

    addAndMakeVisible(loadProjectButton);
    loadProjectButton.onClick = [this]() { loadProject(); };

    addAndMakeVisible(exportWavButton);
    exportWavButton.onClick = [this]() { exportWav(); };

    addAndMakeVisible(audioSettingsButton);
    audioSettingsButton.onClick = [this]() { openAudioSettings(); };

    addAndMakeVisible(collapseSettingsButton);
    collapseSettingsButton.onClick = [this]()
    {
        setTrackSettingsCollapsed(!trackSettingsCollapsed);
    };

    auto styleActionButton = [&](juce::TextButton& b, juce::Colour colour)
    {
        b.setColour(juce::TextButton::buttonColourId, colour.withAlpha(0.90f));
        b.setColour(juce::TextButton::buttonOnColourId, colour.brighter(0.14f).withAlpha(0.96f));
        b.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.96f));
        b.setColour(juce::TextButton::textColourOnId, juce::Colours::white.withAlpha(1.0f));
    };

    styleActionButton(addTrackButton, juce::Colour::fromRGB(32, 92, 138));
    styleActionButton(resetButton, juce::Colour::fromRGB(28, 132, 88));
    styleActionButton(saveProjectButton, juce::Colour::fromRGB(68, 83, 108));
    styleActionButton(loadProjectButton, juce::Colour::fromRGB(68, 83, 108));
    styleActionButton(exportWavButton, juce::Colour::fromRGB(120, 88, 34));
    styleActionButton(audioSettingsButton, juce::Colour::fromRGB(76, 60, 118));
    styleActionButton(collapseSettingsButton, juce::Colour::fromRGB(52, 72, 100));

    addAndMakeVisible(selectedTrackLabel);
    selectedTrackLabel.setJustificationType(juce::Justification::centredLeft);
    selectedTrackLabel.setText("Selected: Track 1", juce::dontSendNotification);
    selectedTrackLabel.setColour(juce::Label::textColourId, panelText);

    addAndMakeVisible(tracksLabel);
    tracksLabel.setText("Tracks", juce::dontSendNotification);
    tracksLabel.setJustificationType(juce::Justification::centredLeft);
    tracksLabel.setColour(juce::Label::textColourId, panelText);

    addAndMakeVisible(tracksViewport);
    tracksViewport.setViewedComponent(&tracksListContent, false);
    tracksViewport.setScrollBarsShown(true, false);
    tracksViewport.setScrollBarThickness(10);

    auto configureSlider = [this, panelText, controlBg, controlOutline, accent](juce::Slider& s,
                                                                                 juce::Label& l,
                                                                                 const juce::String& title,
                                                                                 double min,
                                                                                 double max,
                                                                                 double step)
    {
        addAndMakeVisible(s);
        addAndMakeVisible(l);

        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 68, 20);
        s.setRange(min, max, step);
        s.setSkewFactorFromMidPoint((min + max) * 0.5);
        s.setColour(juce::Slider::backgroundColourId, controlBg);
        s.setColour(juce::Slider::trackColourId, accent.withAlpha(0.92f));
        s.setColour(juce::Slider::thumbColourId, juce::Colours::white.withAlpha(0.95f));
        s.setColour(juce::Slider::textBoxBackgroundColourId, controlBg.brighter(0.06f));
        s.setColour(juce::Slider::textBoxOutlineColourId, controlOutline.withAlpha(0.85f));
        s.setColour(juce::Slider::textBoxTextColourId, panelText);
        s.onValueChange = [this]() { updateSelectedTrackFromInspector(); };

        l.setText(title, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centredLeft);
        l.setColour(juce::Label::textColourId, panelText);
    };

    configureSlider(bpmSlider, bpmLabel, "Tempo (BPM)", 20.0, 320.0, 0.1);
    configureSlider(numeratorSlider, numeratorLabel, "Time Sig Numerator", 1.0, 15.0, 1.0);
    configureSlider(denominatorSlider, denominatorLabel, "Time Sig Denominator", 1.0, 16.0, 1.0);
    configureSlider(loopBarsSlider, loopBarsLabel, "Loop Duration (Bars)", 0.25, 64.0, 0.25);
    configureSlider(thicknessSlider, thicknessLabel, "Spiral Thickness", 0.25, 4.0, 0.05);
    configureSlider(orbitWarpSlider, orbitWarpLabel, "Orbit Warp", 0.0, 1.0, 0.01);
    configureSlider(spiralTwistSlider, spiralTwistLabel, "Spiral Bend", -1.0, 1.0, 0.01);
    configureSlider(phaseSlider, phaseLabel, "Phase (Degrees)", 0.0, 360.0, 1.0);
    configureSlider(xRotationSlider, xRotationLabel, "X Rotation (deg)", -85.0, 85.0, 1.0);
    configureSlider(yRotationSlider, yRotationLabel, "Y Rotation (deg)", -85.0, 85.0, 1.0);
    configureSlider(xOffsetSlider, xOffsetLabel, "X Offset (px)", -400.0, 400.0, 1.0);
    configureSlider(yOffsetSlider, yOffsetLabel, "Y Offset (px)", -400.0, 400.0, 1.0);

    addAndMakeVisible(infoLabel);
    infoLabel.setText("", juce::dontSendNotification);
    infoLabel.setColour(juce::Label::textColourId, panelSubtleText);

    setTrackSettingsCollapsed(false);

    addTrack();
    addTrack();

    startTimerHz(30);
    grabKeyboardFocus();
}

MainComponent::~MainComponent()
{
    for (const auto& midiDevice : juce::MidiInput::getAvailableDevices())
        audioDeviceManager.removeMidiInputDeviceCallback(midiDevice.identifier, this);

    stopTimer();
}

void MainComponent::paint(juce::Graphics& g)
{
    juce::ColourGradient gradient(juce::Colour::fromRGB(9, 12, 18), 0.0f, 0.0f,
                                  juce::Colour::fromRGB(18, 25, 36), 0.0f, static_cast<float>(getHeight()), false);
    g.setGradientFill(gradient);
    g.fillAll();

    auto layout = getLocalBounds().reduced(12).toFloat();
    auto topBarBounds = layout.removeFromTop(36.0f);
    layout.removeFromTop(8.0f);
    auto inspectorBounds = layout.removeFromRight(360.0f);

    g.setColour(juce::Colour::fromRGB(18, 24, 34).withAlpha(0.92f));
    g.fillRoundedRectangle(topBarBounds, 10.0f);
    g.setColour(juce::Colour::fromRGB(74, 92, 120).withAlpha(0.55f));
    g.drawRoundedRectangle(topBarBounds.reduced(0.5f), 10.0f, 1.0f);

    juce::ColourGradient inspectorFill(juce::Colour::fromRGB(20, 28, 40).withAlpha(0.95f),
                                       inspectorBounds.getX(),
                                       inspectorBounds.getY(),
                                       juce::Colour::fromRGB(14, 20, 30).withAlpha(0.92f),
                                       inspectorBounds.getRight(),
                                       inspectorBounds.getBottom(),
                                       false);
    g.setGradientFill(inspectorFill);
    g.fillRoundedRectangle(inspectorBounds, 14.0f);
    g.setColour(juce::Colour::fromRGB(72, 94, 124).withAlpha(0.55f));
    g.drawRoundedRectangle(inspectorBounds.reduced(0.5f), 14.0f, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.drawLine(inspectorBounds.getX() + 14.0f,
               inspectorBounds.getY() + 34.0f,
               inspectorBounds.getRight() - 14.0f,
               inspectorBounds.getY() + 34.0f,
               1.0f);

    const auto canvasBounds = getLocalBounds().reduced(12).withTrimmedTop(44).withTrimmedRight(368).toFloat();
    juce::ColourGradient canvasGlow(juce::Colour::fromRGB(28, 42, 58).withAlpha(0.24f),
                                    canvasBounds.getCentreX(),
                                    canvasBounds.getCentreY(),
                                    juce::Colour::fromRGB(8, 12, 18).withAlpha(0.05f),
                                    canvasBounds.getRight(),
                                    canvasBounds.getBottom(),
                                    true);
    g.setGradientFill(canvasGlow);
    g.fillRoundedRectangle(canvasBounds.expanded(6.0f), 18.0f);

    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.fillRoundedRectangle(canvasBounds, 14.0f);

    juce::ColourGradient vignette(juce::Colours::transparentBlack,
                                  canvasBounds.getCentreX(),
                                  canvasBounds.getCentreY(),
                                  juce::Colours::black.withAlpha(0.35f),
                                  canvasBounds.getRight(),
                                  canvasBounds.getBottom(),
                                  true);
    g.setGradientFill(vignette);
    g.fillRoundedRectangle(canvasBounds, 14.0f);

    g.setColour(juce::Colour::fromRGB(92, 116, 148).withAlpha(0.38f));
    g.drawRoundedRectangle(canvasBounds.reduced(0.5f), 14.0f, 1.0f);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(12);

    auto topBar = area.removeFromTop(36);
    addTrackButton.setBounds(topBar.removeFromLeft(120));
    topBar.removeFromLeft(8);
    resetButton.setBounds(topBar.removeFromLeft(90));
    topBar.removeFromLeft(12);
    saveProjectButton.setBounds(topBar.removeFromLeft(112));
    topBar.removeFromLeft(6);
    loadProjectButton.setBounds(topBar.removeFromLeft(112));
    topBar.removeFromLeft(6);
    exportWavButton.setBounds(topBar.removeFromLeft(96));
    topBar.removeFromLeft(6);
    audioSettingsButton.setBounds(topBar.removeFromLeft(118));
    topBar.removeFromLeft(10);
    selectedTrackLabel.setBounds(topBar);

    area.removeFromTop(8);

    auto inspector = area.removeFromRight(360);
    auto rows = inspector.reduced(10);

    auto placeRow = [&rows](juce::Label& label, juce::Slider& slider)
    {
        auto row = rows.removeFromTop(42);
        label.setBounds(row.removeFromTop(16));
        slider.setBounds(row.removeFromTop(22));
        rows.removeFromTop(3);
    };

    auto placePairRow = [&rows](juce::Label& leftLabel, juce::Slider& leftSlider,
                                juce::Label& rightLabel, juce::Slider& rightSlider)
    {
        auto row = rows.removeFromTop(42);
        auto left = row.removeFromLeft((row.getWidth() - 10) / 2);
        row.removeFromLeft(10);
        auto right = row;

        leftLabel.setBounds(left.removeFromTop(16));
        leftSlider.setBounds(left.removeFromTop(22));

        rightLabel.setBounds(right.removeFromTop(16));
        rightSlider.setBounds(right.removeFromTop(22));

        rows.removeFromTop(3);
    };

    collapseSettingsButton.setBounds(rows.removeFromTop(24));
    rows.removeFromTop(4);

    if (!trackSettingsCollapsed)
    {
        placePairRow(bpmLabel, bpmSlider, numeratorLabel, numeratorSlider);
        placePairRow(denominatorLabel, denominatorSlider, loopBarsLabel, loopBarsSlider);
        placePairRow(thicknessLabel, thicknessSlider, orbitWarpLabel, orbitWarpSlider);
        placePairRow(spiralTwistLabel, spiralTwistSlider, phaseLabel, phaseSlider);
        placePairRow(xRotationLabel, xRotationSlider, yRotationLabel, yRotationSlider);
        placePairRow(xOffsetLabel, xOffsetSlider, yOffsetLabel, yOffsetSlider);
    }

    rows.removeFromTop(8);

    tracksLabel.setBounds(rows.removeFromTop(22));
    rows.removeFromTop(4);

    const bool hasInfoText = infoLabel.getText().trim().isNotEmpty();
    auto infoArea = hasInfoText ? rows.removeFromBottom(64) : juce::Rectangle<int>();
    tracksViewport.setBounds(rows);

    const int rowHeight = 27;
    const int contentHeight = juce::jmax(tracksViewport.getHeight(), selectTrackButtons.size() * rowHeight + 2);
    const int contentWidth = juce::jmax(300, tracksViewport.getWidth() - 12);
    tracksListContent.setSize(contentWidth, contentHeight);

    for (int i = 0; i < selectTrackButtons.size(); ++i)
    {
        auto row = juce::Rectangle<int>(0, i * rowHeight, contentWidth, 24);
        constexpr int gap = 6;
        const int selectW = juce::jlimit(84, 120, static_cast<int>(contentWidth * 0.30f));
        const int remaining = juce::jmax(180, contentWidth - selectW - (gap * 4));
        const int toggleW = juce::jmax(44, remaining / 4);
        const int divisionW = juce::jmax(52, remaining - (toggleW * 3));

        selectTrackButtons[i]->setBounds(row.removeFromLeft(selectW));
        row.removeFromLeft(gap);
        hideButtons[i]->setBounds(row.removeFromLeft(toggleW));
        row.removeFromLeft(gap);
        muteButtons[i]->setBounds(row.removeFromLeft(toggleW));
        row.removeFromLeft(gap);
        if (i < syncButtons.size())
            syncButtons[i]->setBounds(row.removeFromLeft(toggleW));
        row.removeFromLeft(gap);
        if (i < syncDivisionBoxes.size())
            syncDivisionBoxes[i]->setBounds(row.removeFromLeft(divisionW));
    }

    infoLabel.setVisible(hasInfoText);
    if (hasInfoText)
        infoLabel.setBounds(infoArea);

    auto canvas = area.reduced(2);
    for (int i = 0; i < trackComponents.size(); ++i)
        trackComponents[i]->setBounds(canvas);
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    const auto c = key.getTextCharacter();
    if (c == 'o' || c == 'O')
    {
        openOscSettings();
        return true;
    }
    if (c == 'm' || c == 'M')
    {
        openMidiSettings();
        return true;
    }

    return false;
}

void MainComponent::setTrackSettingsCollapsed(bool shouldCollapse)
{
    trackSettingsCollapsed = shouldCollapse;
    collapseSettingsButton.setButtonText(trackSettingsCollapsed ? "Expand Settings" : "Collapse Settings");

    const bool show = !trackSettingsCollapsed;
    bpmLabel.setVisible(show); bpmSlider.setVisible(show);
    numeratorLabel.setVisible(show); numeratorSlider.setVisible(show);
    denominatorLabel.setVisible(show); denominatorSlider.setVisible(show);
    loopBarsLabel.setVisible(show); loopBarsSlider.setVisible(show);
    thicknessLabel.setVisible(show); thicknessSlider.setVisible(show);
    orbitWarpLabel.setVisible(show); orbitWarpSlider.setVisible(show);
    spiralTwistLabel.setVisible(show); spiralTwistSlider.setVisible(show);
    phaseLabel.setVisible(show); phaseSlider.setVisible(show);
    xRotationLabel.setVisible(show); xRotationSlider.setVisible(show);
    yRotationLabel.setVisible(show); yRotationSlider.setVisible(show);
    xOffsetLabel.setVisible(show); xOffsetSlider.setVisible(show);
    yOffsetLabel.setVisible(show); yOffsetSlider.setVisible(show);

    resized();
    repaint();
}

void MainComponent::timerCallback()
{
    if (engine == nullptr)
        return;

    double syncedBpm = 0.0;
    bool hasMidiSync = false;
    {
        const juce::ScopedLock lock(midiClockLock);
        const auto nowSec = juce::Time::getMillisecondCounterHiRes() * 0.001;
        hasMidiSync = midiClockRunning
            && midiClockBpm > 0.0
            && (nowSec - lastMidiClockArrivalSec) < 1.0;
        syncedBpm = midiClockBpm;
    }

    if (hasMidiSync && trackComponents.size() > 0)
    {
        for (int i = 0; i < trackComponents.size(); ++i)
        {
            auto trackState = trackComponents[i]->getState();
            if (!trackState.syncToMidiClock)
                continue;

            const auto division = juce::jlimit(1, 16, trackState.midiSyncDivision);
            const auto targetBpm = syncedBpm / static_cast<double>(division);

            if (std::abs(trackState.bpm - targetBpm) > 0.02)
            {
                trackState.bpm = targetBpm;
                trackComponents[i]->setState(trackState);
                engine->setTrackTempo(i, targetBpm);
                if (selectedTrackIndex == i)
                    updateInspectorFromSelectedTrack();
            }
        }
    }

    const auto phases = engine->getCurrentPhases();
    const auto previews = engine->getWaveformPreviews();
    const auto triggers = engine->consumeRecentTriggerEvents();

    for (int i = 0; i < trackComponents.size() && i < phases.size(); ++i)
        trackComponents[i]->setPlayheadPhase(phases[i]);

    for (const auto& preview : previews)
    {
        for (auto* track : trackComponents)
            track->applyWaveformPreview(preview);
    }

    for (const auto& trigger : triggers)
    {
        if (!juce::isPositiveAndBelow(trigger.trackIndex, trackComponents.size()))
            continue;

        trackComponents[trigger.trackIndex]->flashLine(trigger.lineId);

        if (triggerOscEnabled && triggerOscSocket != nullptr)
        {
            const int trackNumber = trigger.trackIndex + 1;
            const int lineNumber = trigger.lineIndex + 1;
            juce::MemoryOutputStream oscPacket;
            buildOscTriggerMessage(oscPacket, trackNumber, lineNumber);
            triggerOscSocket->write(oscHost,
                                    oscPort,
                                    oscPacket.getData(),
                                    static_cast<int>(oscPacket.getDataSize()));
        }
    }
}

void MainComponent::handleIncomingMidiMessage(juce::MidiInput* /*source*/, const juce::MidiMessage& message)
{
    const auto nowSec = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const juce::ScopedLock lock(midiClockLock);

    if (message.isMidiStart() || message.isMidiContinue())
    {
        midiClockRunning = true;
        lastMidiClockTimeSec = -1.0;
        return;
    }

    if (message.isMidiStop())
    {
        midiClockRunning = false;
        return;
    }

    if (!message.isMidiClock())
        return;

    midiClockRunning = true;
    lastMidiClockArrivalSec = nowSec;

    if (lastMidiClockTimeSec < 0.0)
    {
        lastMidiClockTimeSec = nowSec;
        return;
    }

    const auto dt = nowSec - lastMidiClockTimeSec;
    lastMidiClockTimeSec = nowSec;

    if (dt < 0.0005 || dt > 0.2)
        return;

    if (smoothedMidiClockIntervalSec <= 0.0)
        smoothedMidiClockIntervalSec = dt;
    else
        smoothedMidiClockIntervalSec = (smoothedMidiClockIntervalSec * 0.88) + (dt * 0.12);

    const auto bpm = 60.0 / (smoothedMidiClockIntervalSec * 24.0);
    if (bpm >= 20.0 && bpm <= 320.0)
        midiClockBpm = bpm;
}

void MainComponent::mouseUp(const juce::MouseEvent& event)
{
    if (!event.mods.isRightButtonDown())
        return;

    auto* button = dynamic_cast<juce::TextButton*>(event.eventComponent);
    if (button == nullptr)
        return;

    const int index = selectTrackButtons.indexOf(button);
    if (index < 0)
        return;

    juce::PopupMenu menu;
    menu.addItem(2, "Duplicate Track");
    if (trackComponents.size() > 1)
        menu.addItem(1, "Delete Track");
    else
        menu.addItem(1, "Delete Track", false, false);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(button),
                       [this, index](int result)
                       {
                           if (result == 1)
                               deleteTrack(index);
                           else if (result == 2)
                               duplicateTrack(index);
                       });
}

void MainComponent::addTrack()
{
    SpiralTrackState state;
    state.name = "Track " + juce::String(trackComponents.size() + 1);
    state.colourIndex = trackComponents.size();
    state.bpm = 120.0 + trackComponents.size() * 6.0;
    state.loopBars = 2.0 + (trackComponents.size() % 3);

    auto* track = trackComponents.add(new SpiralTrackComponent(state));
    addAndMakeVisible(track);

    auto* selectButton = selectTrackButtons.add(new juce::TextButton(state.name));
    selectButton->addMouseListener(this, false);
    tracksListContent.addAndMakeVisible(selectButton);

    auto* hideButton = hideButtons.add(new juce::ToggleButton("Hide"));
    hideButton->setColour(juce::ToggleButton::textColourId, juce::Colour::fromRGB(216, 225, 238));
    hideButton->setColour(juce::ToggleButton::tickColourId, juce::Colour::fromRGB(92, 180, 246));
    hideButton->setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour::fromRGB(74, 88, 108));
    tracksListContent.addAndMakeVisible(hideButton);

    auto* muteButton = muteButtons.add(new juce::ToggleButton("Mute"));
    muteButton->setColour(juce::ToggleButton::textColourId, juce::Colour::fromRGB(216, 225, 238));
    muteButton->setColour(juce::ToggleButton::tickColourId, juce::Colour::fromRGB(255, 160, 110));
    muteButton->setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour::fromRGB(74, 88, 108));
    tracksListContent.addAndMakeVisible(muteButton);

    auto* syncButton = syncButtons.add(new juce::ToggleButton("Sync"));
    syncButton->setColour(juce::ToggleButton::textColourId, juce::Colour::fromRGB(216, 225, 238));
    syncButton->setColour(juce::ToggleButton::tickColourId, juce::Colour::fromRGB(120, 230, 150));
    syncButton->setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour::fromRGB(74, 88, 108));
    tracksListContent.addAndMakeVisible(syncButton);

    auto* syncDivision = syncDivisionBoxes.add(new juce::ComboBox());
    for (int d = 1; d <= 16; ++d)
        syncDivision->addItem("/" + juce::String(d), d);
    syncDivision->setSelectedId(1, juce::dontSendNotification);
    syncDivision->setColour(juce::ComboBox::backgroundColourId, juce::Colour::fromRGB(26, 34, 48));
    syncDivision->setColour(juce::ComboBox::outlineColourId, juce::Colour::fromRGB(74, 88, 108));
    syncDivision->setColour(juce::ComboBox::textColourId, juce::Colour::fromRGB(216, 225, 238));
    syncDivision->setColour(juce::ComboBox::arrowColourId, juce::Colour::fromRGB(216, 225, 238));
    tracksListContent.addAndMakeVisible(syncDivision);

    track->setViewTransform(viewScale, viewOffset);
    rebindTrackCallbacks();

    if (trackComponents.size() == 1)
        selectedTrackIndex = 0;

    setSelectedTrackIndex(selectedTrackIndex);
    syncEngineTracks();
    updateInspectorFromSelectedTrack();
    resized();
}

void MainComponent::deleteTrack(int index)
{
    if (!juce::isPositiveAndBelow(index, trackComponents.size()))
        return;

    if (trackComponents.size() <= 1)
        return;

    if (juce::isPositiveAndBelow(index, selectTrackButtons.size()))
        selectTrackButtons[index]->removeMouseListener(this);

    trackComponents.remove(index, true);
    selectTrackButtons.remove(index, true);
    hideButtons.remove(index, true);
    muteButtons.remove(index, true);
    syncButtons.remove(index, true);
    syncDivisionBoxes.remove(index, true);

    if (selectedTrackIndex == index)
        selectedTrackIndex = juce::jmax(0, index - 1);
    else if (selectedTrackIndex > index)
        --selectedTrackIndex;

    if (!juce::isPositiveAndBelow(selectedTrackIndex, trackComponents.size()))
        selectedTrackIndex = juce::jmax(0, trackComponents.size() - 1);

    rebindTrackCallbacks();
    setSelectedTrackIndex(selectedTrackIndex);
    syncEngineTracks();
    resized();
}

void MainComponent::duplicateTrack(int index)
{
    if (!juce::isPositiveAndBelow(index, trackComponents.size()))
        return;

    auto source = trackComponents[index]->getState();
    source.name = source.name + " Copy";
    source.colourIndex = trackComponents.size();

    for (auto& line : source.lines)
        line.lineId = juce::Uuid().toString();

    auto* track = new SpiralTrackComponent(source);
    track->setViewTransform(viewScale, viewOffset);
    trackComponents.insert(index + 1, track);
    addAndMakeVisible(track);

    auto* selectButton = new juce::TextButton(source.name);
    selectButton->addMouseListener(this, false);
    selectTrackButtons.insert(index + 1, selectButton);
    tracksListContent.addAndMakeVisible(selectButton);

    auto* hideButton = new juce::ToggleButton("Hide");
    hideButton->setColour(juce::ToggleButton::textColourId, juce::Colour::fromRGB(216, 225, 238));
    hideButton->setColour(juce::ToggleButton::tickColourId, juce::Colour::fromRGB(92, 180, 246));
    hideButton->setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour::fromRGB(74, 88, 108));
    hideButton->setToggleState(source.hidden, juce::dontSendNotification);
    hideButtons.insert(index + 1, hideButton);
    tracksListContent.addAndMakeVisible(hideButton);

    auto* muteButton = new juce::ToggleButton("Mute");
    muteButton->setColour(juce::ToggleButton::textColourId, juce::Colour::fromRGB(216, 225, 238));
    muteButton->setColour(juce::ToggleButton::tickColourId, juce::Colour::fromRGB(255, 160, 110));
    muteButton->setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour::fromRGB(74, 88, 108));
    muteButton->setToggleState(source.muted, juce::dontSendNotification);
    muteButtons.insert(index + 1, muteButton);
    tracksListContent.addAndMakeVisible(muteButton);

    auto* syncButton = new juce::ToggleButton("Sync");
    syncButton->setColour(juce::ToggleButton::textColourId, juce::Colour::fromRGB(216, 225, 238));
    syncButton->setColour(juce::ToggleButton::tickColourId, juce::Colour::fromRGB(120, 230, 150));
    syncButton->setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour::fromRGB(74, 88, 108));
    syncButton->setToggleState(source.syncToMidiClock, juce::dontSendNotification);
    syncButtons.insert(index + 1, syncButton);
    tracksListContent.addAndMakeVisible(syncButton);

    auto* syncDivision = new juce::ComboBox();
    for (int d = 1; d <= 16; ++d)
        syncDivision->addItem("/" + juce::String(d), d);
    syncDivision->setSelectedId(juce::jlimit(1, 16, source.midiSyncDivision), juce::dontSendNotification);
    syncDivision->setColour(juce::ComboBox::backgroundColourId, juce::Colour::fromRGB(26, 34, 48));
    syncDivision->setColour(juce::ComboBox::outlineColourId, juce::Colour::fromRGB(74, 88, 108));
    syncDivision->setColour(juce::ComboBox::textColourId, juce::Colour::fromRGB(216, 225, 238));
    syncDivision->setColour(juce::ComboBox::arrowColourId, juce::Colour::fromRGB(216, 225, 238));
    syncDivisionBoxes.insert(index + 1, syncDivision);
    tracksListContent.addAndMakeVisible(syncDivision);

    rebindTrackCallbacks();
    setSelectedTrackIndex(index + 1);
    syncEngineTracks();
    resized();
}

void MainComponent::rebindTrackCallbacks()
{
    for (int index = 0; index < trackComponents.size(); ++index)
    {
        auto* track = trackComponents[index];
        auto* selectButton = selectTrackButtons[index];
        auto* hideButton = hideButtons[index];
        auto* muteButton = muteButtons[index];
        auto* syncButton = syncButtons[index];
        auto* syncDivisionBox = syncDivisionBoxes[index];

        hideButton->setColour(juce::ToggleButton::textColourId, juce::Colour::fromRGB(216, 225, 238));
        hideButton->setColour(juce::ToggleButton::tickColourId, juce::Colour::fromRGB(92, 180, 246));
        hideButton->setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour::fromRGB(74, 88, 108));
        muteButton->setColour(juce::ToggleButton::textColourId, juce::Colour::fromRGB(216, 225, 238));
        muteButton->setColour(juce::ToggleButton::tickColourId, juce::Colour::fromRGB(255, 160, 110));
        muteButton->setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour::fromRGB(74, 88, 108));
        syncButton->setColour(juce::ToggleButton::textColourId, juce::Colour::fromRGB(216, 225, 238));
        syncButton->setColour(juce::ToggleButton::tickColourId, juce::Colour::fromRGB(120, 230, 150));
        syncButton->setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour::fromRGB(74, 88, 108));
        syncDivisionBox->setColour(juce::ComboBox::backgroundColourId, juce::Colour::fromRGB(26, 34, 48));
        syncDivisionBox->setColour(juce::ComboBox::outlineColourId, juce::Colour::fromRGB(74, 88, 108));
        syncDivisionBox->setColour(juce::ComboBox::textColourId, juce::Colour::fromRGB(216, 225, 238));
        syncDivisionBox->setColour(juce::ComboBox::arrowColourId, juce::Colour::fromRGB(216, 225, 238));

        track->setViewChangedCallback([this](float scale, juce::Point<float> offset)
        {
            viewScale = scale;
            viewOffset = offset;
            applyViewToAllTracks();
        });

        track->setChangeCallback([this]()
        {
            syncEngineTracks();
        });

        track->setSelectCallback([this, index]()
        {
            setSelectedTrackIndex(index);
        });

        selectButton->onClick = [this, index]()
        {
            setSelectedTrackIndex(index);
        };

        hideButton->onClick = [this, index]()
        {
            if (!juce::isPositiveAndBelow(index, trackComponents.size()))
                return;

            auto updated = trackComponents[index]->getState();
            updated.hidden = hideButtons[index]->getToggleState();
            trackComponents[index]->setState(updated);

            if (updated.hidden && selectedTrackIndex == index)
                setSelectedTrackIndex(juce::jmax(0, index - 1));

            syncEngineTracks();
            refreshTrackControlStates();
        };

        muteButton->onClick = [this, index]()
        {
            if (!juce::isPositiveAndBelow(index, trackComponents.size()))
                return;

            auto updated = trackComponents[index]->getState();
            updated.muted = muteButtons[index]->getToggleState();
            trackComponents[index]->setState(updated);
            syncEngineTracks();
            refreshTrackControlStates();
        };

        syncButton->onClick = [this, index]()
        {
            if (!juce::isPositiveAndBelow(index, trackComponents.size()))
                return;

            auto updated = trackComponents[index]->getState();
            updated.syncToMidiClock = syncButtons[index]->getToggleState();
            trackComponents[index]->setState(updated);
            syncEngineTracks();
            refreshTrackControlStates();
        };

        syncDivisionBox->onChange = [this, index]()
        {
            if (!juce::isPositiveAndBelow(index, trackComponents.size()))
                return;

            auto updated = trackComponents[index]->getState();
            updated.midiSyncDivision = juce::jlimit(1, 16, syncDivisionBoxes[index]->getSelectedId());
            trackComponents[index]->setState(updated);
            syncEngineTracks();
            refreshTrackControlStates();
        };
    }
}

void MainComponent::syncEngineTracks()
{
    juce::Array<SpiralTrackState> states;
    states.ensureStorageAllocated(trackComponents.size());

    for (auto* track : trackComponents)
        states.add(track->getState());

    if (engine != nullptr)
        engine->setTracks(std::move(states));
}

double MainComponent::loopDurationSeconds(const SpiralTrackState& track) const
{
    const auto beatsPerBar = track.timeSigNumerator * (4.0 / juce::jmax(1, track.timeSigDenominator));
    const auto totalBeats = beatsPerBar * juce::jmax(0.25, track.loopBars);
    return totalBeats * (60.0 / juce::jmax(1.0, track.bpm));
}

juce::var MainComponent::trackToVar(const SpiralTrackState& track) const
{
    auto obj = std::make_unique<juce::DynamicObject>();
    obj->setProperty("name", track.name);
    obj->setProperty("colourIndex", track.colourIndex);
    obj->setProperty("bpm", track.bpm);
    obj->setProperty("timeSigNumerator", track.timeSigNumerator);
    obj->setProperty("timeSigDenominator", track.timeSigDenominator);
    obj->setProperty("loopBars", track.loopBars);
    obj->setProperty("thickness", track.thickness);
    obj->setProperty("orbitWarpAmount", track.orbitWarpAmount);
    obj->setProperty("spiralTwistAmount", track.spiralTwistAmount);
    obj->setProperty("phaseOffsetDegrees", track.phaseOffsetDegrees);
    obj->setProperty("xRotationDegrees", track.xRotationDegrees);
    obj->setProperty("yRotationDegrees", track.yRotationDegrees);
    obj->setProperty("xOffset", track.xOffset);
    obj->setProperty("yOffset", track.yOffset);
    obj->setProperty("hidden", track.hidden);
    obj->setProperty("muted", track.muted);
    obj->setProperty("syncToMidiClock", track.syncToMidiClock);
    obj->setProperty("midiSyncDivision", track.midiSyncDivision);

    juce::Array<juce::var> lines;
    for (const auto& line : track.lines)
    {
        auto l = std::make_unique<juce::DynamicObject>();
        l->setProperty("lineId", line.lineId);
        l->setProperty("pythonScriptPath", line.pythonScriptPath);
        l->setProperty("scriptType", line.scriptType == TriggerLineData::ScriptType::bar ? "bar" : "single_hit");
        l->setProperty("cutToBarEnd", line.cutToBarEnd);
        l->setProperty("volume", line.volume);
        l->setProperty("fadeInMs", line.fadeInMs);
        l->setProperty("fadeOutMs", line.fadeOutMs);
        l->setProperty("x1", line.screenLine.getStartX());
        l->setProperty("y1", line.screenLine.getStartY());
        l->setProperty("x2", line.screenLine.getEndX());
        l->setProperty("y2", line.screenLine.getEndY());
        lines.add(juce::var(l.release()));
    }
    obj->setProperty("lines", juce::var(lines));

    return juce::var(obj.release());
}

bool MainComponent::trackFromVar(const juce::var& v, SpiralTrackState& out) const
{
    if (!v.isObject())
        return false;

    const auto* o = v.getDynamicObject();
    if (o == nullptr)
        return false;

    out.name = o->getProperty("name").toString();
    out.colourIndex = static_cast<int>(o->getProperty("colourIndex"));
    out.bpm = static_cast<double>(o->getProperty("bpm"));
    out.timeSigNumerator = static_cast<int>(o->getProperty("timeSigNumerator"));
    out.timeSigDenominator = static_cast<int>(o->getProperty("timeSigDenominator"));
    out.loopBars = static_cast<double>(o->getProperty("loopBars"));
    out.thickness = static_cast<double>(o->getProperty("thickness"));
    out.orbitWarpAmount = o->hasProperty("orbitWarpAmount") ? static_cast<double>(o->getProperty("orbitWarpAmount")) : 0.0;
    out.spiralTwistAmount = o->hasProperty("spiralTwistAmount") ? static_cast<double>(o->getProperty("spiralTwistAmount")) : 0.0;
    out.phaseOffsetDegrees = static_cast<double>(o->getProperty("phaseOffsetDegrees"));
    if (o->hasProperty("xRotationDegrees"))
        out.xRotationDegrees = static_cast<double>(o->getProperty("xRotationDegrees"));
    else if (o->hasProperty("zRotationDegrees"))
        out.xRotationDegrees = static_cast<double>(o->getProperty("zRotationDegrees"));
    else
        out.xRotationDegrees = 0.0;
    out.yRotationDegrees = o->hasProperty("yRotationDegrees") ? static_cast<double>(o->getProperty("yRotationDegrees")) : 0.0;
    out.xOffset = static_cast<double>(o->getProperty("xOffset"));
    out.yOffset = static_cast<double>(o->getProperty("yOffset"));
    out.hidden = static_cast<bool>(o->getProperty("hidden"));
    out.muted = static_cast<bool>(o->getProperty("muted"));
    out.syncToMidiClock = o->hasProperty("syncToMidiClock") ? static_cast<bool>(o->getProperty("syncToMidiClock")) : false;
    out.midiSyncDivision = o->hasProperty("midiSyncDivision") ? static_cast<int>(o->getProperty("midiSyncDivision")) : 1;

    out.bpm = juce::jlimit(1.0, 500.0, out.bpm);
    out.timeSigNumerator = juce::jlimit(1, 32, out.timeSigNumerator);
    out.timeSigDenominator = juce::jlimit(1, 32, out.timeSigDenominator);
    out.loopBars = juce::jmax(0.25, out.loopBars);
    out.thickness = juce::jlimit(0.25, 4.0, out.thickness);
    out.orbitWarpAmount = juce::jlimit(0.0, 1.0, out.orbitWarpAmount);
    out.spiralTwistAmount = juce::jlimit(-1.0, 1.0, out.spiralTwistAmount);
    out.xRotationDegrees = juce::jlimit(-85.0, 85.0, out.xRotationDegrees);
    out.yRotationDegrees = juce::jlimit(-85.0, 85.0, out.yRotationDegrees);
    out.midiSyncDivision = juce::jlimit(1, 16, out.midiSyncDivision);

    out.lines.clear();
    const auto linesVar = o->getProperty("lines");
    if (linesVar.isArray())
    {
        const auto* arr = linesVar.getArray();
        if (arr != nullptr)
        {
            for (const auto& lv : *arr)
            {
                if (!lv.isObject())
                    continue;
                const auto* lo = lv.getDynamicObject();
                if (lo == nullptr)
                    continue;

                TriggerLineData line;
                line.lineId = lo->getProperty("lineId").toString();
                line.pythonScriptPath = lo->getProperty("pythonScriptPath").toString();
                const auto typeStr = lo->getProperty("scriptType").toString();
                line.scriptType = (typeStr == "bar") ? TriggerLineData::ScriptType::bar
                                                      : TriggerLineData::ScriptType::singleHit;
                line.cutToBarEnd = static_cast<bool>(lo->getProperty("cutToBarEnd"));
                line.volume = lo->hasProperty("volume")
                    ? static_cast<float>(double(lo->getProperty("volume")))
                    : 1.0f;
                line.fadeInMs = lo->hasProperty("fadeInMs")
                    ? static_cast<float>(double(lo->getProperty("fadeInMs")))
                    : 0.0f;
                line.fadeOutMs = lo->hasProperty("fadeOutMs")
                    ? static_cast<float>(double(lo->getProperty("fadeOutMs")))
                    : 0.0f;
                line.volume = juce::jlimit(0.0f, 1.6f, line.volume);
                line.fadeInMs = juce::jlimit(0.0f, 10000.0f, line.fadeInMs);
                line.fadeOutMs = juce::jlimit(0.0f, 10000.0f, line.fadeOutMs);
                line.screenLine = juce::Line<float>(
                    static_cast<float>(double(lo->getProperty("x1"))),
                    static_cast<float>(double(lo->getProperty("y1"))),
                    static_cast<float>(double(lo->getProperty("x2"))),
                    static_cast<float>(double(lo->getProperty("y2"))));
                out.lines.add(std::move(line));
            }
        }
    }

    return true;
}

void MainComponent::saveProject()
{
    pendingProjectChooser = std::make_unique<juce::FileChooser>("Save Orbits Project", juce::File(), "*.orbits.json");
    pendingProjectChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                                       [this](const juce::FileChooser& chooser)
                                       {
                                           const auto file = chooser.getResult();
                                           if (file == juce::File())
                                           {
                                               pendingProjectChooser.reset();
                                               return;
                                           }

                                           juce::Array<juce::var> tracksVar;
                                           for (auto* track : trackComponents)
                                               tracksVar.add(trackToVar(track->getState()));

                                           auto root = std::make_unique<juce::DynamicObject>();
                                           root->setProperty("version", 1);
                                           root->setProperty("tracks", juce::var(tracksVar));
                                           root->setProperty("viewScale", viewScale);
                                           root->setProperty("viewOffsetX", viewOffset.x);
                                           root->setProperty("viewOffsetY", viewOffset.y);

                                           const auto json = juce::JSON::toString(juce::var(root.release()), true);
                                           file.replaceWithText(json);
                                           pendingProjectChooser.reset();
                                       });
}

void MainComponent::loadProject()
{
    pendingProjectChooser = std::make_unique<juce::FileChooser>("Load Orbits Project", juce::File(), "*.orbits.json");
    pendingProjectChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                       [this](const juce::FileChooser& chooser)
                                       {
                                           const auto file = chooser.getResult();
                                           if (file == juce::File())
                                           {
                                               pendingProjectChooser.reset();
                                               return;
                                           }

                                           const auto text = file.loadFileAsString();
                                           auto parsed = juce::JSON::parse(text);
                                           if (!parsed.isObject())
                                           {
                                               pendingProjectChooser.reset();
                                               return;
                                           }

                                           const auto* root = parsed.getDynamicObject();
                                           if (root == nullptr)
                                           {
                                               pendingProjectChooser.reset();
                                               return;
                                           }

                                           juce::OwnedArray<SpiralTrackComponent> newTracks;
                                           juce::OwnedArray<juce::TextButton> newSelectButtons;
                                           juce::OwnedArray<juce::ToggleButton> newHideButtons;
                                           juce::OwnedArray<juce::ToggleButton> newMuteButtons;
                                           juce::OwnedArray<juce::ToggleButton> newSyncButtons;
                                           juce::OwnedArray<juce::ComboBox> newSyncDivisionBoxes;

                                           const auto tracksVar = root->getProperty("tracks");
                                           if (tracksVar.isArray())
                                           {
                                               const auto* arr = tracksVar.getArray();
                                               if (arr != nullptr)
                                               {
                                                   for (const auto& tv : *arr)
                                                   {
                                                       SpiralTrackState state;
                                                       if (!trackFromVar(tv, state))
                                                           continue;

                                                       auto* track = newTracks.add(new SpiralTrackComponent(state));
                                                       track->setViewTransform(viewScale, viewOffset);
                                                       newSelectButtons.add(new juce::TextButton(state.name));
                                                       newHideButtons.add(new juce::ToggleButton("Hide"));
                                                       newMuteButtons.add(new juce::ToggleButton("Mute"));
                                                       newSyncButtons.add(new juce::ToggleButton("Sync"));
                                                       auto* box = newSyncDivisionBoxes.add(new juce::ComboBox());
                                                       for (int d = 1; d <= 16; ++d)
                                                           box->addItem("/" + juce::String(d), d);
                                                       box->setSelectedId(juce::jlimit(1, 16, state.midiSyncDivision), juce::dontSendNotification);
                                                   }
                                               }
                                           }

                                           if (newTracks.isEmpty())
                                           {
                                               pendingProjectChooser.reset();
                                               return;
                                           }

                                           for (auto* b : selectTrackButtons)
                                               b->removeMouseListener(this);

                                           trackComponents.clear(true);
                                           selectTrackButtons.clear(true);
                                           hideButtons.clear(true);
                                           muteButtons.clear(true);
                                           syncButtons.clear(true);
                                           syncDivisionBoxes.clear(true);

                                           while (newTracks.size() > 0)
                                           {
                                               trackComponents.add(newTracks.removeAndReturn(0));
                                               addAndMakeVisible(trackComponents.getLast());

                                               auto* sb = newSelectButtons.removeAndReturn(0);
                                               sb->addMouseListener(this, false);
                                               selectTrackButtons.add(sb);
                                               tracksListContent.addAndMakeVisible(sb);

                                               auto* hb = newHideButtons.removeAndReturn(0);
                                               hideButtons.add(hb);
                                               tracksListContent.addAndMakeVisible(hb);

                                               auto* mb = newMuteButtons.removeAndReturn(0);
                                               muteButtons.add(mb);
                                               tracksListContent.addAndMakeVisible(mb);

                                               auto* syb = newSyncButtons.removeAndReturn(0);
                                               syncButtons.add(syb);
                                               tracksListContent.addAndMakeVisible(syb);

                                               auto* sdb = newSyncDivisionBoxes.removeAndReturn(0);
                                               syncDivisionBoxes.add(sdb);
                                               tracksListContent.addAndMakeVisible(sdb);
                                           }

                                           viewScale = static_cast<float>(double(root->getProperty("viewScale")));
                                           viewOffset = { static_cast<float>(double(root->getProperty("viewOffsetX"))),
                                                          static_cast<float>(double(root->getProperty("viewOffsetY"))) };
                                           viewScale = juce::jlimit(0.2f, 5.0f, viewScale);

                                           selectedTrackIndex = juce::jlimit(0, trackComponents.size() - 1, selectedTrackIndex);
                                           rebindTrackCallbacks();
                                           applyViewToAllTracks();
                                           setSelectedTrackIndex(selectedTrackIndex);
                                           syncEngineTracks();
                                           resized();
                                           pendingProjectChooser.reset();
                                       });
}

void MainComponent::exportWav()
{
    double suggested = 0.0;
    for (auto* track : trackComponents)
        suggested = juce::jmax(suggested, loopDurationSeconds(track->getState()));
    suggested = juce::jmax(1.0, suggested);

    auto* alert = new juce::AlertWindow("Export WAV",
                                        "How many seconds should be rendered?",
                                        juce::AlertWindow::NoIcon);
    alert->addTextEditor("seconds", juce::String(suggested, 2), "Seconds");
    alert->addButton("Export", 1);
    alert->addButton("Cancel", 0);

    alert->enterModalState(true,
                           juce::ModalCallbackFunction::create([this, alert](int result)
                           {
                               if (result != 1)
                                   return;

                               const auto seconds = alert->getTextEditorContents("seconds").getDoubleValue();
                               runExportWav(juce::jmax(0.1, seconds));
                           }),
                           true);
}

void MainComponent::runExportWav(double exportSeconds)
{
    pendingProjectChooser = std::make_unique<juce::FileChooser>("Export WAV", juce::File(), "*.wav");
    pendingProjectChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                                       [this, exportSeconds](const juce::FileChooser& chooser)
                                       {
                                           const auto exportFile = chooser.getResult();
                                           if (exportFile == juce::File())
                                           {
                                               pendingProjectChooser.reset();
                                               return;
                                           }

                                           juce::Array<SpiralTrackState> states;
                                           for (auto* track : trackComponents)
                                               states.add(track->getState());

                                           const int exportSr = 48000;
                                           const int channels = 2;
                                           const int totalSamples = static_cast<int>(std::ceil(exportSeconds * exportSr));
                                           juce::AudioBuffer<float> mix(channels, totalSamples);
                                           mix.clear();

                                           juce::AudioFormatManager fm;
                                           fm.registerBasicFormats();

                                           const auto runRender = [&](const juce::String& scriptPath,
                                                                      int trackIndex,
                                                                      double durationSeconds,
                                                                      juce::File outFile) -> juce::AudioBuffer<float>
                                           {
                                               outFile.deleteFile();
                                               juce::ChildProcess proc;

                                               const juce::StringArray argsFlags {
                                                   "/usr/bin/python3", scriptPath,
                                                   "--track", juce::String(trackIndex),
                                                   "--time", "0.0",
                                                   "--duration", juce::String(durationSeconds, 6),
                                                   "--sample-rate", juce::String(exportSr),
                                                   "--output", outFile.getFullPathName(),
                                                   "--no-play"
                                               };

                                               auto runArgs = [&](const juce::StringArray& a) -> bool
                                               {
                                                   if (!proc.start(a))
                                                       return false;
                                                   if (!proc.waitForProcessToFinish(120000))
                                                       return false;
                                                   return outFile.existsAsFile();
                                               };

                                               bool ok = runArgs(argsFlags);
                                               if (!ok)
                                               {
                                                   const juce::StringArray argsLegacy {
                                                       "/usr/bin/python3", scriptPath,
                                                       outFile.getFullPathName(),
                                                       juce::String(exportSr),
                                                       juce::String(durationSeconds, 6)
                                                   };
                                                   ok = runArgs(argsLegacy);
                                               }

                                               juce::AudioBuffer<float> rendered;
                                               if (!ok)
                                                   return rendered;

                                               std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(outFile));
                                               if (reader == nullptr)
                                                   return rendered;

                                               const int n = static_cast<int>(reader->lengthInSamples);
                                               rendered.setSize(juce::jmax(1, static_cast<int>(reader->numChannels)), n);
                                               rendered.clear();
                                               reader->read(&rendered, 0, n, 0, true, true);
                                               return rendered;
                                           };

                                           for (int trackIndex = 0; trackIndex < states.size(); ++trackIndex)
                                           {
                                               const auto& track = states.getReference(trackIndex);
                                               if (track.muted)
                                                   continue;

                                               const auto loopSeconds = loopDurationSeconds(track);
                                               const auto bars = juce::jmax(0.25, track.loopBars);
                                               const auto barSeconds = loopSeconds / bars;

                                               for (const auto& line : track.lines)
                                               {
                                                   const double renderDuration = (line.scriptType == TriggerLineData::ScriptType::bar) ? barSeconds : 3.0;
                                                   juce::File tmp = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                                                       .getChildFile("orbits_export_" + juce::Uuid().toString() + ".wav");
                                                   auto rendered = runRender(line.pythonScriptPath, trackIndex, renderDuration, tmp);
                                                   if (rendered.getNumSamples() <= 0)
                                                       continue;

                                                   for (auto phase : line.triggerPhases)
                                                   {
                                                       for (double cycleStart = 0.0; cycleStart < exportSeconds; cycleStart += loopSeconds)
                                                       {
                                                           const auto t0 = cycleStart + phase * loopSeconds;
                                                           if (t0 >= exportSeconds)
                                                               break;

                                                           int srcLimit = rendered.getNumSamples();
                                                           if (line.scriptType == TriggerLineData::ScriptType::bar && line.cutToBarEnd)
                                                           {
                                                               const auto barPos = phase * bars;
                                                               const auto fracInBar = barPos - std::floor(barPos);
                                                               const auto remain = juce::jmax(0.0, (1.0 - fracInBar) * barSeconds);
                                                               srcLimit = juce::jmin(srcLimit, static_cast<int>(remain * exportSr));
                                                           }

                                                           const int dstStart = static_cast<int>(t0 * exportSr);
                                                           const int fadeInSamples = juce::jlimit(0, srcLimit, static_cast<int>(std::round(line.fadeInMs * 0.001 * exportSr)));
                                                           const int fadeOutSamples = juce::jlimit(0, srcLimit, static_cast<int>(std::round(line.fadeOutMs * 0.001 * exportSr)));
                                                           for (int i = 0; i < srcLimit; ++i)
                                                           {
                                                               const int dst = dstStart + i;
                                                               if (dst >= totalSamples)
                                                                   break;

                                                               float env = 1.0f;
                                                               if (fadeInSamples > 0 && i < fadeInSamples)
                                                                   env *= static_cast<float>(i) / static_cast<float>(juce::jmax(1, fadeInSamples));
                                                               if (fadeOutSamples > 0 && i >= srcLimit - fadeOutSamples)
                                                               {
                                                                   const auto outIdx = srcLimit - 1 - i;
                                                                   env *= static_cast<float>(juce::jmax(0, outIdx)) / static_cast<float>(juce::jmax(1, fadeOutSamples));
                                                               }

                                                               for (int ch = 0; ch < channels; ++ch)
                                                               {
                                                                   const int srcCh = juce::jmin(ch, rendered.getNumChannels() - 1);
                                                                   mix.addSample(ch, dst, rendered.getSample(srcCh, i) * (0.7f * juce::jlimit(0.0f, 1.6f, line.volume) * env));
                                                               }
                                                           }
                                                       }
                                                   }
                                               }
                                           }

                                           float peak = 0.0f;
                                           for (int ch = 0; ch < channels; ++ch)
                                               peak = juce::jmax(peak, mix.getMagnitude(ch, 0, mix.getNumSamples()));
                                           if (peak > 0.98f)
                                               mix.applyGain(0.98f / peak);

                                           juce::WavAudioFormat wav;
                                           std::unique_ptr<juce::FileOutputStream> out(exportFile.createOutputStream());
                                           if (out == nullptr)
                                           {
                                               pendingProjectChooser.reset();
                                               return;
                                           }

                                           std::unique_ptr<juce::AudioFormatWriter> writer(
                                               wav.createWriterFor(out.get(), exportSr, channels, 16, {}, 0));
                                           if (writer == nullptr)
                                           {
                                               pendingProjectChooser.reset();
                                               return;
                                           }
                                           out.release();
                                           writer->writeFromAudioSampleBuffer(mix, 0, mix.getNumSamples());
                                           pendingProjectChooser.reset();
                                       });
}

void MainComponent::openAudioSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent(audioDeviceManager,
                                                            0, 0, 0, 2,
                                                            true, false, true, false);
    selector->setSize(640, 420);
    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned(selector);
    opts.dialogTitle = "Audio Settings";
    opts.dialogBackgroundColour = juce::Colour::fromRGB(22, 26, 34);
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = true;
    opts.launchAsync();
}

void MainComponent::openOscSettings()
{
    auto* alert = new juce::AlertWindow("OSC Settings",
                                        "Configure destination for trigger OSC messages.",
                                        juce::AlertWindow::NoIcon);
    alert->addTextEditor("host", oscHost, "Host");
    alert->addTextEditor("port", juce::String(oscPort), "Port");
    alert->addButton("Save", 1);
    alert->addButton("Cancel", 0);

    alert->enterModalState(true,
                           juce::ModalCallbackFunction::create([this, alert](int result)
                           {
                               if (result != 1)
                                   return;

                               const auto newHost = alert->getTextEditorContents("host").trim();
                               const auto newPort = alert->getTextEditorContents("port").getIntValue();
                               if (newHost.isEmpty() || newPort <= 0 || newPort > 65535)
                               {
                                   juce::Logger::writeToLog("OSC: invalid host/port");
                                   return;
                               }

                               oscHost = newHost;
                               oscPort = newPort;
                               juce::Logger::writeToLog("OSC: destination set to " + oscHost + ":" + juce::String(oscPort));
                           }),
                           true);
}

void MainComponent::openMidiSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent(audioDeviceManager,
                                                            0, 0, 0, 0,
                                                            true, true, false, true);
    selector->setSize(560, 380);
    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned(selector);
    opts.dialogTitle = "MIDI Settings";
    opts.dialogBackgroundColour = juce::Colour::fromRGB(22, 26, 34);
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = true;
    opts.launchAsync();
}

void MainComponent::updateInspectorFromSelectedTrack()
{
    auto* track = selectedTrackComponent();
    if (track == nullptr)
        return;

    const auto& state = track->getState();

    selectedTrackLabel.setText("Selected: " + state.name
                               + " | phase " + juce::String(state.phaseOffsetDegrees, 0) + " deg"
                               + " | warp " + juce::String(state.orbitWarpAmount, 2)
                               + " | twist " + juce::String(state.spiralTwistAmount, 2)
                               + " | xRot " + juce::String(state.xRotationDegrees, 0) + " deg"
                               + " | yRot " + juce::String(state.yRotationDegrees, 0) + " deg"
                               + " | x " + juce::String(state.xOffset, 0)
                               + " | y " + juce::String(state.yOffset, 0),
                               juce::dontSendNotification);

    bpmSlider.setValue(state.bpm, juce::dontSendNotification);
    numeratorSlider.setValue(state.timeSigNumerator, juce::dontSendNotification);
    denominatorSlider.setValue(state.timeSigDenominator, juce::dontSendNotification);
    loopBarsSlider.setValue(state.loopBars, juce::dontSendNotification);
    thicknessSlider.setValue(state.thickness, juce::dontSendNotification);
    orbitWarpSlider.setValue(state.orbitWarpAmount, juce::dontSendNotification);
    spiralTwistSlider.setValue(state.spiralTwistAmount, juce::dontSendNotification);
    phaseSlider.setValue(state.phaseOffsetDegrees, juce::dontSendNotification);
    xRotationSlider.setValue(state.xRotationDegrees, juce::dontSendNotification);
    yRotationSlider.setValue(state.yRotationDegrees, juce::dontSendNotification);
    xOffsetSlider.setValue(state.xOffset, juce::dontSendNotification);
    yOffsetSlider.setValue(state.yOffset, juce::dontSendNotification);
}

void MainComponent::updateSelectedTrackFromInspector()
{
    auto* track = selectedTrackComponent();
    if (track == nullptr)
        return;

    const auto previous = track->getState();
    auto updated = previous;
    updated.bpm = bpmSlider.getValue();
    updated.timeSigNumerator = static_cast<int>(numeratorSlider.getValue());
    updated.timeSigDenominator = static_cast<int>(denominatorSlider.getValue());
    updated.loopBars = loopBarsSlider.getValue();
    updated.thickness = thicknessSlider.getValue();
    updated.orbitWarpAmount = orbitWarpSlider.getValue();
    updated.spiralTwistAmount = spiralTwistSlider.getValue();
    updated.phaseOffsetDegrees = phaseSlider.getValue();
    updated.xRotationDegrees = xRotationSlider.getValue();
    updated.yRotationDegrees = yRotationSlider.getValue();
    updated.xOffset = xOffsetSlider.getValue();
    updated.yOffset = yOffsetSlider.getValue();

    track->setState(updated);

    const bool affectsSoundOrTiming =
        std::abs(updated.bpm - previous.bpm) > 0.0001
        || updated.timeSigNumerator != previous.timeSigNumerator
        || updated.timeSigDenominator != previous.timeSigDenominator
        || std::abs(updated.loopBars - previous.loopBars) > 0.0001
        || std::abs(updated.orbitWarpAmount - previous.orbitWarpAmount) > 0.0001
        || std::abs(updated.spiralTwistAmount - previous.spiralTwistAmount) > 0.0001
        || std::abs(updated.phaseOffsetDegrees - previous.phaseOffsetDegrees) > 0.0001
        || std::abs(updated.xRotationDegrees - previous.xRotationDegrees) > 0.0001
        || std::abs(updated.yRotationDegrees - previous.yRotationDegrees) > 0.0001
        || std::abs(updated.xOffset - previous.xOffset) > 0.0001
        || std::abs(updated.yOffset - previous.yOffset) > 0.0001;

    if (affectsSoundOrTiming)
    {
        track->recomputeLineTriggers();
        syncEngineTracks();
    }

    updateInspectorFromSelectedTrack();
    refreshTrackControlStates();
}

void MainComponent::setSelectedTrackIndex(int newIndex)
{
    if (!juce::isPositiveAndBelow(newIndex, trackComponents.size()))
        return;

    selectedTrackIndex = newIndex;

    for (int i = 0; i < trackComponents.size(); ++i)
    {
        const auto hidden = trackComponents[i]->getState().hidden;
        trackComponents[i]->setSelected(i == selectedTrackIndex && !hidden);

        if (i == selectedTrackIndex)
            trackComponents[i]->toFront(false);
    }

    updateInspectorFromSelectedTrack();
    refreshTrackControlStates();
    applyViewToAllTracks();
}

void MainComponent::refreshTrackControlStates()
{
    for (int i = 0; i < trackComponents.size(); ++i)
    {
        const auto& state = trackComponents[i]->getState();

        if (i < selectTrackButtons.size())
        {
            selectTrackButtons[i]->setButtonText(state.name);
            const auto base = colourForTrack(state);
            const auto fill = (i == selectedTrackIndex ? base.brighter(0.25f) : base.withMultipliedSaturation(0.85f))
                                  .withAlpha(i == selectedTrackIndex ? 0.92f : 0.72f);
            selectTrackButtons[i]->setColour(juce::TextButton::buttonColourId, fill);
            selectTrackButtons[i]->setColour(juce::TextButton::buttonOnColourId, fill);
            selectTrackButtons[i]->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
            selectTrackButtons[i]->setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.96f));
        }

        if (i < hideButtons.size())
            hideButtons[i]->setToggleState(state.hidden, juce::dontSendNotification);

        if (i < muteButtons.size())
            muteButtons[i]->setToggleState(state.muted, juce::dontSendNotification);

        if (i < syncButtons.size())
            syncButtons[i]->setToggleState(state.syncToMidiClock, juce::dontSendNotification);

        if (i < syncDivisionBoxes.size())
        {
            syncDivisionBoxes[i]->setSelectedId(juce::jlimit(1, 16, state.midiSyncDivision), juce::dontSendNotification);
            syncDivisionBoxes[i]->setEnabled(state.syncToMidiClock);
        }
    }

    repaint();
}

void MainComponent::applyViewToAllTracks()
{
    for (auto* track : trackComponents)
        track->setViewTransform(viewScale, viewOffset);
}

SpiralTrackComponent* MainComponent::selectedTrackComponent() const
{
    if (!juce::isPositiveAndBelow(selectedTrackIndex, trackComponents.size()))
        return nullptr;

    return trackComponents[selectedTrackIndex];
}
