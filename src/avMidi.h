/*----------------------------------------------------------------------------------------------------------
 * avMidi.h
 * 
 * Stub for MIDI-out support 
 * 
 * Tested but disabled.
 * 
 * To re-enable make sure that no other data is output from the Serial port
 * and uncomment the #define ENABLE_MIDI_OUTPUT
 * 
 * Assumes serial port is alreay set up at 31250baud
 * 
 * Source Code Repository:  https://github.com/Meebleeps/MeeBleeps-Freaq-FM-Synth
 * Youtube Channel:         https://www.youtube.com/channel/UC4I1ExnOpH_GjNtm7ZdWeWA
 * 
 * (C) 2021-2022 Meebleeps
*-----------------------------------------------------------------------------------------------------------
*/

#ifndef AVMIDI_H
#define AVMIDI_H

#include <Arduino.h>

// uncomment this to enable midi out support
//#define ENABLE_MIDI_OUTPUT

 
// plays a MIDI note. 
inline void midiNoteOn(uint8_t channel, uint8_t midiNote, uint8_t velocity) 
{  
  Serial.write(channel << 4);
  Serial.write(midiNote);
  Serial.write(velocity);
}

#endif
