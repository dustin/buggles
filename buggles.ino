#include <EEPROM.h>
#include <SPI.h>
#include <avr/interrupt.h>
#include "SUMD.h"
#include "cc2500.h"

// http://focus.ti.com/docs/prod/folders/print/cc2500.html

// Add a loss histogram to the SUMD output
#define DEBUG

// Sync SUMD output with radio input
#define ONESHOT

// Uncomment to use a dedicated GDO pin to signal incoming packets
// #define USE_GDO_0

// Instead of writing out SUMD frames, print out summary of packet processing:
//  . == packet received
//  ! == packet expected, but not received
//  + == channel changed
//  @ == telemetry transmitted
//  T == invalid address
//  N == invalid packet length
//  X == CRC fail
#define SER_PRINT_DEBUG

#ifdef SER_PRINT_DEBUG
#  define DPRINT(a) (Serial.print(a))
#else
#  define DPRINT(a)
#endif

#define FAILSAFE_TIME_MS 1500
#define MAX_MISSING_PKT 20
#define SEEK_CHANSKIP   13

// Microseconds between sumd frames
#define SUMD_FREQ 10000

#define PIN2 0x04
#define PIN3 0x08
#define PIN4 0x10
#define PIN5 0x20
#define PIN6 0x40

#define NPIN2 0xfb
#define NPIN3 0xf7
#define NPIN4 0xef
#define NPIN5 0xdf
#define NPIN6 0xbf

#define CS 2
#define GDO_pin 3

#define CS_cc2500_on  (PORTD  |= PIN2)
#define CS_cc2500_off (PORTD &= NPIN2)

#ifdef USE_GDO_0
// Detect data on dedicated pin
#define DATA_PRESENT ((PIND & PIN3) == PIN3)
#else
// Detect data on SCK
#define DATA_PRESENT ((PINB & PIN4) == PIN4)
#endif

const int rssi_offset = 71;
const int rssi_min = -90;
const int rssi_max = -20;

#define MAX_CHANS 60

const uint8_t eeprom_addr(100);

byte txid[2];
int freq_offset;
uint8_t hopData[MAX_CHANS];
uint8_t numChans;
uint8_t ccData[27];
int rssi;
uint8_t chan;

#define NUM_BUCKETS 10
int loss_histo[NUM_BUCKETS];

#ifdef DEBUG
#define MORE_CHAN NUM_BUCKETS
#else
#define MORE_CHAN 0
#endif

#define NUM_CHAN 8
#define SUMD_RSSI_CHAN NUM_CHAN
SUMD sumd(SUMD_RSSI_CHAN + 1 + MORE_CHAN);

void setup() {
    SPI.begin();

    Serial.begin(115200);

    for (int i = 0; i < NUM_BUCKETS; i++) {
        loss_histo[i] = 0;
    }
    for (int i = 0; i < sumd.nchan(); i++) {
        sumd.setChannel(i, 1500);
    }

    initRadio();

    initSUMDTimer();
    initTimeoutTimer();
}

volatile bool tx_sumd = false;
volatile bool timedout = false;

// Set tx_sumd = true every 10ms
void initSUMDTimer() {
    cli();
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1  = 0;

    OCR1A = 2500;                         // compare match register 16MHz/64/10Hz
    TCCR1B |= (1 << WGM12);               // CTC mode
    TCCR1B |= (1 << CS11) | (1 << CS10);  // 64 prescaler
    TIMSK1 |= (1 << OCIE1A);              // enable timer compare interrupt
    sei();
}

#define T2_9MS (255-141)

// Set timeout = true every 9ms
void initTimeoutTimer() {
    cli();
    TCCR2A = 0;
    TCCR2B = 0;
    TCNT2  = T2_9MS; // about 9ms to go

    TCCR2B |= (1 << CS22) | (1 << CS21)  | (1 << CS20);  // 1024 prescaler
    TIMSK2 |= (1 << TOIE2);              // enable timer overflow interrupt
    sei();
}

ISR(TIMER1_COMPA_vect) {
    tx_sumd = true;
}

void txReady() {
#ifdef ONESHOT
    tx_sumd = true;
    TCNT1 = 0;
#endif
}

ISR(TIMER2_OVF_vect) {
    timedout = true;
}

