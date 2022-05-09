/*
  avSource.h - defines a sound source using a virtual base class and a derived class which implements the particular sound source
   
  in this case, a 2-operator FM voice with envelope & lfo modulation control
*/

#ifndef avSource_h
#define avSource_h
#include "Arduino.h"

#include "avMidi.h"
#include "MutantFMSynthOptions.h"

#include <MozziGuts.h>
#include <EventDelay.h>
#include <Oscil.h>
#include <tables/sin2048_int8.h> // sine table for oscillators & LFO
#include <tables/saw2048_int8.h> // saw table for LFO
#include "revsaw2048_int8.h" // reverse saw table for LFO
#include "square2048_int8.h" // square table for LFO

/** @brief if COMPILE_SMALLER_BINARY flag is defined, omit the pseudorandom waveform */
#ifndef COMPILE_SMALLER_BINARY
#include "pseudorandom2048_int8.h" // noise table for LFO
#endif

#include "nullwaveform2048_int8.h" // zero table for LFO - used to turn carrier off
#include <mozzi_fixmath.h>
#include <ADSR.h>

#include <LowPassFilter.h>

//#define DEBUG_MODE_VERBOSE

// define this here to avoid clipping the filter function
#define MAX_FILTER_RESONANCE  100
#define MAX_FILTER_CUTOFF     240
#define MAX_FILTER_SHAPE      1023
#define MAX_FILTER_ENV_ATTACK 4096
#define MAX_SOURCE_PARAMS     9

#define SYNTH_PARAMETER_MOD_AMOUNT            1
#define SYNTH_PARAMETER_MOD_RATIO             2
#define SYNTH_PARAMETER_MOD_AMOUNT_LFO        6
#define SYNTH_PARAMETER_MOD_AMOUNT_LFODEPTH   3
#define SYNTH_PARAMETER_ENVELOPE_SUSTAIN      4
#define SYNTH_PARAMETER_ENVELOPE_SHAPE        5
#define SYNTH_PARAMETER_ENVELOPE_ATTACK     7
#define SYNTH_PARAMETER_ENVELOPE_DECAY      8
#define SYNTH_PARAMETER_NOTE_DECAY          -2  // this isn't implemented in the synth voice - it's in the sequencer, probably should be though
#define SYNTH_PARAMETER_UNKNOWN             -1



#define SYNTH_PARAMETER_OSC2TUNE          0

#define FM_MODE_EXPONENTIAL 0
#define FM_MODE_LINEAR_HIGH 1
#define FM_MODE_LINEAR_LOW  2
#define FM_MODE_FREE        3
#define MAX_FM_MODES        4

/** @brief if COMPILE_SMALLER_BINARY flag is defined, omit the pseudorandom waveform */
#ifdef COMPILE_SMALLER_BINARY

  #define MAX_LFO_WAVEFORMS       5
  #define MAX_CARRIER_WAVEFORMS   5
  #define MAX_MODULATOR_WAVEFORMS 5

  #define WAVEFORM_SIN          0
  #define WAVEFORM_SAW          1
  #define WAVEFORM_REVSAW       2
  #define WAVEFORM_SQUARE       3
  #define WAVEFORM_NULL         4

#else
  #define MAX_LFO_WAVEFORMS       6
  #define MAX_CARRIER_WAVEFORMS   6
  #define MAX_MODULATOR_WAVEFORMS 6

  #define WAVEFORM_SIN          0
  #define WAVEFORM_SAW          1
  #define WAVEFORM_REVSAW       2
  #define WAVEFORM_SQUARE       3
  #define WAVEFORM_PSEUDORANDOM 4
  #define WAVEFORM_NULL         5
#endif


// increase for wider LFO depth range
// LFO depth will be multiplied by 2^n using a bitshift
// 0 value should be optimised out by the compiler
#define MOD_DEPTH_MULTIPLIER_LFO 0

// increase for wider ENV depth range
// ENV depth will be multiplied by 2^n using a bitshift
// 0 value should be optimised out by the compiler
#define MOD_DEPTH_MULTIPLIER_ENV 0

// this is the shortest decay time that can be audible with the mod envelope with the given CONTROL_RATE
#define MIN_MODULATION_ENV_TIME 30

//#define SYNTH_MODULATION_UPDATE_DIVIDER 2

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
    void setModulationShape(uint16_t newValue);

    void toggleFMMode();
    uint8_t getFMMode();

    void toggleLFOWaveform();
    uint8_t getLFOWaveform();

    void toggleCarrierWaveform();
    uint8_t getCarrierWaveform();

    void toggleModulatorWaveform();
    uint8_t getModulatorWaveform();

  protected:
    
    // for FM oscillator
    void setFreqs(uint8_t midiNote);

    Q16n16 carrierFrequency;
    Q16n16 modulationFrequency;
    Q16n16 modulatorAmount;

    uint8_t  currentGain;
    uint8_t  masterGain;
    uint8_t masterGainBitShift;
    uint8_t lastLFOValue;
    uint8_t fmMode;
    uint8_t lfoWaveform;
    uint8_t carrierWaveform;
    uint8_t modulatorWaveform;

    Oscil<SIN2048_NUM_CELLS, AUDIO_RATE>* carrier;
    Oscil<SIN2048_NUM_CELLS, AUDIO_RATE>* modulator;
    ADSR <CONTROL_RATE, CONTROL_RATE>* envelopeAmp;
    ADSR <CONTROL_RATE, CONTROL_RATE>* envelopeMod;
    Oscil <SIN2048_NUM_CELLS, CONTROL_RATE>* lfo;

  private:
    uint8_t lastMidiNote;
    unsigned int lastNoteLength;
    uint8_t updateCount;


};

#endif
