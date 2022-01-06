#ifndef AVMIDI_H
#define AVMIDI_H

#include <Arduino.h>

#define ENABLE_MIDI_OUTPUT

 
// plays a MIDI note. 
inline void midiNoteOn(uint8_t channel, uint8_t midiNote, uint8_t velocity) 
{  
  Serial.write(channel << 4);
  Serial.write(midiNote);
  Serial.write(velocity);
}

#endif
