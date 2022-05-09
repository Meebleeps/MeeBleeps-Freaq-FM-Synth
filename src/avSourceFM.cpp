/*----------------------------------------------------------------------------------------------------------
 * avSourceFM.cpp
 * 
 * Implements a 2-operator FM voice with
 *    Attack-Release envelope for modulation level
 *    multiple carrier and modulator waveforms
 *    LFO for modulation level with multi-waveform 
 *    multiple FM ratio modes (quantised, high/low linear and fixed)
 * 
 * Source Code Repository:  https://github.com/Meebleeps/MeeBleeps-Freaq-FM-Synth
 * Youtube Channel:         https://www.youtube.com/channel/UC4I1ExnOpH_GjNtm7ZdWeWA
 * 
 * (C) 2021-2022 Meebleeps
*-----------------------------------------------------------------------------------------------------------
*/
#include <MozziGuts.h>
#include <mozzi_midi.h>
#include <mozzi_rand.h>
#include <mozzi_analog.h>
#include <mozzi_fixmath.h>
#include <Oscil.h>  // a template for an oscillator
#include <ADSR.h>
#include <IntMap.h>
#include "avSource.h"
#include "MutantFMSynthOptions.h"

//making LFO update rate lower than control rate to save processing.  places upper limit on LFO frequency
#define LFO_OSCILLATOR_UPDATE_RATE 64

// voice 1
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> carrier1(SIN2048_DATA);
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> modulator1(SIN2048_DATA);
ADSR <CONTROL_RATE, CONTROL_RATE> envelopeAmp1;
ADSR <CONTROL_RATE, CONTROL_RATE> envelopeMod1;
Oscil <SIN2048_NUM_CELLS, LFO_OSCILLATOR_UPDATE_RATE> lfo1(SIN2048_DATA); 

// voice 2
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> carrier2(SIN2048_DATA);
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE>modulator2(SIN2048_DATA);
ADSR <CONTROL_RATE, CONTROL_RATE> envelopeAmp2;
ADSR <CONTROL_RATE, CONTROL_RATE> envelopeMod2;
Oscil <SIN2048_NUM_CELLS, LFO_OSCILLATOR_UPDATE_RATE> lfo2(SIN2048_DATA);  



/*----------------------------------------------------------------------------------------------------------
 * MutatingFM::MutatingFM()
 * create a new instance
 *----------------------------------------------------------------------------------------------------------
 */
MutatingFM::MutatingFM()
{
  masterGain          = 255;
  masterGainBitShift  = 8;
  setOscillator(0);
  setFreqs(33);
  updateCount = 0;
  envelopeMod->setADLevels(255,0);
  param[SYNTH_PARAMETER_MOD_AMOUNT_LFODEPTH] = 0;
}



/*----------------------------------------------------------------------------------------------------------
 * MutatingFM::setOscillator()
 * links this object instance to a given statically defined oscillator, envelope etc
 * terrible design pattern but I had compilation issues when these variables were declared in the class def where they belong
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingFM::setOscillator(uint8_t oscIndex)
{
  switch (oscIndex)
  {
    case 1:
      carrier     = &carrier2;
      modulator   = &modulator2;
      envelopeAmp = &envelopeAmp2;
      envelopeMod = &envelopeMod2;
      lfo         = &lfo2;
      break;

    case 0: 
    default:
      carrier     = &carrier1;
      modulator   = &modulator1;
      envelopeAmp = &envelopeAmp1;
      envelopeMod = &envelopeMod1;
      lfo         = &lfo1;
      break;
  }

  lfo->setFreq((float)0.02);
}



/*----------------------------------------------------------------------------------------------------------
 * MutatingFM::noteOff()
 * turns the note on 
 * currently velocity is ignored
 *----------------------------------------------------------------------------------------------------------
 */
