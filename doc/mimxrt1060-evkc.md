# Xedge on the MIMXRT1060-EVKC

Build web interfaces and connected services on the NXP MIMXRT1060-EVKC, then update your Lua applications through Xedge's browser IDE. This Ethernet profile reserves 70% of the board's 32 MiB SDRAM for Xedge and provides 12 MiB of flash for applications and settings.

## Build

Complete the [workspace and resource setup](../README.md#creating-a-workspace), then run these commands from your Zephyr workspace:

```bash
source ~/zephyrproject/.venv/bin/activate
cd ~/xedgews
# Select revision C and its QSPI flash explicitly.
west build -d build-evkc -b mimxrt1060_evk@C/mimxrt1062/qspi \
  modules/Xedge4Zephyr/XedgeInit -- \
  -DEXTRA_ZEPHYR_MODULES="$PWD/modules/Xedge4Zephyr"
```

Zephyr loads [the board configuration](../XedgeInit/boards/mimxrt1060_evk_mimxrt1062_qspi.conf) and [overlay](../XedgeInit/boards/mimxrt1060_evk_mimxrt1062_qspi.overlay). The overlay requires revision C's 16 MiB QSPI flash. Board revisions with different flash devices need their own configuration.

The firmware boots directly from QSPI flash, including the NXP boot header and SDRAM initialization data. The build outputs are in `build-evkc/zephyr/`, including `zephyr.elf`, `zephyr.bin`, and `zephyr.map`. Rebuild after source changes with `west build -d build-evkc`.

## Flash and connect

1. Install NXP LinkServer and configure the debug probe for it. Follow the revision C power, boot-switch, and probe instructions in the [Zephyr board guide](https://docs.zephyrproject.org/latest/boards/nxp/mimxrt1060_evk/doc/index.html).
2. Connect the board's debug USB port to the programming host and Ethernet to a LAN with DHCP and DNS. Internet access allows clock synchronization and online certificate services.
3. Open the debug probe's serial port at **115200 baud, 8 data bits, no parity, one stop bit**.
4. Program the board from the workspace root:

```bash
# The debug probe must be accessible to the host running this command.
west flash -d build-evkc --runner linkserver
```

Reset the board after programming. Find its DHCP address in the serial output, then open `http://<board-ip>/rtl/`. Continue with [Running Lua Applications](../README.md#running-lua-applications) to create an application and configure HTTPS.

Native mDNS supports SharkCA's local HTTPS names. Start with the [automatic certificate management introduction](https://realtimelogic.com/services/SharkTrust/), then follow the [Xedge certificate manual](https://realtimelogic.com/ba/doc/en/Xedge.html#cert). [Local names and SharkCA](mdns.md) covers Zephyr-specific behavior.

The image uses the first 4 MiB of QSPI flash. LittleFS mounts the remaining 12 MiB at `/xedge` and formats it on first use. Program only the firmware partition during updates to preserve saved applications; avoid a whole-chip erase. The layout boots directly without MCUboot or an update slot.

## Memory and storage

| Region | Start address | Capacity | Use |
| --- | --- | ---: | --- |
| QSPI firmware | `0x60000000` | 4 MiB | Boot header, code, embedded resources, RAM initialization image |
| QSPI LittleFS | `0x60400000` | 12 MiB | Writable Xedge applications and configuration |
| SDRAM | `0x80000000` | 32 MiB | Xedge heap, kernel data, stacks, and network pools |
| Instruction tightly coupled memory (ITCM) | `0x00000000` | 128 KiB | Flash driver code |
| Data tightly coupled memory (DTCM) | `0x20000000` | 128 KiB | Ethernet DMA buffers and descriptors, other DTCM data |

`CONFIG_XEDGE_HEAP_SIZE=23488096` reserves 70% of SDRAM, rounded down to an 8-byte boundary. See [Heap sizing](adding-boards.md#3-place-and-size-the-xedge-heap) when changing this allocation.

BAS worker stacks come from this heap. Budget separately for Zephyr's heap, static stacks, and network pools.

Code and embedded resources are read directly from QSPI flash. Flash-writing code and the controller's constant configuration use ITCM so the filesystem can use the same flash device as the firmware. Keep the relocation in [CMakeLists.txt](../XedgeInit/CMakeLists.txt), the flash-driver relocation settings, and disabled flash/controller logging when modifying this layout.

`CONFIG_USE_DT_CODE_PARTITION` limits firmware to its 4 MiB partition. The LittleFS cache setting is 256 bytes in both Kconfig and the overlay; keep them equal when changing filesystem parameters. To adjust capacity or move a partition, update the overlay and check the resulting `zephyr.dts` and `zephyr.map` files.
