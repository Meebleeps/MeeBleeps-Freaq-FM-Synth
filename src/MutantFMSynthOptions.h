#ifndef MUTANTFMSYNTHOPTION_H
#define MUTANTFMSYNTHOPTION_H


// 09 May 2022
// added compile options for normally-open or normally-closed switches

//  if your switches are NORMALLY OPEN, include this line and comment out the #define below
//#undef SWITCH_TYPE_NORMALLY_CLOSED

//  if your switches are NORMALLY CLOSED, include this line and comment out the #undef above
#define SWITCH_TYPE_NORMALLY_CLOSED




// 09 May 2022
// added compile options to reduce compiled image size to fit into boards with larger bootloaders, or using different compilers such as Arduino IDE
// uncomment the below to reduce the image size by 2KB

//#define COMPILE_SMALLER_BINARY




#endif


