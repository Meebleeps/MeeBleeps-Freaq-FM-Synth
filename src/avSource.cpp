
#include "avSource.h"



MutatingSource::MutatingSource()
{
  // base constructor;
  for (uint8_t i = 0; i < MAX_SOURCE_PARAMS; i++)
  {
    param[i] = 0;
  }

}
