/*----------------------------------------------------------------------------------------------------------
 * avSequencerMultiTrack.h
 * 
 * Defines a multitrack generative sequencer with multiple generative algorithms, musical scale quantisation,
 * variable sequence length (up to 16 steps) and multiple parameter-lock (motion-sequencing) channels
 * 
 * (C) 2021-2022 Meebleeps
*-----------------------------------------------------------------------------------------------------------
*/

#ifndef avSequencerMultiTrack_h
#define avSequencerMultiTrack_h

#include "avSequencer.h"
#include "Arduino.h"
#include <EventDelay.h>
#include <ADSR.h>

#define MAX_SEQUENCER_TRACKS 2

class MutatingSequencerMultiTrack : MutatingSequencer
{
  public:
    MutatingSequencerMultiTrack();

    using MutatingSequencer::newSequence;
    using MutatingSequencer::outputSyncPulse;
    using MutatingSequencer::syncPulse;
    using MutatingSequencer::update;
    using MutatingSequencer::getNextNoteLength;
    using MutatingSequencer::setNextNoteLength;
    using MutatingSequencer::toggleStart;
    using MutatingSequencer::nextAlgorithm;
    using MutatingSequencer::getAlgorithm;
    using MutatingSequencer::setMutationProbability;
    using MutatingSequencer::setNoteProbability;
    using MutatingSequencer::getTonic;
    using MutatingSequencer::setTonic;
    using MutatingSequencer::getScale;
    using MutatingSequencer::setScale;
    using MutatingSequencer::setOctave;
    using MutatingSequencer::getOctave;
    using MutatingSequencer::isRunning;
    

    void setParameterLock(byte channel, int value);
    int  getParameterLock(byte channel);
    void clearAllParameterLocks(byte channel);

    void setParameterLock(byte track, byte channel, int value);
    int  getParameterLock(byte track, byte channel);
    uint16_t getParameterLock(byte channel, byte track, byte step);
    void clearAllParameterLocks(byte track, byte channel);

    byte getCurrentNote();
    byte getCurrentNote(byte track);
    byte getCurrentStep();
    byte getCurrentStep(byte track);
    
    void    setOctaveOffsetTrack1(int8_t offset);
    int8_t  getOctaveOffsetTrack1();

    byte getSequenceLength();
    byte getSequenceLength(byte track);

    void setSequenceLength(byte newLength);
    void setSequenceLength(byte track, byte newLength);

    void mutateSequence();
    void mutateSequenceDefault();
    void mutateSequenceArp();
    void mutateSequenceDrone();
    void mutateSequenceArp2();

    void nextStep(bool restart);
    bool update(bool restart);      //returns true if sequencer moved to next step    

  protected:

    byte currentTrackNote[MAX_SEQUENCER_TRACKS] = {0,0};   
    byte currentTrackStep[MAX_SEQUENCER_TRACKS] = {0,0};                    
    byte trackSequenceLength[MAX_SEQUENCER_TRACKS] = {16,16};
    
    int8_t octaveOffsetTrack1;

};

#endif
