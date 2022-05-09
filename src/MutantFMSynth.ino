/*----------------------------------------------------------------------------------------------------------
 * MutantFMSynth.ino
 * 
 * Main project file for the Mutant FM Synth. 
 * 
 * Instantiates all mutant class objects 
 * Calls the Mozzi audio hook functions
 * manages the user interface
 * 
 * Source Code Repository:  https://github.com/Meebleeps/MeeBleeps-Freaq-FM-Synth
 * Youtube Channel:         https://www.youtube.com/channel/UC4I1ExnOpH_GjNtm7ZdWeWA
 * 
 * (C) 2021-2022 Meebleeps
*-----------------------------------------------------------------------------------------------------------
*/

#include <Arduino.h>
#include <MozziGuts.h>
#include <mozzi_rand.h>
#include <EventDelay.h>


#include "avSource.h"
#include "avSequencerMultiTrack.h"
#include "avMidi.h"
#include "LedMatrix.h"
#include "mutantBitmaps.h"
#include <avr/pgmspace.h>

#include "MutantFMSynthOptions.h"

// define the pins
#define PIN_SYNC_IN       8
#define PIN_SYNC_OUT      2
#define TIMER_SYNC_PULSE_OUTPUT_MILLIS 15

#define PIN_AUDIO_OUT     9
#define PIN_BUTTON0       4
#define PIN_BUTTON1       5
#define PIN_BUTTON2       6
#define PIN_BUTTON3       3
#define PIN_BUTTON4       10
#define PIN_BUTTON5       12
#define MAX_BUTTON_INPUTS 6

#define SEQUENCER_TRACK_0   0
#define SEQUENCER_TRACK_1   1
#define SEQUENCER_TRACK_0_DISPLAY_ROW_0   4
#define SEQUENCER_TRACK_0_DISPLAY_ROW_1   5
#define SEQUENCER_TRACK_1_DISPLAY_ROW_0   6
#define SEQUENCER_TRACK_1_DISPLAY_ROW_1   7
#define LFO_TRACK_0_DISPLAY_ROW           2
#define LFO_TRACK_1_DISPLAY_ROW           2
#define TRIGGER_DISPLAY_ROW               0
#define TRIGGER_DISPLAY_COL               7
#define CURRENT_VOICE_DISPLAY_ROW         1

#define MAX_ANALOG_INPUTS               8
#define MAX_ANALOG_VALUE                1023

#define ANALOG_INPUT_MOD_AMOUNT         7
#define ANALOG_INPUT_DECAY              4
#define ANALOG_INPUT_MUTATION           0
#define ANALOG_INPUT_LFO                1
#define ANALOG_INPUT_MOD_RATIO          6
#define ANALOG_INPUT_STEPCOUNT          2
#define ANALOG_INPUT_MOD_ENVELOPE1      5
#define ANALOG_INPUT_MOD_ENVELOPE2      3

#define ANALOG_INPUT_MOVEMENT_THRESHOLD 5

// listed in order of left to right.
#define BUTTON_INPUT_FUNC   4
#define BUTTON_INPUT_TONIC  5
#define BUTTON_INPUT_SCALE  2
#define BUTTON_INPUT_START  3
#define BUTTON_INPUT_REC    1
#define BUTTON_INPUT_VOICE  0
#define MAX_OSCILLATOR_MODES 1

#define INTERFACE_MODE_NORMAL 0
#define INTERFACE_MODE_SHIFT  1

#define DISPLAY_SETTING_CHANGE_PERSIST_MILLIS 350
#define DISPLAY_SETTING_INTENSITY 4

#define MOTION_RECORD_NONE  0
#define MOTION_RECORD_REC   1
#define MOTION_RECORD_CLEAR 2

// MOZZI variables
// original had at 256 but as CPU got tight had to reduce it
// needs to be at least 128. 64Hz means incoming sync pulses are regularly missed between updates.
#define CONTROL_RATE 128   

// todo:  decrease INTERFACE_UPDATE_DIVIDER  if controls are externally modulated via CV voltage inputs
#define INTERFACE_UPDATE_DIVIDER_ANALOG 5
#define INTERFACE_UPDATE_DIVIDER_DIGITAL 7
#define INTERFACE_UPDATE_DIVIDER_LFO 2
#define INTERFACE_UPDATE_DIVIDER_DISPLAY 32


byte firstTimeStart = true;
byte iTrigger = 1;      
byte iLastTrigger = 0;
byte interfaceMode    = INTERFACE_MODE_NORMAL;
byte controlSynthVoice = 0;
byte motionRecordMode = MOTION_RECORD_NONE;

int iLastAnalogValue[MAX_ANALOG_INPUTS]    = {0,0,0,0,0,0,0,0};
int iCurrentAnalogValue[MAX_ANALOG_INPUTS] = {0,0,0,0,0,0,0,0};

// save space for digital inputs - use a bit rather than a byte per button 
uint8_t bitsLastButton;

// save space for digital inputs - use a bit rather than a byte per button 
uint8_t bitsCurrentButton;

// save space for param lock flags - use a bit rather than a byte per param 
uint8_t bitsLastParamLock;

// time since the last updateAudio()
uint32_t lastUpdateMicros;

// used to divide the calls to updateControl to spread out control reads and save cpu cycles 
uint8_t updateCounter = 0;




// voice 1 instance
MutatingFM          voice0;

// voice 2 instance
MutatingFM          voice1;

// array of pointers to the MutatingFM instances to simplfy the code
MutatingFM*         voices[2];  

// MAX7219 display matrix interface class
LedMatrix           ledDisplay;

// timer to pull sync output low at end of pulse
EventDelay          syncOutputTimer;

// timer to prevent update of the display while a settings icon is being displayed
EventDelay          settingDisplayTimer;

// the sequencer
MutatingSequencerMultiTrack sequencer;





/*----------------------------------------------------------------------------------------------------------
 * setup()
 *----------------------------------------------------------------------------------------------------------
 */
void setup() 
{
  #ifdef ENABLE_MIDI_OUTPUT
  Serial.begin(31250);
  #else
  Serial.begin(115200);
  Serial.print(F("\n\n\n\n\n-----------------------------------------------------------------------\n"));
  Serial.print(F("Freaq - Mozzi-based Mutating FM Synth.   Build "));
  Serial.print  (__DATE__);
  Serial.print  (F(" "));
  Serial.println  (__TIME__);
  #endif

  initialisePins();
  initialiseDisplay();
  initialiseSources();
  initialiseSequencer();
  
  startMozzi(CONTROL_RATE);
}


/*----------------------------------------------------------------------------------------------------------
 * debugPrintMemoryUsage
 * does nothing now.  used while optimising memory usage 
 *----------------------------------------------------------------------------------------------------------
 */
