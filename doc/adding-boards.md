# Adding a board to Xedge for Zephyr

A board port brings Xedge's browser IDE and Lua applications to your hardware. Start with an existing Zephyr board definition, then connect Xedge to its memory, network, entropy, and storage drivers through an application configuration and devicetree overlay.

Complete the [workspace and resource setup](../README.md#creating-a-workspace) first. Repository layout and source conventions are in [AGENTS.md](../AGENTS.md).

## 1. Select the board and memory layout

Find the board in your Zephyr checkout and identify its full target string, including revision and qualifiers. For example, `mimxrt1060_evk@C/mimxrt1062/qspi` selects the EVKC and its QSPI flash. If you need a new Zephyr board definition, follow the [Zephyr board-porting guide](https://docs.zephyrproject.org/latest/hardware/porting/board_porting.html) first.

Choose a board with enough RAM for the Xedge heap plus Zephyr's kernel, network pools, and static stacks. The default Xedge heap alone is 2 MiB. Check the usable capacity of each RAM bank, external-memory initialization, and which memory banks the Ethernet DMA controller can access.

Plan separate storage for firmware, embedded Xedge resources, and writable applications:

| Example | Firmware and resources | Xedge heap | Writable files |
| --- | --- | --- | --- |
| [MIMXRT1020-EVK](mimxrt1020-evk.md#memory-and-storage) | Code and resources in QSPI; flash driver and controller configuration in internal instruction RAM | 70% of 32 MiB SDRAM | Separate 4 MiB QSPI partition |
| [MIMXRT1060-EVKC](mimxrt1060-evkc.md#memory-and-storage) | Code and resources in QSPI; flash driver relocated to internal instruction RAM | 70% of 32 MiB SDRAM | Separate 12 MiB QSPI partition |
| [STM32F746G-DISCO](stm32f746g-disco.md#memory-and-storage) | Code in internal flash; resources read through the QSPI driver | 70% of 8 MiB SDRAM | Separate 14 MiB QSPI partition |

Keep an address table with the start, capacity, and purpose of each region. The three profiles above boot directly without update slots. Include any bootloader or update slots your product needs; the [RW612 guide](frdm-rw612.md) covers a separate layout with TF-M secure firmware and key storage.

## 2. Add a board configuration and overlay

Create `.conf` and `.overlay` files in [XedgeInit/boards](../XedgeInit/boards/), following the board naming rules for your Zephyr version. Copy the closest existing pair as a starting point. The NXP filename includes its qualifiers as underscores; its revision is selected in the build command.

Use the `.conf` file for:

- Network drivers, DHCP, and DNS servers obtained through DHCP.
- The hardware entropy driver, with `CONFIG_TEST_RANDOM_GENERATOR=n`.
- Xedge heap size, thread stacks, Zephyr heap, and network pool sizes.
- Flash, memory-controller, and filesystem drivers.
- The output format and programmer requirements.

Use the `.overlay` for memory regions, devices, pins, flash partitions, and the LittleFS mount at `/xedge`. Keep simulator-only FUSE, simulated flash, static addresses, and test entropy in the simulator configuration.

Match `CONFIG_FS_LITTLEFS_CACHE_SIZE` to the overlay's `cache-size` so Zephyr allocates enough cache memory for the configured number of open files. The existing hardware configurations use 256 bytes for both.

Keep settings specific to the new board in its files. Shared settings belong in [XedgeInit/prj.conf](../XedgeInit/prj.conf) and Xedge option definitions in [zephyr/Kconfig](../zephyr/Kconfig).

Keep native mDNS enabled on every target. The shared configuration supplies the following settings; board profiles inherit them:

| Setting | Shared value | Purpose |
| --- | --- | --- |
| `CONFIG_MDNS_RESPONDER`, `CONFIG_MDNS_RESOLVER` | Boolean `y` | Publish the device's address records and resolve other `.local` hosts |
| `CONFIG_NET_HOSTNAME_ENABLE`, `CONFIG_NET_HOSTNAME_DYNAMIC` | Boolean `y` | Let Lua select the native device hostname |
| `CONFIG_NET_HOSTNAME`, `CONFIG_NET_HOSTNAME_UNIQUE` | String `"xedge"`, Boolean `y` | Default name with the interface's link-address suffix |
| `CONFIG_NET_HOSTNAME_MAX_LEN` | Integer `63` characters | Accept a full DNS host label |
| `CONFIG_DNS_RECONFIGURE_CLEANUP` | Boolean `n` | Preserve multicast resolver addresses across DHCP updates |
| `CONFIG_NET_SOCKETS_SERVICE_STACK_SIZE` | Integer `4096` bytes | Stack space for native DNS and mDNS processing |
| `CONFIG_NET_SOCKETS_SERVICE_THREAD_PRIO` | Integer `2` | Process DNS replies above the BAS threads at priority 5 |

[mdns.c](../XedgeInit/src/mdns.c) registers `ba.createmdns` through `xedgeOpenAUX()` after network initialization. Keep this registration before the `xedge_ready` semaphore is released. The binding uses Zephyr's hostname API; `BA_ENABLE_MDNS=0` and `BA_ENABLE_MDNS_LUA=0` in [zephyr/CMakeLists.txt](../zephyr/CMakeLists.txt) disable the separate BAS implementation, not the Zephyr-backed Lua API. Zephyr has one device hostname, so there is one active Lua instance, used by Xedge when named SharkCA certificates are enabled. See [Local names and SharkCA](mdns.md) for ownership, cleanup, and certificate setup.

For DHCP targets, use `CONFIG_DNS_SERVER_IP_ADDRESSES=n`. After network setup, `xedgeMdnsInit()` adds multicast resolver addresses after the DHCP DNS server so ordinary lookups use unicast DNS. Keep `CONFIG_DNS_RECONFIGURE_CLEANUP=n` so DHCP updates preserve those multicast addresses. The shared configuration also gives the socket-service thread a 4096-byte stack for native mDNS processing.

For a static network configuration, supply the unicast DNS server through `CONFIG_DNS_SERVER_IP_ADDRESSES=y` and `CONFIG_DNS_SERVER1`, as the simulator does. Keep unicast DNS ahead of the IPv4 and IPv6 multicast resolver entries. Retain IPv4 IGMP and IPv6 MLD multicast membership support selected by native mDNS.

The build checks whether the selected Zephyr source passes the correct context between its paired DNS resolver and responder. When needed, [Dns.cmake](../zephyr/Dns.cmake) compiles a corrected copy in the build directory, leaving the Zephyr checkout unchanged. Sources that already pass the paired context are used as supplied.

## 3. Place and size the Xedge heap

Set `CONFIG_XEDGE_HEAP_SIZE` to an integer number of bytes, aligned to 8 bytes. To reserve 70% of a RAM bank, use:

```text
heap_bytes = floor((ram_bytes * 70 / 100) / 8) * 8
```

BAS allocates Lua objects, TLS state, and worker stacks from this heap. Zephyr's heap, static stacks, and network pools need their own space. Account for each memory bank separately: unused SDRAM cannot compensate for an overflowing internal SRAM region.

If the heap needs a separate RAM bank, select its Zephyr linker memory region with an optional `xedge,heap` chosen phandle, as in the STM32 overlay. Ensure external RAM is initialized before `init_dlmalloc()` uses it. Use a `NOLOAD` section when the controller initializes after normal BSS clearing. Preserve the 8-byte alignment and capacity assertion in [main.c](../XedgeInit/src/main.c).

The [firmware configuration table](../README.md#configuring-xedge-for-zephyr) explains Xedge's heap, stack, thread-count, HTTP connection capacity, and priority controls. All targets inherit `CONFIG_XEDGE_HTTP_CONNECTIONS=16`; override it in the board's `.conf` when needed. Account for active TLS memory, Zephyr socket/context limits and packet pools, while reserving network resources for listeners and outbound traffic. Zephyr gives smaller priority numbers higher priority. Keep the DNS socket-service thread above BAS threads so it can process DNS replies during TLS computation; the shared configuration uses socket-service priority 2 and Xedge base priority 5.

## 4. Store resources in external flash when needed

The default build embeds the generated `XedgeZip.c` resource ZIP alongside the firmware. To place it in another flash device, follow the STM32 example:

1. Enable `CONFIG_XEDGE_RESOURCE_FLASH=y` and `CONFIG_FLASH_MAP=y`.
2. Create a fixed-partition node labelled `xedge_resources` and a linker memory region named `XEDGE_RESOURCES`. Give both the same flash location and capacity, accounting for the chip's programming base address.
3. Keep the resource partition separate from writable files. The existing [resource linker script](../zephyr/xedge_resources.ld) places the ZIP and checks its presence and starting address. The linker enforces the region's capacity.
4. Use the [flash ZIP reader](../zephyr/src/flash_zip.c), which reads through `flash_area_read()` and retains its reader and descriptor for the server's lifetime. The flash driver must serialize resource reads with filesystem writes.
5. Generate HEX or ELF when firmware and resources occupy distant addresses. Configure the programmer and external loader to write both regions.

When code executes from the same flash chip used for writable files, configure the flash driver and any required code/data relocation so erase and write operations can run without reading the busy flash. Check interrupt, logging, and cache requirements in the selected Zephyr driver. The NXP configurations demonstrate this arrangement: Zephyr relocates the FlexSPI driver code, and [XedgeInit/CMakeLists.txt](../XedgeInit/CMakeLists.txt) also relocates the controller's constant configuration.

## 5. Build and run

Use a separate build directory and pass the Xedge checkout as an extra Zephyr module:

```bash
cd ~/xedgews
# Replace the target and build directory with those for your board.
BOARD_TARGET=stm32f746g_disco
BUILD_DIR=build-stm32f746
XEDGE_DIR="$PWD/modules/Xedge4Zephyr"
west build -d "$BUILD_DIR" -b "$BOARD_TARGET" "$XEDGE_DIR/XedgeInit" -- \
  -DEXTRA_ZEPHYR_MODULES="$XEDGE_DIR"
```

Check the configure output for the intended `.conf` and `.overlay` files. The generated files explain how Zephyr applied your settings:

| File | What it tells you |
| --- | --- |
| `CMakeCache.txt` | Application/module paths, selected board, and compiler |
| `zephyr/.config` | Resolved Kconfig values |
| `zephyr/zephyr.dts` | Devices, RAM banks, flash partitions, and filesystem mount |
| `zephyr/zephyr.map` | Memory use and placement of code, heaps, stacks, DMA buffers, and resources |
| `zephyr/zephyr.elf` | Load addresses, runtime addresses, and section attributes |
| `zephyr/zephyr.hex` or `.bin` | Image for the programmer |

Ensure the image fits its firmware and resource partitions and excludes the writable filesystem. Program it with the board's supported runner and any required external loader. Connect Ethernet, monitor the console, and open `http://<board-ip>/rtl/`. Follow [Running Lua Applications](../README.md#running-lua-applications) to create and save an application. Preserve the filesystem partition when updating firmware.

Check the resolved `.config` for the shared mDNS settings and the linked image for `xedgeMdnsInit` and Zephyr's native responder. On the board, resolve its `.local` name from the same LAN and resolve a peer from Lua. For a named SharkCA registration, confirm that Xedge owns the single instance and that the issued certificate matches the assigned name.

## 6. Add the board guide

Create `doc/<board-name>.md` using the same structure as the existing board guides: Build, Flash and connect, and Memory and storage. Include the exact board target, programming tool and loader, console settings, and browser connection steps. Link to the README for common application setup and to [Local names and SharkCA](mdns.md) for mDNS and private certificates. Add the board to the README target table. Keep development logs and test reports outside the public documentation.
