#include "IdlePump.h"
#include <chrono>
#include <thread>

IdlePump::IdlePump()
    : juce::Thread("IdlePumpThread")
{
}

IdlePump::~IdlePump()
{
    stop();
}

void IdlePump::start(juce::AudioPluginInstance* plugin,
                     double sr, int bs, int inCh, int outCh)
{
    if (isThreadRunning())
        stop();

    pluginInstance     = plugin;
    sampleRate         = sr;
    blockSize          = bs;
    numInputChannels   = inCh;
    numOutputChannels  = outCh;

    if (pluginInstance != nullptr && blockSize > 0 && sampleRate > 0.0)
        startThread(juce::Thread::Priority::high);
}

void IdlePump::stop()
{
    if (isThreadRunning())
        stopThread(2000);
    pluginInstance = nullptr;
}

void IdlePump::run()
{
    if (pluginInstance == nullptr)
        return;

    const int maxCh = juce::jmax(numInputChannels, numOutputChannels, 1);
    juce::AudioBuffer<float> buffer(maxCh, blockSize);
    juce::MidiBuffer midi;
    juce::Random random;

    // -18 dBFS RMS-ish (broadcast/mixing reference). 10^(-18/20) ≈ 0.1259.
    constexpr float amplitude = 0.125f;

    using clock = std::chrono::steady_clock;
    const auto blockDuration = std::chrono::nanoseconds(
        static_cast<long long>((static_cast<double>(blockSize) / sampleRate) * 1e9));
    auto nextBlockTime = clock::now();

    while (! threadShouldExit())
    {
        // Fill input channels with quiet noise.
        for (int ch = 0; ch < numInputChannels && ch < maxCh; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int s = 0; s < blockSize; ++s)
                data[s] = (random.nextFloat() * 2.0f - 1.0f) * amplitude;
        }
        // Output-only channels start clear each block.
        for (int ch = numInputChannels; ch < maxCh; ++ch)
            buffer.clear(ch, 0, blockSize);

        midi.clear();
        pluginInstance->processBlock(buffer, midi);

        // Pace at real-time rate so we don't burn 100% CPU.
        nextBlockTime += blockDuration;
        auto now = clock::now();
        if (now < nextBlockTime)
        {
            // Sleep in chunks so we remain responsive to thread exit.
            using namespace std::chrono;
            while (! threadShouldExit())
            {
                auto remaining = nextBlockTime - clock::now();
                if (remaining <= nanoseconds(0))
                    break;
                if (remaining > milliseconds(20))
                    std::this_thread::sleep_for(milliseconds(20));
                else
                    std::this_thread::sleep_for(remaining);
            }
        }
        else
        {
            // We fell behind real-time — resync rather than spinning.
            nextBlockTime = now;
        }
    }
}