void debugPrintMemoryUsage()
{
/*
  debugWriteValue("sizeof(synthVoice)", sizeof(synthVoice));
  debugWriteValue("sizeof(sequencer)", sizeof(sequencer));
*/  

}


/*----------------------------------------------------------------------------------------------------------
 * initialiseDisplay
 *----------------------------------------------------------------------------------------------------------
 */
void initialiseDisplay()
{
  ledDisplay.initialise();
  ledDisplay.setOrientation(LEDMATRIX_ORIENTATION_0);
  
  settingDisplayTimer.start(0);

  
  // display logo
  for (uint8_t i = 10; i < 100; i += 10)
  {
    ledDisplay.clearScreen();
    delay(i);
    displaySettingIcon(BITMAP_MEEBLEEPS);
    delay(i);
  }
  
  //fade logo to target intensity
  for(int i = 15; i>= DISPLAY_SETTING_INTENSITY; i--)
  {
    ledDisplay.setIntensity(i);

    delay(20);
  }
  
//  ledDisplay.setIntensity(DISPLAY_SETTING_INTENSITY);
//  displaySettingIcon(BITMAP_MEEBLEEPS);

  #ifndef ENABLE_MIDI_OUTPUT
  Serial.println(F("initialiseDisplay() complete"));
  #endif
}




/*----------------------------------------------------------------------------------------------------------
 * initialisePins
 *----------------------------------------------------------------------------------------------------------
 */
void initialisePins()
{
  #ifndef ENABLE_MIDI_OUTPUT
  Serial.println(F("initialisePins()"));
  #endif

  pinMode(PIN_SYNC_IN, INPUT);          // sync is pulled down by external pulldown resistor
  pinMode(PIN_SYNC_OUT, OUTPUT);
  pinMode(PIN_BUTTON0, INPUT_PULLUP);   // all other buttons are pulled up by internal pullup resistor
  pinMode(PIN_BUTTON1, INPUT_PULLUP);
  pinMode(PIN_BUTTON2, INPUT_PULLUP);
  pinMode(PIN_BUTTON3, INPUT_PULLUP);
  pinMode(PIN_BUTTON4, INPUT_PULLUP);
  pinMode(PIN_BUTTON5, INPUT_PULLUP);

  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  pinMode(A2, INPUT);
  pinMode(A3, INPUT);
  pinMode(A4, INPUT);
  pinMode(A5, INPUT); 
  pinMode(A6, INPUT);
  pinMode(A7, INPUT); 

  // initialise 
  bitsLastButton    = 0;
  bitsCurrentButton = 0;

  for (uint8_t i = 0; i < MAX_ANALOG_INPUTS; i++)
  {
    iLastAnalogValue[i]    = 0;
  }
}



/*----------------------------------------------------------------------------------------------------------
 * initialiseSources
 *----------------------------------------------------------------------------------------------------------
 */
void initialiseSources()
{
  voice0.setOscillator(0);
  voice1.setOscillator(1);

  voices[0] = &voice0;  
  voices[1] = &voice1;  
}



/*----------------------------------------------------------------------------------------------------------
 * initialiseSequencer
 *----------------------------------------------------------------------------------------------------------
 */
void initialiseSequencer()
{
  sequencer.setScale(SCALEMODE_MINOR);
  sequencer.newSequence(16);
}




/*----------------------------------------------------------------------------------------------------------
 * updateControl
 * called by Mozzi Library on CONTROL_RATE frequency
 * spreads out control logic updates so that we don't bog down the CPU
 *
 * every step - check for sync trigger and update sequencer
 * if this gets delayed, note on/off commands will drift
 *----------------------------------------------------------------------------------------------------------
 */
void updateControl()
{
  uint8_t doDisplayUpdate = false;

  lastUpdateMicros = micros();
  updateCounter++;



  // check for incoming sync trigger and if necessary update output sync
  // do every update to ensure minimum sync jitter 
  updateSyncTrigger();

  // update sequencer returns true if the sequencer has moved to next step
  if (updateSequencer())
  {

    // check to see if sequencer should update the output sync pulse
    if (sequencer.outputSyncPulse())
    {
      outputSyncPulse();
    }

    //update the display once per sequencer step   
    // but do the display update at the end of updateControl so that any latency is introduced after all critical functions are processed

    doDisplayUpdate = true; 
  }
  // check controls once every INTERFACE_UPDATE_DIVIDER steps for efficiency
  // don't update these if the sequencer has updated as that is expensive
  else if(updateCounter % INTERFACE_UPDATE_DIVIDER_ANALOG == 0)
  {
    // now check analog controls
    updateAnalogControls();
  
  }
  // never check both analog and digital controls in the same step to causing buffer underflows
  else if(updateCounter % INTERFACE_UPDATE_DIVIDER_DIGITAL == 0)
  {
    // check button controls first to ensure interface mode is correctly set
    updateButtonControls();

  }
  else if(updateCounter % INTERFACE_UPDATE_DIVIDER_LFO == 0)
  {
    //update display of LFO modulation at higher rate than rest of display
    updateLFOModulationDisplay();
  }
  else 
  {
    doDisplayUpdate = (updateCounter % INTERFACE_UPDATE_DIVIDER_DISPLAY == 0);
  }

  #ifndef ENABLE_MIDI_OUTPUT
  //Serial.print(micros() - lastUpdateMicros);
  //Serial.print(F(","));
  #endif

  // now that all controls are updated, update the source.

  voices[0]->updateControl();
  voices[1]->updateControl();

  #ifndef ENABLE_MIDI_OUTPUT
  /*
  Serial.print(micros() - lastUpdateMicros);
  Serial.print(F(","));
  Serial.print(updateCounter % INTERFACE_UPDATE_DIVIDER_DIGITAL);
  Serial.print(F(","));
  Serial.print(updateCounter % INTERFACE_UPDATE_DIVIDER_ANALOG);
  Serial.print(F(","));
  Serial.print(sequenceUpdated);
  Serial.println();
  */
  #endif

  // do the display update last so that any latency is introduced after the notes are updated
  if (doDisplayUpdate)
  {
    updateDisplay();
  }

}



/*----------------------------------------------------------------------------------------------------------
 * updateLFOModulationDisplay
 * displays the current position of the 2 LFOs using 4 LEDs each
 *----------------------------------------------------------------------------------------------------------
 */
void updateLFOModulationDisplay()
{
  if (!firstTimeStart && settingDisplayTimer.ready())
  {
    // divide LFO value of 0-255 to range 0-3 by right-shift 6 bits, then right-shift 128 by that value to get the correct pixel bitfield
    // originally used floating point maths - very slow.
    ledDisplay.setRowPixels(LFO_TRACK_0_DISPLAY_ROW, (128 >> (voice0.getLFOValue() >> 6)) | (8 >> (voice1.getLFOValue() >> 6)));
    
    ledDisplay.refresh();
  }
}



