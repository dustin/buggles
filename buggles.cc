#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include "config.h"
#include "SUMD.h"
#include "cc2500.h"
#include "serial.h"

#include "tinyspi.h"

// http://focus.ti.com/docs/prod/folders/print/cc2500.html


#ifdef SER_PRINT_DEBUG
#  define DPRINT(a) (ser_write(a))
#else
#  define DPRINT(a)
#endif

#define MAX_MISSING_PKT 20
#define SEEK_CHANSKIP   13

// Microseconds between sumd frames
#define SUMD_FREQ 10000

const int rssi_offset = 71;
const int rssi_min = -90;
const int rssi_max = -20;

#define MAX_CHANS 60

uint8_t *eeprom_addr((uint8_t*)100);

typedef uint8_t byte;

byte txid[2];
int freq_offset;
uint8_t hopData[MAX_CHANS];
uint8_t numChans;
uint8_t ccData[27];
int rssi;
uint8_t chan;

#ifdef DEBUG
#define NUM_BUCKETS 10
int loss_histo[NUM_BUCKETS];
#endif /* DEBUG */

#ifdef DEBUG
#define MORE_CHAN NUM_BUCKETS
#else
#define MORE_CHAN 0
#endif

#define NUM_CHAN 8
#define SUMD_RSSI_CHAN NUM_CHAN
SUMD sumd(SUMD_RSSI_CHAN + 1 + MORE_CHAN);

volatile bool tx_sumd = false;
volatile bool failsafe = false;
volatile bool timedout = false;

// Set timeout = true and tx_sumd = true if it's been a bit over 9ms.
void initTimeoutTimer() {
    cli();
    OCR0A = TMR_9ish;
    OCR0B = 0;
    TCCR0B |= _BV(WGM02);             // CTC mode
    TCCR0B |= _BV(CS02) | _BV(CS00);  // 1024 prescaler
    TIMRSK |= _BV(OCIE0A);            // enable timer compare interrupt
    TIMER = 0;

    // The watchdog timer is used for detecting failsafe state.
    wdt_reset();
    _WD_CONTROL_REG = _BV(_WD_CHANGE_BIT) | _BV(WDE);
    // Enable WDT Interrupt, and Set Timeout to ~1 seconds,
	_WD_CONTROL_REG = _BV(WDIE) | _BV(WDP2) | _BV(WDP1);

    sei();
}

ISR(WDT_vect) {
    wdt_reset();
    failsafe = true;
}

ISR(TIMER0_COMPA_vect) {
    timedout = true;
    tx_sumd = true;
}

