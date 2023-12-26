/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

const std::string BitCrusherAudioProcessor::paramsNames[] = { "BitDepth", "Frequency", "Mix", "Volume" };

//==============================================================================
BitCrusherAudioProcessor::BitCrusherAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
	bitDepthParameter  = apvts.getRawParameterValue(paramsNames[0]);
	frequencyParameter = apvts.getRawParameterValue(paramsNames[1]);
	mixParameter       = apvts.getRawParameterValue(paramsNames[2]);
	volumeParameter    = apvts.getRawParameterValue(paramsNames[3]);
}

BitCrusherAudioProcessor::~BitCrusherAudioProcessor()
{
}

//==============================================================================
const juce::String BitCrusherAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool BitCrusherAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool BitCrusherAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool BitCrusherAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double BitCrusherAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int BitCrusherAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int BitCrusherAudioProcessor::getCurrentProgram()
{
    return 0;
}

void BitCrusherAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String BitCrusherAudioProcessor::getProgramName (int index)
{
    return {};
}

void BitCrusherAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void BitCrusherAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
}

void BitCrusherAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool BitCrusherAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void BitCrusherAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
	// Get params
	const float bitDepth = bitDepthParameter->load();
	const float frequency = frequencyParameter->load();
	const float mix = mixParameter->load();
	const float volume = juce::Decibels::decibelsToGain(volumeParameter->load());

	// Mics constants
	const float bitLevels = powf(2.0, bitDepth);
	const float mixInverse = 1.0f - mix;
	const int channels = getTotalNumOutputChannels();
	const int samples = buffer.getNumSamples();
	const int samplesFromFrequency = getSampleRate() / frequency;

	for (int channel = 0; channel < channels; ++channel)
	{
		auto* channelBuffer = buffer.getWritePointer(channel);
		auto& holdValue = m_holdValue[channel];
		auto& samplesToHold = m_samplesToHold[channel];

		for (int sample = 0; sample < samples; ++sample)
		{
			// Get input
			const float in = channelBuffer[sample];
			
			//Reduce sample rate
			samplesToHold--;

			if (samplesToHold <= 0)
			{
				samplesToHold = samplesFromFrequency;
				//BitCrush
				holdValue = (int)(in * bitLevels) / bitLevels;
			}

			// Apply volume
			channelBuffer[sample] = volume * (mix * holdValue + mixInverse * in);
		}
	}
}

//==============================================================================
bool BitCrusherAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* BitCrusherAudioProcessor::createEditor()
{
    return new BitCrusherAudioProcessorEditor(*this, apvts);
}

//==============================================================================
void BitCrusherAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
	auto state = apvts.copyState();
	std::unique_ptr<juce::XmlElement> xml(state.createXml());
	copyXmlToBinary(*xml, destData);
}

void BitCrusherAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
	std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

	if (xmlState.get() != nullptr)
		if (xmlState->hasTagName(apvts.state.getType()))
			apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessorValueTreeState::ParameterLayout BitCrusherAudioProcessor::createParameterLayout()
{
	APVTS::ParameterLayout layout;

	using namespace juce;

	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[0], paramsNames[0], NormalisableRange<float>(  1.0f,    15.0f,  0.1f, 1.0f),     8.0f));
	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[1], paramsNames[1], NormalisableRange<float>( 20.0f, 20000.0f,  1.0f, 0.3f), 10000.0f));
	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[2], paramsNames[2], NormalisableRange<float>(  0.0f,     1.0f, 0.05f, 1.0f),     1.0f));
	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[3], paramsNames[3], NormalisableRange<float>(-12.0f,    12.0f,  0.1f, 1.0f),     0.0f));

	return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BitCrusherAudioProcessor();
}
