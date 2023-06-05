#ifndef __MOUSE_H__
#define __MOUSE_H__

#include "../inputdevice.h"

uint8_t Mouse_attach(InputDevice *c, int device_id, int port);
uint8_t Mouse_probe(int device_id);
uint8_t Mouse_processReport(InputDevice *c, size_t length);

#endif // __MOUSE_H__
