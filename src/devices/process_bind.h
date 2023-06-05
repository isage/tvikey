#ifndef __PROCESS_BIND_H__
#define __PROCESS_BIND_H__

void processBind(InputDevice *c, uint8_t bind);
void processAnalogBind(InputDevice *c, uint8_t bind, uint8_t value);
void processButtonBind(InputDevice *c, uint8_t bind);

#endif