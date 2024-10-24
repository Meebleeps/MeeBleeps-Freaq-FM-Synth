# Meebleeps / Wirehead Freaq FM Synth
## Arduino Synth with Dual-voice 2-OP FM | Generative 2-track Polymetric Sequencer | Mozzi SDK

## NOTE This code is an open-source prototype and is not compatible with the Wirehead Instruments version of Freaq FM hardware
- Meebleeps was my personal project and was rebranded as Wirehead Instruments in 2023 to avoid brand confusion with MeeBlip - another completely separate synth design company.

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

## Important build-notes 

- My builds have used normally-closed switches, so the code assumes this.  09 May 2022: Added complier switches to handle normally-open switches
- This will probably compile to Arduino Uno, but it won't work properly without extensive modification as it uses all 8 of the Nano's analog input pins
- If you have issues with the compiled size not fitting in the Nano, this may be due to different compiler (I use VSCode & PlatformIO) or different bootloader in the Arduino (not sure which I have, but am compiling to Elegoo brand arduino nano). 
  - Check the compiler options in **MutantFMSynthOptions.h** to alter switch type or compile size
  - open the file MutantFMSynthOptions.h
  - find the line: `//#define COMPILE_SMALLER_BINARY`
  - uncomment the line so it looks like this:  `#define COMPILE_SMALLER_BINARY`
  - Then compile. It should now easily fit into the Nano.  This omits the semi-random wave tablesbut you probably won't miss it! :)
- Do not install the Mozzi library from PlatformIO in VSCode, it's outdated and will not build successfully. Instead, install its [latest git master branch](https://github.com/sensorium/Mozzi) to `lib/mozzi`. 
- The default settings target the Nano ATmega328 with the new bootloader. If you have avrdude errors during upload, you might have the old bootloader: change both instances of `nanoatmega328new` to `nanoatmega328` in `plaformio.ini`.

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
-   8x8 LED display with a MAX7219-based control board
-   Simple Mozzi output circuit 
-   Added power filtering caps on DC input and 5V rail
-   Designed to fit into Volca form factor 
-   Laser-cut metalisized acrylic faceplate 
-   Laser-cut wooden enclosure 
-   7V-12DC DC Power input 
-   Access to nano's USB port for firmware upgrades 

## Pins

These are the defaults defined for the faceplate controls:

| Pin | Function | Name in `MutantFMSynth.ino` |
|-----|----------|-----------------------------|
| A0  | Mutation | ANALOG_INPUT_MUTATION 
| A1  | Wobble   | ANALOG_INPUT_LFO
| A2  | Population | ANALOG_INPUT_STEPCOUNT
| A3  | Attack | ANALOG_INPUT_MOD_ENVELOPE2 
| A4  | Lifespan | ANALOG_INPUT_DECAY
| A5  | Decay | ANALOG_INPUT_MOD_ENVELOPE1
| A6  | Ratio | ANALOG_INPUT_MOD_RATIO
| A7  | Depth | ANALOG_INPUT_MOD_AMOUNT 
| D3  | Start | BUTTON_INPUT_START
| D4  | Voice | BUTTON_INPUT_VOICE
| D5  | Rec | BUTTON_INPUT_REC
| D6  | Scales | BUTTON_INPUT_SCALE
| D10 | Func | BUTTON_INPUT_FUNC
| D12 | Tonic | BUTTON_INPUT_TONIC


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
-   The code may not fit in the Arduino Nano when compiled using the Arduino IDE or using different bootloaders

### Compiling on the Arduino IDE

If you have issues with the compiled size not fitting in the Nano, this may be due to different compiler (I use VSCode & PlatformIO) or different bootloader in the Arduino (not sure which I have, but am compiling to Elegoo brand arduino nano). 
- Check the compiler options in **MutantFMSynthOptions.h** to alter switch type or compile size
- open the file MutantFMSynthOptions.h
- find the line: //#define COMPILE_SMALLER_BINARY
- uncomment the line so it looks like this:  #define COMPILE_SMALLER_BINARY
- Then compile. It should now easily fit into the Nano.  This omits the semi-random wave tablesbut you probably won't miss it! :)

### Compiling in VSCode / PlatformIO

- Do not install the Mozzi library from PlatformIO in VSCode, it's outdated and will not build successfully. Instead, install its [latest git master branch](https://github.com/sensorium/Mozzi) to `lib/mozzi`. 
- The default settings target the Nano ATmega328 with the new bootloader. If you have avrdude errors during upload, you might have the old bootloader: change both instances of `nanoatmega328new` to `nanoatmega328` in `plaformio.ini`.


## Thanks

-   Inspired by Hagiwo's modular builds: https://www.youtube.com/channel/UCxErrnnVNEAAXPZvQFwobQw 
-   Mozzi audio library: https://sensorium.github.io/Mozzi/ 
-   Inkscape for vector graphics: https://inkscape.org/ 
-   Laser-cutting by Perth Creative Industries https://www.pcilasercutting.com.au/
-   Fritzing for circuit design: https://fritzing.org/ 
-   Great Scott for electronics tips! https://www.youtube.com/channel/UC6mIxFTvXkWQVEHPsEdflzQ