void  configureRadio() {
    cc2500_resetChip();
    #ifdef USE_GDO_0
    cc2500_writeReg(CC2500_IOCFG0,   0x01);  // RX complete interrupt(GDO0)
    #else
    cc2500_writeReg(CC2500_IOCFG1,   0x01);  // RX complete interrupt(GDO1)
    #endif
    cc2500_writeReg(CC2500_MCSM1,    0x0C);
    cc2500_writeReg(CC2500_MCSM0,    0x18);
    cc2500_writeReg(CC2500_PKTLEN,   sizeof(ccData)-2);
    cc2500_writeReg(CC2500_PKTCTRL0, 0x05);
    cc2500_writeReg(CC2500_PATABLE,  0xFF);
    cc2500_writeReg(CC2500_FSCTRL1,  0x08);
    cc2500_writeReg(CC2500_FSCTRL0,  0x00);
    cc2500_writeReg(CC2500_FREQ2,    0x5C);
    cc2500_writeReg(CC2500_FREQ1,    0x76);
    cc2500_writeReg(CC2500_FREQ0,    0x27);
    cc2500_writeReg(CC2500_MDMCFG4,  0xAA);
    cc2500_writeReg(CC2500_MDMCFG3,  0x39);
    cc2500_writeReg(CC2500_MDMCFG2,  0x11);
    cc2500_writeReg(CC2500_MDMCFG1,  0x23);
    cc2500_writeReg(CC2500_MDMCFG0,  0x7A);
    cc2500_writeReg(CC2500_DEVIATN,  0x42);
    cc2500_writeReg(CC2500_FOCCFG,   0x16);
    cc2500_writeReg(CC2500_BSCFG,    0x6C);
    cc2500_writeReg(CC2500_AGCCTRL2, 0x03);
    cc2500_writeReg(CC2500_AGCCTRL1, 0x40);
    cc2500_writeReg(CC2500_AGCCTRL0, 0x91);
    cc2500_writeReg(CC2500_FREND1,   0x56);
    cc2500_writeReg(CC2500_FREND0,   0x10);
    cc2500_writeReg(CC2500_FSCAL3,   0xA9);
    cc2500_writeReg(CC2500_FSCAL2,   0x05);
    cc2500_writeReg(CC2500_FSCAL1,   0x00);
    cc2500_writeReg(CC2500_FSCAL0,   0x11);
    cc2500_writeReg(CC2500_FSTEST,   0x59);
    cc2500_writeReg(CC2500_TEST2,    0x88);
    cc2500_writeReg(CC2500_TEST1,    0x31);
    cc2500_writeReg(CC2500_TEST0,    0x0B);
    cc2500_writeReg(CC2500_FIFOTHR,  0x0F);
    cc2500_writeReg(CC2500_ADDR,     0x03);
    cc2500_strobe(CC2500_SIDLE);
    // reg 0x07 hack: Append status, filter by address, auto-flush on bad crc, PQT=0
    cc2500_writeReg(CC2500_PKTCTRL1, 0x0D);
    cc2500_writeReg(CC2500_CHANNR,   0x00);
}

uint8_t waitFor(uint8_t *data) {
    CS_cc2500_on;
    for (;;) {
        if (DATA_PRESENT) {
            uint8_t ccLen = cc2500_readReg(CC2500_RXBYTES | CC2500_READ_BURST) & 0x7F;
            if (ccLen) {
                cc2500_readFifo(data, ccLen);
                if (data[ccLen - 1] & 0x80 && data[2] == 1) {
                    return ccLen;
                }
            }
        }
    }
}

void storeBind() {
    eeprom_write_block(txid, eeprom_addr, sizeof(txid));

    eeprom_write_block(hopData, eeprom_addr+10, sizeof(hopData));
    eeprom_write_byte(eeprom_addr + 100, numChans);
}

void getBind() {
    cc2500_strobe(CC2500_SRX);//enter in rx mode
    numChans = 0;
    for (;;) {
        uint8_t ccLen = waitFor(ccData);
        if (ccData[5] == 0) {
            txid[0] = ccData[3];
            txid[1] = ccData[4];
            for (uint8_t n = 0; n < 5; n++) {
                hopData[ccData[5] + n] = ccData[6 + n];
            }
            break;
        }
    }

    bool done = false;
    for (uint8_t bindIdx = 0x05; bindIdx <= 120; bindIdx += 5) {
        while (!done) {
            uint8_t ccLen = waitFor(ccData);
            if ((ccData[3] == txid[0]) && (ccData[4] == txid[1])) {
                if (ccData[5] == bindIdx) {
                    for (uint8_t n = 0; n < 5; n++) {
                        if (ccData[6 + n] <= 3) {
                            done = true;
                            numChans = ccData[5] + n;
                            break;
                        }
                        hopData[ccData[5] + n] = ccData[6 + n];
                    }
                    break;
                }
            }
        }
    }

    storeBind();
    cc2500_strobe(CC2500_SIDLE);
}

unsigned char bindJumper(void) {
    #ifndef TINY
    DDRC &= ~(_BV(PIN0));
    PORTC |= _BV(PIN0);
    if (PINC & _BV(PIN0) == 0) {
        _delay_us(1);
        return 1;
    }
    #endif
    return  0;
}