/*----------------------------------------------------------------------------------------------------------
 * updateSequencer
 * updates sequencer
 * returns true if the sequencer advances a step
 *----------------------------------------------------------------------------------------------------------
 */
int updateSequencer()
{
  byte nextNote[MAX_SEQUENCER_TRACKS];
  uint16_t nextNoteLength;
  
  // if the sequencer is due to make a new step, 
  if (sequencer.update(false))
  {
    nextNoteLength  = sequencer.getNextNoteLength();

    for (uint8_t i=0; i<MAX_SEQUENCER_TRACKS; i++) 
    {
      nextNote[i] = sequencer.getCurrentNote(i);
     
      if (nextNote[i] > 0)
      {
        //todo: find way to extend param locks to channel 2?
        if (i==0)
        {
          getParameterLocks();
        }

        voices[i]->noteOn(nextNote[i], 255, nextNoteLength);

      }
    }
    #ifdef ENABLE_MIDI_OUTPUT
    //midiNoteOn(1,nextNote[0],127);
    if (nextNote[0])
    {
      Serial.write(0x90);
      Serial.write(nextNote[0]-24);
      Serial.write(0x45);
    }
    #else

    #endif


    return true;
  }
  else
  {
    return false;
  }

  
}



/*----------------------------------------------------------------------------------------------------------
 * outputSyncPulse
 * sends a digital signal to the output pin and sets the timer to pull it low after TIMER_SYNC_PULSE_OUTPUT_MILLIS
 *----------------------------------------------------------------------------------------------------------
 */
void outputSyncPulse()
{
  /*
  DEBUG only
  static uint32_t lastSyncOutTimeMicros;
  static uint32_t thisSyncOutTimeMicros;

  thisSyncOutTimeMicros = mozziMicros();
  Serial.print(F("lastSyncOutTimeMicros = "));
  Serial.println(thisSyncOutTimeMicros - lastSyncOutTimeMicros);
  lastSyncOutTimeMicros = thisSyncOutTimeMicros;
  */

  syncOutputTimer.start(TIMER_SYNC_PULSE_OUTPUT_MILLIS);
  digitalWrite(PIN_SYNC_OUT,  HIGH);
}


/*----------------------------------------------------------------------------------------------------------
 * getParameterLocks
 * updates source parameters based on stored modulation sequence
 * parameter locks are only available for first track only.
 *----------------------------------------------------------------------------------------------------------
 */
void getParameterLocks()
{
  uint16_t  thisParamLock;
  uint8_t   thisStep;
  int8_t    synthParamIndex;

  // parameters are recorded on the longest sequence but only applied to track 1 voice
  // this way if track 1 is 5 steps but track 2 is 16 steps, the automation will be 16 steps long
  if (sequencer.getSequenceLength(SEQUENCER_TRACK_1) > sequencer.getSequenceLength(SEQUENCER_TRACK_0))
  {
    thisStep = sequencer.getCurrentStep(SEQUENCER_TRACK_1);
  }
  else
  {
    thisStep = sequencer.getCurrentStep(SEQUENCER_TRACK_0);
  }

  for (uint8_t paramIndex = 0; paramIndex < MAX_PARAMETER_LOCKS; paramIndex++)
  {
    thisParamLock = sequencer.getParameterLock(paramIndex, SEQUENCER_TRACK_0, thisStep);
    
    if (thisParamLock != 0)
    {
      #ifndef ENABLE_MIDI_OUTPUT

      /*
      Serial.print(F("parameter lock for channel "));
      Serial.print(SEQUENCER_TRACK_0);
      Serial.print(F(" parameter "));
      Serial.print(paramIndex);
      Serial.print(F(" value "));
      Serial.println(thisParamLock);
      */
      #endif

      // set the flag for this input - last parameter was a lock
      bitsLastParamLock |= 1 << paramIndex;
    }
    else 
    {
      // if the last step was a parameter lock but this step doesn't have one, set the parameter lock to the current knob position
      if ((bitsLastParamLock >> paramIndex) & 1)
      {
        #ifndef ENABLE_MIDI_OUTPUT

        /*
        Serial.print(F("clearing last parameter lock for channel "));
        Serial.print(SEQUENCER_TRACK_0);
        Serial.print(F(" parameter "));
        Serial.print(paramIndex);
        Serial.print(F(" knob value "));
        Serial.println(iCurrentAnalogValue[getParameterLockControl(paramIndex)]);
        */
        #endif
        thisParamLock = iCurrentAnalogValue[getParameterLockControl(paramIndex)];
      }

      // reset the lock bitflag for this parameter
      bitsLastParamLock &= ~(1 << paramIndex);  

    }
    
    // if there is a parameter lock or we are recovering from a previous step parameter lock 
    if (thisParamLock != 0)
    {
      synthParamIndex = getParameterLockSynthParam(paramIndex);
      
      if (synthParamIndex == -2)
      {
        sequencer.setNextNoteLength(thisParamLock);
      }
      else if (synthParamIndex >= 0)
      {
        voices[SEQUENCER_TRACK_0]->setParam(synthParamIndex, thisParamLock);
      }
    }
    //}
  }

}



/*----------------------------------------------------------------------------------------------------------
 * setParameterLock
 * saves the given parameter value onto a modulation channel
 *----------------------------------------------------------------------------------------------------------
 */
inline void setParameterLock(int8_t paramChannel, uint16_t value)
{
  switch (motionRecordMode)
  {
    // 14/02/22: moved this here as is the most common case, assuming no branch prediction on arduion platforms
    case MOTION_RECORD_NONE:
        // do nothing 
        break;

    case MOTION_RECORD_REC:
        sequencer.setParameterLock(paramChannel, value);
        #ifndef ENABLE_MIDI_OUTPUT
        //Serial.println(F("setParameterLock(0)"));
        #endif
        break;

    case MOTION_RECORD_CLEAR:
        sequencer.clearAllParameterLocks(paramChannel);
        #ifndef ENABLE_MIDI_OUTPUT
        //Serial.println(F("Clear ParameterLock(0)"));
        #endif
        break;

  }
}



/*----------------------------------------------------------------------------------------------------------
 * getParameterLockChannel
 * returns the sequencer modulation channel for a given analog control index 
 * 
 * These are a bit of a kludge - would be better as a lookup table.
 *----------------------------------------------------------------------------------------------------------
 */
