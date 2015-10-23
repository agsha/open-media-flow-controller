#include "nkn_vpe_types.h"

uint24_t nkn_vpe_convert_uint32_to_uint24(uint32_t l) 
{
  uint24_t r;
  r.b0 = (uint8_t)((l & 0x00FF0000U) >> 16);
  r.b1 = (uint8_t)((l & 0x0000FF00U) >> 8);
  r.b2 = (uint8_t) (l & 0x000000FFU);
  return r;
}
