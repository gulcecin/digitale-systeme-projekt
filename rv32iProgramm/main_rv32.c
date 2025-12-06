#include <stdint.h>
#include <string.h>
#include "printf.h"
#include "fptc.h"
#include "dma_test.h"

#define APP_START (0x00000000)
#define APP_LEN   (0x200)
#define APP_ENTRY (0x00000000)

/* 
   What I am doing here is:
   
   • The simulator provides ToF data in memory as groups of four fixed-point
     numbers per pixel: [a0, a1, a2, a3]. There are 8×8 = 64 pixels total.
   • For each pixel I compute one angle in radians:
         angle = atan2( a3 - a1 , a2 - a0 )
   • I must do all math in fixed-point using only what fptc.h exposes.
   • Finally I print an 8×8 matrix of these angles, each with exactly 2 decimals.

   External *algorithm* references I relied on (ideas only, no code copied):
   [R1] atan2 definition and quadrant rules (C standard library reference):
       https://en.cppreference.com/w/c/numeric/math/atan2
   [R2] Maclaurin series for arctan(z) = z − z^3/3 + z^5/5 − … :
       https://mathworld.wolfram.com/MaclaurinSeries.html
   [R3] Identity for |z|>1: atan(z) = sgn(z)·π/2 − atan(1/|z|):
       https://mathworld.wolfram.com/InverseTangent.html
   [R4] Fixed-point printing idea (split integer/fraction, scale & round):
       J. Yiu, “The Definitive Guide to ARM Cortex-M3/M4,” Elsevier, 2014.
   • The fixed-point API (types, constants, helpers) comes from our fptc.h. :contentReference[oaicite:1]{index=1}
*/

//toDo fpt fpt_atan2(fpt y, fpt x)

static void print_fpt_2dec(fpt v) {
    int neg = (v < 0);
    if (neg) v = -v;                         /* work with |v| */

    int ip = (int)(v >> FPT_FBITS);          /* integer part */

    fpt frac_mask = (((fpt)1) << FPT_FBITS) - 1;
    fpt rem = (fpt)(v & frac_mask);          /* fractional remainder */

    /* two decimals with integer rounding: (rem*100 + 0.5*scale) >> FPT_FBITS */
    long long num = (long long)rem * 100;    /* 64-bit intermediate to be safe */
    int fp2 = (int)((num + ((long long)1 << (FPT_FBITS - 1))) >> FPT_FBITS);

    if (neg) printf("-");
    if (fp2 < 10) printf("%d.0%d", ip, fp2);
    else          printf("%d.%d",  ip, fp2);

   /*
   print_fpt_2dec(v):
   I print a fixed-point value as “integer.two_decimals” without using floats.
   Steps:
     1) remember the sign and use |v|
     2) integer part  = v >> FPT_FBITS
     3) fraction rem  = v & ((1<<FPT_FBITS)-1)
     4) two decimals  = round( rem * 100 / (1<<FPT_FBITS) )
   Rounding-to-nearest is done by adding half-scale before shifting. (Idea [R4])
   */
}

static fpt fpt_atan_basic(fpt z) {
    if (z == 0) return (fpt)0;

    int sgn = 1;
    if (z < 0) { sgn = -1; z = (fpt)(-z); }  /* atan(−z) = −atan(z) */

    if (z <= FPT_ONE) {
        /* Maclaurin up to z^5: z − z^3/3 + z^5/5 (all terms in fixed-point) */
        fpt z2 = fpt_mul(z, z);      /* z^2 */
        fpt z3 = fpt_mul(z2, z);     /* z^3 */
        fpt z5 = fpt_mul(z3, z2);    /* z^5 */

        fpt term1 = z;               /*  z          */
        fpt term2 = (fpt)(z3 / 3);   /*  z^3 / 3    */
        fpt term3 = (fpt)(z5 / 5);   /*  z^5 / 5    */

        fpt res = (fpt)(term1 - term2 + term3);
        return (sgn > 0) ? res : (fpt)(-res);
    } else {
        /* |z|>1 → fold into (0,1] using the identity with π/2 (idea [R3]) */
        fpt inv   = fpt_div(FPT_ONE, z);     /* 1/z in the same Q format */
        fpt small = fpt_atan_basic(inv);     /* recurse on ≤1 where series is OK */
        fpt res   = (fpt)(FPT_HALF_PI - small);
        return (sgn > 0) ? res : (fpt)(-res);
    }

    /*
    fpt_atan_basic(z):
    A very small fixed-point atan(z), enough for two-decimal output.
    Plan:
     • if z == 0                 → 0
     • if z < 0                  → −atan(−z)   (odd symmetry)
     • if |z| ≤ 1                → Maclaurin: z − z^3/3 + z^5/5    [R2]
     • if |z| > 1                → sgn(z)·π/2 − atan(1/|z|)        [R3]
    All operations use the fixed-point helpers/constants from fptc.h.  [oai_citation:2‡fptc.h](file-service://file-W8vqmvLUKsGzAoJfcHsTCX)
    */
}