void tuning() {
    cc2500_strobe(CC2500_SRX);
    int count1 = 0;
    while (1) {
        count1++;
        if (freq_offset >= 250) {
            freq_offset = 0;
        }
        if (count1 > 3000) {
            count1 = 0;
            cc2500_writeReg(CC2500_FSCTRL0, freq_offset);
            freq_offset += 10;
        }
        if (DATA_PRESENT) {
            uint8_t ccLen = cc2500_readReg(CC2500_RXBYTES | CC2500_READ_BURST) & 0x7F;
            if (ccLen) {
                cc2500_readFifo((uint8_t *)ccData, ccLen);
                if (ccData[ccLen - 1] & 0x80 && ccData[2] == 1 && ccData[5] == 0) {
                    break;
                }
            }
        }
    }
}

void bindRadio() {
    bool binding = bindJumper();
    while (1) {
        if (!binding) { //bind complete or no bind
            eeprom_read_block(txid, eeprom_addr, sizeof(txid));
            if (txid[0] == 0xff && txid[1] == 0xff) {
                // No valid txid, forcing bind
                binding = true;
                continue;
            }
            eeprom_read_block(hopData, eeprom_addr+10, sizeof(hopData));
            numChans = eeprom_read_byte(eeprom_addr + 100);
            freq_offset = eeprom_read_byte(eeprom_addr + 101);
            break;
        } else {
            tuning();
            eeprom_write_byte(eeprom_addr + 101, freq_offset);
            getBind();
            return;
        }
    }
}

void updateRSSI(int rssi_dec) {
    if (rssi_dec < 128) {
        rssi = ((rssi_dec / 2) - rssi_offset);
    } else {
        rssi = (((rssi_dec - 256) / 2)) - rssi_offset;
    }

    if (rssi > rssi_max) {
        rssi = rssi_max;
    }
    rssi = rssi < rssi_min ? rssi_min : rssi;
}

void nextChannel(uint8_t skip) {
    chan += skip;
    if (chan >= numChans) chan -= numChans;
    cc2500_writeReg(CC2500_CHANNR, hopData[chan]);
    cc2500_writeReg(CC2500_FSCAL3, 0x89);
    DPRINT('+');
}

int missingPackets = 0;
int skips = 0;

