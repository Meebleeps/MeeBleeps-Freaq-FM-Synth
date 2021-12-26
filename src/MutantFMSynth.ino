#include <Arduino.h>
#include <MozziGuts.h>
#include <mozzi_rand.h>
#include <EventDelay.h>

#include "avSource.h"
#include "avSequencerMultiTrack.h"
#include "LedMatrix.h"
#include <avr/pgmspace.h>

// define the pins
#define PIN_SYNC_IN       8
#define PIN_SYNC_OUT      2
#define TIMER_SYNC_PULSE_OUTPUT_MILLIS 10

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
#define LFO_TRACK_1_DISPLAY_ROW           3

#define MAX_ANALOG_INPUTS               6
#define MAX_ANALOG_VALUE                1023

#define ANALOG_INPUT_MOD_AMOUNT         0
#define ANALOG_INPUT_DECAY              1
#define ANALOG_INPUT_MUTATION           2
#define ANALOG_INPUT_LFORATE            3
#define ANALOG_INPUT_MOD_RATIO          4
#define ANALOG_INPUT_STEPCOUNT          5

#define ANALOG_INPUT_MOVEMENT_THRESHOLD 13

// listed in order of left to right.
#define BUTTON_INPUT_FUNC   0
#define BUTTON_INPUT_TONIC  3
#define BUTTON_INPUT_SCALE  2
#define BUTTON_INPUT_START  1
#define BUTTON_INPUT_REC    5
#define BUTTON_INPUT_RETRIG 4
#define MAX_OSCILLATOR_MODES 1

#define INTERFACE_MODE_NORMAL 0
#define INTERFACE_MODE_SHIFT  1

#define DISPLAY_SETTING_CHANGE_PERSIST_MILLIS 350

#define MOTION_RECORD_NONE  0
#define MOTION_RECORD_REC   1
#define MOTION_RECORD_CLEAR 2

// todo:  decrease INTERFACE_UPDATE_DIVIDER  if controls are externally modulated via CV voltage inputs
#define INTERFACE_UPDATE_DIVIDER_ANALOG 4
#define INTERFACE_UPDATE_DIVIDER_DIGITAL 8

const PROGMEM byte BITMAP_MEEBLEEPS[]  = {B00011000,B01111110,B11011011,B11011011,B11011011,B11011011,B11000011,B00000110};
const PROGMEM byte BITMAP_NUMERALS[8][8]  = {
                                       {B00000000,B00000000,B00000000,B11100000,B10100000,B10100000,B10100000,B11100111}
                                      ,{B00000000,B00000000,B00000000,B01000000,B01000000,B01000000,B01000111,B01000000}
                                      ,{B00000000,B00000000,B00000000,B11100000,B00100000,B11100111,B10000000,B11100000}
                                      ,{B00000000,B00000000,B00000000,B11100000,B00100111,B11100000,B00100000,B11100000}
                                      ,{B00000000,B00000000,B00000000,B10100111,B10100000,B11100000,B00100000,B00100000}
                                      ,{B00000000,B00000000,B00000111,B11100000,B10000000,B11100000,B00100000,B11100000}
                                      ,{B00000000,B00000111,B00000000,B11100000,B10000000,B11100000,B10100000,B11100000}
                                      ,{B00000000,B00000111,B00000000,B11100000,B00100000,B00100000,B00100000,B00100000}
                                    };
const PROGMEM byte BITMAP_ALPHA[7][8]  = {
                                     {B00000000,B00000000,B00000000,B11100000,B10100000,B11100000,B10100000,B10100111}
                                    ,{B00000000,B00000000,B00000000,B11100000,B10100000,B11000000,B10100111,B11100000}
                                    ,{B00000000,B00000000,B00000000,B11100000,B10000000,B10000111,B10000000,B11100000}
                                    ,{B00000000,B00000000,B00000000,B11000000,B10100111,B10100000,B10100000,B11000000}
                                    ,{B00000000,B00000000,B00000000,B11100111,B10000000,B11000000,B10000000,B11100000}
                                    ,{B00000000,B00000000,B00000111,B11100000,B10000000,B11000000,B10000000,B10000000}
                                    ,{B00000000,B00000111,B00000000,B11100000,B10000000,B10000000,B10100000,B11100000}
                                  };