static fpt fpt_atan2(fpt y, fpt x) {
    if (x > 0) {
        return fpt_atan_basic( fpt_div(y, x) );
    } else if (x < 0) {
        fpt a = fpt_atan_basic( fpt_div(y, x) );
        return (y >= 0) ? (fpt)(a + FPT_PI) : (fpt)(a - FPT_PI);
    } else { /* x == 0 */
        if (y > 0)  return FPT_HALF_PI;
        if (y < 0)  return (fpt)(-FPT_HALF_PI);
        return (fpt)0;  /* define atan2(0,0) as 0 for this assignment */
    }
/* 
   WHAT I DO HERE: — fpt_atan2(y, x):
   I implement the standard quadrant rules (definition of atan2) — see [R1]:
     • x > 0                 → atan(y/x)
     • x < 0, y ≥ 0          → atan(y/x) + π
     • x < 0, y < 0          → atan(y/x) − π
     • x = 0, y > 0          →  π/2
     • x = 0, y < 0          → −π/2
     • x = 0, y = 0          →  0  (by convention)
   Everything is in fixed-point; I only use fpt_div and my fpt_atan_basic().
*/    
}

void main(void) {
   printf("\r\nRISC-V Prozessor der\n\rHumboldt Universitaet zu Berlin\r\n");

/* 
       WHAT I DO HERE IS: — read → compute → print (8×8 matrix)
       • Data layout per pixel is [a0, a1, a2, a3] (fixed-point, same Q everywhere).
       • I compute:  y = a3 − a1   and   x = a2 − a0
       • Then:       angle = fpt_atan2(y, x)
       • I print each angle with exactly two decimals; space between columns.
       • At the end of each row I print a newline so the output forms 8 lines.
*/
    
      /* 1) How many 32-bit values are in 'tof'? (dma_test.h defines the array) */
    int total_vals   = (int)(sizeof(tof) / sizeof(tof[0]));   /* raw element count */
    int total_pixels = total_vals / 4;                        /* 4 values per pixel */

    /* 2) I format the output to 8 columns; the number of rows depends on data. */
    const int COLS = 8;
    int rows = (total_pixels + COLS - 1) / COLS;  /* round-up division */

    int idx = 0;       /* linear index into tof[], advances by +4 per pixel */
    int printed = 0;   /* how many pixels I actually printed */

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < COLS; ++c) {
            /* Safety: stop if there aren’t 4 values left for one full pixel */
            if (idx + 3 >= total_vals) break;

            /* Memory order per pixel: [a0, a1, a2, a3] */
            fpt a0 = (fpt)tof[idx + 0];
            fpt a1 = (fpt)tof[idx + 1];
            fpt a2 = (fpt)tof[idx + 2];
            fpt a3 = (fpt)tof[idx + 3];
            idx += 4;
            printed++;

            /* Vector for atan2, as specified in the assignment */
            fpt y = (fpt)(a3 - a1);
            fpt x = (fpt)(a2 - a0);

            /* Fixed-point angle (radians) */
            fpt ang = fpt_atan2(y, x);

            /* Print with two decimals; put a space between columns */
            print_fpt_2dec(ang);
            if (c < COLS - 1) printf(" ");
        }
        printf("\n");
    }

    /* If the data did not contain exactly 64 pixels, I mention it. */
    if (printed != 64) {
        printf("Note: printed %d pixels (source had %d values = %d pixels).\n",
               printed, total_vals, total_pixels);
    }

    while(1); /*bare metal: stay here*/
}
