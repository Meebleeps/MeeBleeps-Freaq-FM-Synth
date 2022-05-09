/*----------------------------------------------------------------------------------------------------------
 * avSequencer.cpp
 * 
 * Implements a generative sequencer with multiple generative algorithms, musical scale quantisation,
 * variable sequence length (up to 16 steps) and multiple parameter-lock (motion-sequencing) channels
 * 
 * (C) 2021-2022 Meebleeps
*-----------------------------------------------------------------------------------------------------------
*/
#include <mozzi_rand.h>
#include <MozziGuts.h>

#include "Arduino.h"
#include "avSequencer.h"


/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::MutatingSequencer()
 * initialises the sequencer
 *----------------------------------------------------------------------------------------------------------
 */
MutatingSequencer::MutatingSequencer()
{
  running             = false;
  mutationAlgorithm   = MUTATE_ALGO_DEFAULT;

  // initialise sequence
  for (uint8_t i = 0; i < MAX_SEQUENCE_LENGTH; i++)
  {
    notes[i] = 0;
  }
  
  for (uint8_t i = 0; i < MAX_SCALE_LENGTH; i++)
  {
    scaleNotes[i] = 0;
  }

  for (uint8_t i = 0; i < MAX_SEQUENCE_LENGTH; i++)
  {
    for (uint8_t m = 0; m < MAX_PARAMETER_LOCKS; m++)
    {
      parameterLocks[m][i] = 0;
    }
  }

  currentStep         = 0;  
  retrigStep          = 0;     
  retrigState         = false;                        
  tonicNote           = 9;   // default to A
  octave              = 1;                           
  mutationProbability = 0;
  noteProbability      = 70;
  tonicProbability    = 30;
  sequenceLength      = 16;
  bpm                 = 130;
  currentNote         = tonicNote;
  nextStepNoteLength  = 80;
  octaveSpread        = 3;  
  scaleNoteCount      = 7;

  // deleted this to allow for different pulse steps defined by the UI layer
  //syncPulseSteps        = 2;      // standard 2 steps per sync pulse as per volca
  syncPulseClockDivide  = 1;      // divide the clock by this many steps
  
  nextStepTimeMillis  = (15000 / bpm); // 16th note length for given BPM
  
  lastSyncPulseTimeMicros = 0;
  
  initialiseScale(SCALEMODE_PENTA);
  newSequence(sequenceLength);

  _debugOutput        = true;
  printParameters();
}



/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::start()
 * starts the sequencer at step 0
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingSequencer::start()
{
  #ifndef ENABLE_MIDI_OUTPUT
  Serial.println(F("Sequencer.Start()"));
  #endif
  running             = true;
  ignoreNextSyncPulse = true;
  //nextStep(true);
}




/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::stop()
 * stops the sequencer
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingSequencer::stop()
{
  #ifndef ENABLE_MIDI_OUTPUT
  Serial.println(F("Sequencer.Stop()"));
  #endif
  running = false;
}


/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::toggleStart()
 * starts the sequencer if it is stopped, otherwise stops it
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingSequencer::toggleStart()
{
  if (!running)
  {
    start();
  }
  else
  {
    stop();  
  }
}



/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::setScale()
 * sets the scale and generates a new sequence according to the new scale
 *----------------------------------------------------------------------------------------------------------
 */
 void MutatingSequencer::setScale(uint8_t scaleMode)
{
  initialiseScale(scaleMode);
  newSequence(sequenceLength);
  #ifndef ENABLE_MIDI_OUTPUT
  Serial.print(F("NEW SCALE: "));
  Serial.println(scaleMode);
  #endif
}


/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::getScale()
 * returns the current scale mode
 *----------------------------------------------------------------------------------------------------------
 */
 uint8_t MutatingSequencer::getScale()
{
  return currentScaleMode;
}


/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::getAlgorithm()
 * returns the current mutation algorithm
 *----------------------------------------------------------------------------------------------------------
 */
uint8_t MutatingSequencer::getAlgorithm()
{
  return mutationAlgorithm;
}


/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::setAlgorithm()
 * sets the current mutation algorithm
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingSequencer::setAlgorithm(uint8_t newValue)
{
  mutationAlgorithm = newValue % MAX_MUTATE_ALGO_COUNT;
}