const PROGMEM byte BITMAP_ALGORITHMS[2][8]  = {
                                        {B01000001,B10010000,B00000100,B01010000,B01000010,B01001000,B01000001,B01000100}
                                      , {B00100001,B01000010,B00000100,B11101000,B00100000,B11100001,B10000010,B11100100}
                                      };

const PROGMEM byte BITMAP_VOICES[2][8]  = {
                                        {B11110000,B00000000,B00000000,B10101000,B10101000,B10101000,B01001000,B01001000}
                                      , {B00001111,B00000000,B00000000,B10101110,B10100010,B10101110,B01001000,B01001110}
};


byte firstTimeStart = true;
byte iTrigger = 1;      
byte iLastTrigger = 0;
byte interfaceMode    = INTERFACE_MODE_NORMAL;
byte controlSynthVoice = 0;
byte motionRecordMode = MOTION_RECORD_NONE;

int iLastAnalogValue[MAX_ANALOG_INPUTS]    = {0,0,0,0,0,0};
int iCurrentAnalogValue[MAX_ANALOG_INPUTS] = {0,0,0,0,0,0};

//byte iCurrentButton[MAX_BUTTON_INPUTS];
//byte iLastButton[MAX_BUTTON_INPUTS];

uint8_t bitsLastButton;
uint8_t bitsCurrentButton;
uint8_t bitsLastParamLock;


inline uint8_t getLastButtonState(uint8_t buttonIndex)
{
  return (bitsLastButton >> buttonIndex) & 1;
}

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

inline uint8_t getCurrentButtonState(uint8_t buttonIndex)
{
  return (bitsCurrentButton >> buttonIndex) & 1;
}

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


/*
  on start
    for each analog value set targetvalue[i][currentmode] = current_value


  on mode change
  
  // store knob positions of old mode
  for each analog value set targetvalue[i][oldmode] = current_value

    // if current value is within +- ANALOG_TRACK_TOLERANCE then track the knob
    set trackknob[i] = abs(current_value[i] -targetvalue[i][newmode]) < ANALOG_TRACK_TOLERANCE;

    set targetcrossingdirection = if current_value[i] < targetvalue[i][newmode] then 1 else -1 end


  on knob value change
    // if we are not already trakcing the knob and current value is within +- ANALOG_TRACK_TOLERANCE then start tracking the knob
    if !trackknob[i] set trackknob[i] = abs(current_value[i] -targetvalue[i][newmode]) < ANALOG_TRACK_TOLERANCE;


*/

byte updateCounter = 0;


// MOZZI variables
#define CONTROL_RATE 128   // reduce this from 256 to save processor to get our extra oscillator
 

MutatingFM          voice0;
MutatingFM          voice1;
MutatingFM*         voices[2];  // array to hold the voices to simplfy the code (but does make it less efficient)
MutatingSequencerMultiTrack sequencer;
LedMatrix           ledDisplay;
EventDelay          syncOutputTimer;
EventDelay          settingDisplayTimer;
bool                settingDisplayIconOn;





/*----------------------------------------------------------------------------------------------------------
 * setup()
 *----------------------------------------------------------------------------------------------------------
 */
void setup() 
{
  Serial.begin(115200);
  Serial.print(F("\n\n\n\n\n-----------------------------------------------------------------------\n"));
  Serial.print(F("Freaq - Mozzi-based Mutating FM Synth.   Build "));
  Serial.print  (__DATE__);
  Serial.print  (F(" "));
  Serial.println  (__TIME__);

  initialisePins();
  initialiseDisplay();
  initialiseSources();
  initialiseSequencer();
  
  startMozzi(CONTROL_RATE);
}


/*----------------------------------------------------------------------------------------------------------
 * debugPrintMemoryUsage
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
  
  settingDisplayIconOn = false;
  settingDisplayTimer.start(0);

  // display logo
  for (uint8_t i=10; i< 100; i+=10)
  {
    ledDisplay.clearScreen();
    delay(i);
    //ledDisplay.displayIcon(BITMAP_MEEBLEEPS);
    displaySettingIcon(BITMAP_MEEBLEEPS);
    delay(i);
  }
  
  for(int i = 15; i>= 0; i--)
  {
    ledDisplay.setIntensity(i);

    delay(20);
  }

  ledDisplay.setIntensity(4);
  // ledDisplay.displayIcon(BITMAP_MEEBLEEPS);
  displaySettingIcon(BITMAP_MEEBLEEPS);

  
  Serial.println(F("initialiseDisplay() complete"));
}




/*----------------------------------------------------------------------------------------------------------
 * initialisePins
 *----------------------------------------------------------------------------------------------------------
 */
