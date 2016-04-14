#ifndef SERIAL_H
#define SERIAL_H 1

#include <stdlib.h>

extern "C" {
void initSerial();
void ser_write(const uint8_t v);
void ser_write_block(const uint8_t *v, unsigned int len);
}

#endif /* SERIAL_H */