inline int8_t getParameterLockChannel(uint8_t analogControlIndex)
{
  switch(analogControlIndex)
  {
    case ANALOG_INPUT_MOD_AMOUNT:    return PARAM_LOCK_CHANNEL_0;       
    case ANALOG_INPUT_DECAY:         return PARAM_LOCK_CHANNEL_1;
    case ANALOG_INPUT_MOD_RATIO:     return PARAM_LOCK_CHANNEL_2;
    case ANALOG_INPUT_MOD_ENVELOPE1: return PARAM_LOCK_CHANNEL_3;
    case ANALOG_INPUT_MOD_ENVELOPE2: return PARAM_LOCK_CHANNEL_4;
    case ANALOG_INPUT_LFO:           return PARAM_LOCK_CHANNEL_5;
    default:  return -1;
  }
}


/*----------------------------------------------------------------------------------------------------------
 * getParameterLockControl
 * returns the analog control index for a given sequencer modulation channel 
 *----------------------------------------------------------------------------------------------------------
 */
inline int8_t getParameterLockControl(uint8_t paramChannelIndex)
{
  switch(paramChannelIndex)
  {
    case PARAM_LOCK_CHANNEL_0: return ANALOG_INPUT_MOD_AMOUNT;
    case PARAM_LOCK_CHANNEL_1: return ANALOG_INPUT_DECAY;
    case PARAM_LOCK_CHANNEL_2: return ANALOG_INPUT_MOD_RATIO;
    case PARAM_LOCK_CHANNEL_3: return ANALOG_INPUT_MOD_ENVELOPE1;
    case PARAM_LOCK_CHANNEL_4: return ANALOG_INPUT_MOD_ENVELOPE2;
    case PARAM_LOCK_CHANNEL_5: return ANALOG_INPUT_LFO;
    default:  return -1;
  }
}


/*----------------------------------------------------------------------------------------------------------
 * getParameterLockSynthParam
 * returns the sytnh parameter for a given sequencer modulation channel 
 *----------------------------------------------------------------------------------------------------------
 */
inline int8_t getParameterLockSynthParam(uint8_t paramChannelIndex)
{
  switch(paramChannelIndex)
  {
    case PARAM_LOCK_CHANNEL_0: return SYNTH_PARAMETER_MOD_AMOUNT;
    case PARAM_LOCK_CHANNEL_1: return SYNTH_PARAMETER_NOTE_DECAY;
    case PARAM_LOCK_CHANNEL_2: return SYNTH_PARAMETER_MOD_RATIO;
    case PARAM_LOCK_CHANNEL_3: return SYNTH_PARAMETER_ENVELOPE_DECAY;
    case PARAM_LOCK_CHANNEL_4: return SYNTH_PARAMETER_ENVELOPE_ATTACK;
    case PARAM_LOCK_CHANNEL_5: return SYNTH_PARAMETER_MOD_AMOUNT_LFODEPTH;
    default:  return SYNTH_PARAMETER_UNKNOWN;
  }
}






/*----------------------------------------------------------------------------------------------------------
 * updateSyncTrigger
 * check for incoming sync trigger and update sequencer
 * also check to see if output sync trigger needs to be pulled low
 *----------------------------------------------------------------------------------------------------------
 */
void updateSyncTrigger()
{
  iTrigger = digitalRead(PIN_SYNC_IN);   // read the sync pin
  if (iTrigger != iLastTrigger)
  {
    digitalWrite(PIN_SYNC_OUT, iTrigger);
    
    //trigger note on or off
    if (iTrigger == HIGH)
    {
      #ifndef ENABLE_MIDI_OUTPUT
      //Serial.println("sync!");
      #endif
      sequencer.syncPulse(SYNC_STEPS_PER_PULSE);
    }

    iLastTrigger = iTrigger;
  }

  // check to see if the output sync pulse needs to be pulled low
  if (syncOutputTimer.ready())
  {
    digitalWrite(PIN_SYNC_OUT, LOW);
  }
  
}


/*----------------------------------------------------------------------------------------------------------
 * getLastButtonState
 * returns the last state for the given button 
 * uses a bitfield to compress 
 *----------------------------------------------------------------------------------------------------------
 */
inline uint8_t getLastButtonState(uint8_t buttonIndex)
{
  return (bitsLastButton >> buttonIndex) & 1;
}

/*----------------------------------------------------------------------------------------------------------
 * getCurrentButtonState
 * returns the current state for the given button 
 *----------------------------------------------------------------------------------------------------------
 */
inline uint8_t getCurrentButtonState(uint8_t buttonIndex)
{
  return (bitsCurrentButton >> buttonIndex) & 1;
}

/*----------------------------------------------------------------------------------------------------------
 * setLastButtonState
 * sets the last state for the given button 
 *----------------------------------------------------------------------------------------------------------
 */
inline void setLastButtonState(uint8_t buttonIndex, uint8_t value)
{
  if (value)
  {
    bitsLastButton |= 1 << buttonIndex;
  }
  else
  {
    bitsLastButton &= ~(1 << buttonIndex);  
  }
}


/*----------------------------------------------------------------------------------------------------------
 * setCurrentButtonState
 * sets the current state for the given button 
 *----------------------------------------------------------------------------------------------------------
 */
inline void setCurrentButtonState(uint8_t buttonIndex, uint8_t value)
{
  if (value)
  {
    bitsCurrentButton |= 1 << buttonIndex;
  }
  else
  {
    bitsCurrentButton &= ~(1 << buttonIndex);  
  }
}


/*----------------------------------------------------------------------------------------------------------
 * updateButtonControls
 * check for button presses
 *----------------------------------------------------------------------------------------------------------
 */
