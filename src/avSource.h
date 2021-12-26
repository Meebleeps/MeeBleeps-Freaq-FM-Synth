/*
  avSequencer.h - defines a mutating generative sequencer

*/

#ifndef avSource_h
#define avSource_h
#include "Arduino.h"
#include "PDResonant.h"
#include <EventDelay.h>
#include <MozziGuts.h>
#include <Oscil.h>
#include <tables/sin2048_int8.h> // sine table for oscillator
#include <mozzi_fixmath.h>
#include <ADSR.h>

#include <LowPassFilter.h>

//#define DEBUG_MODE_VERBOSE

// define this here to avoid clipping the filter function
#define MAX_FILTER_RESONANCE  100
#define MAX_FILTER_CUTOFF     240
#define MAX_FILTER_SHAPE      1023
#define MAX_FILTER_ENV_ATTACK 4096
#define MAX_FILTER_ENV_DECAY  4096
#define MIN_FILTER_ENV_DECAY  40
#define MAX_SOURCE_PARAMS     9

#define SYNTH_PARAMETER_MOD_AMOUNT            1
#define SYNTH_PARAMETER_MOD_RATIO             2
#define SYNTH_PARAMETER_MOD_AMOUNT_LFO        6
#define SYNTH_PARAMETER_MOD_AMOUNT_LFODEPTH   3
#define SYNTH_PARAMETER_ENVELOPE_SUSTAIN      4
#define SYNTH_PARAMETER_ENVELOPE_SHAPE        5
#define SYNTH_PARAMETER_ENVELOPE_ATTACK     7
#define SYNTH_PARAMETER_ENVELOPE_DECAY      8

#define SYNTH_PARAMETER_OSC2TUNE          0



class MutatingSource
{
  public:
    MutatingSource();

    virtual int noteOn(byte pitch, byte velocity, unsigned int length);
    virtual int noteOff();
    virtual inline int updateAudio();
    virtual inline void updateControl();
    virtual int mutate();
    virtual void setGain(byte gain);
    virtual void setParam(uint8_t paramIndex, uint16_t newValue);
    virtual uint16_t getParam(byte paramIndex);
    
    
  protected:
    uint16_t param[MAX_SOURCE_PARAMS];
    uint16_t lastParam[MAX_SOURCE_PARAMS];

    EventDelay noteOffTimer;
    
};

class MutatingFM : public MutatingSource
{
  public:
    MutatingFM();

    int noteOn(uint8_t pitch, uint8_t velocity, uint16_t length);  
    int noteOff();
    int updateAudio();
    void updateControl();
    int mutate();
    void setParam(uint8_t paramIndex, uint16_t newValue);
    uint16_t getParam(uint8_t paramIndex);
    
    void setGain(uint8_t gain);

    void setOscillator(uint8_t oscIndex);
    uint8_t getLFOValue();
    void  setLFOFrequency(float freq);
    void  setLFODepth(uint8_t depth);
    void setModulationShape(uint16_t newValue);


  protected:
    
    // for FM oscillator
    void setFreqs(uint8_t midiNote);

    Q16n16 carrierFrequency;
    Q16n16 modulationFrequency;
    Q16n16 modulatorAmount;

    uint8_t  currentGain;
    uint8_t  masterGain;
    uint8_t lastLFOValue;

    Oscil<SIN2048_NUM_CELLS, AUDIO_RATE>* carrier;
    Oscil<SIN2048_NUM_CELLS, AUDIO_RATE>* modulator;
    ADSR <CONTROL_RATE, CONTROL_RATE>* envelopeAmp;
    ADSR <CONTROL_RATE, CONTROL_RATE>* envelopeMod;
    Oscil <SIN2048_NUM_CELLS, CONTROL_RATE>* lfo;

  private:
    uint8_t lastMidiNote;
    unsigned int lastNoteLength;


};

#endif
