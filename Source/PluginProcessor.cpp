#include "PluginProcessor.h"

PhantomAsymProcessor::PhantomAsymProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                        .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "PARAMETERS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PhantomAsymProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "depth", 1 }, "Depth",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { "mode", 1 }, "Mode",
        StringArray { "1 cycle yes / 1 no", "Half-wave (every cycle)" }, 0));

    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { "first", 1 }, "First Cycle",
        StringArray { "Silence", "Pass" }, 0));

    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { "polarity", 1 }, "Polarity",
        StringArray { "Positive", "Negative" }, 0));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "output", 1 }, "Output",
        NormalisableRange<float> (-12.0f, 6.0f, 0.01f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    return { params.begin(), params.end() };
}

void PhantomAsymProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    controlState = 0.0f;
    cycleOn = false;
    nextDuck = true;
    wasPositive = false;
    cyclePeak = 0.0f;
    prevPeak = 0.0f;
    quietCount = 0;
}

bool PhantomAsymProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    if (out != layouts.getMainInputChannelSet())
        return false;

    const auto sc = layouts.getChannelSet (true, 1);
    if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void PhantomAsymProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto mainBuf = getBusBuffer (buffer, true, 0);
    auto scBuf   = getBusBuffer (buffer, true, 1);
    const int numCh      = mainBuf.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    auto* scBus = getBus (true, 1);
    const bool hasSidechain = scBus != nullptr && scBus->isEnabled() && scBuf.getNumChannels() > 0;
    const int  scCh = juce::jmin (scBuf.getNumChannels(), 2);

    const float depth     = apvts.getRawParameterValue ("depth")->load() / 100.0f;
    const int   mode      = (int) apvts.getRawParameterValue ("mode")->load();
    const bool  firstDuck = apvts.getRawParameterValue ("first")->load() < 0.5f;
    const float polarity  = apvts.getRawParameterValue ("polarity")->load() < 0.5f ? 1.0f : -1.0f;
    const float outGain   = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("output")->load());

    // Valores fijos internos
    const float hys          = juce::Decibels::decibelsToGain (-90.0f);   // solo para detectar cycles (practicamente sin threshold)
    const int   resetSamples = (int) (0.06 * currentSampleRate);          // 60 ms sin señal = reinicio
    const float smoothCoef   = std::exp (-1.0f / (0.0015f * (float) currentSampleRate)); // 1.5 ms

    for (int s = 0; s < numSamples; ++s)
    {
        // Señal del sidechain (mono) con su signo
        float sc = 0.0f;
        if (hasSidechain)
        {
            for (int c = 0; c < scCh; ++c)
                sc += scBuf.getReadPointer (c)[s];
            sc /= (float) scCh;
        }
        const float subSigned = polarity * sc;

        // Peak del cycle en curso
        cyclePeak = juce::jmax (cyclePeak, std::abs (sc));

        // Detector de cycles (cruce ascendente = inicio de un cycle)
        if (! wasPositive && subSigned > hys)
        {
            wasPositive = true;
            cycleOn   = nextDuck;
            nextDuck  = ! nextDuck;
            prevPeak  = cyclePeak;          // peak del cycle que acaba de terminar
            cyclePeak = std::abs (sc);
        }
        else if (wasPositive && subSigned < -hys)
        {
            wasPositive = false;
        }

        // Sin señal en el sidechain: reinicia, cada golpe arranca igual
        if (std::abs (subSigned) < hys) ++quietCount; else quietCount = 0;
        if (quietCount > resetSamples)
        {
            cycleOn = false;
            nextDuck = firstDuck;
            wasPositive = false;
            cyclePeak = 0.0f;
            prevPeak = 0.0f;
            quietCount = resetSamples;
        }

        // Cantidad de efecto: sigue la amplitud del sidechain.
        // 0 dBFS (1.0) = Depth completo, hacia -inf dB = sin efecto.
        const float amount = juce::jmin (1.0f, juce::jmax (prevPeak, cyclePeak));
        const bool  active = (mode == 0) ? cycleOn : wasPositive;
        const float target = active ? amount : 0.0f;

        controlState = target + smoothCoef * (controlState - target);

        // Ganancia: solo atenua (maximo 1.0), nunca amplifica
        const float gain = (1.0f - depth * controlState) * outGain;

        for (int ch = 0; ch < numCh; ++ch)
            mainBuf.getWritePointer (ch)[s] *= gain;
    }
}

juce::AudioProcessorEditor* PhantomAsymProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void PhantomAsymProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PhantomAsymProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PhantomAsymProcessor();
}