int MutatingFM::noteOn(byte pitch, byte velocity, unsigned int length)
{
  #ifndef ENABLE_MIDI_OUTPUT
  
  //Serial.print(F("MutatingFM::noteOn("));
  //Serial.print(pitch);
  //Serial.println(F(")"));
  #endif


  if (velocity > 0 && pitch > 0)
  {
    if (length != lastNoteLength)
    {
      envelopeAmp->setTimes(0,length,0,50);
      envelopeAmp->setADLevels(255,200);
    }
    envelopeAmp->noteOn(false); 

    // setup modulation envelope attack, decay time based on parameters
    if (param[SYNTH_PARAMETER_ENVELOPE_ATTACK] < MIN_MODULATION_ENV_TIME)
    {
      envelopeMod->setTimes(0,param[SYNTH_PARAMETER_ENVELOPE_DECAY]+MIN_MODULATION_ENV_TIME,length,50);
    }
    else
    {
      envelopeMod->setTimes(param[SYNTH_PARAMETER_ENVELOPE_ATTACK],param[SYNTH_PARAMETER_ENVELOPE_DECAY]+MIN_MODULATION_ENV_TIME,length,50);
    }

    setFreqs(pitch);
    envelopeMod->noteOn(true);

    #ifndef ENABLE_MIDI_OUTPUT
    
    /*
    Serial.print(F("note on: envelopeMod ADSR=("));
    Serial.print(param[SYNTH_PARAMETER_ENVELOPE_ATTACK]);
    Serial.print(F(", "   ));
    Serial.print(param[SYNTH_PARAMETER_ENVELOPE_DECAY]);
    Serial.print(F(", Sustain Level="   ));
    Serial.print(param[SYNTH_PARAMETER_ENVELOPE_SUSTAIN] >> 2);
    Serial.print(F(", 50)"   ));
    Serial.println();
    */
    #endif

    lastNoteLength = length;
  }
  else
  {
    noteOff();
  }
  return 0;
}


/*----------------------------------------------------------------------------------------------------------
 * MutatingFM::noteOff()
 * turns the note off
 *----------------------------------------------------------------------------------------------------------
 */
int MutatingFM::noteOff()
{
  envelopeAmp->noteOff(); 
  return 0;

}


/*----------------------------------------------------------------------------------------------------------
 * MutatingFM::updateAudio()
 * returns the next audio sample
 *----------------------------------------------------------------------------------------------------------
 */
inline int MutatingFM::updateAudio()
{
  // TODO:  ignores master gain due to audio glitches when shifting by a variable rather than a constant
  //        optimise and reinstate
  //return MonoOutput::fromNBit(9, (((int16_t)carrier->phMod(modulatorAmount * modulator->next() >> 8) * currentGain) >> masterGainBitShift));

  // return audio without master gain
  return MonoOutput::fromNBit(9, (((int16_t)carrier->phMod(modulatorAmount * modulator->next() >> 8) * currentGain) >> 8));
}





/*----------------------------------------------------------------------------------------------------------
 * MutatingFM::updateControl()
 * updates the envelopes and modulation control
 *----------------------------------------------------------------------------------------------------------
 */
inline void MutatingFM::updateControl()
{
  #ifdef SYNTH_MODULATION_UPDATE_DIVIDER
  // CONTROL_RATE has to be high to reduce jitter in the sequencer but running these calcs at 128hz seems to be too high, so only do it every 2nd call
  if (++updateCount % SYNTH_MODULATION_UPDATE_DIVIDER == 0)
  {
  #endif

    // update amplitude & modulation envelopes
    // 20-30 micros
    envelopeAmp->update();
    envelopeMod->update();

  #ifdef SYNTH_MODULATION_UPDATE_DIVIDER
  }
  #endif

    // store gain for use in updateAudio
    currentGain     = envelopeAmp->next();  

    // store LFO value for use in displaying the lfo position onscreen.
    lastLFOValue    = lfo->next()+128;

    modulatorAmount = ((((uint32_t)param[SYNTH_PARAMETER_MOD_AMOUNT])          * (uint32_t)envelopeMod->next())
                      + (((uint32_t)param[SYNTH_PARAMETER_MOD_AMOUNT_LFODEPTH]) * (uint32_t)lastLFOValue)
                      );
    #ifndef ENABLE_MIDI_OUTPUT

    //Serial.println(Q16n16_to_float(modulatorAmount));
    #endif

}



