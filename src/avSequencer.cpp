/*----------------------------------------------------------------------------------------------------------
 * avSequencer.cpp
 * 
 * Implements a generative sequencer with multiple generative algorithms, musical scale quantisation,
 * variable sequence length (up to 16 steps) and multiple parameter-lock (motion-sequencing) channels
 * 
 * (C) 2021 Meebleeps
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
    for (uint8_t m = 0; m < 6; m++)
    {
      parameterLocks[m][i] = 0;
    }
  }

  currentStep         = 0;  
  retrigStep          = 0;     
  retrigState         = false;                        
  tonicNote           = 33;   // default to A2                           
  mutationProbability = 0;
  noteProbability      = 70;
  tonicProbability    = 30;
  rachetProbability   = 30;
  scatterProbability  = 0;
  shufflePct          = 50;
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

  duckingEnvelope.setTimes(80,1000,1000,5);
  duckingEnvelope.setADLevels(0,255);
  duckingCounter = 0;

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
  Serial.println(F("Sequencer.Start()"));
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
  Serial.println(F("Sequencer.Stop()"));

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
  Serial.print(F("NEW SCALE: "));
  Serial.println(scaleMode);
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
  Serial.print(F("New Algorithm = "));
  Serial.println(mutationAlgorithm);
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
    Serial.print(F("OLD TONIC: "));
    Serial.print(tonicNote);
    Serial.print(F(", NEW TONIC: "));
    Serial.println(newTonic);
  
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
      scaleNotes[6] = 8;
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
  
  duckingEnvelope.update();

  if (running)
  {
    if (restart || (running && stepTimer.ready()))
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
    currentStep = ++currentStep % sequenceLength;
  }

  //Serial.print(F("nextStep()  step="));
  //Serial.println(currentStep);


  currentNote = notes[currentStep];

  mutateSequence();
  

  if(duckingCounter == 0)
  {
    duckingEnvelope.noteOn(true); 
  }
  duckingCounter = ++duckingCounter % 4;

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
  
  if(syncPulseLive)
  {
    // this step is the first one after a sync pulse
    // adjust the next step 

    timeElapsedSincePulseMicros = currentStepTimeMicros - lastSyncPulseTimeMicros;
    
    //Serial.print(F("timeElapsedSincePulseMicros="));
    //Serial.print(timeElapsedSincePulseMicros);

    if(nextStepTimeMillis*1000 > timeElapsedSincePulseMicros)
    {
      forecastTimeToNextStepMicros = nextStepTimeMillis*1000 - timeElapsedSincePulseMicros;
    }
    else
    {
      forecastTimeToNextStepMicros = nextStepTimeMillis*1000;  
    }
    //Serial.print(F(" adjusted="));
    //Serial.println(forecastTimeToNextStepMicros);
    
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
}


void MutatingSequencer::testSequence()
{
  for (uint8_t i = 0; i < MAX_SEQUENCE_LENGTH; i++)
  {
    notes[i] = tonicNote + i;
  }
}



void MutatingSequencer::newSequence(byte seqLength)
{
  if (seqLength > 0 && seqLength < MAX_SEQUENCE_LENGTH)
  {
    sequenceLength = seqLength;
  }
  
  for(int i=0; i < sequenceLength; i++)
  { 
               //root     //scale note                        //octave
    notes[i] = tonicNote + scaleNotes[rand(scaleNoteCount)] + (12*rand(octaveSpread));

    //sprinkle the tonic in there with a bit more frequency
    if (rand(100) < tonicProbability) 
    {
      notes[i] = tonicNote + (12*rand(octaveSpread));
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
          notes[seqStep] = tonicNote + (12*rand(octaveSpread));
        }
        else
        {
                        //root        //scale note                        //octave
          notes[seqStep] = tonicNote + scaleNotes[rand(scaleNoteCount)] + (12*rand(octaveSpread));
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
    startOctave = rand(octaveSpread);
    stepSize    = rand(2) + 1;

    if (rand(100) < noteProbability)
    {
      Serial.print(F("new arpeggio run: "));
      for (uint8_t i = 0; i < runLength; i++)
      {
        seqStep = (startStep + i) % MAX_SEQUENCE_LENGTH;
        seqNote = scaleNotes[(startNote + (i*stepSize*runDirection) ) % scaleNoteCount];
        notes[seqStep] = tonicNote + seqNote + (12*(startOctave + (startNote + i < scaleNoteCount ? 0 : 1)));
        Serial.print(notes[seqStep]);
        Serial.print(F(","));
      }
      Serial.println();
  }
  }

}

void MutatingSequencer::mutateMutation()
{
  
  noteProbability      = 50 + rand(50);
  mutationProbability = 10 + rand(20);
  rachetProbability   = rand(40);
  //shufflePct          = 35 + rand(35);

  if (shufflePct < 50) shufflePct = 50;
}

void MutatingSequencer::mutateNoteLength()
{
  setNextNoteLength(rand(500));
}

void MutatingSequencer::setNextNoteLength(uint16_t newNoteLength)
{
  if (newNoteLength > 0 and newNoteLength <= 65000 && newNoteLength != nextStepNoteLength)
  {
    nextStepNoteLength = newNoteLength;  
    Serial.print(F("nextStepNoteLength="));
    Serial.println(nextStepNoteLength);
  }
}



uint16_t MutatingSequencer::getNextNoteLength()
{
  return nextStepNoteLength;
}



void MutatingSequencer::setScatterProbability(byte newProbability)
{
  if(newProbability >= 0 && newProbability <= 100 && newProbability != scatterProbability)
  {
    scatterProbability = newProbability;
    Serial.print(F("scatterProbability="));
    Serial.println(scatterProbability);
  }

}

byte MutatingSequencer::getScatterProbability()
{
  return scatterProbability;
}

bool MutatingSequencer::isScattered()
{
  return rand(100) < scatterProbability;
}


byte MutatingSequencer::getSequenceLength()
{
  return sequenceLength;  
}

void MutatingSequencer::setSequenceLength(byte newLength)
{
  if(newLength > 0 && newLength <= MAX_SEQUENCE_LENGTH && sequenceLength != newLength)
  {
    sequenceLength = newLength;
    Serial.print(F("sequenceLength="));
    Serial.println(sequenceLength);
  }
}

void MutatingSequencer::setNoteProbability(byte newProbability)
{
  if (newProbability >= 0 && newProbability <= 100 && newProbability != noteProbability)
  {
    noteProbability = newProbability;
    Serial.print(F("noteProbability="));
    Serial.println(noteProbability);
  }
}

byte MutatingSequencer::getNoteProbability()
{
  return noteProbability;
}


void MutatingSequencer::setMutationProbability(byte newProbability)
{
  if (newProbability >= 0 && newProbability <= 100 && newProbability != mutationProbability)
  {
    mutationProbability = newProbability;
    Serial.print(F("mutationProbability="));
    Serial.println(mutationProbability);
  }
}

byte MutatingSequencer::getMutationProbability()
{
  return mutationProbability;
}



void MutatingSequencer::printParameters()
{
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
}


byte MutatingSequencer::getDuckingEnvelope()
{
  return duckingEnvelope.next();
}


int MutatingSequencer::getDuckingAmount()
{
  return duckingAmount;
}

    
void MutatingSequencer::setDuckingAmount(int newDuckingAmount)
{
  if (newDuckingAmount > 255)
  {
    duckingAmount = 255;
  }
  else if (newDuckingAmount < 0)
  {
    duckingAmount = 0;  
  }
  else
  {
    duckingAmount = newDuckingAmount;      
  }
}
    


void MutatingSequencer::print()
{
  printSequence();
}


void MutatingSequencer::printSequence()
{
  for(int i=0; i < sequenceLength; i++)
  {
    Serial.print(notes[i]);
    Serial.print(F(" "));
  }
  Serial.print(F("}\n"));
  
}