void initialisePins()
{
  Serial.println(F("initialisePins()"));


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
 * every INTERFACE_UPDATE_DIVIDER steps, check controls
 * every step - check for sync trigger and update sequencer
 * if this gets delayed, note on/off commands will drift
 *----------------------------------------------------------------------------------------------------------
 */
void updateControl()
{
  updateCounter++;

  // do this every time to ensure sync
  updateSyncTrigger();

  // update sequencer returns true if the seqeuncer has moved to next step
  if (updateSequencer())
  {
    //only update the display once per sequencer step    
    updateDisplay();

    // if the sequencer is on an even step - output a sync pulse
    if (sequencer.outputSyncPulse())
    {
      outputSyncPulse();
    }

  }

  // check to see if the output sync pulse needs to be pulled low
  if (syncOutputTimer.ready())
  {
    digitalWrite(PIN_SYNC_OUT, LOW);
  }


  // check controls once every INTERFACE_UPDATE_DIVIDER steps for efficiency
  if(updateCounter % INTERFACE_UPDATE_DIVIDER_DIGITAL == 0)
  {
    // check button controls first to ensure interface mode is correctly set
    updateButtonControls();

  }

  // check controls once every INTERFACE_UPDATE_DIVIDER steps for efficiency
  if(updateCounter % INTERFACE_UPDATE_DIVIDER_ANALOG == 0)
  {
    // now check analog controls
    updateAnalogControls();

    //update display of LFO modulation at higher rate than rest of display
    updateLFOModulationDisplay();
    
  }



  // now that all controls are updated, update the source.

  voices[0]->updateControl();
  voices[1]->updateControl();


}



void updateLFOModulationDisplay()
{
  if (!firstTimeStart && settingDisplayTimer.ready())
  {
    ledDisplay.drawLineH(LFO_TRACK_0_DISPLAY_ROW, 0, 8, false);
    ledDisplay.drawLineH(LFO_TRACK_1_DISPLAY_ROW, 0, 8, false);
    ledDisplay.setPixel(scaleAnalogInput(voice0.getLFOValue()*4,8), LFO_TRACK_0_DISPLAY_ROW, true);
    ledDisplay.setPixel(scaleAnalogInput(voice1.getLFOValue()*4,8), LFO_TRACK_1_DISPLAY_ROW, true);
    ledDisplay.refresh();
  }
}



/*----------------------------------------------------------------------------------------------------------
 * updateSequencer
 * updates sequencer
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
        getParameterLocks();

        voices[i]->noteOn(nextNote[i], 255, nextNoteLength);

      }
    }

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
  syncOutputTimer.start(TIMER_SYNC_PULSE_OUTPUT_MILLIS);
  digitalWrite(PIN_SYNC_OUT,  HIGH);
}


/*----------------------------------------------------------------------------------------------------------
 * updateSequencerModulation
 * updates source parameters based on stored modulation sequence
 * parameter locks are stored in the first track only
 *----------------------------------------------------------------------------------------------------------
 */
void getParameterLocks()
{
  uint16_t  thisParamLock;
  uint16_t  lastParamLock;
  uint8_t   thisStep;
  uint8_t   lastStep;

  //  todo: re-enable this loop if parameter locks are stored per track
  //for(uint8_t i = 0; i < MAX_SEQUENCER_TRACKS; i++)
  //{
    thisStep = sequencer.getCurrentStep(SEQUENCER_TRACK_0);

    for (uint8_t paramIndex = 0; paramIndex < MAX_ANALOG_INPUTS; paramIndex++)
    {
      thisParamLock = sequencer.getParameterLock(paramIndex, SEQUENCER_TRACK_0, thisStep);
      
      if (thisParamLock != 0)
      {
        /*
        Serial.print(F("parameter lock for channel "));
        Serial.print(i);
        Serial.print(F(" parameter "));
        Serial.print(paramIndex);
        Serial.print(F(" value "));
        Serial.println(thisParamLock);
        */
        // set the flag for this input - last parameter was a lock
        bitsLastParamLock |= 1 << paramIndex;
      }
      else 
      {
        // if the last step was a parameter lock but this step doesn't have one, set the parameter lock to the current knob position
        if ((bitsLastParamLock >> paramIndex) & 1)
        {
          /*
          Serial.print(F("clearing last parameter lock for channel "));
          Serial.print(i);
          Serial.print(F(" parameter "));
          Serial.print(paramIndex);
          Serial.print(F(" knob value "));
          Serial.println(iCurrentAnalogValue[i]);
          */
          thisParamLock = iCurrentAnalogValue[SEQUENCER_TRACK_0];
        }

        // reset the lock bitflag for this parameter
        bitsLastParamLock &= ~(1 << paramIndex);  

      }
      
      // if there is a parameter lock or we are recovering from a previous step parameter lock 
      if (thisParamLock != 0)
      {
        switch(paramIndex)
        {
          case ANALOG_INPUT_DECAY: 
            sequencer.setNextNoteLength(thisParamLock);
            break;
            
          case ANALOG_INPUT_MOD_AMOUNT: 
            voices[SEQUENCER_TRACK_0]->setParam(SYNTH_PARAMETER_MOD_AMOUNT, thisParamLock);
            break;
            
          case ANALOG_INPUT_MOD_RATIO: 
            voices[SEQUENCER_TRACK_0]->setParam(SYNTH_PARAMETER_MOD_RATIO, iCurrentAnalogValue[ANALOG_INPUT_MOD_RATIO]);
            break;
            
          case ANALOG_INPUT_LFORATE: 
            voices[SEQUENCER_TRACK_0]->setParam(SYNTH_PARAMETER_ENVELOPE_SHAPE, iCurrentAnalogValue[ANALOG_INPUT_LFORATE]);
            break;
        }
      }
    //}
  }


  /*
  if (sequencer.getParameterLock(0) != 0)
  {
    sequencer.setNextNoteLength(sequencer.getParameterLock(0));
  }
  else
  {
    updateNoteDecay(true);
  }

  if (sequencer.getParameterLock(1) != 0)
  {
    voices[0]->setParam(1,sequencer.getParameterLock(1));
  }
  else
  {
    updateOscillatorDetune(true);
  }

  if (sequencer.getParameterLock(2) != 0)
  {
    voices[0]->setParam(2,sequencer.getParameterLock(2));
  }
  else
  {
    updateFilterCutoff(true);
  }

  if (sequencer.getParameterLock(3) != 0)
  {
    voices[0]->setParam(3,sequencer.getParameterLock(3));
  }
  else
  {
    updateFilterResonance(true);
  }

  if (sequencer.getParameterLock(4) != 0)
  {
    voices[0]->setParam(4,sequencer.getParameterLock(4));
  }
  else
  {
    updateFilterShape(true);
  }
  */
}




/*----------------------------------------------------------------------------------------------------------
 * updateSyncTrigger
 * check for sync trigger and update sequencer
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
      //Serial.println("sync!");
      sequencer.syncPulse(SYNC_STEPS_PER_PULSE);
    }

    iLastTrigger = iTrigger;
  }

  
}


/*----------------------------------------------------------------------------------------------------------
 * updateButtonControls
 * check for button presses
 *----------------------------------------------------------------------------------------------------------
 */
void updateButtonControls()
{
  setCurrentButtonState(0,digitalRead(PIN_BUTTON0));
  setCurrentButtonState(1,digitalRead(PIN_BUTTON1));
  setCurrentButtonState(2,digitalRead(PIN_BUTTON2));
  setCurrentButtonState(3,digitalRead(PIN_BUTTON3));
  setCurrentButtonState(4,digitalRead(PIN_BUTTON4));
  setCurrentButtonState(5,digitalRead(PIN_BUTTON5));

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
    Serial.print(interfaceMode);
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
    Serial.print(F("motionRecordMode="));
    Serial.println(motionRecordMode);
  }

  /*
  if (iCurrentButton[BUTTON_INPUT_RETRIG] != iLastButton[BUTTON_INPUT_RETRIG])
  {
    switch(interfaceMode)
    {
      case INTERFACE_MODE_NORMAL:   
        sequencer.setRetrigger(iCurrentButton[BUTTON_INPUT_RETRIG] == HIGH);
        break;
      case INTERFACE_MODE_SHIFT:    
        sequencer.setRetrigger(iCurrentButton[BUTTON_INPUT_RETRIG] == HIGH); // todo: change algorithm?
        break;
    }
  }
  */
  if (getCurrentButtonState(BUTTON_INPUT_RETRIG) != getLastButtonState(BUTTON_INPUT_RETRIG))
  {
    switch(interfaceMode)
    {
      case INTERFACE_MODE_NORMAL:   
        if (getCurrentButtonState(BUTTON_INPUT_RETRIG) == HIGH)
        {
          updateSynthControl();
        }
        break;
      case INTERFACE_MODE_SHIFT:    
        // todo: shift mode
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
        sequencer.syncPulse(SYNC_STEPS_PER_TAP);  // todo: send tap tempo message to the sequencer
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
        updateTonic(-1);
        break;
    }
  }

  if (getCurrentButtonState(BUTTON_INPUT_SCALE) != getLastButtonState(BUTTON_INPUT_SCALE) && getCurrentButtonState(BUTTON_INPUT_SCALE) == HIGH)
  {
    switch(interfaceMode)
    {
      case INTERFACE_MODE_NORMAL:   
        // todo: update scale 
        updateScale();
        break;
      case INTERFACE_MODE_SHIFT:    
        updateAlgorithm();
        break;
    }
  }

  for (uint8_t i=0; i<MAX_BUTTON_INPUTS; i++)
  {
    // debug only
    
    if (getLastButtonState(i) != getCurrentButtonState(i))
    {
      Serial.print(F("Button state changed - button["));
      Serial.print(i);
      Serial.print(F("] state = "));
      Serial.println(getCurrentButtonState(i));

      ledDisplay.setPixel(i,1, getCurrentButtonState(i) == HIGH);
    }
    
  }


  bitsLastButton = bitsCurrentButton;
  //setLastButtonState() iLastButton[i] = getCurrentButtonState(i);
  

  ledDisplay.refresh();
  

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
  Serial.print(F("Voice Control="));
  Serial.println(controlSynthVoice);
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
      Serial.print(F("analog input changed: "));
      Serial.println(i);

      switch(i)
      {
        case ANALOG_INPUT_DECAY: 
          updateNoteDecay(false);
          setParameterLock(ANALOG_INPUT_DECAY, iCurrentAnalogValue[ANALOG_INPUT_DECAY]);
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
          setParameterLock(ANALOG_INPUT_MOD_AMOUNT, iCurrentAnalogValue[ANALOG_INPUT_MOD_AMOUNT]);

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
          setParameterLock(ANALOG_INPUT_MOD_RATIO, iCurrentAnalogValue[ANALOG_INPUT_MOD_RATIO]);
          
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
          
          sequencer.setSequenceLength(controlSynthVoice, scaleAnalogInput(iCurrentAnalogValue[ANALOG_INPUT_STEPCOUNT],16) + 1);
          
         break;

        case ANALOG_INPUT_LFORATE:
          //todo: when programming for nano with 8 analog inputs, separate these inputs into: ATTACK, DECAY, LFO
          switch (interfaceMode)
          {
            case INTERFACE_MODE_NORMAL: 
              voices[controlSynthVoice]->setParam(SYNTH_PARAMETER_ENVELOPE_SHAPE, iCurrentAnalogValue[ANALOG_INPUT_LFORATE]);
              setParameterLock(ANALOG_INPUT_LFORATE, iCurrentAnalogValue[ANALOG_INPUT_LFORATE]);
              break;
            case INTERFACE_MODE_SHIFT:  
              voices[controlSynthVoice]->setLFOFrequency((float)scaleAnalogInputNonLinear(iCurrentAnalogValue[ANALOG_INPUT_LFORATE],512,100,5000)/(float)100.0);
              break;
          }
          
          break;
      }
      
      iLastAnalogValue[i] = iCurrentAnalogValue[i];
    }
  }
  
}


