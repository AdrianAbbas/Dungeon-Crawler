#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

DungeonCrawlerProcessor::DungeonCrawlerProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "DUNGEON_CRAWLER", dc::createParameterLayout()) {
    location = parameters.getRawParameterValue(dc::ids::location);
    intensity = parameters.getRawParameterValue(dc::ids::intensity);
    mix = parameters.getRawParameterValue(dc::ids::mix);
    input = parameters.getRawParameterValue(dc::ids::input);
    output = parameters.getRawParameterValue(dc::ids::output);
    bypass = parameters.getRawParameterValue(dc::ids::bypass);
}

void DungeonCrawlerProcessor::prepareToPlay(double sampleRate, int) {
    rate = std::isfinite(sampleRate) ? juce::jlimit(8000.0, 384000.0, sampleRate) : 48000.0;
    world.prepare(rate);
    limiter.prepare(rate);
    latency = world.latencySamples();
    for (auto& d : dryDelay) d.prepare(static_cast<std::size_t>(latency + 8));
    for (auto* s : { &inputGain, &outputGain, &wetMix }) s->setTime(0.02, rate);
    inputGain.snap(dc::dsp::dbToGain(input->load()));
    outputGain.snap(dc::dsp::dbToGain(output->load()));
    wetMix.snap(mix->load());
    bypassRamp.prepare(0.025, rate);
    bypassRamp.value = bypass->load() >= 0.5f ? 1.0f : 0.0f;
    neutralRamp.prepare(0.03, rate);
    neutralRamp.value = intensity->load() <= neutralIntensity ? 1.0f : 0.0f;
    setLatencySamples(latency);
    inputLevel = 0;
    outputLevel = 0;
}

bool DungeonCrawlerProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
    // mono->mono, stereo->stereo and mono->stereo (Logic offers it on mono tracks).
    const auto in = layout.getMainInputChannelSet(), out = layout.getMainOutputChannelSet();
    const auto monoOrStereo = [](const juce::AudioChannelSet& set) {
        return set == juce::AudioChannelSet::mono() || set == juce::AudioChannelSet::stereo();
    };
    return monoOrStereo(in) && monoOrStereo(out) && in.size() <= out.size();
}

void DungeonCrawlerProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { process(buffer, false); }
void DungeonCrawlerProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { process(buffer, true); }

