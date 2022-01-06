
#include <MozziGuts.h>
#include <mozzi_midi.h>
#include <mozzi_rand.h>
#include <mozzi_analog.h>
#include <mozzi_fixmath.h>
#include <Oscil.h>  // a template for an oscillator
#include <ADSR.h>
#include <IntMap.h>
#include "avSource.h"


// voice 1
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> carrier1(SIN2048_DATA);
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> modulator1(SIN2048_DATA);
ADSR <CONTROL_RATE, CONTROL_RATE> envelopeAmp1;
ADSR <CONTROL_RATE, CONTROL_RATE> envelopeMod1;
Oscil <SIN2048_NUM_CELLS, CONTROL_RATE> lfo1(SIN2048_DATA);

// voice 2
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> carrier2(SIN2048_DATA);
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE>modulator2(SIN2048_DATA);
ADSR <CONTROL_RATE, CONTROL_RATE> envelopeAmp2;
ADSR <CONTROL_RATE, CONTROL_RATE> envelopeMod2;
Oscil <SIN2048_NUM_CELLS, CONTROL_RATE> lfo2(SIN2048_DATA);


MutatingFM::MutatingFM()
{
  masterGain = 255;
  setOscillator(0);
  setFreqs(33);
  updateCount = 0;
    envelopeMod->setADLevels(255,0);


  param[SYNTH_PARAMETER_MOD_AMOUNT_LFODEPTH] = 0;

}


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
    //envelopeMod->setADLevels(param[SYNTH_PARAMETER_MOD_AMOUNT],min(param[SYNTH_PARAMETER_FILTER_SUSTAIN],param[SYNTH_PARAMETER_MOD_AMOUNT]));
    //envelopeMod->setTimes(param[SYNTH_PARAMETER_ENVELOPE_ATTACK],param[SYNTH_PARAMETER_ENVELOPE_DECAY],length,50);
  
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
  /*
  int16_t mix;
  mix = carrier->phMod(modulatorAmount * modulator->next() >> 8);
  mix = (mix * currentGain) >> 8;
  return MonoOutput::fromNBit(9, mix);
  */
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

    //modulatorAmount = (uint32_t)(envelopeMod->next() * lastLFOValue) << 3; //(baseModAmount + lfoModAmount + envModAmount) << 16;

    // modulatorAmount is a Q16n16 unsigned fixed-point value. 1.0 = 1<<16.
    // musically interesting range seems to be ~0-10

    //modulation amount = MOD_ENVELOPE * ENVELOPE_AMOUNT + LFO*LFO_AMOUNT

    /*
    modulatorAmount = (((uint32_t)param[SYNTH_PARAMETER_MOD_AMOUNT] * ((uint32_t)envelopeMod->next())) << MOD_DEPTH_MULTIPLIER_ENV)
                      + ((((uint32_t)param[SYNTH_PARAMETER_MOD_AMOUNT_LFODEPTH] * ((uint32_t)lastLFOValue)) >> 1))// MOD_DEPTH_MULTIPLIER_LFO))
                      ;
    */
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
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingFM::setGain(byte gain)
{
  masterGain = gain;
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
      modulationFrequency = (uint32_t)param[SYNTH_PARAMETER_MOD_RATIO] << 19; //0-1023 << 20 into a Q16n16 value = 0-16000hz
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
//        envelopeMod->setAttackTime(param[SYNTH_PARAMETER_ENVELOPE_ATTACK]);
        envelopeMod->setTimes(param[SYNTH_PARAMETER_ENVELOPE_ATTACK],param[SYNTH_PARAMETER_ENVELOPE_DECAY]+MIN_MODULATION_ENV_TIME,lastNoteLength,50);
        break;

      case SYNTH_PARAMETER_ENVELOPE_DECAY:

//        envelopeMod->setDecayTime(param[SYNTH_PARAMETER_ENVELOPE_DECAY]);
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


void MutatingFM::toggleFMMode()
{
  fmMode = (fmMode + 1) % MAX_FM_MODES;
}

uint8_t MutatingFM::getFMMode()
{
  return fmMode;
}

int MutatingFM::mutate()
{
  //
  return 0; 
 
}




uint8_t MutatingFM::getLFOValue()
{
  return lastLFOValue;
}

void MutatingFM::setLFOFrequency(float freq)
{
  lfo->setFreq_Q16n16(float_to_Q16n16(freq));
}

void MutatingFM::setLFODepth(uint8_t depth)
{

}
