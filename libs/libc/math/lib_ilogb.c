/****************************************************************************
 * libs/libc/math/lib_ilogb.c
 *
 * This file is a part of NuttX.
 *
 * It derives from the FreeBSD math library, a compatibile, BSD-style license.
 * Ported by: Joao Matos
 *
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

#ifdef CONFIG_HAVE_DOUBLE
double ilogb(double x)
{
  int32_t hx,lx,ix;

  EXTRACT_WORDS(hx,lx,x);
  hx &= 0x7fffffff;
  if(hx<0x00100000) {
      if((hx|lx)==0)
    return FP_ILOGB0;
      else			/* subnormal x */
    if(hx==0) {
        for (ix = -1043; lx>0; lx<<=1) ix -=1;
    } else {
        for (ix = -1022,hx<<=11; hx>0; hx<<=1) ix -=1;
    }
      return ix;
  }
  else if (hx<0x7ff00000) return (hx>>20)-1023;
  else if (hx>0x7ff00000 || lx!=0) return FP_ILOGBNAN;
  else return INT_MAX;
}
#endif
