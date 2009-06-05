/*
  This file defines:
  - a macro SINCOS_TRIM_X(y,x) which trims the value x to interval [0..2)
  - global REAL4 arrays sincosLUTbase[] and sincosLUTdiff[] as lookup tables
  - a function void local_sin_cos_2PI_LUT_init(void) that inits these
  - a function local_sin_cos_2PI_LUT_trimmed(*sin,*cos,x) that uses the
    lookup tables to evaluate sin and cos values of 2*Pi*x if x is
    already trimmed to the interval [0..2)
  - macros SINCOS_STEP1..6 for the individual steps of
    local_sin_cos_2PI_LUT_trimmed() (to be mixed into the hotloop code)
  - a type ux_t for a variable ux to be used in these macros
  - macros SINCOS_LUT_RES, SINCOS_ADDS, SINCOS_MASK1, SINCOS_MASK2, SINCOS_SHIFT

  The following macros determine the code that is actually built:
  _MSC_VER       : are we using the Microsoft compiler that doesn't know C99?
  _ARCH_PPC      : are we compiling for PowerPC?
  LAL_NDEBUG     : are we compiling in LAL debug mode?
  __BIG_ENDIAN__ : has the architecture big-endian byt order?
*/


/*
 the way of trimming x to the interval [0..2) for the sin_cos_LUT functions
 give significant differences in speed, so we provide various ways here.
 We also record the way we are using for logging
*/

#ifdef _MSC_VER /* no C99 rint() */
#define SINCOS_TRIM_X(y,x) \
  { \
    __asm FLD     QWORD PTR x 	\
    __asm FRNDINT             	\
    __asm FSUBR   QWORD PTR x 	\
    __asm FLD1                	\
    __asm FADDP   ST(1),ST	\
    __asm FSTP    QWORD PTR y 	\
    }
#elif _ARCH_PPC
/* floor() is actually faster here, as we don't have to set the rounding mode */
#define SINCOS_TRIM_X(y,x) y = x - floor(x);
#else
#define SINCOS_TRIM_X(y,x) y = x - rint(x) + 1.0;
#endif


/* sincos constants */
#define SINCOS_ADDS  402653184.0
#define SINCOS_MASK1 0xFFFFFF
#define SINCOS_MASK2 0x003FFF
#define SINCOS_SHIFT 14

/* sin/cos Lookup tables */
#define SINCOS_LUT_RES 1024 /* should be multiple of 4 */

/* global VARIABLES to be used in (global) macros */
static REAL4 sincosLUTbase[SINCOS_LUT_RES+SINCOS_LUT_RES/4];
static REAL4 sincosLUTdiff[SINCOS_LUT_RES+SINCOS_LUT_RES/4];
/* shift cos tables 90 deg. to sin table */
static const REAL4* cosLUTbase = sincosLUTbase + (SINCOS_LUT_RES/4);
static const REAL4* cosLUTdiff = sincosLUTdiff + (SINCOS_LUT_RES/4);

/* allow to read the higher bits of a 'double' as an 'int' */
union {
  REAL8 asreal;
  struct {
#ifdef __BIG_ENDIAN__
    INT4 dummy;
    INT4 intval;
#else
    INT4 intval;
    INT4 dummy;
#endif
  } as2int;
} sincosUX;
 
static INT4 sincosI, sincosN;


/* LUT evaluation */

#ifndef SINCOS_X87_GNU_ASS


/* x must already been trimmed to interval [0..2) */
/* - syntactic sugar -|   |- here is the actual code - */
#define SINCOS_PROLOG
#define SINCOS_STEP1(x)   sincosUX.asreal = x + SINCOS_ADDS;
#define SINCOS_STEP2      sincosI = sincosUX.as2int.intval & SINCOS_MASK1;
#define SINCOS_STEP3      sincosN = sincosUX.as2int.intval & SINCOS_MASK2;
#define SINCOS_STEP4      sincosI = sincosI >> SINCOS_SHIFT;
#define SINCOS_STEP5(s)   *s = sincosLUTbase[sincosI] + sincosN * sincosLUTdiff[sincosI];
#define SINCOS_STEP6(c)   *c = cosLUTbase[sincosI]    + sincosN * cosLUTdiff[sincosI];
#define SINCOS_EPILOG(s,c,x)



#else /* the very same in x87 gcc inline assembler */


#define SINCOS_VARIABLES				\
    static REAL8 sincos_adds = SINCOS_ADDS;		\
    static REAL4 *scd 	     = &(sincosLUTdiff[0]);	\
    static REAL4 *scb 	     = &(sincosLUTbase[0]);	\
    static REAL8 tmp;
#define SINCOS_PROLOG        { SINCOS_VARIABLES __asm __volatile (
#define SINCOS_TRIM1(x)					\
  "fldl    %[" #x "]      \n\t" /* st: x */			\
  "fistpll %[tmp]         \n\t" /* tmp=(INT8)(round((x)) */	\
  "fld1                   \n\t" /* st: 1.0 */			\
  "fildll  %[tmp]         \n\t" /* st: 1.0;(round((x))*/ 
