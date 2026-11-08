# Xedge on the MIMXRT1020-EVK

Build and update Lua applications from your browser on the NXP MIMXRT1020-EVK. This Ethernet profile gives Xedge 70% of the board's 32 MiB SDRAM and reserves 4 MiB of its 8 MiB QSPI flash for applications and settings.

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
4. Keep SW8 in normal QSPI boot mode: **1 OFF, 2 OFF, 3 ON, 4 OFF**. Use the [RT1020 flashing script](../scripts/flash_rt1020.py) with one of the following commands.

The script uses a full system reset (`SYSRESETREQ`), masks interrupts, and disables SysTick before programming. This prevents an already running RTOS from disrupting the flash operation. The default core-only reset (`VECTRESET`) used by the generic pyOCD commands can leave interrupts and peripherals active, so use this script for RT1020 updates instead of plain `west flash` or `pyocd flash`.

On Linux, run from the Zephyr workspace root with the USB probe accessible to Linux. Install pyOCD in your active Python environment, then replace `YOUR_PROBE_ID` with the exact ID shown by `pyocd list`:

```bash
python -m pip install "pyocd==0.46.1"
python -m pyocd list
# Select the exact probe and the complete ROM-bootable image.
python modules/Xedge4Zephyr/scripts/flash_rt1020.py \
  --probe YOUR_PROBE_ID --image build-rt1020/zephyr/zephyr.bin
```

For a WSL build with the USB probe connected to Windows, copy `build-rt1020/zephyr/zephyr.bin` to `build/rt1020/zephyr.bin` inside your Windows Xedge4Zephyr checkout, creating that directory if needed. Run these commands in PowerShell from the checkout root, replacing `YOUR_PROBE_ID` with the exact ID shown by `pyocd list`:

```powershell
python -m pip install "pyocd==0.46.1"
python -X utf8 -m pyocd list
# Use the Windows USB probe with the image copied from WSL.
python -X utf8 .\scripts\flash_rt1020.py --probe YOUR_PROBE_ID --image .\build\rt1020\zephyr.bin
```

The script programs at `0x60000000` using sector erase, preserving the `/xedge` partition at `0x60400000`. It then performs another full system reset, lets the ROM initialize QSPI and SDRAM, and halts at the application entry to compare every image byte with flash. On success it prints `RT1020_FLASH_VERIFIED` with the image's SHA-256 and starts Xedge. Add `--verify-only` to reset, verify, and boot an already installed image without programming. The optional `--frequency` argument takes a positive integer SWD clock rate in Hz; its default is `1000000`.

If the probe cannot connect, first check power, the J23 data cable, and the SWD jumpers. Use ROM recovery only when debugger access remains unavailable: disconnect all power, set SW8 to **1 OFF, 2 OFF, 3 OFF, 4 ON**, and reconnect. The script can program in that mode, but its boot/readback step requires normal QSPI boot and will time out. After programming, disconnect all power, restore normal SW8 settings, reconnect, and rerun the same command with `--verify-only`. See the [Zephyr board guide](https://docs.zephyrproject.org/latest/boards/nxp/mimxrt1020_evk/doc/index.html) for probe configuration.

Find the DHCP address in the serial output and open `http://<board-ip>/rtl/`. Continue with [Running Lua Applications](../README.md#running-lua-applications) to create applications and configure HTTPS.

Native mDNS supports SharkCA's local HTTPS names. Start with the [automatic certificate management introduction](https://realtimelogic.com/services/SharkTrust/), then follow the [Xedge certificate manual](https://realtimelogic.com/ba/doc/en/Xedge.html#cert). [Local names and SharkCA](mdns.md) covers Zephyr-specific behavior.

The Ethernet MAC address is derived from the processor's silicon ID and remains stable across reboots. You can reserve its DHCP address in your router to keep the browser URL fixed.

## Memory and storage

| Region | Start address | Capacity | Use |
| --- | --- | ---: | --- |
| QSPI firmware | `0x60000000` | 4 MiB | Boot header, executable code, embedded resources, RAM initialization image |
| QSPI LittleFS | `0x60400000` | 4 MiB | Writable applications and settings, mounted at `/xedge` |
| SDRAM | `0x80000000` | 32 MiB | Xedge heap, kernel data, stacks, and network pools |

`CONFIG_XEDGE_HEAP_SIZE=23488096` reserves 70% of SDRAM, rounded down to an 8-byte boundary. See [Heap sizing](adding-boards.md#3-place-and-size-the-xedge-heap) when changing this allocation.

The boot image initializes SDRAM before the application uses it. BAS worker stacks come from the Xedge heap; Zephyr's heap, static stacks, and network pools use additional RAM. The main initialization and time-synchronization thread has a separate 4,096-byte stack.

Firmware and embedded resources are read directly from QSPI. The flash driver and its controller configuration are relocated into internal instruction RAM for filesystem writes, and Ethernet DMA buffers use internal data RAM. Keep the flash-driver relocation settings in Kconfig and [CMakeLists.txt](../XedgeInit/CMakeLists.txt) when modifying this layout.

LittleFS formats its partition on first use. Use sector programming for firmware updates and avoid a whole-chip erase to retain saved applications. The image boots directly without MCUboot or an update slot. Keep `CONFIG_FS_LITTLEFS_CACHE_SIZE=256` equal to the overlay's cache size when changing filesystem settings.
