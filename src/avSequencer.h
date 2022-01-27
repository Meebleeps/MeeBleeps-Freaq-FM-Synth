/*----------------------------------------------------------------------------------------------------------
 * avSequencer.h
 * 
 * Defines a generative sequencer with multiple generative algorithms, musical scale quantisation,
 * variable sequence length (up to 16 steps) and multiple parameter-lock (motion-sequencing) channels
 * 
 * (C) 2021 Meebleeps
*-----------------------------------------------------------------------------------------------------------
*/

#ifndef avSequencer_h
#define avSequencer_h

#include "Arduino.h"
#include "avMidi.h"
#include <EventDelay.h>
#include <ADSR.h>

#ifndef CONTROL_RATE 
#define CONTROL_RATE 256
#endif


#define MAX_SEQUENCE_LENGTH 16
#define MAX_SCALE_LENGTH 12

#define SCALEMODE_MAJOR 0
#define SCALEMODE_MINOR 1
#define SCALEMODE_PENTA 2
#define SCALEMODE_GOA 3
#define SCALEMODE_OCTAVE 4
#define SCALEMODE_FIFTHS 5
#define MAX_SCALE_COUNT 6

#define MUTATE_ALGO_DEFAULT     0
#define MUTATE_ALGO_ARPEGGIATED 1
#define MUTATE_ALGO_DRONE       2
#define MUTATE_ALGO_ARP2        3
#define MAX_MUTATE_ALGO_COUNT   4

#define MUTATE_MAX_OCTAVE_SPREAD 3

#define AUDIO_OUTPUT_LATENCY_COMPENSATION_MICROS 8000
#define SYNC_STEPS_PER_PULSE 2
#define SYNC_STEPS_PER_TAP 4

#define MAX_PARAMETER_LOCKS 6
#define PARAM_LOCK_CHANNEL_0  0
#define PARAM_LOCK_CHANNEL_1  1
#define PARAM_LOCK_CHANNEL_2  2
#define PARAM_LOCK_CHANNEL_3  3
#define PARAM_LOCK_CHANNEL_4  4
#define PARAM_LOCK_CHANNEL_5  5


//#define SEQUENCER_TESTMODE

class MutatingSequencer
{
  public:
    MutatingSequencer();
    MutatingSequencer(byte stepCount, byte scaleCount);

    void start();
    void stop();
    void toggleStart();
    
    void newSequence(byte seqLength);
    void testSequence();
    
    void mutateSequence();
    void mutateSequenceDefault();
    void mutateSequenceArp();
    void mutateNoteLength();
    void mutateMutation();
    
    void nextStep(bool restart);
    bool update(bool restart);      //returns true if sequencer moved to next step
    void syncPulse(int stepsPerClick);   //accepts a sync pulse to synchronise the timer
    int8_t outputSyncPulse(); 
    
    void print();
    void printParameters();
    void printSequence();

    void setScale(uint8_t  scaleMode);
    uint8_t getScale();

    void setTonic(int8_t newTonic);
    int8_t getTonic();

    void setOctave(int8_t newOctave);
    int8_t getOctave();

    void setRetrigger(bool newRetrigState);

    void setParameterLock(byte channel, int value);
    int  getParameterLock(byte channel);
    void clearAllParameterLocks(byte channel);
    
    void      setNextNoteLength(uint16_t newNoteLength);
    uint16_t  getNextNoteLength();

    void setNoteProbability(byte newProbability);
    byte getNoteProbability();

    void setMutationProbability(byte newProbability);
    byte getMutationProbability();
    
    byte getCurrentNote();
    byte getCurrentStep();
    
    byte getSequenceLength();
    void setSequenceLength(byte newLength);

    uint8_t getAlgorithm();
    void setAlgorithm(uint8_t newValue);
    void nextAlgorithm();
    
    uint8_t isRunning();

  protected:
    byte mutationProbability;
    byte noteProbability;
    byte tonicProbability;
    byte mutationAlgorithm;

    byte sequenceLength;
    byte scaleNoteCount;
    uint16_t nextStepNoteLength;

    bool retrigState = false;
    byte retrigStep = 0;

    byte currentNote = 0;
    byte currentStep = 0;

    //TODO: change to store these in uint8_t to halve space, then can store 2 tracks of param locks in same space (with reduced resolution) 
    // parameter  values to get compressed to 8 bits on storage and expanded back out to 10 bits on retrival
    int parameterLocks[MAX_PARAMETER_LOCKS][MAX_SEQUENCE_LENGTH];

    //byte syncPulseSteps;
    byte syncPulseClockDivide;              //todo allow fractional to accelerate clock

    uint32_t timeSinceLastSyncPulseMicros;
    uint32_t lastSyncPulseTimeMicros;
    uint32_t lastStepTimeMicros;
    uint32_t nextStepTimeMicros;
    uint32_t nextStepTimeMillis;
    
    bool running;

    uint8_t currentScaleMode;
    byte tonicNote;     
    byte octave;               

    byte bpm;
    byte notes[MAX_SEQUENCE_LENGTH];
    byte scaleNotes[MAX_SCALE_LENGTH];                  
    byte octaveSpread;

    byte duckingCounter;
    
  private:
    bool _debugOutput;
    void initialiseScale(int scaleMode);
    EventDelay stepTimer;
    void setNextStepTimer();
    bool syncPulseLive;
    uint32_t syncPulseCount;


    bool ignoreNextSyncPulse;
    
    
};

#endif
