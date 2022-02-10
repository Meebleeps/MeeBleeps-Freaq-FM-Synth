/*----------------------------------------------------------------------------------------------------------
 * LedMatrix.h
 * 
 * Defines an interface to a MAX7219 driving an 8x8 LED matrix
 * 
 * Source Code Repository:  https://github.com/Meebleeps/MeeBleeps-Freaq-FM-Synth
 * Youtube Channel:         https://www.youtube.com/channel/UC4I1ExnOpH_GjNtm7ZdWeWA
 * 
 * (C) 2021-2022 Meebleeps
*-----------------------------------------------------------------------------------------------------------
*/
#ifndef ledMatrix_h
#define ledMatrix_h
#include "Arduino.h"

// constants for LED matrix orientation.  0 = MAX7219 driver to the right of the display
#define LEDMATRIX_ORIENTATION_0   0
#define LEDMATRIX_ORIENTATION_90  1
#define LEDMATRIX_ORIENTATION_180 2
#define LEDMATRIX_ORIENTATION_270 3

#define LEDMATRIX_MAXROWS 8
#define LEDMATRIX_MAXCOLS 8

#define LEDMATRIX_MODIFIED_ALL 255
#define LEDMATRIX_MODIFIED_NONE 0

// constants for pin control
#define PIN_LEDMATRIX_DIN 11
#define PIN_LEDMATRIX_CS  7
#define PIN_LEDMATRIX_CLK 13

// constants for display
#define LED_DECODE_MODE 9
#define LED_INTENSITY   10
#define LED_SCAN_LIMIT  11
#define LED_SHUTDOWN    12
#define LED_DISPLAYTEST 16

class LedMatrix
{
  public:
    LedMatrix();
    void initialise();

    void setPixel(byte x, byte y, bool value);
    void togglePixel(byte x, byte y);
    void setRowPixels(byte y, byte value);
    void setColPixels(byte x, byte value);
    
    void drawLineV(byte x, byte startY, byte endY, bool on);
    void drawLineH(byte y, byte startX, byte endX, bool on);
    
    void displayIcon(byte* bitmap);

    void clearScreen();
    void refresh();

    void setIntensity(int intensity);
    void setOrientation(int orientation);

    void sparkle(int amount);
  protected:

    void    writeDisplayData(uint8_t address, uint8_t value);
    byte    orientation;
    byte    vRAM[8];

  private:
    byte    displayBit[8] = {1,2,4,8,16,32,64,128};
    byte    rowModified;
    
};

#endif
