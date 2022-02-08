/*----------------------------------------------------------------------------------------------------------
 * avSequencerMultiTrack.h
 * 
 * Implements a multitrack generative sequencer with multiple generative algorithms, musical scale quantisation,
 * variable sequence length (up to 16 steps) and multiple parameter-lock (motion-sequencing) channels
 * 
 * (C) 2021 Meebleeps
*-----------------------------------------------------------------------------------------------------------
*/
#include <mozzi_rand.h>
#include <MozziGuts.h>

#include "Arduino.h"
#include "avSequencerMultiTrack.h"



/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencerMultiTrack::MutatingSequencerMultiTrack()
 * initialises the sequencer
 *----------------------------------------------------------------------------------------------------------
 */
MutatingSequencerMultiTrack::MutatingSequencerMultiTrack()
{
  // all other settings 

  for (uint8_t i=0; i<MAX_SEQUENCER_TRACKS; i++) 
  {
    currentTrackStep[i]    = 0;
    trackSequenceLength[i] = 16;
    currentTrackNote[i]    = tonicNote;
  }

  // setup different defaults for this synth due to the "busy" nature of having 2 tracks 
  bpm             = 60;
  noteProbability = 30;
}

/*---------------------------------------------------------------------------------------------------------------
 * MutatingSequencerMultiTrack::update()
 * overrides MutatingSequencer::update() so that MutatingSequencerMultiTrack::nextStep is called instead of MutatingSequencer::nextStep
 *---------------------------------------------------------------------------------------------------------------
 */
bool MutatingSequencerMultiTrack::update(bool restart)
{
  if (MutatingSequencer::update(restart))
  {
    nextStep(restart);
    mutateSequence();
    
    return true;
  }
  else
  {
    return false;
  }
}


/*---------------------------------------------------------------------------------------------------------------
 * MutatingSequencerMultiTrack::nextStep()
 * Overrides nextStep to provide multi-track sequencing
 *---------------------------------------------------------------------------------------------------------------
 */
void MutatingSequencerMultiTrack::nextStep(bool restart)
{
  MutatingSequencer::nextStep(restart);

  if (restart)
  {
    for (uint8_t i=0; i < MAX_SEQUENCER_TRACKS; i++) currentTrackStep[i] = 0; 
    duckingCounter = 0;
  }
  else
  {
    for (uint8_t i=0; i < MAX_SEQUENCER_TRACKS; i++) currentTrackStep[i] = ++currentTrackStep[i] % trackSequenceLength[i];
    duckingCounter++;
  }

  #ifndef ENABLE_MIDI_OUTPUT
  //Serial.print(F("MutatingSequencerMultiTrack::nextStep()  step="));
  //Serial.println(currentTrackStep[0]);
  #endif

  currentNote = notes[currentTrackStep[0]];

}


/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencerMultiTrack::getCurrentNote()
 * gets the note value for the current step
 *----------------------------------------------------------------------------------------------------------
 */
byte MutatingSequencerMultiTrack::getCurrentNote()
{
  return getCurrentNote(0);
}



/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencerMultiTrack::getCurrentNote()
 * gets the note value for the current step for track n
 *----------------------------------------------------------------------------------------------------------
 */
byte MutatingSequencerMultiTrack::getCurrentNote(byte track)
{
  byte theNote;
 
  if (!retrigState)
  {
    theNote = notes[currentTrackStep[track]];
  }
  else
  {
    theNote = notes[retrigStep];
  }
 
  if (theNote == 0 || track != 0 || octaveOffsetTrack1 == 0)
  {
    return theNote;
  }
  else
  {
    return theNote + (12*octaveOffsetTrack1);
  }
}


/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencerMultiTrack::getCurrentStep()
 * gets  the current playing step
 *----------------------------------------------------------------------------------------------------------
 */
byte MutatingSequencerMultiTrack::getCurrentStep()
{
  return getCurrentStep(0);
}


/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencerMultiTrack::getCurrentStep()
 * gets  the current playing step
 *----------------------------------------------------------------------------------------------------------
 */
byte MutatingSequencerMultiTrack::getCurrentStep(byte track)
{
  if(!retrigState)
  {
    return currentTrackStep[track];
  }
  else
  {
    return retrigStep;
  }
  
}

