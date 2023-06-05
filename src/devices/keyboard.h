#ifndef __KEYBOARD_H__
#define __KEYBOARD_H__

#include "../inputdevice.h"

uint8_t Keyboard_attach(InputDevice *c, int device_id, int port);
uint8_t Keyboard_probe(int device_id);
uint8_t Keyboard_processReport(InputDevice *c, size_t length);

#endif // __KEYBOARD_H__
