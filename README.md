# Xedge for Zephyr

Build your device's web interface and connected services with [Xedge](https://realtimelogic.com/products/xedge/) for Zephyr. [Xedge](https://realtimelogic.com/products/xedge/) brings the [Barracuda App Server](https://realtimelogic.com/products/barracuda-application-server/) (BAS), Lua runtime, and a browser-based integrated development environment (IDE) to Zephyr. Keep hardware control in C and develop your applications in Lua from the browser, without reflashing firmware for each application change.

Xedge includes Ethernet, IPv4/IPv6, SharkSSL HTTPS, asynchronous sockets, UDP, MQTT, Modbus, OPC UA, and Internet time synchronization.

![Xedge browser IDE](https://realtimelogic.com/images/xedge/v1/Xedge.png)

*Edit and run Lua applications on the device from your browser.*

## Getting Started

Start with the Linux simulator or one of the board profiles below. Each profile includes the network, memory, and storage configuration needed to build Xedge.

Allow at least **2 MiB for the Xedge heap**, plus RAM for Zephyr and your native application. The hardware profiles allocate 70% of external RAM to Xedge. Adjust `CONFIG_XEDGE_HEAP_SIZE` to suit your application, and budget separately for [HTTP connections and network resources](#configuring-xedge-for-zephyr).

| Target | Build and run guide | Xedge heap | Application storage |
| --- | --- | ---: | ---: |
| Linux / WSL2 simulator | [Simulator setup below](#run-the-simulator) | 2 MiB | 1 MiB simulated flash |
| NXP MIMXRT1020-EVK | [Board guide](doc/mimxrt1020-evk.md) | 70% of 32 MiB SDRAM | 4 MiB QSPI flash |
| NXP MIMXRT1060-EVKC | [Board guide](doc/mimxrt1060-evkc.md) | 70% of 32 MiB SDRAM | 12 MiB QSPI flash |
| NXP FRDM-RW612 with PSA / TF-M | [Board guide](doc/frdm-rw612.md) | 70% of 8 MiB PSRAM | 16 MiB QSPI flash |
| ST STM32F746G-DISCO | [Board guide](doc/stm32f746g-disco.md) | 70% of 8 MiB SDRAM | 14 MiB QSPI flash |

1. Follow [Compiling the C Code](#compiling-the-c-code) for the simulator, RT1020, RT1060, or STM32. The FRDM-RW612 uses a separate NXP workspace; start with its [board guide](doc/frdm-rw612.md).
2. Build and start your target, then [open the browser IDE and create an application](#running-lua-applications).
3. [Set up trusted HTTPS](#automatic-certificate-management) and [authentication](#configure-https-and-access) for access to the device.

To bring Xedge to your own hardware, follow [Adding a board](doc/adding-boards.md).

## Using an AI Agent (optional)

For AI-assisted builds and porting, give your coding agent [AGENTS.md](AGENTS.md). It maps the source tree, build workflows, and integration requirements; the board guides provide target-specific programming instructions.

## Understanding the Xedge Architecture

Xedge separates your device's native firmware from the Lua applications you develop and deploy through the browser. Keep drivers and time-sensitive hardware control in C. Use Lua and Lua Server Pages (LSP) for web interfaces, application logic, and network services.

| Layer | Role in your product |
| --- | --- |
| Zephyr and your C code | Hardware drivers, networking, threads, and native device services |
| Barracuda App Server | Lua runtime, web server, protocol libraries, and SharkSSL Transport Layer Security (TLS) |
| Xedge | Browser IDE, application management, and certificate setup |
| Your Lua/LSP applications | Device interfaces and services that you can update without rebuilding the firmware |

The firmware includes Xedge's built-in Lua modules and browser interface. LittleFS holds your writable applications and settings at `/xedge`. [C-to-Lua bindings](#xedge-and-oem-integration) connect applications to your hardware; the same application model is used by Xedge32 and Xedge for ThreadX.

## Compiling the C Code

### Prerequisites

Build on Linux or in WSL2 on Windows. Follow the [Zephyr setup guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html) to install host tools, the Zephyr SDK, west, and a Python virtual environment. The common workspace uses Zephyr commit `935a5283f19197cd048dd93b40a87774a35ea9a4` (4.3.99), Zephyr SDK 1.0.0 with ARM GCC 14.3.0, west 1.5.0, and Python 3.12 or newer. For the FRDM-RW612, use the [NXP ZSDK workspace instructions](doc/frdm-rw612.md#prepare-the-workspace) instead of step 1 below.

Automatic resource generation also needs `bash`, `zip`, and a host C compiler (`cc`).

Activate the virtual environment created during Zephyr setup:

```bash
# Adjust this path if you created the environment elsewhere.
source ~/zephyrproject/.venv/bin/activate
```

For the simulator on an x86-64 Debian/Ubuntu host, install the 32-bit FUSE development libraries and multilib compilers. FUSE exposes the simulated filesystem to the host:

```bash
# native_sim builds as a 32-bit Linux application.
sudo dpkg --add-architecture i386
sudo apt-get update
sudo apt-get install -y libfuse-dev:i386 gcc-multilib g++-multilib
```

### Creating a workspace

#### 1. Initialize Zephyr

```bash
# Create a dedicated workspace and select the Zephyr revision.
west init -m https://github.com/zephyrproject-rtos/zephyr ~/xedgews
cd ~/xedgews
git -C zephyr checkout --detach 935a5283f19197cd048dd93b40a87774a35ea9a4
west update
west zephyr-export
python -m pip install -r zephyr/scripts/requirements.txt
```

If you already have a west workspace, use it instead of initializing another one. Substitute its path for `~/xedgews` in the following commands.

#### 2. Clone Xedge

```bash
# Clone Xedge, then initialize its BAS runtime and Lua/web resources.
mkdir -p ~/xedgews/modules
cd ~/xedgews/modules
git clone https://github.com/RealTimeLogic/Xedge4Zephyr
cd Xedge4Zephyr
git submodule update --init --recursive
```

The submodule command downloads the BAS and BAS-Resources revisions recorded by this repository so the runtime and packaged resources match. Cloning with `--recurse-submodules` performs the same initialization.

To update an existing checkout, run:

```bash
cd ~/xedgews/modules/Xedge4Zephyr
# Pull repository updates, then select their recorded dependency revisions.
git pull --ff-only
git submodule update --init --recursive
```

Rebuild your target after updating. Use the recorded dependency commits to keep the runtime and resources compatible.

#### 3. Embedded resources are built automatically

The firmware build packages the BAS-Resources submodule's Lua modules and browser interface into a ZIP, then generates `XedgeZip.c` inside that target's build directory. Source changes trigger regeneration. There is no manual copy step, and separate builds do not overwrite each other's resources.

Resource generation selects the Lua TPM module for your [key-protection backend](#tpm). `CONFIG_XEDGE_PSA=y` packages `BAS-Resources/src/mako/.lua/psatpm.lua` as `tpm.lua`; other builds use softTPM. A compile-time check catches a backend mismatch. JavaScript and CSS are packaged without minification.

OPC UA is enabled for all Xedge targets: the build includes the `src/opcua` Lua modules and sets `BA_ENABLE_LUA_OPCUA=1` for BAS and Xedge's native address-space support. Changes to OPC UA sources trigger resource regeneration. The bundled OPC UA certificate setup uses `ba.create.key`, which is unavailable in PSA builds; use a softTPM target for that workflow. Xedge's automatic HTTPS certificates use `ba.tpm` and support both backends.

The module always compiles BAS and Xedge with `BA_ENABLE_EMBEDDED_CA=1`. The certificate authority (CA) roots for outgoing TLS connections come from `BAS/inc/cacert.h`; the resource ZIP omits `cacert.shark`. Updating the compiled CA roots requires a firmware rebuild. Edit the resource sources rather than the generated C file.

#### 4. Build your target

For hardware, continue with the [RT1020](doc/mimxrt1020-evk.md#build), [RT1060](doc/mimxrt1060-evkc.md#build), or [STM32](doc/stm32f746g-disco.md#build) instructions. For the PSA build, use the [FRDM-RW612 guide and its separate NXP workspace](doc/frdm-rw612.md#build). For the simulator:

```bash
cd ~/xedgews
# Let pkg-config find the simulator's 32-bit FUSE library.
export PKG_CONFIG_PATH="/usr/lib/i386-linux-gnu/pkgconfig:${PKG_CONFIG_PATH}"
west build -d build-native -b native_sim modules/Xedge4Zephyr/XedgeInit -- \
  -DEXTRA_ZEPHYR_MODULES="$PWD/modules/Xedge4Zephyr"
```

`EXTRA_ZEPHYR_MODULES` adds this checkout to Zephyr's module list. If Xedge is outside the workspace, use its absolute path for both the application and module arguments. In WSL, a Windows path such as `C:\projects\Xedge4Zephyr` becomes `/mnt/c/projects/Xedge4Zephyr`.

Keep a separate build directory for each target. Zephyr loads the target's `.conf` and `.overlay` files from `XedgeInit/boards/`. Use a new build directory when switching targets or checkouts. For subsequent builds, run `west build -d build-native`, or substitute the hardware build directory.

## Run the simulator

### Connect its virtual network

The simulator uses `10.200.200.2`. The host's `zeth` virtual Ethernet interface uses `10.200.200.1` and routes simulator traffic to the host network.

Copy the interface configuration once:

```bash
# Run after west update has installed tools/net-tools.
cp ~/xedgews/modules/Xedge4Zephyr/zeth.conf ~/xedgews/tools/net-tools/zeth.conf
```

Start the interface and enable routing and Network Address Translation (NAT) before launching the simulator:

```bash
sudo ~/xedgews/tools/net-tools/net-setup.sh start
# Uses the host's default outbound interface and gateway.
sudo bash ~/xedgews/modules/Xedge4Zephyr/setup-gateway.sh
cd ~/xedgews
west build -d build-native -t run
```

The gateway script enables IPv4 forwarding, adds firewall rules, and uses routing tables 100 and 200. If your host already uses those tables, choose unused IDs in the script. To override network detection, pass `WAN_IFACE` (interface name) and `LAN_DEFAULT_GW` (gateway IPv4 address) as strings:

```bash
# Replace these example values with those shown by ip route.
sudo env WAN_IFACE=eth0 LAN_DEFAULT_GW=192.168.1.1 \
  bash ~/xedgews/modules/Xedge4Zephyr/setup-gateway.sh
```

On Linux, open [the Xedge IDE](http://10.200.200.2/rtl/). On WSL2, use one of the access methods below.

### Open it from Windows with WSL2

For browser access through a local forwarder, install `socat` and run this in a separate WSL terminal:

```bash
sudo apt-get install -y socat
# Keep this process running while using the IDE.
socat TCP-LISTEN:8090,fork,reuseaddr TCP:10.200.200.2:80
```

Open [Xedge through localhost](http://localhost:8090/rtl/). If Windows-to-WSL localhost forwarding is disabled, use `http://<wsl-ip>:8090/rtl/`; find the WSL IPv4 address with `ip -4 addr show eth0`.

For direct access to all simulator ports, add a Windows route instead. Run this in an Administrator PowerShell, replacing the example address with the WSL IPv4 address:

```powershell
# Refresh this route if WSL's address changes after a restart.
route add 10.200.200.0 mask 255.255.255.252 172.29.54.156
```

Then open [the simulator IDE](http://10.200.200.2/rtl/).

### Storage and startup

The first boot formats empty simulated flash as LittleFS and mounts it at `/xedge`. Keep the flash backing file and use the same run directory to retain applications and settings across restarts. A missing `xcfg.bin` message disappears after you save Xedge settings.

The application obtains the time from `pool.ntp.org` using SNTP. If time synchronization fails, check the host routing, DNS, and outbound UDP port 123. Accurate time is needed for certificate validation. The simulator uses test entropy and is intended for development.

## Running Lua Applications

Open `http://<device-ip>/rtl/`, using the address printed by the board's serial console or `10.200.200.2` for the simulator. The browser IDE lets you edit files and start or stop applications on the device. Files saved in the disk filesystem are stored under `/xedge` in LittleFS.

### Create your first web application

Follow [Using Xedge](https://realtimelogic.com/ba/doc/en/Xedge.html) to create, save, run, and debug Lua or LSP applications. The same browser workflow applies to every Xedge port. On Zephyr, save applications in **disk** to keep them in LittleFS at `/xedge`. Enable **Auto Start** for applications that should run after a restart.

### Configure HTTPS and access

HTTPS listens on port 443. The initial demonstration certificate causes a browser trust warning. Open the IDE over HTTP for initial setup.

Start with [Automatic Certificate Management](#automatic-certificate-management) to choose SharkTrustX or SharkCA. Both use the [TPM interface](#tpm) to protect the certificate's private key.

## Configuring Xedge for Zephyr

```bash
# Substitute your hardware build directory when configuring a board.
west build -d build-native -t menuconfig
```

Search for `XEDGE` with `/`. Save permanent settings in `XedgeInit/prj.conf` for all targets or in the target's `XedgeInit/boards/*.conf` for one board.

| Setting | Type and default | Use |
| --- | --- | --- |
| `CONFIG_XEDGE_HEAP_SIZE` | Integer bytes; 2,097,152 | BAS/Lua heap; hardware boards override this to 70% of their external RAM |
| `CONFIG_XEDGE_STACK_SIZE` | Integer bytes; 24,000 | Xedge server stack and BAS thread stack size |
| `CONFIG_XEDGE_MAX_THREADS` | Integer; 2, range 1 to 10 | BAS server worker limit |
| `CONFIG_XEDGE_HTTP_CONNECTIONS` | Integer; 16, range 4 to 65,535 | Connection slots shared by HTTP and HTTPS, including idle persistent connections; BAS enforces a minimum of its HTTP command count plus three |
| `CONFIG_XEDGE_THREAD_PRIORITY` | Integer; 5, range 2 to 15 | Server priority; smaller numbers mean higher priority |
| `CONFIG_XEDGE_ENABLE_DISK_IO` | Boolean; `y` with filesystem support | Store applications and settings in `/xedge` |
| `CONFIG_XEDGE_ENABLE_DEBUGGER` | Boolean; `y` | Lua debugger support |
| `CONFIG_XEDGE_ENABLE_ENCRYPTION_KEY` | Boolean; `y` without PSA | Include softTPM key material; supply your [product secret](#default-builds-softtpm) for deployment |
| `CONFIG_XEDGE_PSA` | Boolean; `n`, enabled by the FRDM-RW612 profile | Use TF-M secure key storage and cryptography for the TPM and TLS; automatically package the PSA Lua module |
| `CONFIG_XEDGE_ENABLE_SHARKTRUST` | Boolean; `y` | SharkTrustX certificate service integration; configure the service token in `BAS/examples/xedge/src/tokengen.c` |

All targets, including the simulator, default to 16 HTTP/HTTPS connection slots, with either softTPM or PSA. Adjust `CONFIG_XEDGE_HTTP_CONNECTIONS` independently of the worker-thread count. Active connections also consume TLS memory and Zephyr network resources. The shared configuration provides 32 network contexts; leave capacity for listening sockets, DNS, mDNS and outbound clients when increasing the HTTP connection count. On the RW612, also size the [TF-M cryptographic resources](doc/frdm-rw612.md#size-the-cryptographic-resources) for the required concurrency.

Xedge's heap is separate from Zephyr's heap, network pools, and static thread stacks. Keep the DNS socket-service thread at a higher priority than BAS threads; the shared configuration uses priorities 2 and 5 respectively. See [Adding a board](doc/adding-boards.md) for memory placement and storage configuration.

## Automatic Certificate Management

Start with the [introduction to automatic certificate management](https://realtimelogic.com/services/SharkTrust/) for the public and private trust options. Then follow the [Xedge certificate manual](https://realtimelogic.com/ba/doc/en/Xedge.html#cert) to configure SharkTrustX or SharkCA. The manual covers enrollment, renewal, portal credentials, and browser trust.

For IDE users and access control, follow the [Xedge authentication and configuration guide](https://realtimelogic.com/ba/doc/en/Xedge.html#conf).

### SharkCA and mDNS

Xedge's multicast DNS (mDNS) support is primarily for **SharkCA's local HTTPS names**. SharkCA issues the certificate; mDNS lets a browser find the device by its assigned `.local` name. Xedge manages that registration automatically. Connecting computers must also trust the zone's device CA root; resolving a name alone does not establish HTTPS trust.

Use the [SharkCA user manual](https://github.com/RealTimeLogic/SharkCA/blob/main/doc/user-manual.md) for portal and trust setup. Application-managed names are an optional use of the [mDNS Lua API](https://realtimelogic.com/ba/doc/en/lua/auxlua.html#mdns).

All Zephyr board profiles and the simulator include the native responder and resolver. Named SharkCA certificates own the single `ba.createmdns` instance; leave it available to Xedge. See [Local names and SharkCA](doc/mdns.md) for Zephyr's hostname behavior and simulator networking limits.

## TPM

Both softTPM and the PSA backend provide the Lua [Trusted Platform Module (TPM) API, `ba.tpm`](https://realtimelogic.com/ba/doc/en/lua/auxlua.html#TPM), for named keys, signing, certificates, and encryption-key derivation. [Automatic certificate management](#automatic-certificate-management) uses this API to protect the private key. Saved certificate state contains a key reference instead of a plaintext private key.

SharkSSL provides TLS in both builds. The default builds use native SharkSSL cryptography and softTPM; the FRDM-RW612 profile uses a PSA provider for cryptographic operations and persistent key storage. Neither backend requires a separate TPM chip.

### Default builds: softTPM

The standard builds use softTPM with native SharkSSL. See the [softTPM manual](https://realtimelogic.com/ba/doc/en/SoftTPM.html) for its key-derivation and protection model.

**Before production, replace the example product secret.** The host ships with example [key material](BAS/examples/xedge/src/EncryptionKey.h). Supply your own generated header through `XEDGE_SOFTTPM_KEY_HEADER`, as described in the [BAS build guide](BAS/doc/compiling.md#xedge-rtos), and provide stable per-device input through `xedgeOpenAUX()`. Follow the [softTPM integration guide](https://realtimelogic.com/ba/examples/xedge/readme.html#TPM) to protect those inputs and plan key retention. The simulator's test entropy is for development only.

### Freedom FRDM-RW612: PSA with hardware isolation

The FRDM-RW612 profile adds hardware-enforced separation between the application and its private keys. It uses the Platform Security Architecture (PSA) Crypto API through Trusted Firmware-M (TF-M). The [RW612's Arm TrustZone hardware](https://www.nxp.com/products/RW612) isolates TF-M from the Zephyr/Xedge application. TF-M manages persistent signing keys in its secure storage and performs private-key operations on the application's behalf. Xedge receives key identifiers and public information; the private signing-key bytes remain in the secure environment. See [Zephyr's TF-M key-management overview](https://docs.zephyrproject.org/latest/services/tfm/overview.html#key-management-and-derivation).

The board profile enables `CONFIG_XEDGE_PSA=y`, builds TF-M alongside Xedge, and automatically selects the matching PSA Lua module. Your application uses `ba.tpm` for private-key operations, and Xedge's automatic certificate enrollment and renewal use the same protected keys. This includes [SharkCA certificates for local networks](doc/mdns.md#configure-sharkca-certificates). Follow the [FRDM-RW612 guide](doc/frdm-rw612.md#build) to build and flash `tfm_merged.hex`, which contains the bootloader, secure firmware, and Xedge.

By default, this profile encrypts settings with a persistent device root stored by TF-M. Preserve that root, the signing keys, and `/xedge` across updates. See [Preserve keys and settings](doc/frdm-rw612.md#preserve-keys-and-settings) before changing the storage layout or key-protection backend.

The example uses NXP's development boot keys. Provision production boot keys and apply debug restrictions for deployment. The [BAS PSA integration guide](https://realtimelogic.com/ba/doc/en/C/reference/html/PSAIntegration.html) explains the provider requirements when adapting this design to another target.

#### Share encrypted settings across a product family

You can configure `ba.tpm.globalkey()` to derive the same encryption keys on every device in a product family. This lets those devices decrypt shared configuration data while `ba.tpm.uniquekey()` remains device-specific. It requires a firmware configuration change and provisioning a shared secret before Xedge starts.

1. In [zephyr/CMakeLists.txt](zephyr/CMakeLists.txt), change `BA_PSA_GLOBALKEY_USE_DEVICE_ROOT=1` to `BA_PSA_GLOBALKEY_USE_DEVICE_ROOT=0` inside the `CONFIG_XEDGE_PSA` block. The integer option accepts `0` for a separately provisioned global root or `1` for the automatic device root. This build definition applies to both BAS and the Xedge host.
2. Generate one cryptographically random **32-byte secret** for the product family. Install that same secret in each device's TF-M persistent key store as global-root key ID `1`, using the provider's secure provisioning process or a trusted native `psa_import_key()` integration. Use the key type, algorithm, usage permissions, and storage location specified in the [shared-root provisioning instructions](https://realtimelogic.com/ba/doc/en/C/reference/html/PSAIntegration.html#psa-shared-root). Keep the secret out of application source and Lua files. BAS requires this root to exist; it does not generate a shared root automatically.
3. Leave key ID `2` for BAS to generate an independent device root on each device. Rebuild and flash using the [RW612 board guide](doc/frdm-rw612.md#build), with the global root provisioned before the first normal Xedge startup. Preserve both roots and `/xedge` during subsequent updates.
4. Use the same derivation settings, string key name, and requested length in bytes (an integer) for each `globalkey()` call that must match across devices. This profile uses HKDF-SHA-512. Verify that two provisioned devices can read the shared encrypted data, including after a restart.

Xedge's `xcfg.bin` encryption also uses `globalkey()`. Recover existing settings while their original root is available and migrate them before switching; changing roots does not re-encrypt saved data automatically. Sharing the global root does not copy TLS signing keys or certificates between devices.

## Embedding Lua Apps and LittleFS Storage

Xedge's built-in modules and browser interface are embedded in the firmware by the [resource build](#3-embedded-resources-are-built-automatically). Edit their sources in `BAS-Resources`; rebuild the firmware to update the embedded ZIP.

Use the browser IDE to save your applications in `/xedge`, the writable LittleFS filesystem. These files and Xedge's saved settings survive restarts. To retain them during a firmware update, program only the firmware and resource regions. Follow your board guide's partition and erase instructions; the RW612 also requires preserving [TF-M key storage](doc/frdm-rw612.md#preserve-keys-and-settings).

## Xedge and OEM Integration

Keep device drivers and time-sensitive control in your native firmware, and expose the operations your applications need through Lua.

For shared host options and production integration, see the [standalone Xedge build guide](https://realtimelogic.com/ba/examples/xedge/readme.html).

Add C-to-Lua bindings in `xedgeOpenAUX()` in [XedgeInit/src/main.c](XedgeInit/src/main.c) to expose sensors, device controls, or existing firmware services. Keep the startup synchronization in that function. Examples are provided in [led.c](BAS/examples/xedge/src/led.c) and [AsynchLua.c](BAS/examples/xedge/src/AsynchLua.c).

Follow the [Lua binding tutorial](https://tutorial.realtimelogic.com/Lua-Bindings.lsp) to create bindings, then use them from your Xedge applications. The [online Lua tutorials](https://tutorial.realtimelogic.com/) cover web applications, network clients, messaging, and other BAS APIs.

## Xedge on Other Platforms

Xedge brings the same browser-based Lua and LSP development workflow to several
embedded platforms. Explore the port that fits your hardware and RTOS.

- [Xedge32](https://github.com/RealTimeLogic/Xedge32): Xedge for ESP32 devices.
- [Xedge for ThreadX](https://github.com/RealTimeLogic/Xedge4ThreadX): Xedge integrated with ThreadX, NetX Duo, and FileX.
- [Xedge for FreeRTOS](https://realtimelogic.com/downloads/bas/rt1020/): Ready-to-run project for the NXP i.MX RT1020 evaluation board.
- [Xedge for NuttX](https://nuttx.apache.org/docs/latest/applications/examples/xedge_demo/index.html): Example and build instructions in the Apache NuttX documentation.
- [Generic Xedge port](https://github.com/RealTimeLogic/BAS/blob/main/doc/compiling.md#xedge-rtos): BAS build instructions for integrating Xedge with your own RTOS target.

## Licensing and Dependencies

See this repository's [LICENSE](LICENSE), the [BAS license](BAS/LICENSE),
and the [BAS-Resources license](BAS-Resources/LICENSE). Other dependencies
retain their own licenses and notices.

## Export restrictions

This distribution includes cryptographic software. The country in
which you currently reside may have restrictions on the import,
possession, use, and/or re-export to another country, of
encryption software. BEFORE using any encryption software, please
check your country's laws, regulations and policies concerning the
import, possession, or use, and re-export of encryption software, to
see if this is permitted. See https://www.wassenaar.org/ for more
information.

The U.S. Government Department of Commerce, Bureau of Industry and
Security (BIS), has classified this software as Export Commodity
Control Number (ECCN) 5D002.C.1, which includes information security
software using or performing cryptographic functions with asymmetric
algorithms. The form and manner of this distribution makes it
eligible for export under the License Exception ENC Technology
Software Unrestricted (TSU) exception (see the BIS Export
Administration Regulations, Section 740.13) for both object code and
source code.

The following page provides details on the included cryptographic
software: https://github.com/RealTimeLogic/SharkSSL