/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::incrAlgorithm()
 * moves to the next algorithm
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingSequencer::nextAlgorithm()
{
  mutationAlgorithm = (mutationAlgorithm + 1) % MAX_MUTATE_ALGO_COUNT;
  #ifndef ENABLE_MIDI_OUTPUT
  Serial.print(F("New Algorithm = "));
  Serial.println(mutationAlgorithm);
  #endif
}




/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::getCurrentNote()
 * gets the note value for the current step
 *----------------------------------------------------------------------------------------------------------
 */
byte MutatingSequencer::getCurrentNote()
{
  if (!retrigState)
  {
    return notes[currentStep];
  }
  else
  {
    return notes[retrigStep];
  }
}


/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::getCurrentStep()
 * gets  the current playing step
 *----------------------------------------------------------------------------------------------------------
 */
byte MutatingSequencer::getCurrentStep()
{
  if(!retrigState)
  {
    return currentStep;
  }
  else
  {
    return retrigStep;
  }
  
}


/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::setRetrigger()
 * sets retrig on or off
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingSequencer::setRetrigger(bool newRetrigState)
{
  if (newRetrigState != retrigState)
  {
    if(newRetrigState)
    {
      retrigState = true;
      retrigStep = currentStep;
    }
    else
    {
      retrigState = false;
    }
  }
}


/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::setparameterLocks()
 * records the given value as a parameter lock on the given channel
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingSequencer::setParameterLock(byte channel, int value)
{
  parameterLocks[channel][currentStep] = value;
}


/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::getparameterLocks()
 * returns the parameter lock on the given channel for the current step
 *----------------------------------------------------------------------------------------------------------
 */
int MutatingSequencer::getParameterLock(byte channel)
{
  return parameterLocks[channel][currentStep];
}


/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::clearAllParameterLocks()
 * clears all parameter locks for the given channel
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingSequencer::clearAllParameterLocks(byte channel)
{
  for (uint8_t i = 0; i < MAX_SEQUENCE_LENGTH; i++)
  {
    parameterLocks[channel][i] = 0;
  }
}



/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::setTonic()
 * sets a new tonic
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingSequencer::setTonic(int8_t newTonic)
{
  int8_t noteShift;
  
  if (newTonic != tonicNote)
  { 
    #ifndef ENABLE_MIDI_OUTPUT
    Serial.print(F("OLD TONIC: "));
    Serial.print(tonicNote);
    Serial.print(F(", NEW TONIC: "));
    Serial.println(newTonic);
    #endif

    noteShift = newTonic - tonicNote;
    tonicNote = newTonic;
    
    // update the whole sequence to the new tonic
    for(int i=0; i< MAX_SEQUENCE_LENGTH; i++)
    {
      if(notes[i] > 0) notes[i] += noteShift;      
    }
    printSequence();
  }
}

/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::setTonic()
 * sets a new tonic
 *----------------------------------------------------------------------------------------------------------
 */
int8_t MutatingSequencer::getTonic()
{
  return tonicNote;
}



/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::setOctave()
 * sets a new octave
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingSequencer::setOctave(int8_t newOctave)
{
  int8_t noteShift;
  
  if (newOctave != octave)
  { 
    noteShift = (newOctave - octave) * 12;
    octave = newOctave;
    
    // update the whole sequence to the new tonic
    for(int i=0; i< MAX_SEQUENCE_LENGTH; i++)
    {
      if(notes[i] > 0) notes[i] += noteShift;      
    }

    printSequence();
  }

}

/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::getOctave()
 * returns the current octave
 *----------------------------------------------------------------------------------------------------------
 */
int8_t MutatingSequencer::getOctave()
{
  return octave;
}