/*
void updateDuckingAmount()
{
  sequencer.setDuckingAmount(scaleAnalogInput(iCurrentAnalogValue[ANALOG_INPUT_OSC2TUNE],255));
}
*/

inline void setParameterLock(uint8_t paramChannel, uint16_t value)
{
  switch (motionRecordMode)
  {
    case MOTION_RECORD_REC:
        sequencer.setParameterLock(paramChannel, value);
        Serial.println(F("setParameterLock(0)"));
        break;

    case MOTION_RECORD_CLEAR:
        sequencer.clearAllParameterLocks(paramChannel);
        Serial.println(F("Clear ParameterLock(0)"));
        break;

    case MOTION_RECORD_NONE:
        // do nothing 
        break;
  }
}



/*----------------------------------------------------------------------------------------------------------
 * updateNoteDecay
 * 
 *----------------------------------------------------------------------------------------------------------
 */
void updateNoteDecay(bool ignoreRecordMode)
{
  uint16_t nextNoteLength;
  
  nextNoteLength = scaleAnalogInputNonLinear(iCurrentAnalogValue[ANALOG_INPUT_DECAY], 768, 2048, 10000);
  sequencer.setNextNoteLength(nextNoteLength);
  
}



/*----------------------------------------------------------------------------------------------------------
 * updateTonic
 *----------------------------------------------------------------------------------------------------------
 */
