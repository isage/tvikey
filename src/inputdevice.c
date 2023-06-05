#include "inputdevice.h"

#include "devices/keyboard.h"
#include "devices/mouse.h"

#include <psp2kern/kernel/suspend.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/usbd.h>

void on_read_data(int32_t result, int32_t count, void *arg)
{
  // process buffer

  InputDevice *c = (InputDevice *)arg;
  if (result == 0 && count > 0 && arg)
  {
    if (c->inited)
    {
      int ret = 0;
      switch (c->type)
      {
        case MOUSE:
          ret = Mouse_processReport(c, count);
          break;
        case KEYBOARD:
          ret = Keyboard_processReport(c, count);
          break;
        default:
          break;
      }
      if (ret)
        ksceKernelPowerTick(0); // cancel sleep timers.
    }
  }

  usb_read(c);
}

void on_write_data(int32_t result, int32_t count, void *arg)
{
  // check status
  // do nothing?
}

void usb_read(InputDevice *c)
{
  int ret;

  if (!c->inited)
    return;

  ret = ksceUsbdInterruptTransfer(c->pipe_in, c->buffer, c->buffer_size, on_read_data, c);

  if (ret < 0)
  {
    ksceDebugPrintf("ksceUsbdInterruptTransfer(in) error: 0x%08x\n", ret);
    // error out
  }
}

void usb_write(InputDevice *c, uint8_t *data, int len)
{
  int ret;
  ret = ksceUsbdInterruptTransfer(c->pipe_out, data, len, on_write_data, c);

  if (ret < 0)
  {
    ksceDebugPrintf("ksceUsbdInterruptTransfer(out) error: 0x%08x\n", ret);
    // error out
  }
}
