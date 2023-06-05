#include "config.h"
#include "devices/keyboard.h"
#include "devices/mouse.h"
#include "inputdevice.h"
#include "scancodes/scancodes.h"
#include "util/ini.h"

#include <psp2kern/ctrl.h>
#include <psp2kern/kernel/debug.h>
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/proc_event.h>
#include <psp2kern/kernel/suspend.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/sysroot.h>
#include <psp2kern/sblaimgr.h>
#include <psp2kern/usbd.h>
#include <psp2kern/usbserv.h>
#include <taihen.h>

#define MAX_DEVICES 2

#define DECL_FUNC_HOOK(name, ...)                                                                                      \
  static tai_hook_ref_t name##HookRef;                                                                                 \
  static SceUID name##HookUid = -1;                                                                                    \
  static int name##HookFunc(__VA_ARGS__)

#define BIND_FUNC_OFFSET_HOOK(name, pid, modid, segidx, offset, thumb)                                                 \
  name##HookUid = taiHookFunctionOffsetForKernel((pid), &name##HookRef, (modid), (segidx), (offset), thumb,            \
                                                 (const void *)name##HookFunc)

#define BIND_FUNC_EXPORT_HOOK(name, pid, module, lib_nid, func_nid)                                                    \
  name##HookUid = taiHookFunctionExportForKernel((pid), &name##HookRef, (module), (lib_nid), (func_nid),               \
                                                 (const void *)name##HookFunc)

