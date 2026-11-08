# Working on Xedge4Zephyr

This repository runs the Barracuda App Server (BAS) and Xedge Lua application environment on Zephyr. Start with the [README](README.md) for architecture, builds, and application setup. Use the [RT1020](doc/mimxrt1020-evk.md), [RT1060](doc/mimxrt1060-evkc.md), [STM32](doc/stm32f746g-disco.md), or [RW612](doc/frdm-rw612.md) guide for hardware, and [Adding a board](doc/adding-boards.md) for porting. The RW612 uses a separate NXP workspace and TF-M secure firmware.

## Repository map

| Path | Purpose |
| --- | --- |
| `XedgeInit/src/main.c` | Heap and entropy initialization, startup, `/xedge` filesystem root, time synchronization, and `xedgeOpenAUX()` Lua binding hook |
| `XedgeInit/src/mdns.c` | Single-instance `ba.createmdns` binding to Zephyr's native hostname and mDNS responder, also used by named SharkCA certificates |
| `XedgeInit/CMakeLists.txt` | Application entry point and RAM relocation of FlexSPI controller constants for XIP targets |
| `XedgeInit/prj.conf` | Shared application configuration |
| `XedgeInit/boards/*.conf` | Per-board driver and resource settings |
| `XedgeInit/boards/*.overlay` | Devices, RAM placement, flash partitions, and filesystem mounts |
| `zephyr/CMakeLists.txt`, `zephyr/Kconfig` | BAS source selection and Xedge options |
| `zephyr/Resources.cmake`, `scripts/build_resources.py` | Per-build resource generation and PSA/softTPM selection |
| `scripts/flash_rt1020.py` | RT1020 full-system-reset flashing, complete image readback, and boot |
| `zephyr/Dns.cmake` | Build-local compatibility fix for Zephyr DNS dispatcher callback contexts |
| `XedgeInit/tfm/frdm_rw612/` | TF-M configuration and the board-specific filesystem security mapping |
| `zephyr/src/flash_zip.c`, `zephyr/xedge_resources.ld` | External-flash resource ZIP support |
| `BAS/` | Native runtime, public headers, and Zephyr port |
| `BAS-Resources/` | Lua/web sources and `build/Xedge.sh` resource generator |
| `zeth.conf`, `setup-gateway.sh` | Simulator host networking |
| `doc/` | Board setup and porting guides |

## Build workflow

Follow the [common setup](README.md#creating-a-workspace) to install dependencies. CMake generates resources automatically in each build directory. Build on Linux or WSL2. Use `EXTRA_ZEPHYR_MODULES` to select this checkout and keep separate output directories for each target:

| Target | Build directory |
| --- | --- |
| `native_sim` | `build-native` |
| `mimxrt1020_evk` | `build-rt1020` |
| `mimxrt1060_evk@C/mimxrt1062/qspi` | `build-evkc` |
| `stm32f746g_disco` | `build-stm32f746` |
| `frdm_rw612/rw612/ns` | `build-xedge-rw612` in the separate NXP workspace; see [board guide](doc/frdm-rw612.md) |

Confirm application and module paths in `CMakeCache.txt` when multiple checkouts exist. Keep BAS and BAS-Resources revisions together so the Lua runtime and packaged resources match. The resource generator verifies the selected TPM module and adds a compile-time backend guard.

Keep OPC UA enabled in both resource generation and native compilation:
`scripts/build_resources.py` selects `BA_ENABLE_LUA_OPCUA=1`,
`zephyr/Resources.cmake` tracks `BAS-Resources/src/opcua`, and
`zephyr/CMakeLists.txt` supplies the same feature macro to BAS and Xedge.
The existing Xedge host then registers the native `opcua_ns0` module. Do not
add a second registration or edit either submodule to enable this feature.
Do not infer PSA certificate-workflow compatibility from resource inclusion;
its legacy key API remains a separate integration issue.

For RT1020 flashing, use `scripts/flash_rt1020.py --probe <exact-probe-id> --image <zephyr.bin>` as described in the [board guide](doc/mimxrt1020-evk.md#flash-and-connect). It follows the ThreadX method: connect under reset, explicitly reset with `SYSRESETREQ`, mask interrupts with `PRIMASK`, and disable SysTick before the RAM flash algorithm. Use sector erase to preserve `/xedge`, then another full system reset, ROM initialization, and full image readback at the application entry before resuming. Do not substitute generic `west flash` or `pyocd flash` commands that use core-only `VECTRESET`. Keep SW8 in normal QSPI boot mode for routine updates; use ROM recovery only if debugger access remains unavailable after checking power, cables, and SWD jumpers. The script's `--verify-only` option also resets and boots the board.

## Source conventions

- Inspect the root and submodule status before editing and preserve existing work. Follow [BAS/AGENTS.md](BAS/AGENTS.md) for native runtime changes and use the public headers for API definitions.
- Make BAS fixes in the authoritative SDK, then regenerate the amalgamation and copy it into the BAS submodule. Do not hand-edit generated BAS code or maintain patches over it.
- Compile one BAS amalgamation with the selected Zephyr thread, generic socket, allocator, and filesystem sources. Keep source selection in `zephyr/CMakeLists.txt`.
- Put board-specific settings in that board's `.conf` and `.overlay`. Keep the simulator's 2 MiB heap, static network, FUSE, and test entropy settings independent of hardware configurations.
- Preserve heap alignment, external-memory initialization, hardware entropy seeding, and the `xedge_ready` synchronization when adding Lua bindings or startup code.
- Keep writable files at `/xedge` separate from firmware/resources. Match LittleFS cache sizes in Kconfig and devicetree. Keep the external resource partition and linker region consistent.
- Keep DNS socket-service priority above BAS thread priorities. Current values are 2 and 5 respectively; smaller Zephyr numbers mean higher priority.
- On RW612, keep `CONFIG_ARM_NONSECURE_PREEMPTIBLE_SECURE_CALLS=n`. Use Zephyr's TF-M dispatcher to prevent thread switches during PSA calls while leaving interrupts enabled. Keep this setting board-specific.
- Keep the shared native mDNS responder, resolver, and dynamic hostname settings enabled for every board and the simulator. Register `ba.createmdns` before releasing `xedge_ready`; keep the separate BAS responder disabled. There is one active Lua instance per device, owned by Xedge for named SharkCA certificates. See [Local names and SharkCA](doc/mdns.md) for the public API and setup, and [Adding a board](doc/adding-boards.md#2-add-a-board-configuration-and-overlay) for DNS ordering, stack sizing, and dispatcher compatibility.

## Checking changes

Build the affected target after board changes. After shared C, CMake, Kconfig, or dependency changes, build all listed targets and exercise the affected behavior in the simulator. Use hardware for peripheral-dependent changes. Inspect the resolved configuration, devicetree, and map when changing memory or storage placement.

For documentation changes, check commands against source, links and anchors, Markdown fences, and `git diff --check`; no firmware rebuild is needed. Public documentation should explain setup, use, configuration, and porting. Keep test histories, debug narratives, temporary helpers, and local results under ignored `build/` or outside the repository.

Publish dependency changes before updating parent submodule pointers so a fresh clone gets the intended sources. Keep generated resources and binaries out of commits. Do not commit or push unless requested.
