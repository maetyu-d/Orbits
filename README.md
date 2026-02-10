# Orbits

A realtime JUCE app for sequencing Python-based synth scripts on spiral timelines that can be of independent tempo, time signature, and shape. Double click and drag to place a line across the playhead path of an active spiral (track), and then select the sound (.py synthesis file) that this line represents. Whenever the spiral's playhead crosses the line, the sound will be played.

![](https://github.com/maetyu-d/Orbits/blob/main/Screenshot%202026-02-10%20at%2014.00.46.png)

## What this prototype does

- Multiple spiral tracks, each with independent:
  - tempo (BPM)
  - time signature
  - loop duration in bars
- All spirals are overlaid in the same shared canvas space.
- Per-track controls:
  - `Hide` (visual only)
  - `Mute` (suppresses trigger firing)
- Spiral size/shape are derived from musical parameters (tempo + meter + loop bars).
- Draw lines across a spiral track.
- On mouse release, choose a `.py` file for that line.
- The line is intersected against the spiral path; each intersection becomes a trigger point.
- On assignment and relevant timing changes, each line's script is pre-rendered to WAV and loaded into an in-memory audio buffer.
- As each track playhead advances in the audio callback, trigger crossings play back the pre-rendered buffer immediately.

## Build (macOS)

Prereqs:
- Xcode command line tools
- CMake >= 3.22
- JUCE available via `JUCE_DIR` **or** internet access for CMake FetchContent

```bash
cd /Users/md/Downloads/Orbits
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
open build/SpiralSynthTimeline_artefacts/Release/Spiral\ Synth\ Timeline.app
```

If JUCE is installed locally:

```bash
cmake -S . -B build -DJUCE_DIR=/path/to/JUCE
```

## Usage

1. Launch app.
2. Click `Add Track` for more spiral tracks.
3. Select a track and adjust tempo/time signature/loop duration.
4. Toggle `Hide` or `Mute` per track as needed.
5. Drag a line across the selected spiral track.
6. Choose a Python script (`Scripts/example_synth.py` provided).
7. When the playhead hits trigger points, your script runs with:
   - `--track <index>`
   - `--time <globalSeconds>`
   - `--duration <trackLoopSeconds>`
   - `--sample-rate <currentDeviceSampleRate>`

## Notes

- The app currently executes scripts via `/usr/bin/python3`.
- Triggered scripts are external processes; no direct audio return path to JUCE is implemented yet.
- Scripts are re-rendered when script assignment or per-track musical timing changes.
- Audio output includes a short click on trigger to confirm timing.