/*----------------------------------------------------------------------------------------------------------
 * MutatingFM::setGain()
 * sets the gain 
 * stores it as a 0-255 byte and also as a bitshift value of gain reduction 8 (max volume) - 17 (off)
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingFM::setGain(byte gain)
{
  masterGain = gain;

  if (masterGain == 0)
  {
    masterGainBitShift = 17;
  }
  else
  {
    masterGainBitShift = 8 + (((uint16_t)264 - (uint16_t)masterGain) >> 5);
  }
}




/*----------------------------------------------------------------------------------------------------------
 * MutatingFM::setFreqs()
 * calculates the frequencies based on the current parameters
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingFM::setFreqs(uint8_t midiNote) 
{
  uint8_t multIndex;
  float freqshift[17] = {0,0.03125,0.0625,0.125,0.25,0.5,1,1.5,2,2.5,3,3.5,4,5,6,7,8};


  if(midiNote != lastMidiNote && midiNote != 0)
  {
    carrierFrequency  = Q16n16_mtof(Q8n0_to_Q16n16(midiNote));
    carrier->setFreq_Q16n16(carrierFrequency);
    lastMidiNote = midiNote;
  }

  // added EXPONENTIAL & LINEAR FM modes - exponential is fixed to defined multiples
  // LINEAR is carrier * value with two ranges
  switch(fmMode)
  {
    case FM_MODE_EXPONENTIAL:
      lastParam[SYNTH_PARAMETER_MOD_RATIO] = param[SYNTH_PARAMETER_MOD_RATIO];

      multIndex = param[SYNTH_PARAMETER_MOD_RATIO]/61;//(1023/17);

      if (multIndex>0)
      {
        modulationFrequency = carrierFrequency * freqshift[multIndex];
      }
      else
      {
        modulationFrequency = carrierFrequency*((float)1.0 / ((512-param[SYNTH_PARAMETER_MOD_RATIO])+1));
      }
      break;

    case FM_MODE_LINEAR_HIGH:
      modulationFrequency = carrierFrequency*(float)param[SYNTH_PARAMETER_MOD_RATIO]/100;// carrierFrequency*((float)1.0 / ((512-param[SYNTH_PARAMETER_MOD_RATIO])+1));
      break;

    case FM_MODE_LINEAR_LOW:
      modulationFrequency = carrierFrequency*(float)param[SYNTH_PARAMETER_MOD_RATIO]/10000;// carrierFrequency*((float)1.0 / ((512-param[SYNTH_PARAMETER_MOD_RATIO])+1));
      break;

    case FM_MODE_FREE:
      modulationFrequency = (uint32_t)param[SYNTH_PARAMETER_MOD_RATIO] << 17; //0-1023 << 17 into a Q16n16 value = 0-2000hz
      break;
  }

  modulator->setFreq_Q16n16(modulationFrequency);



/*
  #ifndef ENABLE_MIDI_OUTPUT
  
  if (midiNote != 0)
  {
    if (carrier == &carrier1)
    {
      Serial.print(F("OSC1:  "));
    }
    else
    {
      Serial.print(F("OSC2:  "));
    }


    Serial.print(F("set frequency note="));
    Serial.print(midiNote);
    Serial.print(F(", carrier="));
    Serial.print(Q16n16_to_float(carrierFrequency));
    Serial.print(F(", modfreq="));
    Serial.print(Q16n16_to_float(modulationFrequency));
    Serial.println();
  }
  #endif

  */
}



