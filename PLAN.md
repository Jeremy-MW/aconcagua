# Aconcagua — Implementation Plan

## Project Overview

Aconcagua is a **standalone JUCE application** that acts as a minimal **VST3 host** for benchmarking plugin `processBlock` performance. The user loads a VST3 plugin, configures test parameters, presses "Go," and gets detailed timing results.

The name references the highest peak in the Americas — this is a peak-performance measurement tool.

## Technical Stack

- **Framework:** JUCE (installed at `C:/JUCE`)
- **Build system:** CMake
- **C++ standard:** C++20
- **Compiler:** MSVC / Visual Studio 2026
- **Platform:** Windows (no cross-platform targets currently)
- **Plugin format:** VST3 only

## Project Structure

```
aconcagua/
├── CMakeLists.txt
├── CLAUDE.md
├── PLAN.md                         # This file
└── Source/
    ├── Main.cpp                    # JUCEApplication entry point
    ├── MainWindow.h/cpp            # Main window hosting TabbedComponent
    ├── PluginHost/
    │   ├── PluginLoader.h/cpp      # VST3 loading via AudioPluginFormatManager
    │   └── BenchmarkEngine.h/cpp   # Dedicated thread, processBlock loop, timing
    ├── UI/
    │   ├── ConfigTab.h/cpp         # Load button, parameter controls, Go button
    │   ├── ResultsTab.h/cpp        # Stats display, multi-run comparison table
    │   ├── HistogramTab.h/cpp      # Per-block timing distribution chart
    │   └── AconcaguaLookAndFeel.h/cpp  # Dark theme
    └── Model/
        ├── BenchmarkConfig.h       # Config struct (all user-selectable parameters)
        ├── BenchmarkResult.h       # Result struct (raw timings + computed stats)
        └── ResultsExporter.h/cpp   # CSV file export
```

## User-Configurable Parameters

| Parameter        | Range / Options                              |
|------------------|----------------------------------------------|
| Block size       | 2–2000 samples (integer)                     |
| Number of blocks | User-defined integer (how many processBlock calls) |
| MIDI note count  | How many simultaneous MIDI note-ons to send   |
| Input type       | White noise, MIDI notes, or both              |
| Sample rate      | 44100, 48000, 88200, 96000, 192000 Hz (combo box) |
| Channel config   | Mono, Stereo, 5.1, 7.1 (combo box)           |

## Output / Metrics

- **Per-run stats:** Total elapsed time, average time per block, min, max, standard deviation
- **Spike detection:** Flag any block exceeding `mean + 2× std_dev` — count and list these
- **Histogram:** Visual distribution of per-block timings (rendered as a custom JUCE component)
- **Multi-run comparison:** Table accumulating results from multiple runs within a session, for side-by-side comparison across different settings or plugins
- **CSV export:** Export individual runs or the full comparison table to `.csv`

## UI Design

- **Layout:** `juce::TabbedComponent` with three tabs: Config, Results, Histogram
- **Theme:** Dark background, light text — custom `LookAndFeel_V4` subclass
- **Config Tab:** Plugin load button (opens file chooser for `.vst3`), all parameter controls (sliders/combo boxes), "Go" button to start benchmark, status label showing loaded plugin name
- **Results Tab:** Stats panel for the most recent run + scrollable comparison table for all runs in the session. Each row shows: run #, plugin name, block size, sample rate, channel config, total time, avg, min, max, std dev, spike count
- **Histogram Tab:** Custom-painted component showing distribution of per-block timings for the selected run
- **Plugin GUI:** Toggle button to open/close the loaded plugin's editor in a separate `DocumentWindow`

## Architecture Details

### PluginLoader (`PluginHost/PluginLoader.h/cpp`)
- Wraps `juce::AudioPluginFormatManager` — registers `juce::VST3PluginFormat` only
- `loadPlugin(File vst3File)` — scans the file, creates an `AudioPluginInstance`
- Calls `prepareToPlay(sampleRate, blockSize)` with the user's chosen settings
- Owns the plugin instance lifecycle (prepare → benchmark → release)
- Exposes the plugin's editor component for the optional GUI toggle