/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::initialiseScale()
 * sets up the notes of each scale
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingSequencer::initialiseScale(int scaleMode)
{
  currentScaleMode = scaleMode % MAX_SCALE_COUNT;
  
  switch(scaleMode)
  {
    case SCALEMODE_MINOR:
      scaleNotes[0] = 0;
      scaleNotes[1] = 2;
      scaleNotes[2] = 3;
      scaleNotes[3] = 5;
      scaleNotes[4] = 7;
      scaleNotes[5] = 8;
      scaleNotes[6] = 10;
      scaleNoteCount = 7;
      break;
     
    case SCALEMODE_MAJOR:
      scaleNotes[0] = 0;
      scaleNotes[1] = 2;
      scaleNotes[2] = 4;
      scaleNotes[3] = 5;
      scaleNotes[4] = 7;
      scaleNotes[5] = 9;
      scaleNotes[6] = 11;
      scaleNoteCount = 7;
      break;

    case SCALEMODE_PENTA:
      scaleNotes[0] = 0;
      scaleNotes[1] = 3;
      scaleNotes[2] = 5;
      scaleNotes[3] = 7;
      scaleNotes[4] = 10;
      scaleNoteCount = 5;
      break;

    case SCALEMODE_GOA:
      scaleNotes[0] = -2;
      scaleNotes[1] = 0;
      scaleNotes[2] = 1;
      scaleNotes[3] = 13;
      scaleNotes[4] = 0;
      scaleNotes[5] = 7;
      scaleNotes[6] = 8;
      scaleNotes[7] = 8;
      scaleNoteCount = 8;
      break;

    case SCALEMODE_OCTAVE:
      scaleNotes[0] = 0;
      scaleNotes[1] = 12;
      scaleNoteCount = 2;
      break;

    case SCALEMODE_FIFTHS:
      scaleNotes[0] = 0;
      scaleNotes[1] = 7;
      scaleNotes[2] = 12;
      scaleNoteCount = 3;
      break;


  }
}



/*---------------------------------------------------------------------------------------------------------------
 * MutatingSequencer::update()
 * Call this function every time Mozzi updates controls at CONTROL_RATE
 * if stepTimer is ready, advance to next step and return true
 *---------------------------------------------------------------------------------------------------------------
 */
bool MutatingSequencer::update(bool restart)
{
  bool processStep = false;
  
  if (running)
  {
    if (restart || (stepTimer.ready() || syncPulseLive))
    {
      nextStep(restart);
      processStep = true;
    }
  }

  return processStep;
}



/*---------------------------------------------------------------------------------------------------------------
 * MutatingSequencer::nextStep()
 * Move ths sequencer to the next step
 *---------------------------------------------------------------------------------------------------------------
 */
void MutatingSequencer::nextStep(bool restart)
{
  if (restart)
  {
    currentStep    = 0; 
    duckingCounter = 0;
  }
  else
  {
    currentStep = (currentStep + 1) % sequenceLength;
    duckingCounter++;
  }
  #ifndef ENABLE_MIDI_OUTPUT
  //Serial.print(F("nextStep()  step="));
  //Serial.println(currentStep);
  #endif


  currentNote = notes[currentStep];

  

  setNextStepTimer();

}


/*---------------------------------------------------------------------------------------------------------------
 * MutatingSequencer::setNextStepTimer()
 * Move ths sequencer to the next step
 *---------------------------------------------------------------------------------------------------------------
 */
void MutatingSequencer::setNextStepTimer()
{
  uint32_t currentStepTimeMicros;
  uint32_t timeElapsedSincePulseMicros;
  uint32_t forecastTimeToNextStepMicros;
  
  currentStepTimeMicros = mozziMicros();
  
  #ifndef ENABLE_MIDI_OUTPUT
  Serial.print(F("step time micros="));
  Serial.println(currentStepTimeMicros - lastStepTimeMicros);
  #endif

  if(syncPulseLive)
  {
    // this step is the first one after a sync pulse
    // adjust the next step 

    timeElapsedSincePulseMicros = currentStepTimeMicros - lastSyncPulseTimeMicros;

    if(nextStepTimeMillis*1000 > timeElapsedSincePulseMicros)
    {
      forecastTimeToNextStepMicros = nextStepTimeMillis*1000 - timeElapsedSincePulseMicros;
    }
    else
    {
      forecastTimeToNextStepMicros = nextStepTimeMillis*1000;  
    }
    #ifndef ENABLE_MIDI_OUTPUT
    //Serial.print(F(" adjusted="));
    //Serial.println(forecastTimeToNextStepMicros);
    #endif

    stepTimer.start(forecastTimeToNextStepMicros/1000 + 3); 
    
    syncPulseLive = false;
  }
  else
  {
    stepTimer.start(nextStepTimeMillis + 3); 
  }
  
  lastStepTimeMicros = currentStepTimeMicros;
  
}


