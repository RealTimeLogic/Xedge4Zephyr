# Xedge on the STM32F746G-DISCO

Give the STM32F746G Discovery board a web interface you can develop and update from your browser. This Ethernet profile reserves 70% of the board's 8 MiB accessible SDRAM for Xedge and uses its 16 MiB Quad-SPI (QSPI) flash for embedded resources and application files. The board is also sold as 32F746GDISCOVERY. Its LCD and touch controller are disabled in this profile.

## Build

Complete the [workspace and resource setup](../README.md#creating-a-workspace), then run these commands from your Zephyr workspace:

```bash
source ~/zephyrproject/.venv/bin/activate
cd ~/xedgews
# Keep this board's build separate from other targets.
west build -d build-stm32f746 -b stm32f746g_disco \
  modules/Xedge4Zephyr/XedgeInit -- \
  -DEXTRA_ZEPHYR_MODULES="$PWD/modules/Xedge4Zephyr"
```

Zephyr loads [the board configuration](../XedgeInit/boards/stm32f746g_disco.conf) and [overlay](../XedgeInit/boards/stm32f746g_disco.overlay). Rebuild after source changes with `west build -d build-stm32f746`.

Use **`build-stm32f746/zephyr/zephyr.hex`** for programming. It contains the internal-flash firmware and external-flash resource ZIP at their correct addresses. A flat BIN would include the large gap between those addresses, so this target disables BIN output. The build also produces `zephyr.elf` and `zephyr.map`.

## Flash and connect

1. Install [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html). STM32CubeIDE is not required for this workflow.
2. Connect the board's ST-LINK USB connector to the programming host. Connect Ethernet to a LAN with DHCP and DNS. Internet access allows clock synchronization and online certificate services.
3. Open the ST-LINK virtual serial port at **115200 baud, 8 data bits, no parity, one stop bit**.
4. Program the HEX file with the external loader **`N25Q128A_STM32F746G-DISCO.stldr`**. This writes both internal flash and the QSPI resource partition.

### Program from Linux

Make STM32CubeProgrammer's command-line executable available on `PATH` and ensure the host can access the USB probe. The Zephyr runner selects the external loader:

```bash
# Run from the Zephyr workspace root.
west flash -d build-stm32f746 --runner stm32cubeprogrammer
```

### Program from Windows after a WSL build

Copy `zephyr.hex` to a Windows directory. Open it in STM32CubeProgrammer, connect through ST-LINK/SWD, select the external loader above, and download with verification enabled. Reset the board after programming.

Alternatively, run this PowerShell command from the directory containing `zephyr.hex`:

```powershell
# Adjust the installation path if needed. Programs both regions and resets.
$programmer = 'C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin'
& "$programmer\STM32_Programmer_CLI.exe" -c port=SWD mode=UR `
  -el "$programmer\ExternalLoader\N25Q128A_STM32F746G-DISCO.stldr" `
  -d .\zephyr.hex -v -hardRst
```

Find the board's DHCP address in the serial output and open `http://<board-ip>/rtl/`. Continue with [Running Lua Applications](../README.md#running-lua-applications) to create an application and configure HTTPS. The board's display remains unused.

Native mDNS supports SharkCA's local HTTPS names. Start with the [automatic certificate management introduction](https://realtimelogic.com/services/SharkTrust/), then follow the [Xedge certificate manual](https://realtimelogic.com/ba/doc/en/Xedge.html#cert). [Local names and SharkCA](mdns.md) covers Zephyr-specific behavior.

## Memory and storage

| Region | Start address | Capacity | Use |
| --- | --- | ---: | --- |
| Internal flash | `0x08000000` | 1 MiB | All executable code, constants, RAM initialization image |
| Internal SRAM | `0x20010000` | 256 KiB | Kernel data, static stacks, network pools, 32 KiB Zephyr heap |
| Data tightly coupled memory (DTCM) | `0x20000000` | 64 KiB | Ethernet DMA buffers and descriptors |
| SDRAM | `0xc0000000` | 8 MiB | Xedge heap and BAS worker stacks |
| QSPI resource partition | `0x90000000` | 2 MiB | Embedded Xedge Lua and web resource ZIP |
| QSPI LittleFS partition | `0x90200000` | 14 MiB | Writable Xedge applications and configuration |

`CONFIG_XEDGE_HEAP_SIZE=5872024` reserves 70% of SDRAM, rounded down to an 8-byte boundary. See [Heap sizing](adding-boards.md#3-place-and-size-the-xedge-heap) when changing this allocation.

The overlay's `xedge,heap` entry places the heap in SDRAM. Its `NOLOAD` section prevents startup from clearing external RAM before the memory controller is ready. The application checks the SDRAM controller before initializing the allocator.

`CONFIG_XEDGE_RESOURCE_FLASH=y` stores the resource ZIP in QSPI. At runtime, resources are read through the flash driver, which serializes these reads with LittleFS writes. All executable code remains in internal flash. Programming internal flash alone is insufficient because Xedge also needs the resource ZIP.

LittleFS mounts its separate 14 MiB partition at `/xedge` and formats it on first use. When updating firmware, program only the sectors covered by `zephyr.hex`; avoid a whole-chip erase to retain applications and settings.

The board configuration keeps network pools in internal SRAM: 64 packet and data buffers per direction, 512 bytes per data buffer, and a 32 KiB Zephyr heap. It uses a 4,096-byte main initialization stack, separate from the 24,000-byte Xedge server stack. Keep `CONFIG_FS_LITTLEFS_CACHE_SIZE=256` equal to the overlay's cache size when adjusting storage settings.

For connectors, jumpers, and peripheral details, see the [Zephyr STM32F746G Discovery guide](https://docs.zephyrproject.org/latest/boards/st/stm32f746g_disco/doc/index.html).