### BenchmarkEngine (`PluginHost/BenchmarkEngine.h/cpp`)
- Inherits `juce::Thread` — runs at elevated priority (`Thread::Priority::highest`)
- On `run()`:
  1. Allocates `juce::AudioBuffer<float>` sized to (numChannels × blockSize)
  2. Fills buffer with white noise if input type includes noise (using `juce::Random`)
  3. Prepares a `juce::MidiBuffer` with the configured number of note-on messages if input type includes MIDI (spread across MIDI channels/notes)
  4. Loops `numBlocks` times:
     - If white noise: refill buffer with fresh noise each block (or reuse — TBD based on overhead)
     - Record `std::chrono::high_resolution_clock::now()` before `processBlock`
     - Call `pluginInstance->processBlock(buffer, midiBuffer)`
     - Record time after, store the duration in a `std::vector<double>` (microseconds)
  5. Signals completion back to the message thread via `juce::AsyncUpdater` or similar
- **Critical:** The timing loop must have minimal overhead — no allocations, no locks, no JUCE message thread interaction during measurement

### BenchmarkResult (`Model/BenchmarkResult.h`)
- Stores: `std::vector<double> blockTimings` (microseconds per block)
- Stores: config snapshot (block size, sample rate, channel config, plugin name, note count)
- Computed on demand or at construction: total, mean, min, max, stdDev, spikeCount, spikeIndices
- Spike = any timing > `mean + 2 * stdDev`

### BenchmarkConfig (`Model/BenchmarkConfig.h`)
- Plain struct: blockSize, numBlocks, sampleRate, numChannels, numMidiNotes, inputType (enum: Noise, Midi, Both)
- Channel config maps to int: mono=1, stereo=2, 5.1=6, 7.1=8

### ResultsExporter (`Model/ResultsExporter.h/cpp`)
- `exportRun(BenchmarkResult, File)` — writes single run to CSV
- `exportComparison(std::vector<BenchmarkResult>, File)` — writes comparison table to CSV
- CSV columns: run#, plugin, blockSize, sampleRate, channels, notes, total_ms, avg_us, min_us, max_us, stddev_us, spikes

---

## Implementation Stages

### Stage 1: CMake Scaffold + Empty Window
**Status: COMPLETE**

Create the CMake project that links JUCE, compiles, and shows an empty dark-themed window.

Files to create:
- `CMakeLists.txt` — `juce_add_gui_app`, link `juce::juce_audio_utils`, `juce::juce_audio_processors`, `juce::juce_dsp`, set C++20, point to `C:/JUCE`
- `Source/Main.cpp` — `JUCEApplication` subclass, creates MainWindow
- `Source/MainWindow.h/cpp` — `DocumentWindow` subclass with a `TabbedComponent` (3 empty tabs: Config, Results, Histogram)
- `Source/UI/AconcaguaLookAndFeel.h/cpp` — `LookAndFeel_V4` dark colour scheme applied globally

Validation: Project compiles with CMake, opens a dark window with three empty tabs.

---

### Stage 2: Plugin Loading
**Status: COMPLETE**

Implement VST3 loading so the user can browse for and load a plugin.

Files to create:
- `Source/PluginHost/PluginLoader.h/cpp`

Files to modify:
- `Source/UI/ConfigTab.h/cpp` — add "Load Plugin" button and status label

