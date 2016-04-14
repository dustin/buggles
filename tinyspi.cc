/*----------------------------------------------------------------------*
 * tinySPI.h - Arduino hardware SPI master library for ATtiny44/84,     *
 * and ATtiny45/85.                                                     *
 *                                                                      *
 * Jack Christensen 24Oct2013                                           *
 *                                                                      *
 * CC BY-SA:                                                            *
 * This work is licensed under the Creative Commons Attribution-        *
 * ShareAlike 3.0 Unported License. To view a copy of this license,     *
 * visit http://creativecommons.org/licenses/by-sa/3.0/ or send a       *
 * letter to Creative Commons, 171 Second Street, Suite 300,            *
 * San Francisco, California, 94105, USA.                               *
 *----------------------------------------------------------------------*/

#include "config.h"
#include <avr/io.h>

#include "tinySPI.h"

tinySPI::tinySPI()
{
}

void tinySPI::begin(void)
{
#ifdef TINY
    USICR &= ~(_BV(USISIE) | _BV(USIOIE) | _BV(USIWM1));
    USICR |= _BV(USIWM0) | _BV(USICS1) | _BV(USICLK);
    SPI_DDR_PORT |= _BV(USCK_DD_PIN);   //set the USCK pin as output
    SPI_DDR_PORT |= _BV(DO_DD_PIN);     //set the DO pin as output
    SPI_DDR_PORT &= ~_BV(DI_DD_PIN);    //set the DI pin as input
#else
    DDRB |= _BV(PB2) | _BV(PB3) | _BV(PB5); // SS, SCK and MOSI as outputs
    PINB |= _BV(PB2);              // Raise SS
    SPCR |= _BV(MSTR);               // Set as Master
    SPSR |= _BV(SPI2X);
    SPCR |= _BV(SPE);                // Enable SPI
    DDRB &= ~_BV(PB4);             // MISO as input
#endif
}

void tinySPI::setDataMode(uint8_t spiDataMode)
{
    #ifdef TINY
    if (spiDataMode == SPI_MODE1)
        USICR |= _BV(USICS0);
    else
        USICR &= ~_BV(USICS0);
    #endif
}

uint8_t tinySPI::transfer(uint8_t data)
{
#ifdef TINY
    USIDR = data;
    USISR = _BV(USIOIF);                //clear counter and counter overflow interrupt flag
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { //ensure a consistent clock period
        while ( !(USISR & _BV(USIOIF)) ) USICR |= _BV(USITC);
    }
    return USIDR;
#else
    SPDR = data;                 // send the data
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        while(!(SPSR & _BV(SPIF)));  // wait until transmission is complete
    }
    return SPDR;
#endif
}

void tinySPI::end(void)
{
    #ifdef TINY
    USICR &= ~(_BV(USIWM1) | _BV(USIWM0));
    #endif
}

tinySPI SPI = tinySPI();                //instantiate a tinySPI object