void updateButtonControls()
{
  //  IMPORTANT:
  //  These calls assume you are using NORMALLY CLOSED switches, which is what I used in the build
  //  if your switches are NORMALLY OPEN delete the definition of SWITCH_TYPE_NORMALLY_CLOSED in MutantFMSynthOptions.h

  #ifdef SWITCH_TYPE_NORMALLY_CLOSED
  setCurrentButtonState(0,digitalRead(PIN_BUTTON0));
  setCurrentButtonState(1,digitalRead(PIN_BUTTON1));
  setCurrentButtonState(2,digitalRead(PIN_BUTTON2));
  setCurrentButtonState(3,digitalRead(PIN_BUTTON3));
  setCurrentButtonState(4,digitalRead(PIN_BUTTON4));
  setCurrentButtonState(5,digitalRead(PIN_BUTTON5));
  #else
  setCurrentButtonState(0, !digitalRead(PIN_BUTTON0));
  setCurrentButtonState(1, !digitalRead(PIN_BUTTON1));
  setCurrentButtonState(2, !digitalRead(PIN_BUTTON2));
  setCurrentButtonState(3, !digitalRead(PIN_BUTTON3));
  setCurrentButtonState(4, !digitalRead(PIN_BUTTON4));
  setCurrentButtonState(5, !digitalRead(PIN_BUTTON5));
  #endif
  
  // TODO: increase efficiency - only execute this entire block if at least one button is pressed, which is bitsCurrentButton != 0 

  // if func button is not pressed, all UI controls are normal
  if (getCurrentButtonState(BUTTON_INPUT_FUNC)  != getLastButtonState(BUTTON_INPUT_FUNC))
  {
    if(getCurrentButtonState(BUTTON_INPUT_FUNC) == LOW)
    {
      interfaceMode = INTERFACE_MODE_NORMAL;
    }
    else
    {
      interfaceMode = INTERFACE_MODE_SHIFT;
    }
    #ifndef ENABLE_MIDI_OUTPUT
    Serial.print(interfaceMode);
    #endif
  }


  if (getCurrentButtonState(BUTTON_INPUT_REC) != getLastButtonState(BUTTON_INPUT_REC))
  {
    if (getCurrentButtonState(BUTTON_INPUT_REC) == LOW)
    {
        motionRecordMode = MOTION_RECORD_NONE;
    }
    else
    {
      switch(interfaceMode)
      {
        case INTERFACE_MODE_NORMAL:   
          motionRecordMode = MOTION_RECORD_REC;
          break;

        case INTERFACE_MODE_SHIFT:    
          motionRecordMode = MOTION_RECORD_CLEAR;
          break;
      }
    }
    #ifndef ENABLE_MIDI_OUTPUT
    Serial.print(F("motionRecordMode="));
    Serial.println(motionRecordMode);
    #endif
  }

  if (getCurrentButtonState(BUTTON_INPUT_VOICE) != getLastButtonState(BUTTON_INPUT_VOICE))
  {
    switch(interfaceMode)
    {
      case INTERFACE_MODE_NORMAL:   
        if (getCurrentButtonState(BUTTON_INPUT_VOICE) == HIGH)
        {
          updateSynthControl();
        }
        break;

      case INTERFACE_MODE_SHIFT:    
        if (getCurrentButtonState(BUTTON_INPUT_VOICE) == HIGH)
        {
          if (getCurrentButtonState(BUTTON_INPUT_REC) == HIGH)
          {
            // if user if holding down FUNC & REC when they hit VOICE, then change the waveform type for the current voice carrier
            voices[controlSynthVoice]->toggleCarrierWaveform();
            displaySettingIcon(BITMAP_WAVEFORMS[voices[controlSynthVoice]->getCarrierWaveform()]);
          }
          else
          {
            voices[controlSynthVoice]->toggleFMMode();
            //todo: display icon for current FM mode
            displaySettingIcon(BITMAP_FMMODE[voices[controlSynthVoice]->getFMMode()]);
          }
        }     

        break;
    }
  }
  
  if (getCurrentButtonState(BUTTON_INPUT_START) != getLastButtonState(BUTTON_INPUT_START) && getCurrentButtonState(BUTTON_INPUT_START) == HIGH)
  {
    switch(interfaceMode)
    {
      case INTERFACE_MODE_NORMAL:   
        startStopSequencer();     
        break;
      case INTERFACE_MODE_SHIFT:    
        if (getCurrentButtonState(BUTTON_INPUT_REC) == HIGH)
        {
          // if user if holding down FUNC & REC when they hit START, then change the waveform type for the current voice MODULATOR
          voices[controlSynthVoice]->toggleModulatorWaveform();
          displaySettingIcon(BITMAP_WAVEFORMS[voices[controlSynthVoice]->getModulatorWaveform()]);
        }
        else
        {
          // send tap tempo message to the sequencer
          sequencer.syncPulse(SYNC_STEPS_PER_TAP);  
        }
        break;
    }
  }

  if (getCurrentButtonState(BUTTON_INPUT_TONIC) != getLastButtonState(BUTTON_INPUT_TONIC) && getCurrentButtonState(BUTTON_INPUT_TONIC) == HIGH)
  {
    switch(interfaceMode)
    {
      case INTERFACE_MODE_NORMAL:   
        updateTonic(1);   
        break;

      case INTERFACE_MODE_SHIFT:    
        if (getCurrentButtonState(BUTTON_INPUT_REC) == HIGH)
        {
          // TODO: if user if holding down FUNC & REC when they hit TONIC, then change the octave of the current voice only
          sequencer.setOctaveOffsetTrack1((sequencer.getOctaveOffsetTrack1() + 1 ) % 4);
          displaySettingIcon(BITMAP_NUMERALS[sequencer.getOctaveOffsetTrack1()] );
        }
        else
        {
          sequencer.setOctave((sequencer.getOctave() + 1) % 7);
          displayTonicIcon();
        }
        break;
    }
  }

  if (getCurrentButtonState(BUTTON_INPUT_SCALE) != getLastButtonState(BUTTON_INPUT_SCALE) && getCurrentButtonState(BUTTON_INPUT_SCALE) == HIGH)
  {
    switch(interfaceMode)
    {
      case INTERFACE_MODE_NORMAL:   
        updateScale();
        break;

      case INTERFACE_MODE_SHIFT:    
        if (getCurrentButtonState(BUTTON_INPUT_REC) == HIGH)
        {
          // if user if holding down FUNC & REC when they hit SCALE, then change LFO mode
          voices[controlSynthVoice]->toggleLFOWaveform();
          displaySettingIcon(BITMAP_WAVEFORMS[voices[controlSynthVoice]->getLFOWaveform()]);

        }
        else
        {
          updateAlgorithm();
        }
        break;
    }
  }
  
  /*
  DEBUG ONLY
  for (uint8_t i=0; i<MAX_BUTTON_INPUTS; i++)
  {
    // debug only
    
    if (getLastButtonState(i) != getCurrentButtonState(i))
    {
      #ifndef ENABLE_MIDI_OUTPUT
      Serial.print(F("Button state changed - button["));
      Serial.print(i);
      Serial.print(F("] state = "));
      Serial.println(getCurrentButtonState(i));
      #endif

      ledDisplay.setPixel(i,1, getCurrentButtonState(i) == HIGH);
    }
    
  }

  ledDisplay.refresh();
  */
  bitsLastButton = bitsCurrentButton;

}


/*----------------------------------------------------------------------------------------------------------
 * startStopSequencer
 * starts the sequencer if stopped, or stops it if started
 *----------------------------------------------------------------------------------------------------------
 */
void startStopSequencer()
{

  byte nextNote;
  uint16_t nextNoteLength;
  if (firstTimeStart)
  {
    ledDisplay.clearScreen();
    sequencer.newSequence(16);
    firstTimeStart = false;
  }

  sequencer.toggleStart();

  // if the sequencer is due to make a new step, 
  if (sequencer.update(true))
  {
    nextNote        = sequencer.getCurrentNote();
    nextNoteLength  = sequencer.getNextNoteLength();
    
    if (nextNote > 0)
    {
      getParameterLocks();

      voices[0]->noteOn(nextNote, 255, nextNoteLength);
    }
    updateDisplay();
  }

}