#define UNBIND_FUNC_HOOK(name)                                                                                         \
  ({                                                                                                                   \
    if (name##HookUid > 0)                                                                                             \
      taiHookReleaseForKernel(name##HookUid, name##HookRef);                                                           \
  })

static int started = 0;

static InputDevice devices[MAX_DEVICES];

bindings_t bind_config;
bindings_t last_bind_config;
bindings_t shell_bind_config;

static inline int clamp(int value, int min, int max)
{
  if (value <= min)
    return min;
  if (value >= max)
    return max;
  return value;
}

static void patchControlData(int port, SceCtrlData *data, int count, uint8_t negative, uint8_t triggers_ext)
{
  if (port > 1)
    return;

  uint32_t buttons = 0x00000000;

  for (int d = 0; d < MAX_DEVICES; d++)
  {
    if (!devices[d].inited || !devices[d].attached)
      continue;

    ControlData *controlData = &(devices[d].controlData);

    if (port == 0)
    { // for port 0 use button emulation
      buttons |= controlData->buttons;
    }

    for (int i = 0; i < count; i++)
    {
      if (port > 0)
      {
        // Set the button data from the controller, with optional negative logic
        if (negative)
          data[i].buttons &= ~controlData->buttons;
        else
          data[i].buttons |= controlData->buttons;
      }

      data[i].lt = clamp(data[i].lt + controlData->lt, 0, 255);
      data[i].rt = clamp(data[i].rt + controlData->rt, 0, 255);

      // Set the stick data from the controller
      data[i].lx = clamp(data[i].lx + controlData->leftX - 127, 0, 255);
      data[i].ly = clamp(data[i].ly + controlData->leftY - 127, 0, 255);
      data[i].rx = clamp(data[i].rx + controlData->rightX - 127, 0, 255);
      data[i].ry = clamp(data[i].ry + controlData->rightY - 127, 0, 255);
    }
  }

  if (port == 0)
  { // for port 0 use button emulation
    ksceCtrlSetButtonEmulation(0, 0, buttons, buttons, 16);
  }
}

#define DECL_FUNC_HOOK_CTRL(name, negative, triggers)                                                                  \
  DECL_FUNC_HOOK(name, int port, SceCtrlData *data, int count)                                                         \
  {                                                                                                                    \
    int ret = TAI_CONTINUE(int, name##HookRef, port, data, count);                                                     \
    if (ret >= 0)                                                                                                      \
      patchControlData(port, data, count, (negative), (triggers));                                                     \
    return ret;                                                                                                        \
  }

DECL_FUNC_HOOK_CTRL(ksceCtrlPeekBufferPositive, 0, 0)
DECL_FUNC_HOOK_CTRL(ksceCtrlReadBufferPositive, 0, 0)
DECL_FUNC_HOOK_CTRL(ksceCtrlPeekBufferNegative, 1, 0)
DECL_FUNC_HOOK_CTRL(ksceCtrlReadBufferNegative, 1, 0)
DECL_FUNC_HOOK_CTRL(ksceCtrlPeekBufferPositiveExt, 0, 0)
DECL_FUNC_HOOK_CTRL(ksceCtrlReadBufferPositiveExt, 0, 0)

DECL_FUNC_HOOK_CTRL(ksceCtrlPeekBufferPositive2, 0, 1)
DECL_FUNC_HOOK_CTRL(ksceCtrlReadBufferPositive2, 0, 1)
DECL_FUNC_HOOK_CTRL(ksceCtrlPeekBufferNegative2, 1, 1)
DECL_FUNC_HOOK_CTRL(ksceCtrlReadBufferNegative2, 1, 1)
DECL_FUNC_HOOK_CTRL(ksceCtrlPeekBufferPositiveExt2, 0, 1)
DECL_FUNC_HOOK_CTRL(ksceCtrlReadBufferPositiveExt2, 0, 1)

int libtvikey_probe(int device_id);
int libtvikey_attach(int device_id);
int libtvikey_detach(int device_id);

static const SceUsbdDriver libtvikeyDriver = {
    .name   = "libtvikey",
    .probe  = libtvikey_probe,
    .attach = libtvikey_attach,
    .detach = libtvikey_detach,
};

int libtvikey_usbcharge_probe(int device_id)
{
  return SCE_USBD_PROBE_FAILED;
}
int libtvikey_usbcharge_attach(int device_id)
{
  return SCE_USBD_PROBE_FAILED;
}
int libtvikey_usbcharge_detach(int device_id)
{
  return SCE_USBD_PROBE_FAILED;
}

static const SceUsbdDriver libtvikeyFakeUsbchargeDriver = {
    .name   = "usb_charge",
    .probe  = libtvikey_usbcharge_probe,
    .attach = libtvikey_usbcharge_attach,
    .detach = libtvikey_usbcharge_detach,
};

int libtvikey_probe(int device_id)
{
  SceUsbdDeviceDescriptor *device;
  ksceDebugPrintf("probing device: %x\n", device_id);
  device = (SceUsbdDeviceDescriptor *)ksceUsbdScanStaticDescriptor(device_id, 0, SCE_USBD_DESCRIPTOR_DEVICE);
  if (device)
  {
    ksceDebugPrintf("vendor: %04x\n", device->idVendor);
    ksceDebugPrintf("product: %04x\n", device->idProduct);

    if (Mouse_probe(device_id))
      return SCE_USBD_PROBE_SUCCEEDED;
    if (Keyboard_probe(device_id))
      return SCE_USBD_PROBE_SUCCEEDED;

    ksceDebugPrintf("Not supported!\n");
    return SCE_USBD_PROBE_FAILED;
  }
  return SCE_USBD_PROBE_FAILED;
}

int libtvikey_attach(int device_id)
{
  SceUsbdDeviceDescriptor *device;
  ksceDebugPrintf("attaching device: %x\n", device_id);
  device = (SceUsbdDeviceDescriptor *)ksceUsbdScanStaticDescriptor(device_id, 0, SCE_USBD_DESCRIPTOR_DEVICE);
  if (device)
  {
    ksceDebugPrintf("vendor: %04x\n", device->idVendor);
    ksceDebugPrintf("product: %04x\n", device->idProduct);
    int i;

    {
      int cont = -1;

      // find free slot
      for (int i = 0; i < MAX_DEVICES; i++)
      {
        if (!devices[i].attached || !devices[i].inited)
        {
          cont = i;
          break;
        }
      }

      if (cont == -1)
        return SCE_USBD_ATTACH_FAILED;

      if (!devices[cont].attached)
      {
        if (Mouse_attach(&devices[cont], device_id, cont))
        {
          // something gone wrong during usb init
          if (!devices[cont].inited)
          {
            ksceDebugPrintf("Can't init mouse\n");
            return SCE_USBD_ATTACH_FAILED;
          }
          ksceDebugPrintf("Attached!\n");
          return SCE_USBD_ATTACH_SUCCEEDED;
        }
        else if (Keyboard_attach(&devices[cont], device_id, cont))
        {
          // something gone wrong during usb init
          if (!devices[cont].inited)
          {
            ksceDebugPrintf("Can't init kb\n");
            return SCE_USBD_ATTACH_FAILED;
          }
          ksceDebugPrintf("Attached!\n");
          return SCE_USBD_ATTACH_SUCCEEDED;
        }
      }
    }
  }
  return SCE_USBD_ATTACH_FAILED;
}

int libtvikey_detach(int device_id)
{

  for (int i = 0; i < MAX_DEVICES; i++)
  {
    if (devices[i].inited && devices[i].device_id == device_id)
    {
      devices[i].attached     = 0;
      devices[i].inited       = 0;
      devices[i].pipe_in      = 0;
      devices[i].pipe_out     = 0;
      devices[i].pipe_control = 0;
      return SCE_USBD_DETACH_SUCCEEDED;
    }
  }

  return SCE_USBD_DETACH_FAILED;
}

static int libtvikey_sysevent_handler(int resume, int eventid, void *args, void *opt)
{
  if (resume && started)
  {
    if (ksceSblAimgrIsGenuineVITA())
      ksceUsbServMacSelect(2, 0); // re-set host mode
  }
  return 0;
}

typedef struct
{
  int loaded;
  char titleid[16];
  bindings_t b;
} configuration;

#undef BINDING
#define BINDING(sc, code, name) {sc, name},

const static struct
{
  KbScancode val;
  const char *str;
} kb_conversion[] = {
#include "scancodes/kb_scancodes.h"
};

const static struct
{
  KbScancodeMod val;
  const char *str;
} kb_mod_conversion[] = {
    {KB_MODIFIER_LEFTCTRL, "KB_LEFT_CTRL"},   {KB_MODIFIER_LEFTSHIFT, "KB_LEFT_SHIFT"},
    {KB_MODIFIER_LEFTALT, "KB_LEFT_ALT"},     {KB_MODIFIER_LEFTGUI, "KB_LEFT_GUI"},
    {KB_MODIFIER_RIGHTCTRL, "KB_RIGHT_CTRL"}, {KB_MODIFIER_RIGHTSHIFT, "KB_RIGHT_SHIFT"},
    {KB_MODIFIER_RIGHTALT, "KB_RIGHT_ALT"},   {KB_MODIFIER_RIGHTGUI, "KB_RIGHT_GUI"},
};

const static struct
{
  MouseScancode val;
  const char *str;
} ms_conversion[] = {
    {MS_SCANCODE_1, "MOUSE_1"},     {MS_SCANCODE_2, "MOUSE_2"},      {MS_SCANCODE_3, "MOUSE_3"},
    {MS_SCANCODE_XM, "MOUSE_LEFT"}, {MS_SCANCODE_XP, "MOUSE_RIGHT"}, {MS_SCANCODE_YM, "MOUSE_UP"},
    {MS_SCANCODE_YP, "MOUSE_DOWN"},
};

const static struct
{
  VitaScancode val;
  const char *str;
} vita_conversion[] = {
    {V_SCANCODE_DUP, "DPAD_UP"},
    {V_SCANCODE_DDOWN, "DPAD_DOWN"},
    {V_SCANCODE_DLEFT, "DPAD_LEFT"},
    {V_SCANCODE_DRIGHT, "DPAD_RIGHT"},
    {V_SCANCODE_CROSS, "CROSS"},
    {V_SCANCODE_SQUARE, "SQUARE"},
    {V_SCANCODE_TRIANGLE, "TRIANGLE"},
    {V_SCANCODE_CIRCLE, "CIRCLE"},
    {V_SCANCODE_SELECT, "SELECT"},
    {V_SCANCODE_START, "START"},
    {V_SCANCODE_PS, "PS"},
    {V_SCANCODE_L1, "L1"},
    {V_SCANCODE_L2, "L2"},
    {V_SCANCODE_L3, "L3"},
    {V_SCANCODE_R1, "R1"},
    {V_SCANCODE_R2, "R2"},
    {V_SCANCODE_R3, "R3"},
    {V_SCANCODE_LXM, "LEFT_ANALOG_LEFT"},
    {V_SCANCODE_LXP, "LEFT_ANALOG_RIGHT"},
    {V_SCANCODE_LYP, "LEFT_ANALOG_DOWN"},
    {V_SCANCODE_LYM, "LEFT_ANALOG_UP"},
    {V_SCANCODE_RXM, "RIGHT_ANALOG_LEFT"},
    {V_SCANCODE_RXP, "RIGHT_ANALOG_RIGHT"},
    {V_SCANCODE_RYP, "RIGHT_ANALOG_DOWN"},
    {V_SCANCODE_RYM, "RIGHT_ANALOG_UP"},

};

VitaScancode str2enum(const char *str)
{
  for (int i = 0; i < sizeof(vita_conversion) / sizeof(vita_conversion[0]); ++i)
  {
    if (strcmp(str, vita_conversion[i].str) == 0)
    {
      return vita_conversion[i].val;
    }
  }

  return V_SCANCODE_UNKNOWN;
}

int atoi(const char *str)
{
  int sign = 1, base = 0, i = 0;

  while (str[i] == ' ')
  {
    i++;
  }

  if (str[i] == '-' || str[i] == '+')
  {
    sign = 1 - 2 * (str[i++] == '-');
  }

  while (str[i] >= '0' && str[i] <= '9')
  {
    if (base > INT_MAX / 10 || (base == INT_MAX / 10 && str[i] - '0' > 7))
    {
      if (sign == 1)
        return INT_MAX;
      else
        return INT_MIN;
    }
    base = 10 * base + (str[i++] - '0');
  }
  return base * sign;
}

static int config_handler(void *user, const char *section, const char *name, const char *value)
{
  configuration *pconfig = (configuration *)user;

  if (strcmp(section, pconfig->titleid) == 0)
  {
    pconfig->loaded = 1;

    for (int i = 0; i < sizeof(kb_conversion) / sizeof(kb_conversion[0]); ++i)
    {
      if (!strcmp(name, kb_conversion[i].str))
      {
        VitaScancode s = str2enum(value);
        if (s != V_SCANCODE_UNKNOWN)
        {
          pconfig->b.kb[kb_conversion[i].val] = s;
        }
      }
    }

    for (int i = 0; i < sizeof(kb_mod_conversion) / sizeof(kb_mod_conversion[0]); ++i)
    {
      if (!strcmp(name, kb_mod_conversion[i].str))
      {
        VitaScancode s = str2enum(value);
        if (s != V_SCANCODE_UNKNOWN)
        {
          pconfig->b.kb_mod[i] = s;
        }
      }
    }

    for (int i = 0; i < sizeof(ms_conversion) / sizeof(ms_conversion[0]); ++i)
    {
      if (!strcmp(name, ms_conversion[i].str))
      {
        VitaScancode s = str2enum(value);
        if (s != V_SCANCODE_UNKNOWN)
        {
          pconfig->b.mouse[ms_conversion[i].val] = s;
        }
      }
    }

    if (!strcmp(name, "MS_SENSITIVITY_X"))
    {
      pconfig->b.mouse_sensitivity_x = atoi(value);
    }

    if (!strcmp(name, "MS_SENSITIVITY_Y"))
    {
      pconfig->b.mouse_sensitivity_y = atoi(value);
    }
  }

  return 1;
}

static SceUID last_loaded_pid;

void reset_config()
{
  memcpy(&bind_config, &shell_bind_config, sizeof(bindings_t));
}

void restore_last_config()
{
  memcpy(&bind_config, &last_bind_config, sizeof(bindings_t));
}

void load_shell_config()
{
  // fill default config
  memset(&bind_config, 0, sizeof(bindings_t));

  configuration config;
  strncpy(config.titleid, "shell", 16);

  config.loaded = 0;
  memset(config.b.kb, 0, 256);
  memset(config.b.kb_mod, 0, 8);
  memset(config.b.mouse, 0, 8);

  int error = ini_parse("ux0:/data/tvikey.ini", config_handler, &config, "shell");
  if (error != 0)
  {
    if (error < 0)
      ksceDebugPrintf("Can't load 'ux0:/data/tvikey.ini': 0x%08x\n", error);
    else
      ksceDebugPrintf("Can't load 'ux0:/data/tvikey.ini' at %d\n", error);
  }

  if (config.loaded)
  {
    ksceDebugPrintf("Config loaded for shell\n");
    memcpy(&shell_bind_config, &(config.b), sizeof(bindings_t));
    memcpy(&bind_config, &(config.b), sizeof(bindings_t));
  }
  else
  {
    memset(&shell_bind_config, 0, sizeof(bindings_t));
    shell_bind_config.mouse_sensitivity_x = 10;
    shell_bind_config.mouse_sensitivity_y = 10;

    shell_bind_config.kb[SC_UP_ARROW]    = V_SCANCODE_DUP;
    shell_bind_config.kb[SC_DOWN_ARROW]  = V_SCANCODE_DDOWN;
    shell_bind_config.kb[SC_LEFT_ARROW]  = V_SCANCODE_DLEFT;
    shell_bind_config.kb[SC_RIGHT_ARROW] = V_SCANCODE_DRIGHT;

    shell_bind_config.kb[SC_ESCAPE] = V_SCANCODE_START;
    shell_bind_config.kb[SC_F1]     = V_SCANCODE_SELECT;

    shell_bind_config.kb[SC_ENTER]                  = V_SCANCODE_CROSS;
    shell_bind_config.kb[SC_BACKSPACE]              = V_SCANCODE_CIRCLE;
    shell_bind_config.kb[SC_SPACE]                  = V_SCANCODE_TRIANGLE;
    shell_bind_config.kb_mod[KB_MODIFIER_RIGHTCTRL] = V_SCANCODE_SQUARE;

    shell_bind_config.kb[SC_END]       = V_SCANCODE_L1;
    shell_bind_config.kb[SC_PAGE_DOWN] = V_SCANCODE_R1;
    shell_bind_config.kb[SC_HOME]      = V_SCANCODE_L2;
    shell_bind_config.kb[SC_PAGE_UP]   = V_SCANCODE_R2;
    shell_bind_config.kb[SC_INSERT]    = V_SCANCODE_L3;
    shell_bind_config.kb[SC_DELETE]    = V_SCANCODE_R3;

    shell_bind_config.mouse[MS_SCANCODE_1]  = V_SCANCODE_R1;
    shell_bind_config.mouse[MS_SCANCODE_2]  = V_SCANCODE_L1;
    shell_bind_config.mouse[MS_SCANCODE_3]  = V_SCANCODE_TRIANGLE;
    shell_bind_config.mouse[MS_SCANCODE_XM] = V_SCANCODE_LXM;
    shell_bind_config.mouse[MS_SCANCODE_XP] = V_SCANCODE_LXP;
    shell_bind_config.mouse[MS_SCANCODE_YM] = V_SCANCODE_LYM;
    shell_bind_config.mouse[MS_SCANCODE_YP] = V_SCANCODE_LYP;
    memcpy(&bind_config, &shell_bind_config, sizeof(bindings_t));
  }
}

int libtvikey_proc_create(SceUID pid, SceProcEventInvokeParam2 *a2, int a3)
{
  char titleid[16];
  ksceKernelSysrootGetProcessTitleId(pid, titleid, 16);

  ksceDebugPrintf("title_id = %s\n", titleid);

  // skip loading for shell, we do that in another place
  if (strcmp(titleid,"main") == 0 ) return 0;

  configuration config;
  strncpy(config.titleid, titleid, 16);

  config.loaded = 0;
  memset(config.b.kb, 0, 256);
  memset(config.b.kb_mod, 0, 8);
  memset(config.b.mouse, 0, 8);

  int error = ini_parse("ux0:/data/tvikey.ini", config_handler, &config, titleid);
  if (error != 0)
  {
    if (error < 0)
      ksceDebugPrintf("Can't load 'ux0:/data/tvikey.ini': 0x%08x\n", error);
    else
      ksceDebugPrintf("Can't load 'ux0:/data/tvikey.ini' at %d\n", error);
  }

  if (config.loaded)
  {
    ksceDebugPrintf("Config loaded for %s\n", titleid);
    memcpy(&bind_config, &(config.b), sizeof(bindings_t));
    memcpy(&last_bind_config, &(config.b), sizeof(bindings_t));
    last_loaded_pid = pid;
  }
  else
  {
  }

  return 0;
}

int libtvikey_proc_stop(SceUID pid, int event_type, SceProcEventInvokeParam1 *a3, int a4)
{
    if (pid != 0 && pid == last_loaded_pid && event_type == 0x1000)
    {
        reset_config();
    }
    return 0;
}

int libtvikey_proc_start(SceUID pid, int event_type, SceProcEventInvokeParam1 *a3, int a4)
{
    if (pid != 0 && pid == last_loaded_pid && event_type == 0x10000)
    {
      restore_last_config();
    }
    return 0;
}

int libtvikey_proc_exit(SceUID pid, SceProcEventInvokeParam1 *a3, int a4)
{
    if (pid != 0 && pid == last_loaded_pid)
    {
        last_loaded_pid = 0;
    }

}

int libtvikey_proc_kill(SceUID pid, SceProcEventInvokeParam1 *a3, int a4)
{
    if (pid != 0 && pid == last_loaded_pid)
    {
        last_loaded_pid = 0;
    }
}


static SceUID proc_handler_uid;

static const SceProcEventHandler proc_handler = {.size           = 0x1C,
                                                 .create         = libtvikey_proc_create,
                                                 .exit           = libtvikey_proc_exit,
                                                 .kill           = libtvikey_proc_kill,
                                                 .stop           = libtvikey_proc_stop,
                                                 .start          = libtvikey_proc_start,
                                                 .switch_process = NULL};



int module_start(SceSize args, void *argp)
{
  tai_module_info_t modInfo;
  modInfo.size = sizeof(tai_module_info_t);

  last_loaded_pid = 0;

  for (int i = 0; i < MAX_DEVICES; i++)
  {
    devices[i].inited   = 0;
    devices[i].attached = 0;
  }

  if (taiGetModuleInfoForKernel(KERNEL_PID, "SceCtrl", &modInfo) < 0)
    return SCE_KERNEL_START_FAILED;

  // Hook control data functions
  BIND_FUNC_EXPORT_HOOK(ksceCtrlPeekBufferPositive, KERNEL_PID, "SceCtrl", TAI_ANY_LIBRARY, 0xEA1D3A34);
  BIND_FUNC_EXPORT_HOOK(ksceCtrlReadBufferPositive, KERNEL_PID, "SceCtrl", TAI_ANY_LIBRARY, 0x9B96A1AA);
  BIND_FUNC_EXPORT_HOOK(ksceCtrlPeekBufferNegative, KERNEL_PID, "SceCtrl", TAI_ANY_LIBRARY, 0x19895843);
  BIND_FUNC_EXPORT_HOOK(ksceCtrlReadBufferNegative, KERNEL_PID, "SceCtrl", TAI_ANY_LIBRARY, 0x8D4E0DD1);
  BIND_FUNC_OFFSET_HOOK(ksceCtrlPeekBufferPositiveExt, KERNEL_PID, modInfo.modid, 0, 0x3928 | 1, 1);
  BIND_FUNC_OFFSET_HOOK(ksceCtrlReadBufferPositiveExt, KERNEL_PID, modInfo.modid, 0, 0x3BCC | 1, 1);

  // Hook extended control data functions
  BIND_FUNC_OFFSET_HOOK(ksceCtrlPeekBufferPositive2, KERNEL_PID, modInfo.modid, 0, 0x3EF8 | 1, 1);
  BIND_FUNC_OFFSET_HOOK(ksceCtrlReadBufferPositive2, KERNEL_PID, modInfo.modid, 0, 0x449C | 1, 1);
  BIND_FUNC_OFFSET_HOOK(ksceCtrlPeekBufferNegative2, KERNEL_PID, modInfo.modid, 0, 0x41C8 | 1, 1);
  BIND_FUNC_OFFSET_HOOK(ksceCtrlReadBufferNegative2, KERNEL_PID, modInfo.modid, 0, 0x47F0 | 1, 1);
  BIND_FUNC_OFFSET_HOOK(ksceCtrlPeekBufferPositiveExt2, KERNEL_PID, modInfo.modid, 0, 0x4B48 | 1, 1);
  BIND_FUNC_OFFSET_HOOK(ksceCtrlReadBufferPositiveExt2, KERNEL_PID, modInfo.modid, 0, 0x4E14 | 1, 1);

  started = 1;

  if (ksceSblAimgrIsGenuineVITA())
  {
    ksceUsbServMacSelect(2, 0);
  }

  // fill default config
  load_shell_config();

  int ret_drv = ksceUsbdRegisterDriver(&libtvikeyDriver);
  ksceDebugPrintf("ksceUsbdRegisterDriver = 0x%08x\n", ret_drv);

  // remove sony usb_charge driver that intercepts HID devices
  ret_drv = ksceUsbdUnregisterDriver(&libtvikeyFakeUsbchargeDriver);
  ksceDebugPrintf("ksceUsbdUnregisterDriver = 0x%08x\n", ret_drv);

  ksceKernelRegisterSysEventHandler("ztvikey_sysevent", libtvikey_sysevent_handler, NULL);

  proc_handler_uid = ksceKernelRegisterProcEventHandler("ztvikey_procevent", &proc_handler, 0);

  return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize args, void *argp)
{
  // Unhook control data functions
  UNBIND_FUNC_HOOK(ksceCtrlReadBufferNegative);
  UNBIND_FUNC_HOOK(ksceCtrlPeekBufferPositive);
  UNBIND_FUNC_HOOK(ksceCtrlReadBufferPositive);
  UNBIND_FUNC_HOOK(ksceCtrlPeekBufferNegative);
  UNBIND_FUNC_HOOK(ksceCtrlPeekBufferPositiveExt);
  UNBIND_FUNC_HOOK(ksceCtrlReadBufferPositiveExt);

  // Unhook extended control data functions
  UNBIND_FUNC_HOOK(ksceCtrlPeekBufferPositive2);
  UNBIND_FUNC_HOOK(ksceCtrlReadBufferPositive2);
  UNBIND_FUNC_HOOK(ksceCtrlPeekBufferNegative2);
  UNBIND_FUNC_HOOK(ksceCtrlReadBufferNegative2);
  UNBIND_FUNC_HOOK(ksceCtrlPeekBufferPositiveExt2);
  UNBIND_FUNC_HOOK(ksceCtrlReadBufferPositiveExt2);

  ksceKernelUnregisterProcEventHandler(proc_handler_uid);

  return SCE_KERNEL_STOP_SUCCESS;
}

void _start()
{
  module_start(0, NULL);
}