Implementation:
- `PluginLoader` registers `VST3PluginFormat` with `AudioPluginFormatManager`
- Load button opens `juce::FileChooser` filtered to `*.vst3`
- On selection, `PluginLoader::loadPlugin()` scans and instantiates the plugin
- Display plugin name in status label on success, error message on failure
- Plugin is not yet prepared (that happens at benchmark time with the user's chosen sample rate / block size)

Validation: Can load a known VST3 plugin and see its name displayed. Invalid files show an error.

---

### Stage 3: Benchmark Engine
**Status: COMPLETE**

Implement the core benchmarking loop on a dedicated thread.

Files to create:
- `Source/PluginHost/BenchmarkEngine.h/cpp`
- `Source/Model/BenchmarkConfig.h`
- `Source/Model/BenchmarkResult.h`

Implementation:
- `BenchmarkEngine` inherits `juce::Thread`
- Accepts a `BenchmarkConfig` and a pointer to the loaded `AudioPluginInstance`
- On start: calls `prepareToPlay` on the plugin with configured sample rate and block size
- Runs the timing loop as described in Architecture Details above
- On completion: constructs a `BenchmarkResult`, posts it back to the message thread
- After benchmark: calls `releaseResources` on the plugin
- `BenchmarkResult` computes all stats (mean, min, max, stdDev, spikes) from the raw timings vector

Validation: Load a plugin, trigger a benchmark programmatically (hardcoded params), verify timing data is captured and stats computed correctly. Print results to `DBG()`.

---

### Stage 4: Config Tab UI
**Status: COMPLETE**

Wire up all user-configurable parameters in the Config tab.

Files to create/complete:
- `Source/UI/ConfigTab.h/cpp` — full implementation

Controls:
- "Load Plugin" button + plugin name label (from Stage 2)
- Block size: `juce::Slider` (range 2–2000, integer, default 512)
- Number of blocks: `juce::Slider` or text editor (range 1–1,000,000, default 10000)
- Sample rate: `juce::ComboBox` (44100, 48000, 88200, 96000, 192000, default 44100)
- Channel config: `juce::ComboBox` (Mono, Stereo, 5.1, 7.1, default Stereo)
- Number of MIDI notes: `juce::Slider` (range 0–128, default 0)
- Input type: `juce::ComboBox` (White Noise, MIDI, Both)
- "Go" button — disabled until a plugin is loaded, starts the benchmark
- Progress indicator during benchmark run

Validation: All controls functional, "Go" triggers benchmark with selected settings, results appear in DBG output.

---

### Stage 5: Results Tab
**Status: COMPLETE**

Display benchmark results and multi-run comparison.

Files to create:
- `Source/UI/ResultsTab.h/cpp`

Implementation:
- **Latest run panel:** Displays total time, avg, min, max, std dev, spike count for the most recent run
- **Comparison table:** `juce::TableListBox` with columns: Run #, Plugin, Block Size, Sample Rate, Channels, Notes, Total (ms), Avg (μs), Min (μs), Max (μs), StdDev (μs), Spikes
- Each completed benchmark run appends a row to the comparison table
- Clicking a row selects that run (used by histogram tab to show its distribution)
- Store `std::vector<BenchmarkResult>` in a shared model accessible to both Results and Histogram tabs

Validation: Run multiple benchmarks with different settings, verify all runs appear in the table with correct values.

---

### Stage 6: Histogram Tab
**Status: COMPLETE**

Visualize the timing distribution for a selected run.

Files to create:
- `Source/UI/HistogramTab.h/cpp`

Implementation:
- Custom `juce::Component` that paints a histogram
- X-axis: time buckets (auto-scaled based on min/max of the selected run)
- Y-axis: block count per bucket
- Highlight spike threshold line (mean + 2σ) in a contrasting color (e.g., red)
- Bars exceeding the spike threshold rendered in a different color
- Axis labels, bucket count auto-determined (e.g., 50 bins)
- Responds to row selection changes in the Results tab

Validation: Histogram renders correctly for various distributions, spike line visible, updates when selecting different runs.

---

### Stage 7: CSV Export
**Status: COMPLETE**

Files to create:
- `Source/Model/ResultsExporter.h/cpp`

Files to modify:
- `Source/UI/ResultsTab.h/cpp` — add "Export Run" and "Export All" buttons

Implementation:
- "Export Run" — saves the selected run's per-block timings + summary stats to CSV
- "Export All" — saves the comparison table to CSV
- Both open a `FileChooser` for save location
- CSV format as described in Architecture Details

Validation: Exported CSV files open correctly in Excel/Sheets with expected columns and data.

---

### Stage 8: Plugin Editor Toggle
**Status: COMPLETE**

Files to modify:
- `Source/UI/ConfigTab.h/cpp` — add "Show Plugin GUI" toggle button
- `Source/MainWindow.h/cpp` or new file — plugin editor window management

Implementation:
- Toggle button in Config tab, disabled until plugin is loaded
- Opens a separate `DocumentWindow` containing the plugin's `createEditor()` component
- Window closes when plugin is unloaded or toggle is pressed again
- Plugin editor window is non-modal, can remain open during benchmarking

Validation: Toggle opens/closes the plugin editor window. Loading a new plugin closes the old editor.

---

### Stage 9: Polish + Edge Cases
**Status: COMPLETE**

- Disable "Go" button during active benchmark, re-enable on completion
- Handle plugin load failures gracefully (clear previous plugin state, show error)
- Handle plugins that don't accept MIDI (disable MIDI options or show warning)
- Handle plugins with specific channel requirements (validate against selected config)
- Ensure `releaseResources` + `prepareToPlay` is called correctly when changing settings between runs
- Window sizing / resizable behavior
- Keyboard shortcuts (Enter = Go, Escape = cancel benchmark if running)
- About dialog with app name/version

Validation: Stress test with various plugins, edge-case block sizes (2, 2000), rapid re-runs, plugin swapping.