/*----------------------------------------------------------------------------------------------------------
 * MutatingPhasor::setFilterShape()
 * 
 * adjusts attack, decay and sustain level for the filter to provide 1-knob control of filter envelope
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingFM::setModulationShape(uint16_t newValue)
{
  #ifndef ENABLE_MIDI_OUTPUT

  Serial.print(F("setModulationShape("));
  Serial.print(newValue);
  Serial.print(F(") "));
  #endif
  if (newValue >= 0 && newValue < MAX_FILTER_SHAPE)
  {
    #ifndef ENABLE_MIDI_OUTPUT
    Serial.print(F(" settiing ATTACK "));
    #endif

    // for first 60% of range, attack = 0
    // for second part of range, attack is linear
    if (newValue < 600)
    {
      param[SYNTH_PARAMETER_ENVELOPE_ATTACK] = 0;
    }
    else
    {
      param[SYNTH_PARAMETER_ENVELOPE_ATTACK] = (newValue - 600)*2;
    }

    // set decay
    #ifndef ENABLE_MIDI_OUTPUT
    Serial.print(F(" settiing DECAY"));
    #endif

    param[SYNTH_PARAMETER_ENVELOPE_DECAY] = newValue;

    // set sustain volume
    // for first 1/2 of range, sustain is cubic
    // for second 3/4 of range, sustain is maxxed
    // for last part sustain decreases from 255 to 128
    #ifndef ENABLE_MIDI_OUTPUT
    Serial.print(F(" settiing SUSTAIN"));
    #endif

    if (newValue < 512)
    {
      uint32_t p = pow(newValue,3) / 262144;
      param[SYNTH_PARAMETER_ENVELOPE_SUSTAIN] = min(p,255);// (255.0 * (float)newValue) / 512;
    }
    else if (newValue < 768)
    {
      param[SYNTH_PARAMETER_ENVELOPE_SUSTAIN] = 255;
    }
    else
    {
      param[SYNTH_PARAMETER_ENVELOPE_SUSTAIN] = 255 + ((768 - newValue)/2);
    }

    param[SYNTH_PARAMETER_ENVELOPE_SHAPE] = newValue;
  }
  #ifndef ENABLE_MIDI_OUTPUT

  Serial.println(F(")"));
  #endif
}






void MutatingFM::setParam(uint8_t paramIndex, uint16_t newValue)
{
  if (param[paramIndex] != newValue)
  {
    param[paramIndex] = newValue;

    switch(paramIndex)
    {
      case SYNTH_PARAMETER_MOD_RATIO:
        setFreqs(0);
        break;
      
      case SYNTH_PARAMETER_MOD_AMOUNT:
        envelopeMod->setADLevels(param[SYNTH_PARAMETER_MOD_AMOUNT] >> 2,0);
        break;

      case SYNTH_PARAMETER_ENVELOPE_SHAPE:
        setModulationShape(newValue);
        envelopeMod->setTimes(param[SYNTH_PARAMETER_ENVELOPE_ATTACK],param[SYNTH_PARAMETER_ENVELOPE_DECAY]+MIN_MODULATION_ENV_TIME,lastNoteLength,50);
        break;

      case SYNTH_PARAMETER_ENVELOPE_ATTACK:
        envelopeMod->setTimes(param[SYNTH_PARAMETER_ENVELOPE_ATTACK],param[SYNTH_PARAMETER_ENVELOPE_DECAY]+MIN_MODULATION_ENV_TIME,lastNoteLength,50);
        break;

      case SYNTH_PARAMETER_ENVELOPE_DECAY:
        envelopeMod->setTimes(param[SYNTH_PARAMETER_ENVELOPE_ATTACK],param[SYNTH_PARAMETER_ENVELOPE_DECAY]+MIN_MODULATION_ENV_TIME,lastNoteLength,50);
        break;

      case SYNTH_PARAMETER_ENVELOPE_SUSTAIN:
        envelopeMod->setSustainLevel(param[SYNTH_PARAMETER_ENVELOPE_SUSTAIN] >> 2);
        break;

    }
  }
}


uint16_t MutatingFM::getParam(uint8_t paramIndex)
{
  return param[paramIndex];
}



/*----------------------------------------------------------------------------------------------------------
 * toggleFMMode
 * Sets the FM ratio mode 
 * 
 * FM_MODE_EXPONENTIAL    modulator is a quantized multiple of the carrier, 
 * FM_MODE_LINEAR_HIGH    modulator is a multiple of the carrier, with high scale
 * FM_MODE_LINEAR_LOW     modulator is a multiple of the carrier, with low scale
 * FM_MODE_FREE           modulator is not related to the the carrier
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingFM::toggleFMMode()
{
  fmMode = (fmMode + 1) % MAX_FM_MODES;
  setFreqs(0);
}


/*----------------------------------------------------------------------------------------------------------
 * getFMMode
 * Gets the FM ratio mode 
 *----------------------------------------------------------------------------------------------------------
 */
uint8_t MutatingFM::getFMMode()
{
  return fmMode;
}


/*----------------------------------------------------------------------------------------------------------
 * toggleCarrierWaveform
 * Sets the carrier waveform 
 * 
 * set to WAVEFORM_NULL to switch the carrier off
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingFM::toggleCarrierWaveform()
{
  carrierWaveform = (carrierWaveform + 1) % MAX_CARRIER_WAVEFORMS;
  
  switch (carrierWaveform)
  {
    case WAVEFORM_SIN: 
      carrier->setTable(SIN2048_DATA);
      break;

    case WAVEFORM_SAW: 
      carrier->setTable(SAW2048_DATA);
      break;

    case WAVEFORM_REVSAW: 
      carrier->setTable(REVSAW2048_DATA);
      break;

    case WAVEFORM_SQUARE: 
      carrier->setTable(SQUARE2048_DATA);
      break;

/** @brief if COMPILE_SMALLER_BINARY flag is defined, omit the pseudorandom waveform */
#ifndef COMPILE_SMALLER_BINARY
    case WAVEFORM_PSEUDORANDOM: 
      carrier->setTable(PSEUDORANDOM2048_DATA);
      break;
