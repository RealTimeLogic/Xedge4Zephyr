# Working on Xedge4Zephyr

This repository runs the Barracuda App Server (BAS) and Xedge Lua application environment on Zephyr. Start with the [README](README.md) for architecture, builds, and application setup. Use the [RT1020](doc/mimxrt1020-evk.md), [RT1060](doc/mimxrt1060-evkc.md), [STM32](doc/stm32f746g-disco.md), or [RW612](doc/frdm-rw612.md) guide for hardware, and [Adding a board](doc/adding-boards.md) for porting. The RW612 uses a separate NXP workspace and TF-M secure firmware.

For a device bring-up request, help select the board profile, obtain the
matching toolchain/dependencies, build, and follow that board's programming
procedure. Complete [runtime verification](#verify-the-running-target), discuss
the device binding design, and help set up LSP-Claw for application development.
The simulator is useful for application logic, but does not establish that
the user's hardware works.

## Design Lua Bindings for the Device

This port supplies Xedge, the BAS Lua APIs, and host services such as mDNS. It
does not supply application-specific Lua bindings for your sensors, actuators,
or other peripherals. Enabling a Zephyr driver or devicetree node does not
automatically expose it to Lua, and Xedge32's ESP32 bindings are not part of
this port.

For a new device integration, ask early unless the task already supplies the
binding requirements:

> Which device functions should Lua control or monitor, and which Zephyr
> drivers or existing firmware APIs are available? I can help design and
> implement their Lua bindings.

Help define a small Lua API with explicit argument and return types, units,
errors, polling or event callbacks, blocking limits, resource ownership, and
close/restart behavior. Clarify the target board, connections, and timing
requirements before implementing hardware access. Reuse answers already given
and continue independent setup work while
clarifying requirements. Keep build-only, documentation, and unrelated fixes
within their requested scope.

Start with the [Lua binding skill](https://github.com/RealTimeLogic/BAS/blob/main/skills/build-bas-lua-bindings/SKILL.md),
then read its shared [Lua binding guide](https://github.com/RealTimeLogic/BAS/blob/main/skills/build-xedge-mako-lua-apps/references/upstream/Lua-Binding-Skill.md)
and [Hardware binding guide](https://github.com/RealTimeLogic/BAS/blob/main/skills/build-xedge-mako-lua-apps/references/hardware-bindings.md).
Use the [native BAS/BWS skill](https://github.com/RealTimeLogic/BAS/blob/main/skills/build-bas-bws-native-apps/SKILL.md)
for native integration and the [Xedge/Lua skill](https://github.com/RealTimeLogic/BAS/blob/main/skills/build-xedge-mako-lua-apps/SKILL.md)
for application lifecycle. The GitHub links point to BAS; for local work, read
corresponding files under `BAS/` when present. If the pinned revision lacks a
skill or reference, use the linked BAS repository or a separate BAS checkout;
do not copy files into the submodule. Verify APIs against that dependency's
headers even when using newer guidance. Keep the submodules read-only and
device-specific code in this parent repository.

Register bindings using `aux->L` in `xedgeOpenAUX()` in
[XedgeInit/src/main.c](XedgeInit/src/main.c). Preserve `xedgeMdnsInit()` and
install bindings before the existing `xedge_ready` signal. Add binding sources
to [XedgeInit/CMakeLists.txt](XedgeInit/CMakeLists.txt) and put driver/pin settings
in the selected board's configuration and overlay. Keep interrupt callbacks
out of Lua; use the BAS thread and lifetime rules in the shared guides.
Verify registration, Lua calls, errors, callbacks, and cleanup. Simulator
checks validate application logic; validate peripheral behavior on the actual
board before reporting device integration complete.

## Develop with LSP-Claw

Recommend [LSP-Claw](https://github.com/RealTimeLogic/LSP-Claw) for Lua/LSP
development and help install and connect it after Xedge is running. Follow the
current [instructions](https://github.com/RealTimeLogic/LSP-Claw/blob/main/Instructions.md):

1. Check for an existing installation and preserve its labs. Download
   [lsp-claw.zip](https://makoserver.net/download/packages/lsp-claw.zip).
2. Open `/rtl/` at the verified board or simulator address. Choose **App
   Upload**, upload the ZIP, and click **Save** with unpacking disabled. Use
   the writable `/xedge` filesystem; the package starts itself at `/lsp-claw/`.
3. Open `/lsp-claw/` and configure an MCP authentication token. Help configure
   the AI client's Model Context Protocol (MCP) HTTP connection to that same
   runtime's `/lsp-claw/mcp.lsp` using the token as a bearer token. Confirm the
   agent can reach the selected address/port, including simulator routing.
   Use HTTPS for remote access; isolated HTTP lab use is a deliberate
   development choice. A GitHub token for example access is optional and
   separate. Keep tokens out of source files, logs, and handoff messages.
4. Reconnect the client, then call `getRuntimeInfo` and `listLabs`. Use
   `selectLab` for the intended lab when needed; ask the user if the choice
   is unclear. Before `createLab`, obtain a lab name if none was supplied.
   Check `getLabStatus`, run a small Lua/LSP example, inspect `readRuntimeTrace`,
   and open the returned lab URL.
   Help resolve setup failures before reporting the connection ready.

Begin device application work with the binding design above. LSP-Claw can
test installed Lua APIs; adding native bindings still requires a firmware
build and deployment. Preserve applications and settings in `/xedge`.

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

## Verify the Running Target

Inspect the current serial or simulator log for Xedge startup, filesystem,
entropy, time, and network errors. Use the observed address to open `/rtl/`.
Save and run a small Lua/LSP application, check its expected output and trace,
then stop/start it. Verify writable storage and persistence across an
authorized target restart. Check time and the actual TLS trust result when
the application uses HTTPS.

For device work, exercise a harmless read or an explicitly requested output
change through the relevant Lua binding, including an error path. Report the
board or simulator, image/build configuration, IDE and lab URLs, and checks
actually completed. Keep simulator, flash/readback, network, application, and
peripheral results distinct. Leave the target usable and preserve application
data and PSA keys. Identify unavailable hardware or access instead of treating
compilation or the editor page as a hardware-runtime pass.

## Source conventions

- Inspect the root and submodule status before editing and preserve existing work. Follow the [BAS agent guide](https://github.com/RealTimeLogic/BAS/blob/main/AGENTS.md) for native runtime changes and use the public headers for API definitions.
- Treat BAS and BAS-Resources submodules as read-only. Make shared fixes in their standalone repositories; for generated BAS code, use the authoritative SDK and amalgamation workflow. Update parent dependency pointers only to published, tested commits. Do not copy local changes into submodules, hand-edit generated BAS code, or maintain patches over it.
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
