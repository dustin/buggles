#ifndef CONFIG_H

// ----------------------------------------------------------------------
// Config-ish stuff.
// ----------------------------------------------------------------------

//
// Add a loss histogram to the SUMD output
//

// #define DEBUG

//
// Sync SUMD output with radio input rather than using SUMD's normal
// 100Hz output rate.  This veers slightly off spec since we'll be
// outputting packets every 9ms instead of every 10ms, but I doubt any
// FC will be unhappy with that.  The upside is that you'll get
// packets delivered as soon as possible.
//

#define ONESHOT

//
// Uncomment to use a dedicated GDO pin to signal incoming packets.
// This is highly dependent on the radio module you use.  The one I'm
// using on my desk has no GDO_0 available, so I'm using GDO_1 (SCK).
// Fewer pins, but not quite as fancy.
//

// #define USE_GDO_0

//
// Instead of writing out SUMD frames, print out summary of packet processing:
//  . == packet received
//  ! == packet expected, but not received
//  + == channel changed
//  @ == telemetry transmitted
//  T == invalid address
//  N == invalid packet length
//  X == CRC fail
//

// #define SER_PRINT_DEBUG

// ----------------------------------------------------------------------
// End of config-ish stuff.
// ----------------------------------------------------------------------

#if defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__) || \
    defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
# define TINY
# define TIMER TCNT0
#else
# define TIMER TCNT2
#endif

#if F_CPU == 16000000
#  define CPU_SCALE(a) (a)
#elif F_CPU == 8000000
#  define CPU_SCALE(a) (a * 2)
#else
#  error // 8 or 16MHz only !
#endif

#endif /* CONFIG_H */

