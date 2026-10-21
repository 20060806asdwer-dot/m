#ifndef RADIO_H
#define RADIO_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
bool radio_send(const uint8_t *data,size_t n);
void radio_sleep(void);
#endif
