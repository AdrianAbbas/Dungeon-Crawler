#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <optional>

DungeonCrawlerProcessor::DungeonCrawlerProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "DUNGEON_CRAWLER", dc::createParameterLayout([this] { return locationIndex(); })) {
    location = parameters.getRawParameterValue(dc::ids::location);
    subLocation = parameters.getRawParameterValue(dc::ids::subLocation);
    intensity = parameters.getRawParameterValue(dc::ids::intensity);
    mix = parameters.getRawParameterValue(dc::ids::mix);
    input = parameters.getRawParameterValue(dc::ids::input);
    output = parameters.getRawParameterValue(dc::ids::output);
    bypass = parameters.getRawParameterValue(dc::ids::bypass);
    travelTime = parameters.getRawParameterValue(dc::ids::travelTime);
    travelSync = parameters.getRawParameterValue(dc::ids::travelSync);
    for (std::size_t m = 0; m < macros.size(); ++m) macros[m] = parameters.getRawParameterValue(dc::ids::macros[m]);
    for (std::size_t i = 0; i < advanced.size(); ++i) advanced[i] = parameters.getRawParameterValue(dc::ids::advanced(i));
    journeyOn = parameters.getRawParameterValue(dc::ids::journeyOn);
    journeyMode = parameters.getRawParameterValue(dc::ids::journeyMode);
    journeyBars = parameters.getRawParameterValue(dc::ids::journeyBars);
    roamOn = parameters.getRawParameterValue(dc::ids::roamOn);
    roamBars = parameters.getRawParameterValue(dc::ids::roamBars);
    roamRandomness = parameters.getRawParameterValue(dc::ids::roamRandomness);
    roamSeed = parameters.getRawParameterValue(dc::ids::roamSeed);
    midiTravel = parameters.getRawParameterValue(dc::ids::midiTravel);
    midiOctave = parameters.getRawParameterValue(dc::ids::midiOctave);
    atmosLevel = parameters.getRawParameterValue(dc::ids::atmosLevel);
    atmosDensity = parameters.getRawParameterValue(dc::ids::atmosDensity);
    atmosSeed = parameters.getRawParameterValue(dc::ids::atmosSeed);
}

void DungeonCrawlerProcessor::setRoute(const std::vector<dc::Stop>& stops) {
    const int n = juce::jmin(static_cast<int>(stops.size()), dc::maxStops);
    routeLength.store(0);
    for (int k = 0; k < n; ++k) {
        const auto& st = stops[static_cast<std::size_t>(k)];
        const int l = juce::jlimit(0, dc::LocationCount - 1, st.location);
        routeStops[static_cast<std::size_t>(k)].store(l * 8 + dc::clampSubLocation(l, st.sub));
    }
    routeLength.store(n);
}

std::vector<dc::Stop> DungeonCrawlerProcessor::route() const {
    std::vector<dc::Stop> out;
    const int n = routeLength.load();
    for (int k = 0; k < n; ++k) {
        const int code = routeStops[static_cast<std::size_t>(k)].load();
        out.push_back({ code / 8, code % 8 });
    }
    return out;
}

int DungeonCrawlerProcessor::locationIndex() const noexcept {
    const float v = location != nullptr ? location->load() : 0.0f;
    return std::isfinite(v) ? juce::jlimit(0, dc::LocationCount - 1, static_cast<int>(std::lround(v))) : 0;
}

void DungeonCrawlerProcessor::prepareToPlay(double sampleRate, int) {
    rate = std::isfinite(sampleRate) ? juce::jlimit(8000.0, 384000.0, sampleRate) : 48000.0;
    world.prepare(rate);
    atmosphere.prepare(rate);
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

void DungeonCrawlerProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    // MIDI travel: the last note-on in the travel octave chooses the place.
    if (midiTravel->load() >= 0.5f) {
        const int base = juce::jlimit(0, 8, static_cast<int>(std::lround(midiOctave->load()))) * 12;
        for (const auto meta : midi) {
            const auto message = meta.getMessage();
            if (!message.isNoteOn()) continue;
            dc::Stop chosen;
            if (dc::midiPlace(message.getNoteNumber(), base, midiLocation >= 0 ? midiLocation : locationIndex(), chosen)) {
                midiLocation = chosen.location;
                midiSub = chosen.sub;
            }
        }
    } else midiLocation = -1;
    process(buffer, false);
}
void DungeonCrawlerProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { process(buffer, true); }

