// Timer Service Library
// Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// Hardware configuration:
// Timer 4

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "timer_wireless.h"

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

#define NUM_TIMERS_WR 6

_callback_wr fn_wr[NUM_TIMERS_WR];
uint32_t period_wr[NUM_TIMERS_WR];
uint32_t ticks_wr[NUM_TIMERS_WR];
bool reload_wr[NUM_TIMERS_WR];

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void initTimer_ms()
{
    uint8_t i;

    /* Timer 3A initialization */
    SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_R3;
    _delay_cycles(3);


    // Configure Timer 3 for 1 ms tick
    TIMER3_CTL_R &= ~TIMER_CTL_TAEN;                 // turn-off timer before reconfiguring
    TIMER3_CFG_R = TIMER_CFG_32_BIT_TIMER;           // configure as 32-bit timer (A+B)
    TIMER3_TAMR_R = TIMER_TAMR_TAMR_PERIOD;          // configure for periodic mode (count down)
    TIMER3_TAILR_R = 40000;                             // set load value (1 khz rate)
    TIMER3_CTL_R |= TIMER_CTL_TAEN;                  // turn-on timer
    TIMER3_IMR_R |= TIMER_IMR_TATOIM;                // turn-on interrupt
    NVIC_EN1_R |= 1 << (INT_TIMER3A-48);             // turn-on interrupt 51 (TIMER3A)

    for (i = 0; i < NUM_TIMERS_WR; i++)
    {
        period_wr[i] = 0;
        ticks_wr[i] = 0;
        fn_wr[i] = NULL;
        reload_wr[i] = false;
    }
}

bool startOneshotTimer_ms(_callback_wr callback, uint32_t milliseconds)
{
    uint8_t i = 0;
    bool found = false;
    while (i < NUM_TIMERS_WR && !found)
    {
        found = fn_wr[i] == NULL;
        if (found)
        {
            period_wr[i] = milliseconds;
            ticks_wr[i] = milliseconds;
            fn_wr[i] = callback;
            reload_wr[i] = false;
        }
        i++;
    }
    return found;
}

bool startPeriodicTimer_ms(_callback_wr callback, uint32_t milliseconds)
{
    uint8_t i = 0;
    bool found = false;
    while (i < NUM_TIMERS_WR && !found)
    {
        found = fn_wr[i] == NULL;
        if (found)
        {
            period_wr[i] = milliseconds;
            ticks_wr[i] = milliseconds;
            fn_wr[i] = callback;
            reload_wr[i] = true;
        }
        i++;
    }
    return found;
}

bool stopTimer_ms(_callback_wr callback)
{
     uint8_t i = 0;
     bool found = false;
     while (i < NUM_TIMERS_WR && !found)
     {
         found = fn_wr[i] == callback;
         if (found) {
             ticks_wr[i] = 0;
             fn_wr[i] = 0;
         }
         i++;
     }
     return found;
}

bool restartTimer_ms(_callback_wr callback)
{
     uint8_t i = 0;
     bool found = false;
     while (i < NUM_TIMERS_WR && !found)
     {
         found = fn_wr[i] == callback;
         if (found)
             ticks_wr[i] = period_wr[i];
         i++;
     }
     return found;
}

void tickIsr_ms()
{
    uint8_t i;
    for (i = 0; i < NUM_TIMERS_WR; i++)
    {
        if (ticks_wr[i] != 0)
        {
            ticks_wr[i]--;
            if (ticks_wr[i] == 0)
            {
                if (reload_wr[i])
                    ticks_wr[i] = period_wr[i];
                (*fn_wr[i])();
            }
        }
    }
    TIMER3_ICR_R = TIMER_ICR_TATOCINT;
}

// Placeholder random number function
uint32_t random32_ms()
{
    return TIMER3_TAV_R;
}