void initRadio() {
    pinMode(CS, OUTPUT);        //CS output
    pinMode(GDO_pin, INPUT);    //GDO0 pin

    CS_cc2500_on;

    configureRadio();
    bindRadio();
    cc2500_writeReg(CC2500_ADDR, txid[0]);
    cc2500_writeReg(CC2500_FSCTRL0, freq_offset);

    cc2500_writeReg(CC2500_CHANNR, hopData[0]);//0A-hop
    cc2500_writeReg(CC2500_FSCAL3, 0x89); //23-89
    cc2500_strobe(CC2500_SRX);
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

void storeBind()
{
    uint8_t addr = eeprom_addr;
    EEPROM.write(addr, txid[0]);
    EEPROM.write(addr+1, txid[1]);

    for (int i = 0; i < sizeof(hopData); i++) {
        EEPROM.write(addr + 10 + i, hopData[i]);
    }
    EEPROM.write(addr + 100, numChans);
}

unsigned char bindJumper(void) {
    pinMode(A0, INPUT_PULLUP);
    if ( digitalRead(A0) == LOW) {
        delayMicroseconds(1);
        return 1;
    }
    return  0;
}

void bindRadio() {
    bool binding = bindJumper();
    while (1) {
        if (!binding) { //bind complete or no bind
            uint8_t addr = eeprom_addr;
            txid[0] = EEPROM.read(addr);
            txid[1] = EEPROM.read(addr+1);
            if (txid[0] == 0xff && txid[1] == 0xff) {
                // No valid txid, forcing bind
                binding = true;
                continue;
            }
            for (int i = 0; i < sizeof(hopData); i++) {
                hopData[i] = EEPROM.read(addr + 10 + i);
            }
            numChans = EEPROM.read(addr + 100);
            freq_offset = EEPROM.read(addr + 101);
            break;
        } else {
            tuning();
            int addr = 100;
            EEPROM.write(addr + 101, freq_offset);
            getBind();
            return;
        }
    }
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

void updateRSSI(int rssi_dec) {
    if (rssi_dec < 128) {
        rssi = ((rssi_dec / 2) - rssi_offset);
    } else {
        rssi = (((rssi_dec - 256) / 2)) - rssi_offset;
    }

    rssi = constrain(rssi, rssi_min, rssi_max);
}

void nextChannel(uint8_t skip) {
    chan += skip;
    if (chan >= numChans) chan -= numChans;
    cc2500_writeReg(CC2500_CHANNR, hopData[chan]);
    cc2500_writeReg(CC2500_FSCAL3, 0x89);
    TCNT2 = T2_9MS;
    DPRINT("+");
}

int missingPackets = 0;
int skips = 0;
unsigned long prev_pkt = 0;

// Sending telemetry currently isn't loud enough on any hardware I've
// got, but I think it looks something like this (dummy values present
// until I can actually see something).
void sendTelemetry() {
    static int tchan = 0;
    uint8_t telemPkt[18]= {0x11, txid[0], txid[1], 0x4d, 0x7f, 0x5a, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    cc2500_strobe(CC2500_SFRX);

    cc2500_writeFifo(telemPkt, sizeof(telemPkt));
    DPRINT("@");
}

void loop() {
    bool packet = false;
    static uint8_t seq = 0;
    static bool skipNext = false;

    cc2500_strobe(CC2500_SRX);
    CS_cc2500_on;
    while (1) {
        if (timedout) {
            timedout = false;
            txReady();
            cc2500_strobe(CC2500_SIDLE);
            if (missingPackets > MAX_MISSING_PKT) {
                nextChannel(SEEK_CHANSKIP);
                break;
            } else {
                nextChannel(1);
            }
            if (skipNext) {
                sendTelemetry();
            } else {
                DPRINT("!");
                missingPackets++;
                skips++;
            }
            skipNext = false;
            break;
        }

        if ((millis() - prev_pkt > FAILSAFE_TIME_MS)) {
            sumd.setHeader(SUMD_FAILSAFE);
        }

        if (tx_sumd) {
            transmitPacket();
        }

        if (DATA_PRESENT) {
            uint8_t ccLen = cc2500_readReg(CC2500_RXBYTES | CC2500_READ_BURST) & 0x7F;
            if (ccLen > 20)
                ccLen = 20;//
            if (ccLen) {
                cc2500_readFifo((uint8_t *)ccData, ccLen);
                if (ccData[ccLen - 1] & 0x80) { // Only if correct CRC
                    updateRSSI(ccData[ccLen - 2]);
                    missingPackets = 0;
                    if (ccData[0] == 0x11) { // Correct length
                        if (ccData[2] == txid[1]) { // second half of TX id
                            nextChannel(1);
                            packet = true;
                            TCNT2 = T2_9MS;
                            cc2500_strobe(CC2500_SIDLE);
                            break;
                        } else {
                            DPRINT("T");
                        }
                    } else {
                        DPRINT("N");
                    }
                } else {
                    DPRINT("X");
                }
            }
        }
    }
    CS_cc2500_off;

    if (packet == true) {
        DPRINT(".");
        prev_pkt = millis();
        if (++seq == 3) {
            seq = 0;
            skipNext = true;
        }
        int offset = 0;
        while (skips > 0) {
            offset++;
            skips = skips >> 1;
        }
        loss_histo[offset]++;

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
        txReady();
    }

    if (tx_sumd) {
        transmitPacket();
    }
}

void transmitPacket() {
    sumd.setChannel(SUMD_RSSI_CHAN, map(rssi, rssi_min, rssi_max, 1000, 2000));

    // zero out any histo that exceeds 1000 packets, making the output
    // calculation much more simple.
    for (int i = 0; i < MORE_CHAN; i++) {
        if (loss_histo[i] > 1000) {
            loss_histo[i] = 0;
        }
        sumd.setChannel(SUMD_RSSI_CHAN+1+i, loss_histo[i] + 1000);
    }
    #ifndef SER_PRINT_DEBUG
    Serial.write(sumd.bytes(), sumd.size());
    #endif
    tx_sumd = false;
}