void DungeonCrawlerProcessor::process(juce::AudioBuffer<float>& buffer, bool forcedBypass) {
    juce::ScopedNoDenormals noDenormals;
    const auto begin = juce::Time::getHighResolutionTicks();
    double bpm = 120, beatsPerBar = 4;
    std::optional<double> hostBeats;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition()) {
            if (auto tempo = pos->getBpm())
                if (std::isfinite(*tempo)) bpm = juce::jlimit(20.0, 400.0, *tempo);
            if (auto sig = pos->getTimeSignature())
                if (sig->numerator > 0 && sig->denominator > 0) beatsPerBar = juce::jlimit(1.0, 32.0, sig->numerator * 4.0 / sig->denominator);
            if (pos->getIsPlaying())
                if (auto ppq = pos->getPpqPosition())
                    if (std::isfinite(*ppq)) hostBeats = *ppq;
        }

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
    const float amount = juce::jlimit(0.0f, 1.0f, finiteOr(intensity->load(), 1));
    controls.location = locationIndex();
    controls.sub = juce::jlimit(0, dc::maxSubLocations, static_cast<int>(std::lround(finiteOr(subLocation->load(), 0))));
    // Journeys, roaming and MIDI travel steer the place (Milestone 9). The position is
    // the host's when it plays (deterministic: the same bar, the same place), otherwise
    // a free-running count at the host's tempo.
    const double beats = hostBeats ? *hostBeats : freeBeats;
    freeBeats = hostBeats ? *hostBeats : freeBeats + static_cast<double>(buffer.getNumSamples()) / rate * bpm / 60.0;
    bool steering = false;
    if (roamOn->load() >= 0.5f) {
        const auto rateIndex = static_cast<std::size_t>(juce::jlimit(0, static_cast<int>(dc::roamBarChoices.size()) - 1, static_cast<int>(std::lround(roamBars->load()))));
        const double beatsPerStep = dc::roamBarChoices[rateIndex] * beatsPerBar;
        const auto step = static_cast<long long>(std::floor(std::max(0.0, beats) / beatsPerStep));
        controls.sub = dc::roamPlace(controls.location, step, juce::jlimit(0.0f, 1.0f, finiteOr(roamRandomness->load(), 0.5f)),
                                     juce::jlimit(1, 999, static_cast<int>(std::lround(roamSeed->load()))));
        steering = true;
    }
    const int stops = routeLength.load(std::memory_order_acquire);
    if (journeyOn->load() >= 0.5f && stops > 0) {
        const int index = dc::journeyStop(beats, stops, juce::jlimit(1.0f, 16.0f, journeyBars->load()) * beatsPerBar,
                                          juce::jlimit(0, 2, static_cast<int>(std::lround(journeyMode->load()))));
        const int code = routeStops[static_cast<std::size_t>(index)].load(std::memory_order_relaxed);
        controls.location = code / 8;
        controls.sub = code % 8;
        steering = true;
    }
    if (midiTravel->load() >= 0.5f && midiLocation >= 0) {
        controls.location = midiLocation;
        controls.sub = midiSub;
        steering = true;
    }
    steered.store(steering, std::memory_order_relaxed);
    controls.intensity = amount;
    for (std::size_t m = 0; m < macros.size(); ++m) controls.macros[m] = juce::jlimit(0.0f, 1.0f, finiteOr(macros[m]->load(), 0.5f));
    for (std::size_t i = 0; i < advanced.size(); ++i) controls.advanced[i] = finiteOr(advanced[i]->load(), 0);
    const auto sync = static_cast<std::size_t>(juce::jlimit(0, static_cast<int>(dc::travelSyncBeats.size()) - 1,
                                                            static_cast<int>(std::lround(finiteOr(travelSync->load(), 0)))));
    controls.travelSeconds = sync == 0 ? static_cast<double>(juce::jlimit(0.0f, 10.0f, finiteOr(travelTime->load(), 1.5f)))
                                       : dc::dsp::beatsToSeconds(dc::travelSyncBeats[sync], bpm);
    controls.bpm = bpm;
    world.setControls(controls);
    inputGain.set(dc::dsp::dbToGain(juce::jlimit(-24.0f, 24.0f, finiteOr(input->load(), 0))));
    outputGain.set(dc::dsp::dbToGain(juce::jlimit(-24.0f, 12.0f, finiteOr(output->load(), 0))));
    wetMix.set(juce::jlimit(0.0f, 1.0f, finiteOr(mix->load(), 1)));
    const float bypassTarget = forcedBypass || bypass->load() >= 0.5f ? 1.0f : 0.0f;
    const float neutralTarget = amount <= neutralIntensity ? 1.0f : 0.0f;

    const float atmosAmount = juce::jlimit(0.0f, 1.0f, finiteOr(atmosLevel->load(), 0));
    const float atmosDense = juce::jlimit(0.0f, 1.0f, finiteOr(atmosDensity->load(), 0.35f));
    const int atmosSeedValue = juce::jlimit(1, 999, static_cast<int>(std::lround(finiteOr(atmosSeed->load(), 1))));
    const double beatsPerSample = bpm / 60.0 / rate;
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
        world.process(l, r, n, dryAligned[0].data(), dryAligned[1].data());
        // The Atmosphere joins the wet signal (Mix, Intensity 0 and Bypass remove it too).
        if (atmosAmount > 0.0f || atmosphere.activeVoices() > 0)
            atmosphere.process(l, r, n, beats + beatsPerSample * start, beatsPerSample, world.destination(), world.destinationSub(),
                               atmosAmount, atmosDense, atmosSeedValue, dryAligned[0].data(), dryAligned[1].data());
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
    currentSubLocation.store(world.destinationSub(), std::memory_order_relaxed);
    const auto seconds = juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - begin);
    cpuLoad.store(0.9f * cpuLoad.load() + 0.1f * static_cast<float>(seconds * rate / numSamples), std::memory_order_relaxed);
}

