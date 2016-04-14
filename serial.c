#include <avr/io.h>
#include "config.h"
#include <util/setbaud.h>

void initSerial() {
#ifndef TINY // TODO:  Add functional serial for TINY
    DDRD |= _BV(PD1);
	DDRD &= ~_BV(PD0);

    UBRR0H = UBRRH_VALUE;
    UBRR0L = UBRRL_VALUE;

#if USE_2X
    UCSR0A |= _BV(U2X0);
#else
    UCSR0A &= ~(_BV(U2X0));
#endif

    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00); /* 8-bit data */
    UCSR0B = _BV(RXEN0) | _BV(TXEN0);   /* Enable RX and TX */
#endif
}

void ser_write(const uint8_t v) {
#ifndef TINY // TODO:  Add functional serial for TINY
    loop_until_bit_is_set(UCSR0A, UDRE0);
    UDR0 = v;
#endif
}

void ser_write_block(const uint8_t *v, unsigned int len) {
    while(len--) ser_write(*v++);
}