void DungeonCrawlerProcessor::process(juce::AudioBuffer<float>& buffer, bool forcedBypass) {
    juce::ScopedNoDenormals noDenormals;
    const auto begin = juce::Time::getHighResolutionTicks();
    double bpm = 120;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto tempo = pos->getBpm())
                if (std::isfinite(*tempo)) bpm = juce::jlimit(20.0, 400.0, *tempo);

    // Inputs and outputs are counted separately: with mono->stereo the right output
    // carries no input and must still be written.
    const int numSamples = buffer.getNumSamples();
    const int bufferChannels = buffer.getNumChannels();
    const int inputs = juce::jmin(bufferChannels, getTotalNumInputChannels());
    const int outputs = juce::jmin(bufferChannels, getTotalNumOutputChannels());
    for (int ch = inputs; ch < bufferChannels; ++ch) buffer.clear(ch, 0, numSamples);
    if (inputs < 1 || outputs < 1 || numSamples <= 0) return;
    auto* left = buffer.getWritePointer(0);
    auto* right = outputs > 1 ? buffer.getWritePointer(1) : nullptr;
    const bool stereoInput = inputs > 1;

    const auto finiteOr = [](float v, float fallback) { return std::isfinite(v) ? v : fallback; };
    const int place = juce::jlimit(0, dc::LocationCount - 1, static_cast<int>(std::lround(finiteOr(location->load(), 0))));
    const float amount = juce::jlimit(0.0f, 1.0f, finiteOr(intensity->load(), 1));
    inputGain.set(dc::dsp::dbToGain(juce::jlimit(-24.0f, 24.0f, finiteOr(input->load(), 0))));
    outputGain.set(dc::dsp::dbToGain(juce::jlimit(-24.0f, 12.0f, finiteOr(output->load(), 0))));
    wetMix.set(juce::jlimit(0.0f, 1.0f, finiteOr(mix->load(), 1)));
    const float bypassTarget = forcedBypass || bypass->load() >= 0.5f ? 1.0f : 0.0f;
    const float neutralTarget = amount <= neutralIntensity ? 1.0f : 0.0f;

    float inPeak = 0, outPeak = 0;
    for (int start = 0; start < numSamples; start += dc::maxChunk) {
        const int n = juce::jmin(dc::maxChunk, numSamples - start);
        float* l = work[0].data();
        float* r = work[1].data();
        for (int i = 0; i < n; ++i) {
            const auto k = static_cast<std::size_t>(i);
            const float rawL = left[start + i], rawR = stereoInput ? right[start + i] : rawL;
            const float x0 = std::isfinite(rawL) ? juce::jlimit(-32.0f, 32.0f, rawL) : 0.0f;
            const float x1 = std::isfinite(rawR) ? juce::jlimit(-32.0f, 32.0f, rawR) : 0.0f;
            inPeak = juce::jmax(inPeak, std::abs(x0), std::abs(x1));
            dryDelay[0].push(x0);
            dryDelay[1].push(x1);
            dryAligned[0][k] = dryDelay[0].readInteger(static_cast<std::size_t>(latency) + 1);
            dryAligned[1][k] = dryDelay[1].readInteger(static_cast<std::size_t>(latency) + 1);
            const float g = inputGain.next();
            l[i] = x0 * g;
            r[i] = x1 * g;
        }
        world.process(l, r, n, dryAligned[0].data(), dryAligned[1].data(), place, amount, bpm);
        for (int i = 0; i < n; ++i) {
            const auto k = static_cast<std::size_t>(i);
            const float m = wetMix.next(), og = outputGain.next(), bypassed = bypassRamp.next(bypassTarget);
            const float dry = neutralRamp.next(neutralTarget);
            const float dl = dryAligned[0][k], dr = dryAligned[1][k];
            float ol = (dl + (l[i] - dl) * m) * og, orr = (dr + (r[i] - dr) * m) * og;
            ol += dry * (dl * og - ol);
            orr += dry * (dr * og - orr);
            limiter.process(ol, orr);
            ol += bypassed * (dl - ol);
            orr += bypassed * (dr - orr);
            if (!std::isfinite(ol)) ol = 0;
            if (!std::isfinite(orr)) orr = 0;
            if (right != nullptr) { left[start + i] = ol; right[start + i] = orr; }
            else left[start + i] = 0.5f * (ol + orr);
            outPeak = juce::jmax(outPeak, std::abs(ol), std::abs(orr));
        }
    }
    inputLevel.store(juce::jmax(inPeak, inputLevel.load() * 0.85f), std::memory_order_relaxed);
    outputLevel.store(juce::jmax(outPeak, outputLevel.load() * 0.85f), std::memory_order_relaxed);
    travelProgress.store(world.travelProgress(), std::memory_order_relaxed);
    currentDestination.store(world.destination(), std::memory_order_relaxed);
    const auto seconds = juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - begin);
    cpuLoad.store(0.9f * cpuLoad.load() + 0.1f * static_cast<float>(seconds * rate / numSamples), std::memory_order_relaxed);
}

juce::AudioProcessorParameter* DungeonCrawlerProcessor::getBypassParameter() const { return parameters.getParameter(dc::ids::bypass); }
juce::AudioProcessorEditor* DungeonCrawlerProcessor::createEditor() { return new DungeonCrawlerEditor(*this); }

void DungeonCrawlerProcessor::getStateInformation(juce::MemoryBlock& data) {
    auto state = parameters.copyState();
    state.setProperty("version", 1, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, data);
}

void DungeonCrawlerProcessor::setStateInformation(const void* data, int size) {
    // Malformed or foreign data is ignored; values are clamped; missing parameters keep
    // their defaults (a state from an older version stays loadable).
    if (data == nullptr || size <= 0 || size > 1024 * 1024) return;
    auto xml = getXmlFromBinary(data, size);
    if (xml == nullptr || !xml->hasTagName(parameters.state.getType())) return;
    const auto incoming = juce::ValueTree::fromXml(*xml);
    std::vector<std::pair<juce::RangedAudioParameter*, float>> values;
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p))
            values.push_back({ ranged, ranged->convertFrom0to1(ranged->getDefaultValue()) });
    for (const auto& child : incoming) {
        const auto id = child.getProperty("id").toString();
        for (auto& [parameter, value] : values)
            if (parameter->getParameterID() == id) {
                const float v = static_cast<float>(child.getProperty("value", value));
                if (!std::isfinite(v)) return;
                value = parameter->getNormalisableRange().snapToLegalValue(v);
            }
    }
    for (auto& [parameter, value] : values) parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DungeonCrawlerProcessor(); }
