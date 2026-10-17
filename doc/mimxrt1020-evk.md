# Xedge on the MIMXRT1020-EVK

Run Xedge on the NXP MIMXRT1020-EVK using wired Ethernet, 32 MiB SDRAM, and 8 MiB QSPI flash. This configuration gives Xedge 70% of SDRAM and reserves 4 MiB of flash for applications and settings.

## Build

Complete the [workspace and resource setup](../README.md#creating-a-workspace), then build from the Zephyr workspace:

```bash
source ~/zephyrproject/.venv/bin/activate
cd ~/xedgews
# Keep this board's output in its own build directory.
west build -d build-rt1020 -b mimxrt1020_evk \
  modules/Xedge4Zephyr/XedgeInit -- \
  -DEXTRA_ZEPHYR_MODULES="$PWD/modules/Xedge4Zephyr"
```

Zephyr loads [the board configuration](../XedgeInit/boards/mimxrt1020_evk.conf) and [overlay](../XedgeInit/boards/mimxrt1020_evk.overlay). The full target is `mimxrt1020_evk/mimxrt1021`. Rebuild with `west build -d build-rt1020` after source changes.

## Flash and connect

1. Connect the board's debug USB port (J23) to your computer with a data-capable cable. For power from J23, bridge J1 pins 5 and 6. Alternatively, bridge J1 pins 3 and 4 and supply power through the processor's USB port (J9), keeping a second cable connected to J23 for programming and the serial console. Disconnect power before moving the power-selection jumper. On the A3 board, keep SWD jumpers J27/J28 fitted, and fit J25/J26 for the serial console. For other revisions, check the board's schematic for the isolation jumper labels. Connect Ethernet to a LAN with DHCP and DNS. Internet access allows time synchronization and certificate services.
2. Open the debug probe's serial port at **115200 baud, 8 data bits, no parity, one stop bit**.
3. Install [pyOCD](https://pyocd.io/docs/installing.html) on the host that has access to the USB probe. The board's DAPLink probe supports the `mimxrt1020` target.
4. Program the board with one of the following methods.

On Linux, install pyOCD in your active Python environment, then use west:

```bash
python -m pip install pyocd
# Run from the workspace root with the USB probe accessible to Linux.
west flash -d build-rt1020 --runner pyocd
```

For a WSL build with the USB probe connected to Windows, copy `build-rt1020/zephyr/zephyr.bin` to Windows. Run these commands in PowerShell from the directory containing that file:

```powershell
python -m pip install pyocd
python -X utf8 -m pyocd list
# Load the complete boot image at the QSPI base address.
python -X utf8 -m pyocd flash -t mimxrt1020 --erase sector -a 0x60000000 .\zephyr.bin
```

If multiple probes are connected, add `-u <probe-id>` using the ID shown by `pyocd list`. Follow the [Zephyr board guide](https://docs.zephyrproject.org/latest/boards/nxp/mimxrt1020_evk/doc/index.html) for probe configuration. For normal QSPI boot, set SW8 to **1 OFF, 2 OFF, 3 ON, 4 OFF**.

To recover debugger access, disconnect power and set SW8 to **1 OFF, 2 OFF, 3 OFF, 4 ON**, then reconnect power. This starts the processor's ROM serial downloader. Program the image, then disconnect power and restore the normal QSPI boot settings before reconnecting.

After programming, press the processor reset button (SW3), find the DHCP address in the serial output, and open `http://<board-ip>/rtl/`. Continue with [Use Xedge](../README.md#use-xedge) to create applications and configure HTTPS.

The Ethernet MAC address is derived from the processor's silicon ID and remains stable across reboots. You can reserve its DHCP address in your router to keep the browser URL fixed.

## Memory and storage

| Region | Start address | Capacity | Use |
| --- | --- | ---: | --- |
| QSPI firmware | `0x60000000` | 4 MiB | Boot header, executable code, embedded resources, RAM initialization image |
| QSPI LittleFS | `0x60400000` | 4 MiB | Writable applications and settings, mounted at `/xedge` |
| SDRAM | `0x80000000` | 32 MiB | Xedge heap, kernel data, stacks, and network pools |

`CONFIG_XEDGE_HEAP_SIZE=23488096` reserves 70% of SDRAM, rounded down to an 8-byte boundary:

```text
floor((32 * 1024 * 1024 * 70 / 100) / 8) * 8 = 23,488,096 bytes
```

The boot image initializes SDRAM before the application uses it. BAS worker stacks come from the Xedge heap; Zephyr's heap, static stacks, and network pools use additional RAM. The main initialization/SNTP thread has a separate 4,096-byte stack. These settings apply only to this board.

Firmware and embedded resources are read directly from QSPI. The flash driver and its controller configuration are relocated into internal instruction RAM for filesystem writes, and Ethernet DMA buffers use internal data RAM. Keep the flash-driver relocation settings in Kconfig and [CMakeLists.txt](../XedgeInit/CMakeLists.txt) when modifying this layout.

LittleFS formats its partition on first use. Use sector programming for firmware updates and avoid a whole-chip erase to retain saved applications. The image boots directly without MCUboot or an update slot. Keep `CONFIG_FS_LITTLEFS_CACHE_SIZE=256` equal to the overlay's cache size when changing filesystem settings.