#define SINCOS_TRIM2(x)						\
  "fsubrp  %%st,%%st(1)   \n\t" /* st: 1.0 -round(x) */			\
  "faddl   %[" #x "]      \n\t" /* st: x -round(x)+1.0*/		\
  "faddl   %[sincos_adds] \n\t" /* ..continue lin. sin/cos as below */	\
  "fstpl   %[tmp]         \n\t"
#define SINCOS_STEP1(x)							\
  "fldl  %[" #x "]        \n\t" /*st:x */				\
  "faddl %[sincos_adds]   \n\t" /*st:x+A */				\
  "fstpl %[tmp]           \n\t"
#define SINCOS_STEP2							\
  "mov   %[tmp],%%eax     \n\t" /* x +A ->eax (ix)*/			\
  "mov   %%eax,%%edx      \n\t"						\
  "and   $0x3fff,%%eax    \n\t"	/* n  = ix & SINCOS_MASK2 */
#define SINCOS_STEP3						\
  "mov   %%eax,%[tmp]     \n\t"					\
  "mov   %[scd], %%eax    \n\t"					\
  "and   $0xffffff,%%edx  \n\t" /* i  = ix & SINCOS_MASK1;*/
#define SINCOS_STEP4							\
  "fildl %[tmp]           \n\t"						\
  "sar   $0xe,%%edx       \n\t" /* i  = i >> SINCOS_SHIFT;*/		\
  "fld   %%st             \n\t" /* st: n; n; */				\
  "fmuls (%%eax,%%edx,4)  \n\t"
#define SINCOS_STEP5(s)							\
  "mov   %[scb], %%edi    \n\t"						\
  "fadds (%%edi,%%edx,4)  \n\t" /*st:sincosLUTbase[i]+n*sincosLUTdiff[i]; n*/ \
  "add   $0x100,%%edx     \n\t" /*edx+=SINCOS_LUT_RES/4*/		\
  "fstps %[" #s "]        \n\t" /*(*sin)=sincosLUTbase[i]+n*sincosLUTdiff[i]*/
#define SINCOS_STEP6(c)							\
  "fmuls (%%eax,%%edx,4)  \n\t"						\
  "fadds (%%edi,%%edx,4)  \n\t"						\
  "fstps %[" #c "]        \n\t" /*(*cos)=cosbase[i]+n*cosdiff[i];*/
#define SINCOS_OUTPUT(s,c)   [s] "=m" (s), [c] "=m" (c), [tmp] "=m" (tmp)
#define SINCOS_INPUT(x)      [scd] "m" (scd), [scb] "m" (scb), [sincos_adds] "m" (sincos_adds), [x] "m" (x)
#define SINCOS_REGISTERS     "st","st(1)","st(2)","eax","edx","edi","cc"
#define SINCOS_EPILOG(s,c,x) : SINCOS_OUTPUT(s,c) : SINCOS_INPUT(x) : SINCOS_REGISTERS ); }


#endif


/* FUNCTIONS */

/* initailize LUTs */
static void local_sin_cos_2PI_LUT_init (void)
{
  static const REAL8 step = LAL_TWOPI / (REAL8)SINCOS_LUT_RES;
  static const REAL8 div  = 1.0 / ( 1 << SINCOS_SHIFT );
  REAL8 start, end, true_mid, linear_mid;
  int i;

  start = 0.0; /* sin(0 * step) */
  for( i = 0; i < SINCOS_LUT_RES + SINCOS_LUT_RES/4; i++ ) {
    true_mid = sin( ( i + 0.5 ) * step );
    end = sin( ( i + 1 ) * step );
    linear_mid = ( start + end ) * 0.5;
    sincosLUTbase[i] = start + ( ( true_mid - linear_mid ) * 0.5 );
    sincosLUTdiff[i] = ( end - start ) * div;
    start = end;
  }
}


static int local_sin_cos_2PI_LUT_trimmed (REAL4 *s, REAL4 *c, REAL8 x) {

#ifndef LAL_NDEBUG
  if(x > SINCOS_ADDS) {
    LogPrintf(LOG_DEBUG,"sin_cos_LUT: x too large: %22f > %f\n",x,SINCOS_ADDS);
    return XLAL_FAILURE;
  } else if(x < -SINCOS_ADDS) {
    LogPrintf(LOG_DEBUG,"sin_cos_LUT: x too small: %22f < %f\n",x,-SINCOS_ADDS);
    return XLAL_FAILURE;
  }
#endif

  SINCOS_PROLOG
  SINCOS_STEP1(x)
  SINCOS_STEP2
  SINCOS_STEP3
  SINCOS_STEP4
  SINCOS_STEP5(s)
  SINCOS_STEP6(c)
  SINCOS_EPILOG(s,c,x)

  return XLAL_SUCCESS;
}