/*----------------------------------------------------------------------------------------------------------
 * updateSynthControl
 * selects the next voice to control (currently max 2)
 *----------------------------------------------------------------------------------------------------------
 */
void updateSynthControl()
{
  controlSynthVoice = (controlSynthVoice + 1) % MAX_SEQUENCER_TRACKS;
  #ifndef ENABLE_MIDI_OUTPUT
  Serial.print(F("Voice Control="));
  Serial.println(controlSynthVoice);
  #endif
}



/*----------------------------------------------------------------------------------------------------------
 * updateScale
 * updates the sequencer's musical scale
 *----------------------------------------------------------------------------------------------------------
 */
void updateScale()
{
  sequencer.setScale((sequencer.getScale() + 1) % MAX_SCALE_COUNT);
  displaySettingIcon(BITMAP_NUMERALS[sequencer.getScale()]);
}


/*----------------------------------------------------------------------------------------------------------
 * updateScale
 * updates the sequencer's generative algorithm
 *----------------------------------------------------------------------------------------------------------
 */
void updateAlgorithm()
{
  sequencer.nextAlgorithm();
  displaySettingIcon(BITMAP_ALGORITHMS[sequencer.getAlgorithm()]);
}





/*----------------------------------------------------------------------------------------------------------
 * updateAnalogControls
 * check for knob movements
 *----------------------------------------------------------------------------------------------------------
 */
inline void updateAnalogControls()
{
  int rawValue[MAX_ANALOG_INPUTS];

  for (uint8_t i=0; i < MAX_ANALOG_INPUTS; i++)
  {
    rawValue[i] = mozziAnalogRead(A0 + i);
    //rudimentary smoothing - average of this reading and the last 2.
    iCurrentAnalogValue[i] = (iCurrentAnalogValue[i] + rawValue[i] + iLastAnalogValue[i])/3;

    if (analogInputHasChanged(i))
    {
      #ifndef ENABLE_MIDI_OUTPUT

      Serial.print(F("analog input changed: "));
      Serial.println(i);

      #endif

      switch(i)
      {
        case ANALOG_INPUT_DECAY: 
          switch (interfaceMode)
          {
            case INTERFACE_MODE_NORMAL: 
              updateNoteDecay(false);
              break;

            case INTERFACE_MODE_SHIFT:  
              // shift-decay = volume of voice, so each part can be different volume (or off)
              // currently MutatingFM.setGain() does nothing
              // setGain range 0-255
              voices[controlSynthVoice]->setGain(iCurrentAnalogValue[ANALOG_INPUT_DECAY] >> 2);

          }
          // 14/02/22 - moved this from above switch statement as it meant that decay parameter locks weren't getting cleared
          setParameterLock(getParameterLockChannel(ANALOG_INPUT_DECAY), iCurrentAnalogValue[ANALOG_INPUT_DECAY]);
          break;

        case ANALOG_INPUT_MOD_AMOUNT:
          
          switch (interfaceMode)
          {
            case INTERFACE_MODE_NORMAL: 
              voices[controlSynthVoice]->setParam(SYNTH_PARAMETER_MOD_AMOUNT, iCurrentAnalogValue[ANALOG_INPUT_MOD_AMOUNT]);
              break;
            case INTERFACE_MODE_SHIFT:  
              voices[controlSynthVoice]->setParam(SYNTH_PARAMETER_MOD_AMOUNT, iCurrentAnalogValue[ANALOG_INPUT_MOD_AMOUNT]);
              break;
          }
          setParameterLock(getParameterLockChannel(ANALOG_INPUT_MOD_AMOUNT), iCurrentAnalogValue[ANALOG_INPUT_MOD_AMOUNT]);

         break;
        
          
        case ANALOG_INPUT_MOD_RATIO:
          
          switch (interfaceMode)
          {
            case INTERFACE_MODE_NORMAL: 
              voices[controlSynthVoice]->setParam(SYNTH_PARAMETER_MOD_RATIO, iCurrentAnalogValue[ANALOG_INPUT_MOD_RATIO]);
              break;

            case INTERFACE_MODE_SHIFT:  
              voices[controlSynthVoice]->setParam(SYNTH_PARAMETER_MOD_RATIO, iCurrentAnalogValue[ANALOG_INPUT_MOD_RATIO]);
              break;
          }
          setParameterLock(getParameterLockChannel(ANALOG_INPUT_MOD_RATIO), iCurrentAnalogValue[ANALOG_INPUT_MOD_RATIO]);
          
         break;
        
        case ANALOG_INPUT_MUTATION:
          
          switch (interfaceMode)
          {
            case INTERFACE_MODE_NORMAL: 
              sequencer.setMutationProbability(scaleAnalogInputNonLinear(iCurrentAnalogValue[ANALOG_INPUT_MUTATION],512,20,100));
              break;
            case INTERFACE_MODE_SHIFT:  
              sequencer.setNoteProbability(scaleAnalogInput(iCurrentAnalogValue[ANALOG_INPUT_MUTATION],100));
              break;
          }
          
          break;

        case ANALOG_INPUT_STEPCOUNT:
          switch (interfaceMode)
          {
            case INTERFACE_MODE_NORMAL: 
              sequencer.setSequenceLength(controlSynthVoice, scaleAnalogInput(iCurrentAnalogValue[ANALOG_INPUT_STEPCOUNT],16) + 1);
              break;
              
            // if shift, change both sequences to the current knob setting to keep them in sync
            case INTERFACE_MODE_SHIFT:  
              sequencer.setSequenceLength(SEQUENCER_TRACK_0, scaleAnalogInput(iCurrentAnalogValue[ANALOG_INPUT_STEPCOUNT],16) + 1);
              sequencer.setSequenceLength(SEQUENCER_TRACK_1, scaleAnalogInput(iCurrentAnalogValue[ANALOG_INPUT_STEPCOUNT],16) + 1);
              break;
          }
          
          
         break;

        case ANALOG_INPUT_LFO:
          switch (interfaceMode)
          {
            case INTERFACE_MODE_NORMAL: 
              voices[controlSynthVoice]->setParam(SYNTH_PARAMETER_MOD_AMOUNT_LFODEPTH,iCurrentAnalogValue[ANALOG_INPUT_LFO]);
              break;
            case INTERFACE_MODE_SHIFT:  
              // set LFO frequency between 0 - 1000Hz
              voices[controlSynthVoice]->setLFOFrequency((float)scaleAnalogInputNonLinear(iCurrentAnalogValue[ANALOG_INPUT_LFO]+1,512,300,3000)/(float)300.0);
              break;
          }
          setParameterLock(getParameterLockChannel(ANALOG_INPUT_LFO), iCurrentAnalogValue[ANALOG_INPUT_LFO]);
          
          break;


        case ANALOG_INPUT_MOD_ENVELOPE1:
          
          switch (interfaceMode)
          {
            case INTERFACE_MODE_NORMAL: 
              voices[controlSynthVoice]->setParam(SYNTH_PARAMETER_ENVELOPE_DECAY, iCurrentAnalogValue[ANALOG_INPUT_MOD_ENVELOPE1]);
              break;
            case INTERFACE_MODE_SHIFT:  
              //todo: shift function for decay knob
              break;
          }
          setParameterLock(getParameterLockChannel(ANALOG_INPUT_MOD_ENVELOPE1), iCurrentAnalogValue[ANALOG_INPUT_MOD_ENVELOPE1]);
          
          break;


        case ANALOG_INPUT_MOD_ENVELOPE2:
          
          switch (interfaceMode)
          {
            case INTERFACE_MODE_NORMAL: 
              voices[controlSynthVoice]->setParam(SYNTH_PARAMETER_ENVELOPE_ATTACK, iCurrentAnalogValue[ANALOG_INPUT_MOD_ENVELOPE2]);
              break;
            case INTERFACE_MODE_SHIFT:  
              //todo: shift function for attack knob
              break;
          }
          setParameterLock(getParameterLockChannel(ANALOG_INPUT_MOD_ENVELOPE2), iCurrentAnalogValue[ANALOG_INPUT_MOD_ENVELOPE2]);
          
          break;

      }
      
      iLastAnalogValue[i] = iCurrentAnalogValue[i];
    }
  }
  
}



