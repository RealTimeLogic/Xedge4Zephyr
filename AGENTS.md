# Working on Xedge4Zephyr

This repository runs the Barracuda App Server (BAS) and Xedge Lua application environment on Zephyr. Start with the [README](README.md) for setup and usage, the [NXP](doc/mimxrt1060-evkc.md) and [STM32](doc/stm32f746g-disco.md) guides for hardware, and [Adding a board](doc/adding-boards.md) for porting.

## Repository map

| Path | Purpose |
| --- | --- |
| `XedgeInit/src/main.c` | Heap and entropy initialization, startup, `/xedge` filesystem root, time synchronization, and `xedgeOpenAUX()` Lua binding hook |
| `XedgeInit/prj.conf` | Shared application configuration |
| `XedgeInit/boards/*.conf` | Per-board driver and resource settings |
| `XedgeInit/boards/*.overlay` | Devices, RAM placement, flash partitions, and filesystem mounts |
| `zephyr/CMakeLists.txt`, `zephyr/Kconfig` | BAS source selection and Xedge options |
| `zephyr/src/flash_zip.c`, `zephyr/xedge_resources.ld` | External-flash resource ZIP support |
| `BAS/` | Native runtime, public headers, and Zephyr port |
| `BAS-Resources/` | Lua/web sources and `build/Xedge.sh` resource generator |
| `zeth.conf`, `setup-gateway.sh` | Simulator host networking |
| `doc/` | Board setup and porting guides |

## Build workflow

Follow the [common setup](README.md#creating-a-workspace) to install dependencies and generate `BAS/examples/xedge/XedgeZip.c`. Build on Linux or WSL2. Use `EXTRA_ZEPHYR_MODULES` to select this checkout and keep separate output directories for each target:

| Target | Build directory |
| --- | --- |
| `native_sim` | `build-native` |
| `mimxrt1060_evk@C/mimxrt1062/qspi` | `build-evkc` |
| `stm32f746g_disco` | `build-stm32f746` |

Confirm application and module paths in `CMakeCache.txt` when multiple checkouts exist. Regenerate resources after changing their sources or options. Keep BAS and BAS-Resources revisions together so the Lua runtime and packaged resources match.

## Source conventions

- Inspect the root and submodule status before editing and preserve existing work. Follow [BAS/AGENTS.md](BAS/AGENTS.md) for native runtime changes and use the public headers for API definitions.
- Make BAS fixes in the authoritative SDK, then regenerate the amalgamation and copy it into the BAS submodule. Do not hand-edit generated BAS code or maintain patches over it.
- Compile one BAS amalgamation with the selected Zephyr thread, generic socket, allocator, and filesystem sources. Keep source selection in `zephyr/CMakeLists.txt`.
- Put board-specific settings in that board's `.conf` and `.overlay`. Keep the simulator's 2 MiB heap, static network, FUSE, and test entropy settings independent of hardware configurations.
- Preserve heap alignment, external-memory initialization, hardware entropy seeding, and the `xedge_ready` synchronization when adding Lua bindings or startup code.
- Keep writable files at `/xedge` separate from firmware/resources. Match LittleFS cache sizes in Kconfig and devicetree. Keep the external resource partition and linker region consistent.
- Keep DNS socket-service priority above BAS thread priorities. Current values are 2 and 5 respectively; smaller Zephyr numbers mean higher priority.

## Checking changes

Build the affected target after board changes. After shared C, CMake, Kconfig, or dependency changes, build all three targets and exercise the affected behavior in the simulator. Use hardware for peripheral-dependent changes. Inspect the resolved configuration, devicetree, and map when changing memory or storage placement.

For documentation changes, check commands against source, links and anchors, Markdown fences, and `git diff --check`; no firmware rebuild is needed. Public documentation should explain setup, use, configuration, and porting. Keep test histories, debug narratives, temporary helpers, and local results under ignored `build/` or outside the repository.

Publish dependency changes before updating parent submodule pointers so a fresh clone gets the intended sources. Keep generated resources and binaries out of commits. Do not commit or push unless requested.
