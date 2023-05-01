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

#ifndef TIMER_H_
#define TIMER_H_

typedef void (*_callback_wr)();

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void initTimer_ms();
bool startOneshotTimer_ms(_callback_wr callback, uint32_t milliseconds);
bool startPeriodicTimer_ms(_callback_wr callback, uint32_t milliseconds);
bool stopTimer_ms(_callback_wr callback);
bool restartTimer_ms(_callback_wr callback);
void tickIsr_ms();
uint32_t random32_ms();

#endif