void updateTonic(int incr)
{

                          // C   D   E   F   G   A   B   C   D   E   F   G   A   B   C   D   E   F   G   A   B   C   D   E   F   G
  int8_t tonicNotes[26] = { 25, 26, 28, 29, 31, 33, 35, 36, 38, 40, 41, 43, 45, 47, 48, 50, 52, 53, 55, 57, 59, 60, 62, 64, 65, 67};
  byte tonicIndex;
  
  for (uint8_t i = 0; i < 26; i++)
  {
    if(tonicNotes[i] == sequencer.getTonic())
    {
      tonicIndex = i;    
    }
  }
  tonicIndex = (tonicIndex + incr) % 26;
  
  sequencer.setTonic(tonicNotes[tonicIndex]);
  displaySettingIcon(BITMAP_ALPHA[getMidiNoteIconIndex(sequencer.getTonic())]);
}



uint8_t getMidiNoteIconIndex(uint8_t midinote)
{
  uint8_t basenote;
  basenote = (midinote - 9) % 12;

  switch(basenote)
  {
    case 0: return 0;
    case 2: return 1;
    case 3: return 2;
    case 5: return 3;
    case 7: return 4;
    case 8: return 5;
    case 10: return 6;
    default: return 0;
  }
}







/*----------------------------------------------------------------------------------------------------------
 * updateAudio
 * returns the current source audio to be output on pin 9
 *----------------------------------------------------------------------------------------------------------
 */
