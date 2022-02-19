# Meebleeps Freaq FM Synth
## Arduino Synth with Dual-voice 2-OP FM | Generative 2-track Polymetric Sequencer | Mozzi SDK

An evolution of my first Mozzi synth build, this one features dual independent 2-operator FM voices paired with a 2-track generative sequencer.  

Each voice has independent carrier & modulator waveforms, FM ratio, modulation envelope & LFO.

Both tracks share the a single pattern (inspired by the Intellijel Metropolix) and can be stacked in unison or have different polymetric step-counts.  Track 1 can be offset 0-3 octaves above track 2 for separate bass & lead.

It started simple... but ended up cramming the 32K flash to the brim with features and wavetables, maxxing out the interface with obscure button combinations, and pushing the 16Mhz Arduino Nano to the edge!  Upside is it makes lots of funky noises now!


 #### Source Code:      https://github.com/Meebleeps/MeeBleeps-Freaq-FM-Synth
 #### Youtube Demo:     https://www.youtube.com/watch?v=KD6IrcmMkoA
 #### Youtube Channel:  https://www.youtube.com/channel/UC4I1ExnOpH_GjNtm7ZdWeWA

Source released under Creative Commons Attribution-NonCommercial-ShareAlike 4.0
Copyright (C) 2022 Meebleeps

***
__Important build-note:__ my builds have used normally-closed switches, so the code assumes this.  A couple of people building this have had issues because they have normally-open switches, so to adjust for this edit the code in updateButtonControls() to invert the results of each call to digitalRead(). 
***

## Voices

-   2 independent FM voices
-   2-operator FM (for old-school prince of persia vibes! 😂)
-   Multi-mode FM ratios - quantised, free-multiple, independent
-   Multiple operator waveforms for carrier & modulator - Sine, Saw, Reverse Saw, Square, Noise, Off
-   Modulation level controlled by Attack/Decay envelope and LFO per-voice
-   Multiple LFO waveforms (same tables as the carrier oscillators) Sine, Saw, Reverse Saw, Square, Noise

## Sequencer

-   2/1.5 track polymetric sequencer with up to 16 steps per track (Both tracks use same note sequence but can have different step-counts for polymetric phasing)
-   Multiple generative algorithms - (semi)random notes, (semi)random runs, arpeggio, drone 
-   Sequence mutates/evolves at user-defined rate & note-density
-   Selectable tonic, octave & scale quantisation (Major, Minor, Pentatonic, Phrygian (GOA!), Octaves, Fifths)
-   Tap-tempo control
-   Sync input & output (Korg Volca compatible) 
-   16-step parameter-lock recording of synth parameters for track 1 (the Elektron way!)

## Hardware 
-   Arduino Nano (Elegoo) 
-   Simple Mozzi output circuit 
-   Added power filtering caps on DC input and 5V rail
-   Designed to fit into Volca form factor 
-   Laser-cut metalisized acrylic faceplate 
-   Laser-cut wooden enclosure 
-   7V-12DC DC Power input 
-   Access to nano's USB port for firmware upgrades 

## User Guide

### Buttons

-   Tonic - Select tonic through natural notes A - G
    -   [Func] + Tonic - Shift base octave from 0 - 5
    -   [Func][Rec] + Tonic - Select octave offset to Track 1 (track 1 & 2 can be 0-4 octaves apart)
-   Scales - Select current musical scale
    -   [Func] + Scales - Select current generative algorithm
    -   [Func][Rec] + Scales - Select the LFO waveform for the current voice 
-   Rec + knob - Hold down while moving a knob to record those parameters to the sequence 
    -   [Func][Rec] + knob - Hold down while moving a knob to delete the recorded sequence for that parameter 
    -   [Func][Rec] + buttons - Access various secondary functions
-   Start - Start or stop the sequencer
    -   [Func] + Start - Tap tempo
    -   [Func][Rec] + Start - Select the MODULATOR waveform for the current voice 
-   Voice - Select the active track which can be edited using the parameter inputs
    -   [Func] + Voice - Select the FM ratio mode for the current voice
    -   [Func][Rec] + Voice - Select CARRIER Waveform for the current voice 

### Rotary Inputs

-   Mutation - Adjust the likelihood that the sequence will change over time
    -   [Func] + Mutation - Adjust the liklihood that any given step will be a note or a rest
-   Population - Adjust the number of steps for the active track
    -   [Func] + Population - Adjust the number of steps for all tracks
-   Lifespan - Adjust the length of the notes (for all tracks)
-   Ratio - Select the carrier--to-modulator FM ratio (based on the current FM mode)
-   Wobble - Adjust the amount of LFO to apply to the modulator level for the active track
    -   [Func] + Wobble - Adjust the LFO rate for the active track
-   Attack - Adjust the attack time for the modulator level envelope
-   Decay - Adjust the decay time for the modulator level envelope
-   Depth - Adjust the amount of envelope to apply to the modulator level for the active track

## Issues / Limitations

-   Fair bit of jitter in the sequencer timing when tracking external sync due to somewhat naive approach to the 2-step-per-pulse volca sync 
-   Mozzi audio library has 10ms buffer 
-   The code is written for NORMALLY CLOSED switches - see latest updates in MutantFMSynth.ino updateButtonControls() for support for NORMALLY OPEN
-   The code doesn't fit in the Arduino Nano when compiled using the Arduino IDE

### Compiling on the Arduino IDE

A few people have let me know the compiled code doesn't fit into the Arduino Nano when compiled using the Arduino IDE.  This is probably because different compilers optimise in different ways and I have been using the Visual Studio Code + PlatformIO environment, and it *just* fits. 😬

Easiest options to get it to fit are:
-   migrate to VSCode & PlatformIO (it takes a little bit of setting up but it is a much better IDE) 
or
-   delete one of the wavetables.  The wavetables are all 2KB each, so removing one of them will let the code fit without a problem.  From my experience playing with the synth, the least useful is the ramp :) 
-   to remove a wavetable:
  -   in avSource.h: delete the relevant #include in avSource.h
  -   decrease all #define MAX_WAVEFORMS from 6 to 5, 
  -   amend the definitions of #define WAVEFORM to delete the reference to the wavetable you removed.
  -   in mutantBitmaps.h delete the corresponding bitmap from const PROGMEM byte BITMAP_WAVEFORMS[6][8] and change the defintion from [6][8] to [5][8].

When I get some time I will code a fix so it compiles on both without issues. 


## Thanks

-   Inspired by Hagiwo's modular builds: https://www.youtube.com/channel/UCxErrnnVNEAAXPZvQFwobQw 
-   Mozzi audio library: https://sensorium.github.io/Mozzi/ 
-   Inkscape for vector graphics: https://inkscape.org/ 
-   Laser-cutting by Perth Creative Industries https://www.pcilasercutting.com.au/
-   Fritzing for circuit design: https://fritzing.org/ 
-   Great Scott for electronics tips! https://www.youtube.com/channel/UC6mIxFTvXkWQVEHPsEdflzQ

