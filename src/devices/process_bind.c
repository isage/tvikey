#include "../inputdevice.h"
#include "../scancodes/scancodes.h"

#include <psp2kern/ctrl.h>

void processAnalogBind(InputDevice *c, uint8_t bind, uint8_t value)
{
  switch (bind)
  {
    case V_SCANCODE_L2:
      c->controlData.buttons |= SCE_CTRL_LTRIGGER;
      c->controlData.lt = value;
      break;
    case V_SCANCODE_R2:
      c->controlData.buttons |= SCE_CTRL_RTRIGGER;
      c->controlData.rt = value;
      break;

    case V_SCANCODE_LXM:
    case V_SCANCODE_LXP:
      c->controlData.leftX = value;
      break;

    case V_SCANCODE_LYM:
    case V_SCANCODE_LYP:
      c->controlData.leftY = value;
      break;

    case V_SCANCODE_RXM:
    case V_SCANCODE_RXP:
      c->controlData.rightX = value;
      break;

    case V_SCANCODE_RYM:
    case V_SCANCODE_RYP:
      c->controlData.rightY = value;
      break;
  }
}

void processBind(InputDevice *c, uint8_t bind)
{
  switch (bind)
  {
    case V_SCANCODE_DUP:
      c->controlData.buttons |= SCE_CTRL_UP;
      break;
    case V_SCANCODE_DDOWN:
      c->controlData.buttons |= SCE_CTRL_DOWN;
      break;
    case V_SCANCODE_DLEFT:
      c->controlData.buttons |= SCE_CTRL_LEFT;
      break;
    case V_SCANCODE_DRIGHT:
      c->controlData.buttons |= SCE_CTRL_RIGHT;
      break;

    case V_SCANCODE_CROSS:
      c->controlData.buttons |= SCE_CTRL_CROSS;
      break;
    case V_SCANCODE_CIRCLE:
      c->controlData.buttons |= SCE_CTRL_CIRCLE;
      break;
    case V_SCANCODE_TRIANGLE:
      c->controlData.buttons |= SCE_CTRL_TRIANGLE;
      break;
    case V_SCANCODE_SQUARE:
      c->controlData.buttons |= SCE_CTRL_SQUARE;
      break;

    case V_SCANCODE_L1:
      c->controlData.buttons |= SCE_CTRL_L1;
      break;
    case V_SCANCODE_R1:
      c->controlData.buttons |= SCE_CTRL_R1;
      break;

    case V_SCANCODE_L3:
      c->controlData.buttons |= SCE_CTRL_L3;
      break;
    case V_SCANCODE_R3:
      c->controlData.buttons |= SCE_CTRL_R3;
      break;

    case V_SCANCODE_L2:
      c->controlData.buttons |= SCE_CTRL_LTRIGGER;
      c->controlData.lt = 0xFF;
      break;
    case V_SCANCODE_R2:
      c->controlData.buttons |= SCE_CTRL_RTRIGGER;
      c->controlData.rt = 0xFF;
      break;

    case V_SCANCODE_LXM:
      c->controlData.leftX = 0;
      break;
    case V_SCANCODE_LXP:
      c->controlData.leftX = 255;
      break;

    case V_SCANCODE_LYM:
      c->controlData.leftY = 0;
      break;
    case V_SCANCODE_LYP:
      c->controlData.leftY = 255;
      break;

    case V_SCANCODE_RXM:
      c->controlData.rightX = 0;
      break;
    case V_SCANCODE_RXP:
      c->controlData.rightX = 255;
      break;

    case V_SCANCODE_RYM:
      c->controlData.rightY = 0;
      break;
    case V_SCANCODE_RYP:
      c->controlData.rightY = 255;
      break;

    case V_SCANCODE_PS:
      c->controlData.buttons |= SCE_CTRL_PSBUTTON;
      break;

    case V_SCANCODE_START:
      c->controlData.buttons |= SCE_CTRL_START;
      break;
    case V_SCANCODE_SELECT:
      c->controlData.buttons |= SCE_CTRL_SELECT;
      break;
  }
}

void processButtonBind(InputDevice *c, uint8_t bind)
{
  switch (bind)
  {
    case V_SCANCODE_DUP:
      c->controlData.buttons |= SCE_CTRL_UP;
      break;
    case V_SCANCODE_DDOWN:
      c->controlData.buttons |= SCE_CTRL_DOWN;
      break;
    case V_SCANCODE_DLEFT:
      c->controlData.buttons |= SCE_CTRL_LEFT;
      break;
    case V_SCANCODE_DRIGHT:
      c->controlData.buttons |= SCE_CTRL_RIGHT;
      break;

    case V_SCANCODE_CROSS:
      c->controlData.buttons |= SCE_CTRL_CROSS;
      break;
    case V_SCANCODE_CIRCLE:
      c->controlData.buttons |= SCE_CTRL_CIRCLE;
      break;
    case V_SCANCODE_TRIANGLE:
      c->controlData.buttons |= SCE_CTRL_TRIANGLE;
      break;
    case V_SCANCODE_SQUARE:
      c->controlData.buttons |= SCE_CTRL_SQUARE;
      break;

    case V_SCANCODE_L1:
      c->controlData.buttons |= SCE_CTRL_L1;
      break;
    case V_SCANCODE_R1:
      c->controlData.buttons |= SCE_CTRL_R1;
      break;

    case V_SCANCODE_L3:
      c->controlData.buttons |= SCE_CTRL_L3;
      break;
    case V_SCANCODE_R3:
      c->controlData.buttons |= SCE_CTRL_R3;
      break;

    case V_SCANCODE_PS:
      c->controlData.buttons |= SCE_CTRL_PSBUTTON;
      break;

    case V_SCANCODE_START:
      c->controlData.buttons |= SCE_CTRL_START;
      break;
    case V_SCANCODE_SELECT:
      c->controlData.buttons |= SCE_CTRL_SELECT;
      break;
  }
}
