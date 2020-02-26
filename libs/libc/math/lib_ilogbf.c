/****************************************************************************
 * libs/libc/math/lib_ilogbf.c
 *
 * This file is a part of NuttX.
 *
 * It derives from the FreeBSD math library, a compatibile, BSD-style license.
 * Ported by: Joao Matos
 *
 * Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/compiler.h>

#include <math.h>
#include <limits.h>
#include <stdint.h>
#include "math_private.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

float ilogbf(float x)
{
  int32_t hx,ix;

  GET_FLOAT_WORD(hx,x);
  hx &= 0x7fffffff;
  if(hx<0x00800000) {
      if(hx==0)
    return FP_ILOGB0;
      else  /* subnormal x */
          for (ix = -126,hx<<=8; hx>0; hx<<=1) ix -=1;
      return ix;
  }
  else if (hx<0x7f800000) return (hx>>23)-127;
  else if (hx>0x7f800000) return FP_ILOGBNAN;
  else return INT_MAX;
}