/*----------------------------------------------------------------------------------------------------------
 * updateNoteDecay
 * sets the length of the notes
 *----------------------------------------------------------------------------------------------------------
 */
void updateNoteDecay(bool ignoreRecordMode)
{
  uint16_t nextNoteLength;
  
  nextNoteLength = scaleAnalogInputNonLinear(iCurrentAnalogValue[ANALOG_INPUT_DECAY], 512, 512, 16384);
  sequencer.setNextNoteLength(nextNoteLength);
  
}



/*----------------------------------------------------------------------------------------------------------
 * updateTonic
 * sets the tonic to one of the natural notes
 *----------------------------------------------------------------------------------------------------------
 */
void updateTonic(int incr)
{
  int8_t tonicNotes[7] = { 0, 2, 4, 5, 7, 9, 11};
  byte tonicIndex;
  
  for (uint8_t i = 0; i < 7; i++)
  {
    if(tonicNotes[i] == sequencer.getTonic())
    {
      tonicIndex = i;    
    }
  }
  tonicIndex = (tonicIndex + incr) % 7;
  
  sequencer.setTonic(tonicNotes[tonicIndex]);
  displayTonicIcon();

}



/*----------------------------------------------------------------------------------------------------------
 * getMidiNoteIconIndex
 * returns the icon index for the given midi note (represented as A, B C, etc...)
 *----------------------------------------------------------------------------------------------------------
 */
uint8_t getMidiNoteIconIndex(uint8_t midinote)
{
  uint8_t basenote;
  basenote = (midinote) % 12;
  
  // todo: fix this
  switch(basenote)
  {
    case 0: return 2; //C
    case 2: return 3; //D
    case 4: return 4; //E
    case 5: return 5; //F
    case 7: return 6; //G
    case 9: return 0; //A
    case 11: return 1; //B
    default: return 0; //A
  }
}







/*----------------------------------------------------------------------------------------------------------
 * updateAudio
 * returns the current source audio to be output on pin 9
 * mixes the two voices together with simple addition
 *----------------------------------------------------------------------------------------------------------
 */
int updateAudio()
{
  return MonoOutput::fromNBit(9, voices[0]->updateAudio() + voices[1]->updateAudio());
}





/*----------------------------------------------------------------------------------------------------------
 * updateDisplay
 * updates the matrix display
 *----------------------------------------------------------------------------------------------------------
 */
void updateDisplay()
{
  byte currentNote[MAX_SEQUENCER_TRACKS];
  byte currentStep[MAX_SEQUENCER_TRACKS];
  

  if (settingDisplayTimer.ready())
  {
    currentNote[SEQUENCER_TRACK_0] = sequencer.getCurrentNote(SEQUENCER_TRACK_0);
    currentStep[SEQUENCER_TRACK_0] = sequencer.getCurrentStep(SEQUENCER_TRACK_0);
    currentNote[SEQUENCER_TRACK_1] = sequencer.getCurrentNote(SEQUENCER_TRACK_1);
    currentStep[SEQUENCER_TRACK_1] = sequencer.getCurrentStep(SEQUENCER_TRACK_1);
    
    // set current voice display
    ledDisplay.setRowPixels(CURRENT_VOICE_DISPLAY_ROW,controlSynthVoice == SEQUENCER_TRACK_0 ? 240 : 15);
 
    //blank out row 0 & 3 to clear any old icons - these rows are currently otherwise unused
    ledDisplay.setRowPixels(0, 0);
    ledDisplay.setRowPixels(3, 0);


    // clear sequencer step rows
    ledDisplay.setRowPixels(SEQUENCER_TRACK_0_DISPLAY_ROW_0, 0);
    ledDisplay.setRowPixels(SEQUENCER_TRACK_0_DISPLAY_ROW_1, 0);
    ledDisplay.setRowPixels(SEQUENCER_TRACK_1_DISPLAY_ROW_0, 0);
    ledDisplay.setRowPixels(SEQUENCER_TRACK_1_DISPLAY_ROW_1, 0);

    if(currentStep[SEQUENCER_TRACK_0] < 8)
    {
      ledDisplay.setPixel(currentStep[SEQUENCER_TRACK_0]%8, SEQUENCER_TRACK_0_DISPLAY_ROW_0, (currentNote[SEQUENCER_TRACK_0] > 0));
    }
    else
    {
      ledDisplay.setPixel(currentStep[SEQUENCER_TRACK_0]%8, SEQUENCER_TRACK_0_DISPLAY_ROW_1, (currentNote[SEQUENCER_TRACK_0] > 0));
    }  

    if(currentStep[SEQUENCER_TRACK_1] < 8)
    {
      ledDisplay.setPixel(currentStep[SEQUENCER_TRACK_1]%8, SEQUENCER_TRACK_1_DISPLAY_ROW_0, (currentNote[SEQUENCER_TRACK_1] > 0));
    }
    else
    {
      ledDisplay.setPixel(currentStep[SEQUENCER_TRACK_1]%8, SEQUENCER_TRACK_1_DISPLAY_ROW_1, (currentNote[SEQUENCER_TRACK_1] > 0));
    }  

    //display trigger
    ledDisplay.setPixel(TRIGGER_DISPLAY_COL, TRIGGER_DISPLAY_ROW, iTrigger == HIGH);

    displaySequenceLength();

    ledDisplay.refresh();
  }

}


