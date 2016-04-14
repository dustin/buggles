#ifndef CONFIG_H

// ----------------------------------------------------------------------
// Config-ish stuff.
// ----------------------------------------------------------------------

//
// Add a loss histogram to the SUMD output
//

// #define DEBUG

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
//  F == Failsafe (too long since good packet)
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
#endif

#define TIMER TCNT0

#if F_CPU == 16000000
#  define CPU_SCALE(a) (a)
#elif F_CPU == 8000000
#  error wtf
#  define CPU_SCALE(a) (a * 2)
#else
#  error // 8 or 16MHz only !
#endif

#ifdef TINY
#  define SET_GDO (DDRB &= ~(_BV(PIN3)))
#  define CS 4
#  define SET_CS (DDRB |= _BV(PIN4))
#  define CS_cc2500_on   (PORTB |= _BV(PIN4))
#  define CS_cc2500_off  (PORTB &= ~(_BV(PIN4)))
#  define DATA_PRESENT   ((PINB & _BV(PIN3)) == _BV(PIN3))
#  define WD_CONTROL WDTCR
#else
#  define SET_GDO (DDRD &= ~(_BV(PIN4)))
#  define CS 2
#  define SET_CS (DDRD |= _BV(PIN2))
#  define CS_cc2500_on  (PORTD  |= _BV(PIN2))
#  define CS_cc2500_off (PORTD  &= ~(_BV(PIN2)))
#  define WD_CONTROL WDTCSR
#  ifdef USE_GDO_0
     // Detect data on dedicated pin
#    define DATA_PRESENT ((PIND & _BV(PIN3)) == _BV(PIN3))
#  else
     // Detect data on SCK
#    define DATA_PRESENT ((PINB & _BV(PIN4)) == _BV(PIN4))
#  endif
#endif

#define BAUD 115200

#endif /* CONFIG_H */