/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencerMultiTrack::setOctaveOffsetTrack1()
 * allows the setting of an octave offset for track 0
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingSequencerMultiTrack::setOctaveOffsetTrack1(int8_t offset)
{
  if (offset >= -3 && offset <= 3)
  {
    octaveOffsetTrack1 = offset;
  }
}


/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencerMultiTrack::getOctaveOffsetTrack1()
 * gets  the current playing step
 *----------------------------------------------------------------------------------------------------------
 */
int8_t MutatingSequencerMultiTrack::getOctaveOffsetTrack1()
{
  return octaveOffsetTrack1;
}



     



/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencerMultiTrack::setparameterLocks()
 * records the given value as a parameter lock on the given channel
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingSequencerMultiTrack::setParameterLock(byte channel, int value)
{
  uint8_t recordStep;

  if (trackSequenceLength[1] > trackSequenceLength[0])
  {
    recordStep = currentTrackStep[1];
  }
  else
  {
    recordStep = currentTrackStep[0];
  }

  parameterLocks[channel][recordStep] = value;
}


/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencerMultiTrack::getparameterLocks()
 * returns the parameter lock on the given channel for the current step
 *----------------------------------------------------------------------------------------------------------
 */
int MutatingSequencerMultiTrack::getParameterLock(byte channel)
{
  return parameterLocks[channel][currentTrackStep[0]];
}


/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencerMultiTrack::getparameterLocks()
 * returns the parameter lock on the given channel for the current step
 *----------------------------------------------------------------------------------------------------------
 */
uint16_t MutatingSequencerMultiTrack::getParameterLock(byte channel, byte track, byte step)
{
  // todo: return parameter lock per track
  return parameterLocks[channel][step];
}



/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencerMultiTrack::clearAllParameterLocks()
 * clears all parameter locks for the given channel
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingSequencerMultiTrack::clearAllParameterLocks(byte channel)
{
  for (uint8_t i = 0; i < MAX_SEQUENCE_LENGTH; i++)
  {
    parameterLocks[channel][i] = 0;
  }
}



/*---------------------------------------------------------------------------------------------------------------
 * mutateSequence
 * mutate the current sequence according to the selected mutation algorithm
 *---------------------------------------------------------------------------------------------------------------
 */
void MutatingSequencerMultiTrack::mutateSequence()
{

  switch(mutationAlgorithm)
  {
    case MUTATE_ALGO_DEFAULT:
      mutateSequenceDefault();
      break;

    case MUTATE_ALGO_ARPEGGIATED:
      mutateSequenceArp();
      break;

    case MUTATE_ALGO_DRONE:
      mutateSequenceDrone();
      break;

    case MUTATE_ALGO_ARP2:
      mutateSequenceArp2();
      break;


  }
}


/*---------------------------------------------------------------------------------------------------------------
 * mutateSequenceDefault
 * default mutation algorithm
 *---------------------------------------------------------------------------------------------------------------
 */
void MutatingSequencerMultiTrack::mutateSequenceDefault()
{
  int seqStep;

  if(rand(100) < mutationProbability)
  {
 
    seqStep = rand(MAX_SEQUENCE_LENGTH);

    // don't mutate the retrig step if the sequencer is currently retriggering
    // allows the rest of the sequence to mutate while the retrigged step is held
    if (!retrigState || seqStep != retrigStep)
    { 
      if (rand(100) < noteProbability)
      {
        if (rand(100) < tonicProbability) 
        {
          notes[seqStep] = tonicNote + (12*octave) + (12*rand(octaveSpread));
        }
        else
        {
                        //root        //octave        //scale note                        //octave
          notes[seqStep] = tonicNote + (12*octave) + scaleNotes[rand(scaleNoteCount)] + (12*rand(octaveSpread));
        }

        // randomise the deviation
        /*
        if (rand(100) < noteProbability)
        {
          if (rand(100) < noteProbability)
          {
            parameterLocks[1][seqStep] = rand(1024);
          }
          else
          {
            parameterLocks[1][seqStep] = 0;
          }
        }
        */
      }
      else
      {
        notes[seqStep] = 0;
      }
      
      // mutate parameters
    }
  }
}


