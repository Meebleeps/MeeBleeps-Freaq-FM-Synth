/*----------------------------------------------------------------------------------------------------------
 * avSource.cpp
 * 
 * Stub for base MutatingSource class 
 * 
  * Source Code Repository:  https://github.com/Meebleeps/MeeBleeps-Freaq-FM-Synth
 * Youtube Channel:         https://www.youtube.com/channel/UC4I1ExnOpH_GjNtm7ZdWeWA
 * 
 * (C) 2021-2022 Meebleeps
*-----------------------------------------------------------------------------------------------------------
*/
#include "avSource.h"



MutatingSource::MutatingSource()
{
  // base constructor;
  for (uint8_t i = 0; i < MAX_SOURCE_PARAMS; i++)
  {
    param[i] = 0;
  }

}
