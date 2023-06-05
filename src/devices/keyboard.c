#include "keyboard.h"

#include "../config.h"

#include <psp2kern/ctrl.h>
#include <psp2kern/kernel/debug.h>

#define ksceUsbdSetBootProto(pid, iface)                                                                               \
  ({                                                                                                                   \
    SceUsbdDeviceRequest _dr;                                                                                          \
    _dr.bmRequestType = 0x21;                                                                                          \
    _dr.bRequest      = SCE_USBD_REQUEST_SET_INTERFACE;                                                                \
    _dr.wValue        = 0;                                                                                             \
    _dr.wIndex        = iface;                                                                                         \
    _dr.wLength       = 0;                                                                                             \
    ksceUsbdControlTransfer((pid), (&_dr), NULL, NULL, NULL);                                                          \
  })

void kb_cfg_done(int32_t result, int32_t count, void *arg)
{
  // process buffer

  InputDevice *c = (InputDevice *)arg;
  int r          = ksceUsbdSetBootProto(c->pipe_control, c->iface);
  ksceDebugPrintf("ksceUsbdSetBootProto = 0x%08x\n", r);
}

uint8_t Keyboard_probe(int device_id)
{
  SceUsbdInterfaceDescriptor *iface;
  iface = (SceUsbdInterfaceDescriptor *)ksceUsbdScanStaticDescriptor(device_id, 0, SCE_USBD_DESCRIPTOR_INTERFACE);

  while (iface)
  {
    if ((iface->bInterfaceClass == 3) && (iface->bInterfaceSubclass == 1) && (iface->bInterfaceProtocol == 1))
    {
      return 1;
    }
    iface = (SceUsbdInterfaceDescriptor *)ksceUsbdScanStaticDescriptor(device_id, iface, SCE_USBD_DESCRIPTOR_INTERFACE);
  }

  return 0;
}

uint8_t Keyboard_attach(InputDevice *c, int device_id, int port)
{
  c->type        = KEYBOARD;
  c->buffer_size = 8;
  c->device_id   = device_id;
  c->port        = port;

  // check device hid type

  SceUsbdInterfaceDescriptor *iface;
  iface = (SceUsbdInterfaceDescriptor *)ksceUsbdScanStaticDescriptor(device_id, 0, SCE_USBD_DESCRIPTOR_INTERFACE);
  while (iface)
  {
    if ((iface->bInterfaceClass == 3) && (iface->bInterfaceSubclass == 1) && (iface->bInterfaceProtocol == 1))
    {
      break;
    }
    iface = (SceUsbdInterfaceDescriptor *)ksceUsbdScanStaticDescriptor(device_id, iface, SCE_USBD_DESCRIPTOR_INTERFACE);
  }

  if (!iface)
    return 0;

  c->iface = iface->bInterfaceNumber;

  // init endpoints and stuff
  SceUsbdEndpointDescriptor *endpoint;
#if defined(DEBUG)
  ksceDebugPrintf("scanning endpoints for device %d\n", device_id);
#endif
  endpoint = (SceUsbdEndpointDescriptor *)ksceUsbdScanStaticDescriptor(device_id, iface, SCE_USBD_DESCRIPTOR_ENDPOINT);
  while (endpoint)
  {
#if defined(DEBUG)
    ksceDebugPrintf("got EP: %02x\n", endpoint->bEndpointAddress);
#endif
    if ((endpoint->bEndpointAddress & SCE_USBD_ENDPOINT_DIRECTION_BITS) == SCE_USBD_ENDPOINT_DIRECTION_IN
        && !c->pipe_in)
    {
#if defined(DEBUG)
      ksceDebugPrintf("opening in pipe\n");
#endif
      c->pipe_in = ksceUsbdOpenPipe(device_id, endpoint);
#if defined(DEBUG)
      ksceDebugPrintf("= 0x%08x\n", c->pipe_in);
      ksceDebugPrintf("bmAttributes = %x\n", endpoint->bmAttributes);
#endif
      c->buffer_size = endpoint->wMaxPacketSize;
      if (c->buffer_size > 64)
      {
        ksceDebugPrintf("Packet size too big = %d\n", endpoint->wMaxPacketSize);
        return 0;
      }
    }
    if (c->pipe_in > 0)
      break;
    endpoint
        = (SceUsbdEndpointDescriptor *)ksceUsbdScanStaticDescriptor(device_id, endpoint, SCE_USBD_DESCRIPTOR_ENDPOINT);
  }

  if (c->pipe_in > 0)
  {
    SceUsbdConfigurationDescriptor *cdesc;
    if ((cdesc = (SceUsbdConfigurationDescriptor *)ksceUsbdScanStaticDescriptor(device_id, NULL,
                                                                                SCE_USBD_DESCRIPTOR_CONFIGURATION))
        == NULL)
      return 0;

    SceUID control_pipe_id = ksceUsbdOpenPipe(device_id, NULL);
    c->pipe_control        = control_pipe_id;
    // set default config
    int r = ksceUsbdSetConfiguration(control_pipe_id, cdesc->bConfigurationValue, kb_cfg_done, c);
#if defined(DEBUG)
    ksceDebugPrintf("ksceUsbdSetConfiguration = 0x%08x\n", r);
#endif
    if (r < 0)
      return 0;

    c->attached = 1;
    c->inited   = 1;
  }

  usb_read(c);
  return 1;
}

static inline uint8_t bit(uint8_t data, int bit)
{
  return (data >> bit) & 1;
}

static inline int clamp(int value, int min, int max)
{
  if (value <= min)
    return min;
  if (value >= max)
    return max;
  return value;
}

extern bindings_t bind_config;

#include "process_bind.h"

uint8_t Keyboard_processReport(InputDevice *c, size_t length)
{
  // reset everything
  c->controlData.buttons = 0;
  c->controlData.leftX   = 128;
  c->controlData.leftY   = 128;
  c->controlData.rightX  = 128;
  c->controlData.rightY  = 128;
  c->controlData.lt      = 0;
  c->controlData.rt      = 0;

  for (int i = 1; i < 8; i++)
  {
    if (bind_config.kb_mod[i] != 0xFF && bind_config.kb_mod[i] != 0)
    {
      if (bit(c->buffer[0], i))
      {
        processBind(c, bind_config.kb_mod[i]);
      }
    }
  }

  for (int i = 0; i < 256; i++)
  {
    if (bind_config.kb[i] != 0xFF && bind_config.kb[i] != 0)
    {
      for (int j = 2; j < c->buffer_size; j++)
      {
        if (c->buffer[j] > 0 && c->buffer[j] == i)
        {
          processBind(c, bind_config.kb[i]);
        }
      }
    }
  }

  return 1;
}
