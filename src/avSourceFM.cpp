
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
  param[SYNTH_PARAMETER_MOD_AMOUNT_LFODEPTH] = 1023;

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
  
  //Serial.print(F("MutatingFM::noteOn("));
  //Serial.print(pitch);
  //Serial.println(F(")"));

  if (velocity > 0 && pitch > 0)
  {
    if (length != lastNoteLength)
    {
      envelopeAmp->setTimes(0,length,0,50);
      envelopeAmp->setADLevels(255,170);
    }
    envelopeAmp->noteOn(false); 

    // setup modulation envelope attack, decay time based on parameters
    //envelopeMod->setADLevels(param[SYNTH_PARAMETER_MOD_AMOUNT],min(param[SYNTH_PARAMETER_FILTER_SUSTAIN],param[SYNTH_PARAMETER_MOD_AMOUNT]));
    //envelopeMod->setTimes(param[SYNTH_PARAMETER_ENVELOPE_ATTACK],param[SYNTH_PARAMETER_ENVELOPE_DECAY],length,50);



    envelopeMod->noteOn(false);

    

    Serial.print(F("note on: attack level="));
    Serial.println(param[SYNTH_PARAMETER_MOD_AMOUNT] >> 2);

    setFreqs(pitch);
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

  // update amplitude envelope 
  envelopeAmp->update();

  // update modulation amount using envelope and lfo
  envelopeMod->update();
  lastLFOValue = lfo->next()+128;

  modulatorAmount = (uint32_t)(envelopeMod->next() * lastLFOValue) << 3; //(baseModAmount + lfoModAmount + envModAmount) << 16;
  currentGain     = envelopeAmp->next();  

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


  if(midiNote != lastMidiNote && midiNote != 0)
  {
    carrierFrequency  = Q16n16_mtof(Q8n0_to_Q16n16(midiNote));
    carrier->setFreq_Q16n16(carrierFrequency);
    lastMidiNote = midiNote;
  }
  
  lastParam[SYNTH_PARAMETER_MOD_RATIO] = param[SYNTH_PARAMETER_MOD_RATIO];

  multIndex = param[SYNTH_PARAMETER_MOD_RATIO]/79;//(1023/13);
  float freqshift[13] = {0,0.03125,0.0625,0.125,0.25,0.5,1,1.5,2,2.5,3,3.5,4};
  if (multIndex>0)
  {
    modulationFrequency = carrierFrequency * freqshift[multIndex];
  }
  else
  {
    modulationFrequency = carrierFrequency*((float)1.0 / ((512-param[SYNTH_PARAMETER_MOD_RATIO])+1));
  }

  modulator->setFreq_Q16n16(modulationFrequency);



/*
  
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
  Serial.print(F("setModulationShape("));
  Serial.print(newValue);
  Serial.print(F(") "));

  if (newValue >= 0 && newValue < MAX_FILTER_SHAPE)
  {
    Serial.print(F(" settiing ATTACK "));

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
    Serial.print(F(" settiing DECAY"));

    param[SYNTH_PARAMETER_ENVELOPE_DECAY] = newValue + MIN_FILTER_ENV_DECAY;

    // set sustain volume
    // for first 1/2 of range, sustain is cubic
    // for second 3/4 of range, sustain is maxxed
    // for last part sustain decreases from 255 to 128
    Serial.print(F(" settiing SUSTAIN"));

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

  Serial.println(F(")"));

}






void MutatingFM::setParam(uint8_t paramIndex, uint16_t newValue)
{
  if (param[paramIndex] != newValue)
  {
    param[paramIndex] = newValue;
    if ( paramIndex == SYNTH_PARAMETER_MOD_RATIO)
    {
      setFreqs(0);
    }
    else if ( paramIndex == SYNTH_PARAMETER_MOD_AMOUNT)
    {
      envelopeMod->setADLevels(param[SYNTH_PARAMETER_MOD_AMOUNT] >> 2,0);//param[SYNTH_PARAMETER_FILTER_CUTOFF],min(param[SYNTH_PARAMETER_FILTER_SUSTAIN],param[SYNTH_PARAMETER_FILTER_CUTOFF]));
      /*
      Serial.print(F("envelopeMod ADSR=("));
      Serial.print(param[SYNTH_PARAMETER_ENVELOPE_ATTACK]);
      Serial.print(F(","));
      Serial.print(param[SYNTH_PARAMETER_ENVELOPE_DECAY]);
      Serial.print(F(","));
      Serial.print(lastNoteLength);
      Serial.print(F(","));
      Serial.print(50);
      Serial.print(F(") ADLevels="));
      Serial.print(param[SYNTH_PARAMETER_MOD_AMOUNT] >> 2);
      Serial.print(F(","));
      Serial.print(0);
      Serial.println(F(")"));
      */

    }
    else if (paramIndex == SYNTH_PARAMETER_ENVELOPE_SHAPE)
    {
      setModulationShape(newValue);
      envelopeMod->setTimes(param[SYNTH_PARAMETER_ENVELOPE_ATTACK],param[SYNTH_PARAMETER_ENVELOPE_DECAY],lastNoteLength,50);
      /*
      Serial.print(F("envelopeMod ADSR=("));
      Serial.print(param[SYNTH_PARAMETER_ENVELOPE_ATTACK]);
      Serial.print(F(","));
      Serial.print(param[SYNTH_PARAMETER_ENVELOPE_DECAY]);
      Serial.print(F(","));
      Serial.print(lastNoteLength);
      Serial.print(F(","));
      Serial.print(50);
      Serial.print(F(") ADLevels="));
      Serial.print(param[SYNTH_PARAMETER_MOD_AMOUNT] >> 2);
      Serial.print(F(","));
      Serial.print(0);
      Serial.println(F(")"));
      */
    }
  }
}


uint16_t MutatingFM::getParam(uint8_t paramIndex)
{
  return param[paramIndex];
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
