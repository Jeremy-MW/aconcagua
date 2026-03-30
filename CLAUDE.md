# Aconcagua

VST3 plugin performance benchmarking tool built with JUCE 8.

## Build

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
# or --config Release
```

Executable: `build/Aconcagua_artefacts/<Config>/Aconcagua.exe`

## Dependencies

- JUCE 8.0.10 at `C:/JUCE`
- MSVC / Visual Studio 2022
- C++20

## Project Structure

- `Source/Main.cpp` — Application entry point
- `Source/MainWindow.h/cpp` — Main window with tabbed content
- `Source/PluginHost/` — Plugin loading and benchmark engine
- `Source/UI/` — Config, Results, Histogram tabs + dark theme
- `Source/Model/` — Data structs (BenchmarkConfig, BenchmarkResult) and CSV exporter

## Key Defines

- `JUCE_PLUGINHOST_VST3=1` — enables VST3 hosting

## Skills

- `/asc` — Build standalone app (Debug) and launch it
- `/vst3` — Reinstall VST3 release and resources (not applicable to this project)