/*---------------------------------------------------------------------------------------------------------------
 * returns true if the sequencer should output a sync pulse
 * will immediately reset the timer so that next call to update will trigger nextStep()
 * this allows for resyncing the sequence if the trigger tempo changes.
 * then sets the timer for the next step to 
 *---------------------------------------------------------------------------------------------------------------
 */
int8_t MutatingSequencer::outputSyncPulse()
{
  return (duckingCounter % SYNC_STEPS_PER_PULSE) == 0;
}



/*---------------------------------------------------------------------------------------------------------------
 * Call this function whenever a sync pulse is detected.  
 * will immediately reset the timer so that next call to update will trigger nextStep()
 * this allows for resyncing the sequence if the trigger tempo changes.
 * then sets the timer for the next step to 
 *---------------------------------------------------------------------------------------------------------------
 */
void MutatingSequencer::syncPulse(int stepsPerClick)
{
  uint32_t thisSyncPulseMicros;

  if (ignoreNextSyncPulse)
  {
    // reset the flag
    ignoreNextSyncPulse = false;
  }
  else
  {
    syncPulseLive   = true;
    syncPulseCount++;
      
    thisSyncPulseMicros = mozziMicros();
  
    timeSinceLastSyncPulseMicros  = thisSyncPulseMicros - lastSyncPulseTimeMicros;
    lastSyncPulseTimeMicros       = thisSyncPulseMicros;
    
    nextStepTimeMillis = timeSinceLastSyncPulseMicros / (1000 * stepsPerClick) * syncPulseClockDivide;
  
    // immediately make the timer ready, so next call to update will trigger next step
    stepTimer.start(0);   
  
  }
  //Serial.print(F("timeSinceLastSyncPulseMicros = "));
  //Serial.println(timeSinceLastSyncPulseMicros);
}


/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::testSequence()
 * initialises the sequencer with a new test sequence
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingSequencer::testSequence()
{
  for (uint8_t i = 0; i < MAX_SEQUENCE_LENGTH; i++)
  {
    notes[i] = tonicNote + i;
  }
}



/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::newSequence()
 * initialises the sequencer with a new random sequence
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingSequencer::newSequence(byte seqLength)
{
  if (seqLength > 0 && seqLength < MAX_SEQUENCE_LENGTH)
  {
    sequenceLength = seqLength;
  }
  
  for(int i=0; i < sequenceLength; i++)
  { 
               //root     //scale note                        //octave
    notes[i] = tonicNote + (12*octave) + scaleNotes[rand(scaleNoteCount)] + (12*rand(octaveSpread));

    //sprinkle the tonic in there with a bit more frequency
    if (rand(100) < tonicProbability) 
    {
      notes[i] = tonicNote + (12*octave) + (12*rand(octaveSpread));
    }
    
    // sprinkle some rests
    if (rand(100) > noteProbability)
    {
      notes[i] = 0;
    }
  }
}


/*---------------------------------------------------------------------------------------------------------------
 * mutateSequence
 * mutate the current sequence according to the selected mutation algorithm
 *---------------------------------------------------------------------------------------------------------------
 */
void MutatingSequencer::mutateSequence()
{
  switch(mutationAlgorithm)
  {
    case MUTATE_ALGO_DEFAULT:
      mutateSequenceDefault();
      break;

    case MUTATE_ALGO_ARPEGGIATED:
      mutateSequenceArp();
      break;
  }
}


/*---------------------------------------------------------------------------------------------------------------
 * mutateSequenceDefault
 * default mutation algorithm
 *---------------------------------------------------------------------------------------------------------------
 */
void MutatingSequencer::mutateSequenceDefault()
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
                        //root        //scale note                        //octave
          notes[seqStep] = tonicNote + (12*octave) + scaleNotes[rand(scaleNoteCount)] + (12*rand(octaveSpread));
        }
        
      }
      else
      {
        notes[seqStep] = 0;
      }
    }
  }
}


/*---------------------------------------------------------------------------------------------------------------
 * mutateSequenceArp
 * arpegiated mutation algorithm
 *---------------------------------------------------------------------------------------------------------------
 */