/*----------------------------------------------------------------------------------------------------------
 * displaySequenceLength
 * displays a pixel indicating the length of each sequence 
 *----------------------------------------------------------------------------------------------------------
 */
void displaySequenceLength()
{
  byte sequenceLength[MAX_SEQUENCER_TRACKS];

  sequenceLength[SEQUENCER_TRACK_0] = sequencer.getSequenceLength(SEQUENCER_TRACK_0);
  sequenceLength[SEQUENCER_TRACK_1] = sequencer.getSequenceLength(SEQUENCER_TRACK_1);

  // invert last step in sequence so user can see how long the sequence is
  if(sequenceLength[SEQUENCER_TRACK_0] <= 8)
  {
    ledDisplay.togglePixel((sequenceLength[SEQUENCER_TRACK_0]-1)%8, SEQUENCER_TRACK_0_DISPLAY_ROW_0);
  }
  else if (sequenceLength[SEQUENCER_TRACK_0] > 1)
  {
    ledDisplay.togglePixel((sequenceLength[SEQUENCER_TRACK_0]-1)%8, SEQUENCER_TRACK_0_DISPLAY_ROW_1);
  }  
  else
  {
    // if sequence length = 1 then just light step 1
    ledDisplay.setPixel(0, SEQUENCER_TRACK_0_DISPLAY_ROW_0, 1);
  }

  // invert last step in sequence so user can see how long the sequence is
  if(sequenceLength[SEQUENCER_TRACK_1] <= 8)
  {
    ledDisplay.togglePixel((sequenceLength[SEQUENCER_TRACK_1]-1)%8, SEQUENCER_TRACK_1_DISPLAY_ROW_0);
  }
  else if (sequenceLength[1] > 1)
  {
    ledDisplay.togglePixel((sequenceLength[SEQUENCER_TRACK_1]-1)%8, SEQUENCER_TRACK_1_DISPLAY_ROW_1);
  }  
  else
  {
    // if sequence length = 1 then just light step 1
    ledDisplay.setPixel(0, SEQUENCER_TRACK_1_DISPLAY_ROW_0, 1);
  }
  //ledDisplay.refresh();
}

/*----------------------------------------------------------------------------------------------------------
 * displaySettingIcon
 * copies the bitmap values from program memory then displays the given icon on the screen 
 * and sets a timer and a flag for it to be cleared after a set time
 *----------------------------------------------------------------------------------------------------------
 */
void displaySettingIcon(const byte* bitmap)
{
  byte bitmapRAM[8];

  for (byte i = 0; i < 8; i++) 
  {  
    bitmapRAM[i] = pgm_read_byte_near(bitmap + i);
  }

  ledDisplay.displayIcon(&bitmapRAM[0]);
  settingDisplayTimer.start(DISPLAY_SETTING_CHANGE_PERSIST_MILLIS);
}



/*----------------------------------------------------------------------------------------------------------
 * displayTonicIcon
 * OR's the alpha note name with the numeric octave to create a tonic icon eg G3
 *----------------------------------------------------------------------------------------------------------
 */
void displayTonicIcon()
{
  byte bitmapRAM[8];
  const byte* alphaBitmap;
  const byte* octaveBitmap;

  octaveBitmap  = BITMAP_NUMERALS[sequencer.getOctave()];
  alphaBitmap   = BITMAP_ALPHA[getMidiNoteIconIndex(sequencer.getTonic())];

  for (byte i = 0; i < 8; i++) 
  {  
    bitmapRAM[i] = pgm_read_byte_near(alphaBitmap + i) | (pgm_read_byte_near(octaveBitmap + i) >> 4);
  }

  ledDisplay.displayIcon(&bitmapRAM[0]);
  settingDisplayTimer.start(DISPLAY_SETTING_CHANGE_PERSIST_MILLIS);
}


/*----------------------------------------------------------------------------------------------------------
 * debugWriteValue
 *----------------------------------------------------------------------------------------------------------
 */
void debugWriteValue(char* valueName, int value)
{
  #ifndef ENABLE_MIDI_OUTPUT
  Serial.print(valueName);
  Serial.print(F("="));
  Serial.println(value);
  #endif
}






/*----------------------------------------------------------------------------------------------------------
 * scaleAnalogInput
 * scales an analog input from 0-1023 to 0-maxScale
 * also trims off bottom and top end of range to saturate signal at high and low values
 *----------------------------------------------------------------------------------------------------------
 */
int scaleAnalogInput(int rawValue, int maxScale)
{
  if (rawValue < 3)
  {
    return 0;    
  }
  else if (rawValue > MAX_ANALOG_VALUE)
  {
    return maxScale;
  }
  else
  {
    return ((float)rawValue / MAX_ANALOG_VALUE) * maxScale;
  }
}




/*----------------------------------------------------------------------------------------------------------
 * scaleAnalogInput
 * scales an analog input from 0-1023 to 0-maxScale with a non-linear kink at kneeX,kneeY
 * assumes 1000 = max
 * todo: very inefficient - either don't use it or work out a better way to do it.
 *----------------------------------------------------------------------------------------------------------
 */
long scaleAnalogInputNonLinear(long rawValue, long kneeX, long kneeY, long maxScale)
{
  if (rawValue<=kneeX)
  {
    return rawValue * kneeY / kneeX;
  }
  else
  {

    return rawValue * (maxScale-kneeY) / (MAX_ANALOG_VALUE-kneeX) 
           - (maxScale-kneeY)*kneeX/(MAX_ANALOG_VALUE-kneeX) + kneeY;
  }
           
}



/*----------------------------------------------------------------------------------------------------------
 * analogInputHasChanged
 * returns true if the given analog input channel has moved by more than the noise threshold
 *----------------------------------------------------------------------------------------------------------
 */
bool analogInputHasChanged(byte inputChannel)
{
  return abs(iCurrentAnalogValue[inputChannel] - iLastAnalogValue[inputChannel]) > ANALOG_INPUT_MOVEMENT_THRESHOLD;
}


/*----------------------------------------------------------------------------------------------------------
 * loop
 * calls Mozzi's audioHook() function
 * all other control code moved to updateControls()
 *----------------------------------------------------------------------------------------------------------
 */

void loop() 
{
  //required for Mozzi
  audioHook();
}