int updateAudio()
{
//  return MonoOutput::fromNBit(8, voices[0]->updateAudio());
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
  
  byte scaledInputValue[MAX_ANALOG_INPUTS];
  
  if (settingDisplayTimer.ready())
  {
    currentNote[SEQUENCER_TRACK_0] = sequencer.getCurrentNote(SEQUENCER_TRACK_0);
    currentNote[SEQUENCER_TRACK_1] = sequencer.getCurrentNote(SEQUENCER_TRACK_1);
    currentStep[SEQUENCER_TRACK_0] = sequencer.getCurrentStep(SEQUENCER_TRACK_0);
    currentStep[SEQUENCER_TRACK_1] = sequencer.getCurrentStep(SEQUENCER_TRACK_1);
    
    // set current voice display
    ledDisplay.setRowPixels(0,controlSynthVoice == SEQUENCER_TRACK_0 ? 240 : 15);
 

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
    ledDisplay.setPixel(7, 1, iTrigger == HIGH);

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
  settingDisplayIconOn = true;
}


/*----------------------------------------------------------------------------------------------------------
 * debugWriteValue
 *----------------------------------------------------------------------------------------------------------
 */
void debugWriteValue(char* valueName, int value)
{
  Serial.print(valueName);
  Serial.print(F("="));
  Serial.println(value);
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