#endif

    case WAVEFORM_NULL: 
      carrier->setTable(NULLWAVEFORM2048_DATA);
      break;

    default:
      carrier->setTable(SIN2048_DATA);
  }
}


/*----------------------------------------------------------------------------------------------------------
 * getCarrierWaveform
 * Gets the carrier waveform 
 *----------------------------------------------------------------------------------------------------------
 */
uint8_t MutatingFM::getCarrierWaveform()
{
  return carrierWaveform;
}




/*----------------------------------------------------------------------------------------------------------
 * toggleCarrierWaveform
 * Sets the carrier waveform 
 * 
 * set to WAVEFORM_NULL to switch the carrier off
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingFM::toggleModulatorWaveform()
{
  modulatorWaveform = (modulatorWaveform + 1) % MAX_MODULATOR_WAVEFORMS;
  
  switch (modulatorWaveform)
  {
    case WAVEFORM_SIN: 
      modulator->setTable(SIN2048_DATA);
      break;

    case WAVEFORM_SAW: 
      modulator->setTable(SAW2048_DATA);
      break;

    case WAVEFORM_REVSAW: 
      modulator->setTable(REVSAW2048_DATA);
      break;

    case WAVEFORM_SQUARE: 
      modulator->setTable(SQUARE2048_DATA);
      break;

/** @brief if COMPILE_SMALLER_BINARY flag is defined, omit the pseudorandom waveform */
#ifndef COMPILE_SMALLER_BINARY
    case WAVEFORM_PSEUDORANDOM: 
      modulator->setTable(PSEUDORANDOM2048_DATA);
      break;
#endif

    case WAVEFORM_NULL: 
      modulator->setTable(NULLWAVEFORM2048_DATA);
      break;

    default:
      modulator->setTable(SIN2048_DATA);
  }
}


/*----------------------------------------------------------------------------------------------------------
 * getModulatorWaveform
 * Gets the Modulator waveform 
 *----------------------------------------------------------------------------------------------------------
 */
uint8_t MutatingFM::getModulatorWaveform()
{
  return modulatorWaveform;
}





/*----------------------------------------------------------------------------------------------------------
 * toggleLFOWaveform
 * Sets the LFO waveform 
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingFM::toggleLFOWaveform()
{
  lfoWaveform = (lfoWaveform + 1) % MAX_LFO_WAVEFORMS;
  
  switch (lfoWaveform)
  {
    case WAVEFORM_SIN: 
      lfo->setTable(SIN2048_DATA);
      break;

    case WAVEFORM_SAW: 
      lfo->setTable(SAW2048_DATA);
      break;

    case WAVEFORM_REVSAW: 
      lfo->setTable(REVSAW2048_DATA);
      break;

    case WAVEFORM_SQUARE: 
      lfo->setTable(SQUARE2048_DATA);
      break;

/** @brief if COMPILE_SMALLER_BINARY flag is defined, omit the pseudorandom waveform */
#ifndef COMPILE_SMALLER_BINARY
    case WAVEFORM_PSEUDORANDOM: 
      lfo->setTable(PSEUDORANDOM2048_DATA);
      break;    
#endif

    case WAVEFORM_NULL: 
      lfo->setTable(NULLWAVEFORM2048_DATA);
      break;

    default:
      lfo->setTable(SIN2048_DATA);
  }
}


/*----------------------------------------------------------------------------------------------------------
 * getLFOWaveform
 * Gets the LFO waveform 
 *----------------------------------------------------------------------------------------------------------
 */
uint8_t MutatingFM::getLFOWaveform()
{
  return lfoWaveform;
}



/*----------------------------------------------------------------------------------------------------------
 * mutate
 * used to generate randomness in the parameters - not used  
 *----------------------------------------------------------------------------------------------------------
 */
int MutatingFM::mutate()
{
  //
  return 0; 
 
}



/*----------------------------------------------------------------------------------------------------------
 * getLFOValue
 * gets the current LFO value
 *----------------------------------------------------------------------------------------------------------
 */
uint8_t MutatingFM::getLFOValue()
{
  return lastLFOValue;
}



/*----------------------------------------------------------------------------------------------------------
 * setLFOFrequency
 * sets the current LFO frequency
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingFM::setLFOFrequency(float freq)
{
  lfo->setFreq_Q16n16(float_to_Q16n16(freq));
}


