#include <math.h>
#include <stdint.h>

#ifndef ARM_CODE
#define ARM_CODE
#endif

//#define FLOATING_POINT

#ifdef FLOATING_POINT

typedef float fixed_t;

#define FRACT_BITS 0
#define FIXED_MAX INFINITY

#define TO_FIXED(n) ((fixed_t)(n))

ARM_CODE static inline float fixed_to_float(fixed_t n) { return n; }
ARM_CODE static inline int fx_int(fixed_t n) { return n; }
ARM_CODE static inline fixed_t fx_fract(fixed_t n) { float i; return modff(n, &i); }
ARM_CODE static inline fixed_t fx_mul(fixed_t a, fixed_t b) { return a * b; }
ARM_CODE static inline fixed_t fx_div(fixed_t a, fixed_t b) { return a / b; }
ARM_CODE static inline fixed_t fx_sqrt(fixed_t n) { return sqrtf(n); }

#else

typedef int32_t fixed_t;

#define FRACT_BITS 8
#define FIXED_MAX INT_MAX

#define TO_FIXED(n) ((fixed_t)((n) * (1 << FRACT_BITS)))

ARM_CODE
static inline float fixed_to_float(fixed_t n) { return n / (float)(1 << FRACT_BITS); }

ARM_CODE
static inline int fx_int(fixed_t n) { return n >> FRACT_BITS; }

ARM_CODE
static inline fixed_t fx_fract(fixed_t n) { return n & ((1 << FRACT_BITS) - 1); }

ARM_CODE
static inline fixed_t fx_mul(fixed_t a, fixed_t b)
{
	int64_t v = (int64_t)a * (int64_t)b;
	return v >> FRACT_BITS;
}

ARM_CODE
static inline fixed_t fx_div(fixed_t a, fixed_t b)
{
	//return ((int64_t)a << FRACT_BITS) / b;
	return ((int32_t)a << FRACT_BITS) / b;
}

// Taken from here: https://github.com/chmike/fpsqrt/blob/master/fpsqrt.c
// sqrt_fx16_16_to_fx16_16 computes the squrare root of a fixed point with 16 bit
// fractional part and returns a fixed point with 16 bit fractional part. It 
// requires that v is positive. The computation use only 32 bit registers and 
// simple operations.
ARM_CODE
static int32_t sqrt_fx16_16_to_fx16_16(int32_t v) {
    uint32_t t, q, b, r;
    r = (int32_t)v; 
    q = 0;          
    b = 0x40000000UL;
    if( r < 0x40000200 )
    {
        while( b != 0x40 )
        {
            t = q + b;
            if( r >= t )
            {
                r -= t;
                q = t + b; // equivalent to q += 2*b
            }
            r <<= 1;
            b >>= 1;
        }
        q >>= 8;
        return q;
    }
    while( b > 0x40 )
    {
        t = q + b;
        if( r >= t )
        {
            r -= t;
            q = t + b; // equivalent to q += 2*b
        }
        if( (r & 0x80000000) != 0 )
        {
            q >>= 1;
            b >>= 1;
            r >>= 1;
            while( b > 0x20 )
            {
                t = q + b;
                if( r >= t )
                {
                    r -= t;
                    q = t + b;
                }
                r <<= 1;
                b >>= 1;
            }
            q >>= 7;
            return q;
        }
        r <<= 1;
        b >>= 1;
    }
    q >>= 8;
    return q;
}

ARM_CODE
static inline fixed_t fx_sqrt(fixed_t n)
{
	int32_t n16_16 = n << (16 - FRACT_BITS);
	return sqrt_fx16_16_to_fx16_16(n16_16) >> (16 - FRACT_BITS);
}

#endif