// Sending telemetry currently isn't loud enough on any hardware I've
// got, but I think it looks something like this (dummy values present
// until I can actually see something).
void sendTelemetry() {
    static int tchan = 0;
    uint8_t telemPkt[18]= {0x11, txid[0], txid[1], 0x4d, 0x7f, 0x5a, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    cc2500_strobe(CC2500_SFRX);

    cc2500_writeFifo(telemPkt, sizeof(telemPkt));
    DPRINT('@');
}

bool getPacket() {
    bool packet = false;
    if (DATA_PRESENT) {
        uint8_t ccLen = cc2500_readReg(CC2500_RXBYTES | CC2500_READ_BURST) & 0x7F;
        if (ccLen > 20)
            ccLen = 20;//
        if (ccLen) {
            cc2500_readFifo((uint8_t *)ccData, ccLen);
            if (ccData[ccLen - 1] & 0x80) { // Validate cc2500 CRC
                updateRSSI(ccData[ccLen - 2]);
                missingPackets = 0;
                if (ccData[0] == 0x11) { // Correct length
                    if (ccData[2] == txid[1]) { // second half of TX id
                        nextChannel(1);
                        packet = true;
                        TIMER = 0;
                        cc2500_strobe(CC2500_SIDLE);
                    } else {
                        DPRINT('T');
                    }
                } else {
                    DPRINT('N');
                }
            } else {
                DPRINT('X');
            }
        }
    }
    return packet;
}

static inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void transmitPacket() {
    sumd.setChannel(SUMD_RSSI_CHAN, map(rssi, rssi_min, rssi_max, 1000, 2000));

#ifdef DEBUG
    // zero out any histo that exceeds 1000 packets, making the output
    // calculation much more simple.
    for (int i = 0; i < MORE_CHAN; i++) {
        if (loss_histo[i] > 1000) {
            loss_histo[i] = 0;
        }
        sumd.setChannel(SUMD_RSSI_CHAN+1+i, loss_histo[i] + 1000);
    }
#endif /* DEBUG*/

    #ifndef SER_PRINT_DEBUG
    ser_write_block(sumd.bytes(), sumd.size());
    #endif /* SER_PRINT_DEBUG */
    tx_sumd = false;
}

void loop() {
    bool packet = false;
    static uint8_t seq = 0;
    static bool skipNext = false;
    static uint8_t seeking = 0;

    cc2500_strobe(CC2500_SRX);
    CS_cc2500_on;
    while (!packet) {
        if (timedout) {
            timedout = false;
            cc2500_strobe(CC2500_SIDLE);
            if (failsafe) {
                DPRINT('F');
            }
            if (missingPackets > MAX_MISSING_PKT) {
                if (seeking == 0) {
                    nextChannel(SEEK_CHANSKIP);
                    seeking = 1;
                    seq = 0;
                } else if(++seeking > 10) {
                    nextChannel(1);
                    seeking = 1;
                }
                break;
            } else if (seeking == 0) {
                nextChannel(1);
            }
            if (skipNext) {
                sendTelemetry();
            } else {
                DPRINT('!');
                missingPackets++;
                skips++;
                seq = 0;
            }
            skipNext = false;
            break;
        }

        if (failsafe) {
            rssi = rssi_min;
            sumd.setHeader(SUMD_FAILSAFE);
        }

        if (tx_sumd) {
            transmitPacket();
        }

        packet = getPacket();
    }
    CS_cc2500_off;

    if (packet == true) {
        wdt_reset();
        seeking = 0;
        DPRINT('.');
        if (++seq == 3) {
            seq = 0;
            skipNext = true;
        }
        #ifdef DEBUG
        int offset = 0;
        while (skips > 0) {
            offset++;
            skips = skips >> 1;
        }
        loss_histo[offset]++;
        #endif /* DEBUG */

        failsafe = false;
        sumd.setHeader(SUMD_VALID);
        sumd.setChannel(0, 0.67*((uint16_t)(ccData[10] & 0x0F) << 8 | ccData[6]));
        sumd.setChannel(1, 0.67*((uint16_t)(ccData[10] & 0xF0) << 4 | ccData[7]));
        sumd.setChannel(2, 0.67*((uint16_t)(ccData[11] & 0x0F) << 8 | ccData[8]));
        sumd.setChannel(3, 0.67*((uint16_t)(ccData[11] & 0xF0) << 4 | ccData[9]));
        sumd.setChannel(4, 0.67*((uint16_t)(ccData[16] & 0x0F) << 8 | ccData[12]));
        sumd.setChannel(5, 0.67*((uint16_t)(ccData[16] & 0xF0) << 4 | ccData[13]));
        sumd.setChannel(6, 0.67*((uint16_t)(ccData[17] & 0x0F) << 8 | ccData[14]));
        sumd.setChannel(7, 0.67*((uint16_t)(ccData[17] & 0xF0) << 4 | ccData[15]));

        packet = false;
        tx_sumd = true;
    }

    if (tx_sumd) {
        transmitPacket();
    }
}

void initRadio() {
    SET_CS;
    SET_GDO;

    CS_cc2500_on;

    configureRadio();
    bindRadio();
    cc2500_writeReg(CC2500_ADDR, txid[0]);
    cc2500_writeReg(CC2500_FSCTRL0, freq_offset);

    cc2500_writeReg(CC2500_CHANNR, hopData[0]);//0A-hop
    cc2500_writeReg(CC2500_FSCAL3, 0x89); //23-89
    cc2500_strobe(CC2500_SRX);
}

int main() {
    SPI.begin();

    initSerial();

    #ifdef DEBUG
    for (int i = 0; i < NUM_BUCKETS; i++) {
        loss_histo[i] = 0;
    }
    #endif /* DEBUG */

    for (int i = 0; i < sumd.nchan(); i++) {
        sumd.setChannel(i, 1500);
    }

    initRadio();

    initTimeoutTimer();

    for(;;) {
        loop();
    }
}