/*---------------------------------------------------------------------------------------------------------------
 * mutateSequenceArp
 * arpegiated mutation algorithm
 *---------------------------------------------------------------------------------------------------------------
 */
void MutatingSequencerMultiTrack::mutateSequenceArp()
{
  int8_t startStep;
  int8_t startOctave;
  uint8_t startNote;

  uint8_t seqStep;
  uint8_t seqNote;
  uint8_t runLength;
  uint8_t runDirection;
  uint8_t stepSize;
 

  // include the default mutation
  mutateSequenceDefault();

  if(rand(100) < mutationProbability)
  {
    runLength = 3;
    runDirection = 1;//rand(2) - 1; 

    // put in a run of scale notes starting at a random step on a random scalenote in a random octave
    startStep   = rand(MAX_SEQUENCE_LENGTH);
    startNote   = rand(scaleNoteCount);
    startOctave = octave + rand(octaveSpread);
    stepSize    = rand(2) + 1;

    if (rand(100) < noteProbability)
    {
      #ifndef ENABLE_MIDI_OUTPUT
      Serial.print(F("new arpeggio run: "));
      #endif

      for (uint8_t i = 0; i < runLength; i++)
      {
        seqStep = (startStep + i) % MAX_SEQUENCE_LENGTH;
        seqNote = scaleNotes[(startNote + (i*stepSize*runDirection) ) % scaleNoteCount];
        notes[seqStep] = tonicNote + seqNote + (12*(startOctave + (startNote + i < scaleNoteCount ? 0 : 1)));
        #ifndef ENABLE_MIDI_OUTPUT
        Serial.print(notes[seqStep]);
        Serial.print(F(","));
        #endif
      }
      #ifndef ENABLE_MIDI_OUTPUT
      Serial.println();
      #endif  
    }
  }

}

/*---------------------------------------------------------------------------------------------------------------
 * mutateSequenceArp2
 * arpegiated mutation algorithm
 *---------------------------------------------------------------------------------------------------------------
 */
void MutatingSequencerMultiTrack::mutateSequenceArp2()
{
  for (uint8_t i = 0; i < trackSequenceLength[0]; i++)
  {
    notes[i] = tonicNote + (12*octave) + scaleNotes[i % (min(trackSequenceLength[0],scaleNoteCount))];
  }
}


/*---------------------------------------------------------------------------------------------------------------
 * mutateSequenceDrone
 * one tonic note per sequence
 *---------------------------------------------------------------------------------------------------------------
 */
void MutatingSequencerMultiTrack::mutateSequenceDrone()
{

  notes[0] = tonicNote + (12*octave);

  for (int i=1; i < MAX_SEQUENCE_LENGTH; i++)
  {
    notes[i] = 0;
  }
}


/*---------------------------------------------------------------------------------------------------------------
 * getSequenceLength
 *---------------------------------------------------------------------------------------------------------------
 */
byte MutatingSequencerMultiTrack::getSequenceLength()
{
  return trackSequenceLength[0];  
}


/*---------------------------------------------------------------------------------------------------------------
 * getSequenceLength
 * returns the length of the given track
 *---------------------------------------------------------------------------------------------------------------
 */
byte MutatingSequencerMultiTrack::getSequenceLength(byte track)
{
  return trackSequenceLength[track];  
}


/*---------------------------------------------------------------------------------------------------------------
 * setSequenceLength
 * sets the length of track 0
 *---------------------------------------------------------------------------------------------------------------
 */
void MutatingSequencerMultiTrack::setSequenceLength(byte newLength)
{
  setSequenceLength(0, newLength);
}


/*---------------------------------------------------------------------------------------------------------------
 * setSequenceLength
 * sets the length of the given track
 *---------------------------------------------------------------------------------------------------------------
 */
void MutatingSequencerMultiTrack::setSequenceLength(byte track, byte newLength)
{
  if(newLength > 0 && newLength <= MAX_SEQUENCE_LENGTH && trackSequenceLength[track] != newLength)
  {
    trackSequenceLength[track] = newLength;
    #ifndef ENABLE_MIDI_OUTPUT
    Serial.print(F("sequenceLength="));
    Serial.println(trackSequenceLength[track]);
    #endif
  }
}


