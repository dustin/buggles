#include <SPI.h>
#include <util/delay.h>

SPISettings cc2500Settings(10000000, MSBFIRST, SPI_MODE0);

static void begin() {
    CS_cc2500_off;
    while(digitalRead(MISO) == HIGH);
    SPI.beginTransaction(cc2500Settings);
}

static void end() {
    CS_cc2500_on;
    SPI.endTransaction();
}

void cc2500_readFifo(uint8_t *data, int len)
{
    begin();
    SPI.transfer(CC2500_RXFIFO | CC2500_READ_BURST);
    for (int i = 0; i < len; i++) {
        data[i] = SPI.transfer(0xff);
    }
    end();
}

void cc2500_writeFifo(uint8_t *data, uint8_t len)
{
    cc2500_strobe(CC2500_SFTX);//0x3B

    begin();
    SPI.transfer(CC2500_WRITE_BURST | CC2500_TXFIFO);
    for (int i = 0; i < len; i++) {
        SPI.transfer(data[i]);
    }
    end();

    cc2500_strobe(CC2500_STX);//0x35
}

void cc2500_writeReg(uint8_t address, uint8_t data)
{
    begin();
    SPI.transfer(address);
    SPI.transfer(data);
    end();
}

unsigned char cc2500_readReg(unsigned char address)
{
    uint8_t result;
    begin();
    address |= 0x80;
    SPI.transfer(address);
    _delay_us(30);
    result = SPI.transfer(0xff);
    end();
    return (result);
}
void cc2500_strobe(uint8_t address)
{
    begin();
    SPI.transfer(address);
    _delay_us(30);
    end();
}

void cc2500_resetChip(void)
{
    CS_cc2500_on;
    _delay_us(30);
    CS_cc2500_off;
    _delay_us(30);
    CS_cc2500_on;
    _delay_us(45);
    cc2500_strobe(CC2500_SRES);
    _delay_ms(100);
}