juce::AudioProcessorParameter* DungeonCrawlerProcessor::getBypassParameter() const { return parameters.getParameter(dc::ids::bypass); }
juce::AudioProcessorEditor* DungeonCrawlerProcessor::createEditor() { return new DungeonCrawlerEditor(*this); }

void DungeonCrawlerProcessor::getStateInformation(juce::MemoryBlock& data) {
    auto state = parameters.copyState();
    state.setProperty("version", 3, nullptr);
    juce::StringArray stops;
    for (const auto& st : route()) stops.add(juce::String(st.location) + ":" + juce::String(st.sub));
    state.setProperty("route", stops.joinIntoString(","), nullptr);
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
    // The journey route (version 3); older states have none.
    std::vector<dc::Stop> stops;
    for (const auto& token : juce::StringArray::fromTokens(incoming.getProperty("route").toString(), ",", ""))
        if (token.containsChar(':') && static_cast<int>(stops.size()) < dc::maxStops)
            stops.push_back({ juce::jlimit(0, dc::LocationCount - 1, token.upToFirstOccurrenceOf(":", false, false).getIntValue()),
                              juce::jlimit(0, dc::maxSubLocations, token.fromFirstOccurrenceOf(":", false, false).getIntValue()) });
    setRoute(stops);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DungeonCrawlerProcessor(); }
