#include <avr/io.h>
#include "config.h"

#ifdef TINY
#include "BasicSerial3.h"
#else
#include <util/setbaud.h>
#endif

void initSerial() {
#ifdef TINY
    DDRB |= _BV(PB3);
    PORTB |= _BV(PB3);
#else
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
#ifdef TINY
    TxByte(v);
#else
    loop_until_bit_is_set(UCSR0A, UDRE0);
    UDR0 = v;
#endif
}

void ser_write_block(const uint8_t *v, unsigned int len) {
    while(len--) ser_write(*v++);
}
