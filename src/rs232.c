
#include "rs232.h"
#include <stdio.h>

/* 

 In this project we do not receive complete characters directly.
 Instead, the processor gives us only the raw serial line output,
 one bit at a time. The function printOut(val) is called very often,
 and the parameter 'val' tells us the current level of the TX line:
 it is either 0 or 1.

 A character on a serial line is sent in a specific frame:
 first there is one start bit (this is always 0),
 then there are eight data bits (the real information, least significant bit first),
 and finally there is one stop bit (this is always 1).
 So one full character is: start → 8 data bits → stop.

 Our job is to rebuild characters from these single bits.
 To do that we must know the correct timing. The baud rate is 115200,
 which means one bit takes about 8.68 microseconds. 
 Because the simulator calls printOut every half clock cycle of 90 MHz,
 this means we are called about 1563 times during one bit time.
 With this knowledge we can wait for the right number of calls and
 then sample the line exactly in the middle of each bit.
 Sampling in the middle is important, because there the signal is stable.

 The plan :
 - While the line is idle (high = 1), we do nothing.
 - When the line suddenly goes low (1 → 0), we know that a new character begins.
   This is the start bit. We then wait one and a half bit times to reach
   the center of the very first data bit.
 - From now on, every full bit time later we take one sample
   and store this bit into a byte. We repeat this eight times,
   so we collect eight data bits.
 - After that we expect a stop bit (line goes back to 1).
   At this moment we have a full byte, which we can print as a character.
 - Finally we return to the idle state and wait for the next start bit.

 Implemation plan:
   - Use a tiny state machine with three states: IDLE, DATA, STOP.
   - Use a down-counter ("wait") to reach the middle of a bit.
   - Use "ch" to accumulate the 8 sampled bits.
   - Use "bit_index" to know which data bit (0..7) we are sampling.

 With this simple state machine we can turn the incoming bits into
 readable characters on the screen.
*/


void printOut(uint8_t val) {
const unsigned int DIV = 1563u;  // ~ calls per bit time (rounded)

    /* ---------------------------------------------------------------
       Simple state machine:
         0 = IDLE : wait for start bit (line 1 -> 0)
         1 = DATA : sample 8 data bits at the middle of each bit time
         2 = STOP : wait one bit time (stop) then print the character
       --------------------------------------------------------------- */
    static int state = 0;            // start in IDLE

    /* ---------------------------------------------------------------
       "wait" is a down-counter (how many calls left to reach the
       middle of the current bit). Each function call decrements it.
       When it reaches 0, we are at the sampling instant.
       --------------------------------------------------------------- */
    static unsigned int wait = 0;

    /* ---------------------------------------------------------------
       "prev" remembers the TX level from the previous call.
       We use it to detect the falling edge 1->0 (start bit).
       Idle level is 1 for UART.
       --------------------------------------------------------------- */
    static uint8_t prev = 1;

    /* ---------------------------------------------------------------
       "ch" accumulates the 8 sampled data bits into one byte.
       "bit_index" tells which data bit (0..7) we are on.
       Data is LSB-first, so the first sampled bit goes into bit 0.
       --------------------------------------------------------------- */
    static uint8_t ch = 0;
    static int bit_index = 0;

    /* =========================
       State: IDLE (wait for start)
       ========================= */
    if (state == 0) {
        /* Detect a falling edge: previous was 1 (idle), now we see 0.
           That means a new character has started (start bit). */
        if (prev == 1 && (val & 1) == 0) {
            /* Wait 1.5 bit times:
               - 1 full bit time (finish the start bit),
               - plus 0.5 bit time to land in the MIDDLE of the first data bit. */
            wait = DIV + (DIV / 2u);

            /* Prepare for a new character: clear byte and reset bit counter. */
            ch = 0;
            bit_index = 0;

            /* Move to DATA state to sample the 8 data bits. */
            state = 1;
        }

    /* =========================
       State: DATA (collect 8 bits)
       ========================= */
    } else if (state == 1) {
        if (wait > 0) {
            /* Not at the middle yet: count down one step. */
            wait--;
        } else {
            /* We are at the middle of the current data bit: sample it. */
            if ((val & 1) != 0) {
                /* If the line is high (1), set the corresponding bit in 'ch'.
                   LSB-first: the first data bit fills bit position 0, etc. */
                ch |= (uint8_t)(1u << bit_index);
            }

            /* Move to the next data bit. */
            bit_index++;

            if (bit_index < 8) {
                /* More data bits to sample: wait exactly one bit time to
                   reach the middle of the next data bit. */
                wait = DIV;
            } else {
                /* All 8 data bits sampled. Now wait one bit time for STOP. */
                wait = DIV;
                state = 2;  // STOP
            }
        }

    /* =========================
       State: STOP (finish frame)
       ========================= */
    } else if (state == 2) {
        if (wait > 0) {
            /* Wait until the middle of the stop bit. */
            wait--;
        } else {
            /* At this point we consider the frame complete and print the char.
               (We could've check that 'val' is 1 here for a strict stop-bit check,
                but keeping it simple is sufficient for this assignment. I ran the 
                code like this and there wasn't any problem.) */
            printf("%c", (char)ch);
            fflush(stdout);  // ensure immediate output to the terminal

            /* Go back to IDLE and wait for the next start bit. */
            state = 0;
        }
    }

    /* Update 'prev' so we can detect the next falling edge (start bit). */
    prev = (uint8_t)(val & 1);
}
