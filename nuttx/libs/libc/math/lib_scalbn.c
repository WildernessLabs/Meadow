/****************************************************************************
 * libs/libc/math/lib_scalbn.c
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
#include <float.h>
#include "math_private.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

#ifdef CONFIG_HAVE_DOUBLE
static const double
two54   =  1.80143985094819840000e+16, /* 0x43500000, 0x00000000 */
twom54  =  5.55111512312578270212e-17, /* 0x3C900000, 0x00000000 */
huge   = 1.0e+300,
tiny   = 1.0e-300;

double
scalbn (double x, int n)
{
  int32_t k,hx,lx;
  EXTRACT_WORDS(hx,lx,x);
  k = (hx&0x7ff00000)>>20;		/* extract exponent */
  if (k==0) {				/* 0 or subnormal x */
    if ((lx|(hx&0x7fffffff))==0) return x; /* +-0 */
    x *= two54;
    GET_HIGH_WORD(hx,x);
    k = ((hx&0x7ff00000)>>20) - 54;
    if (n< -50000) return tiny*x; 	/*underflow*/
  }
  if (k==0x7ff) return x+x;		/* NaN or Inf */
  k = k+n;
  if (k >  0x7fe) return huge*copysign(huge,x); /* overflow  */
  if (k > 0) 				/* normal result */
    {SET_HIGH_WORD(x,(hx&0x800fffff)|(k<<20)); return x;}
  if (k <= -54) {
    if (n > 50000) 	/* in case integer overflow in n+k */
      return huge*copysign(huge,x);	/*overflow*/
    else
      return tiny*copysign(tiny,x); 	/*underflow*/
	}
  k += 54;				/* subnormal result */
	SET_HIGH_WORD(x,(hx&0x800fffff)|(k<<20));
  return x*twom54;
}
#endif