void MutatingSequencer::mutateSequenceArp()
{
  int8_t startStep;
  int8_t startOctave;
  uint8_t startNote;

  int8_t seqStep;
  uint8_t seqNote;
  uint8_t stepSize;
  uint8_t runLength;
  int8_t runDirection;
  

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

/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::mutateMutation()
 * not used
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingSequencer::mutateMutation()
{
  noteProbability      = 50 + rand(50);
  mutationProbability = 10 + rand(20);
}


/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::mutateNoteLength()
 * not used
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingSequencer::mutateNoteLength()
{
  setNextNoteLength(rand(500));
}


/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::setNextNoteLength()
 * set the next note length
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingSequencer::setNextNoteLength(uint16_t newNoteLength)
{
  if (newNoteLength > 0 and newNoteLength <= 65000 && newNoteLength != nextStepNoteLength)
  {
    nextStepNoteLength = newNoteLength;  
    #ifndef ENABLE_MIDI_OUTPUT
    Serial.print(F("nextStepNoteLength="));
    Serial.println(nextStepNoteLength);
    #endif

  }
}


/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::getNextNoteLength()
 * return the next note length
 *----------------------------------------------------------------------------------------------------------
 */
uint16_t MutatingSequencer::getNextNoteLength()
{
  return nextStepNoteLength;
}


/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::isRunning()
 * return true if the sequencer is running
 *----------------------------------------------------------------------------------------------------------
 */
uint8_t MutatingSequencer::isRunning()
{
  return running;
}


/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::getSequenceLength()
 * return the sequence length
 *----------------------------------------------------------------------------------------------------------
 */
byte MutatingSequencer::getSequenceLength()
{
  return sequenceLength;  
}


/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::setSequenceLength()
 * set the sequence length
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingSequencer::setSequenceLength(byte newLength)
{
  if(newLength > 0 && newLength <= MAX_SEQUENCE_LENGTH && sequenceLength != newLength)
  {
    sequenceLength = newLength;
    #ifndef ENABLE_MIDI_OUTPUT
    Serial.print(F("sequenceLength="));
    Serial.println(sequenceLength);
    #endif
  }
}



/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::setNoteProbability()
 * set the note probability
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingSequencer::setNoteProbability(byte newProbability)
{
  if (newProbability >= 0 && newProbability <= 100 && newProbability != noteProbability)
  {
    noteProbability = newProbability;
    #ifndef ENABLE_MIDI_OUTPUT
    Serial.print(F("noteProbability="));
    Serial.println(noteProbability);
    #endif
  }
}

/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::getNoteProbability()
 * gets the note probability
 *----------------------------------------------------------------------------------------------------------
 */
byte MutatingSequencer::getNoteProbability()
{
  return noteProbability;
}

/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::setMutationProbability()
 * gets the mutation probability
 *----------------------------------------------------------------------------------------------------------
 */
void MutatingSequencer::setMutationProbability(byte newProbability)
{
  if (newProbability >= 0 && newProbability <= 100 && newProbability != mutationProbability)
  {
    mutationProbability = newProbability;
    #ifndef ENABLE_MIDI_OUTPUT
    Serial.print(F("mutationProbability="));
    Serial.println(mutationProbability);
    #endif

  }
}

/*----------------------------------------------------------------------------------------------------------
 * MutatingSequencer::getMutationProbability()
 * gets the mutation probability
 *----------------------------------------------------------------------------------------------------------
 */
byte MutatingSequencer::getMutationProbability()
{
  return mutationProbability;
}



void MutatingSequencer::printParameters()
{
  #ifndef ENABLE_MIDI_OUTPUT

  /*
  Serial.print(F("Parameters: note prob="));
  Serial.print(noteProbability);
  Serial.print(F(",  mutate="));
  Serial.print(mutationProbability);

  Serial.print(F(",  skip="));
  Serial.print(skipProbability);
  Serial.print(F(",  tonic="));
  Serial.print(tonicProbability);
  Serial.print(F(",  rachet="));
  Serial.print(rachetProbability);
  Serial.print(F(",  shuffle="));
  Serial.print(shufflePct);

  

  Serial.print(F("\n"));
  */
  #endif
  
}





void MutatingSequencer::print()
{
  printSequence();
}


void MutatingSequencer::printSequence()
{
  #ifndef ENABLE_MIDI_OUTPUT
  
  for(int i=0; i < sequenceLength; i++)
  {
    Serial.print(notes[i]);
    Serial.print(F(" "));
  }
  Serial.print(F("}\n"));
  #endif
  
}
