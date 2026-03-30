#pragma once
#include <juce_core/juce_core.h>

enum class InputType
{
    Noise,
    Midi,
    Both
};

struct BenchmarkConfig
{
    int blockSize = 512;
    int numBlocks = 10000;
    double sampleRate = 44100.0;
    int numChannels = 2;
    int numMidiNotes = 0;
    InputType inputType = InputType::Noise;
};
