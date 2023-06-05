# Tvikey

PSTV kernel driver for mouse/keyboard

(no, there's no mouse->touch translation)

## Installing
* Copy `tvikey.skprx` into `ur0:tai` folder
* Add `ur0:tai/tvikey.skprx` line under `*KERNEL` in tai config and reboot.
* Create (or copy sample) `tvikey.ini` into `ux0:/data/`, see [config format](CONFIG.md)
* For vita you need usb Y-cable and external power. See [this](https://github.com/isage/vita-usb-ether#hardware) for example.
* You'll need (powered) usb-hub to connect mouse and keyboard at the same time
* Your mouse/keyboard must support usb hid boot protocol (e.g. work in pc bios)

## Building

* Install vitausb from https://github.com/isage/vita-packages-extra
* `mkdir build && cmake -DCMAKE_BUILD_TYPE=Release .. && make`

## License

MIT, see LICENSE.md

## Credits

* **[CreepNT](https://github.com/CreepNT/)** - help and support
* **CBPS discord** - for support and stupid ideas
