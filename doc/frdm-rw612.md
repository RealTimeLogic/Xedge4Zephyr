# Xedge on the FRDM-RW612

Build your device's web interface on the NXP Freedom FRDM-RW612 while keeping HTTPS signing keys in a separate secure environment. Xedge combines browser-based Lua development with **Automatic TLS Certificates**, so you can develop applications and manage certificate issuance and renewal from the IDE. See the [TPM overview](../README.md#tpm) for the softTPM and PSA security models.

This profile uses Ethernet, 8 MiB of external PSRAM, and the Platform Security Architecture (PSA) Crypto API through Trusted Firmware-M (TF-M). The RW612's Arm TrustZone hardware separates TF-M from the nonsecure Zephyr application. TF-M owns the persistent signing keys and performs private-key operations on Xedge's behalf, without an external security chip.

The profile uses NXP's development boot keys. Provision production boot keys and apply the required debug restrictions before deployment.

## Prepare the workspace

Use Linux or WSL2 with the tools in the [common prerequisites](../README.md#prerequisites). Create a separate workspace with [NXP ZSDK nxp-v4.4.1.1](https://github.com/nxp-zephyr/nxp-zsdk/releases/tag/nxp-v4.4.1.1) for this board's TF-M and PSRAM configuration:

```bash
source ~/zephyrproject/.venv/bin/activate
west init -m https://github.com/nxp-zephyr/nxp-zsdk.git \
  --mr nxp-v4.4.1.1 ~/rw612-psa-ws
cd ~/rw612-psa-ws
west update
python -m pip install -r zephyr/scripts/requirements.txt

# Keep Xedge and its recorded dependencies together.
mkdir -p modules
git clone --recurse-submodules https://github.com/RealTimeLogic/Xedge4Zephyr \
  modules/Xedge4Zephyr
```

NXP's PSRAM settings use all eight FlexSPI receive buffers. This release's driver incorrectly asserts that the count must be less than eight. Apply the included one-line boundary correction, which retains assertions and does not change the PSA provider:

```bash
# Run once in the new NXP workspace.
git -C zephyr apply --check \
  "$PWD/modules/Xedge4Zephyr/patches/nxp-v4.4.1.1-flexspi-rx-buffers.patch"
git -C zephyr apply \
  "$PWD/modules/Xedge4Zephyr/patches/nxp-v4.4.1.1-flexspi-rx-buffers.patch"
```

## Build

```bash
cd ~/rw612-psa-ws
# Explicitly select this workspace when other Zephyr installations exist.
export ZEPHYR_BASE="$PWD/zephyr"
XEDGE_DIR="$PWD/modules/Xedge4Zephyr"
west build -d build-xedge-rw612 -b frdm_rw612/rw612/ns \
  "$XEDGE_DIR/XedgeInit" -- -DEXTRA_ZEPHYR_MODULES="$XEDGE_DIR"
```

The same `west build` command packages the matching PSA Lua resources, builds TF-M and Xedge, and produces `build-xedge-rw612/zephyr/tfm_merged.hex`. Use this combined image for initial programming. `zephyr.hex` alone does not contain the secure firmware and bootloader.

SharkSSL remains the TLS implementation and calls the TF-M PSA provider for cryptography. The build sets `CONFIG_XEDGE_PSA=y` and disables softTPM key material. Lua applications use `ba.tpm` for named keys, signing, and certificates; the Boolean `ba.psa` is `true` in this build. Use the [TPM API documentation](https://realtimelogic.com/ba/doc/en/lua/auxlua.html#TPM) for application development. Legacy private-key APIs such as `ba.create.key` are unavailable in PSA mode, including the bundled OPC UA certificate-creation workflow.

The [board configuration](../XedgeInit/boards/frdm_rw612_rw612_ns.conf) selects Ethernet, DHCP, PSA, assertions, and the PSRAM initialization order. The [overlay](../XedgeInit/boards/frdm_rw612_rw612_ns.overlay) sets PSRAM pins, heap placement, and the filesystem. Wi-Fi is not enabled in this example.

Keep `CONFIG_ARM_NONSECURE_PREEMPTIBLE_SECURE_CALLS=n` for this target. Zephyr's TF-M dispatcher temporarily gives the calling thread cooperative priority so it cannot be switched out during a secure PSA operation. It restores the original priority when the call returns. Interrupts remain enabled; thread scheduling resumes between calls.

## Flash and connect

1. Leave **JP3 open**, connect the **J10 MCU-Link USB port** to the programming host, and connect Ethernet to a LAN with DHCP and DNS. Internet access provides clock synchronization and online certificate services.
2. Open the serial console at **115200 baud, 8 data bits, no parity, one stop bit**.
3. Install NXP LinkServer and make the debug probe available to the host running `west`. Follow the [NXP Zephyr board instructions](https://github.com/nxp-zephyr/nxp-zephyr/blob/e5a841432a6262089eabb237e6f0f2eb87f81091/boards/nxp/frdm_rw612/doc/index.rst) for the probe and boot settings.
4. Program the combined image:

```bash
# The board runner selects tfm_merged.hex for a TF-M build.
west flash -d build-xedge-rw612 --runner linkserver
```

If MCU-Link runs SEGGER J-Link firmware, use J-Link Commander instead. Select device `RW612`, SWD, and 4000 kHz. Issue these commands in the connected Commander session, replacing the image path with your combined image:

```text
r0
sleep 200
r1
connect
h
loadfile /absolute/path/to/build-xedge-rw612/zephyr/tfm_merged.hex
r0
sleep 200
r1
```

Wait for `loadfile` to finish programming and verifying before the final reset. The reset-pin sequence starts the complete secure boot chain; a core reset from the running nonsecure application is insufficient. Reopen the serial console if connecting the debugger interrupts its USB serial handle.

Find the DHCP address in the console and open `http://<board-ip>/rtl/`. Continue with [Running Lua Applications](../README.md#running-lua-applications). HTTPS initially uses a self-signed certificate created through the PSA TPM.

Native mDNS supports SharkCA's local HTTPS names. Start with the [automatic certificate management introduction](https://realtimelogic.com/services/SharkTrust/), then follow the [Xedge certificate manual](https://realtimelogic.com/ba/doc/en/Xedge.html#cert). [Local names and SharkCA](mdns.md) covers Zephyr-specific behavior. The PSA backend supplies the certificate's private-key operations.

For firmware updates, follow [Preserve keys and settings](#preserve-keys-and-settings) to retain the device's identity and encrypted configuration.

## Memory and storage

| Region | Address / flash offset | Capacity | Purpose |
| --- | --- | ---: | --- |
| Nonsecure firmware | Flash offset `0x000c0000` | 3 MiB | Xedge, Zephyr, embedded resources |
| Internal nonsecure SRAM | `0x20040000` | 960 KiB | Zephyr static data, networking, all thread stacks; includes a 256 KiB kernel heap |
| External PSRAM | `0x0c000000` | 8 MiB | 5,872,024-byte Xedge heap, placed in a NOLOAD section |
| Xedge filesystem | Flash offset `0x01000000` | 16 MiB | LittleFS mounted at `/xedge` |

The filesystem occupies flash offsets 16-32 MiB. The [secure mapping hook](../XedgeInit/tfm/frdm_rw612/storage.c) grants nonsecure access to only that additional region, using NXP's flash security helper and Security Attribution Unit (SAU) region 6. It leaves the bootloader, update slots, radio firmware, and TF-M key storage in their existing regions. The layout is shared with the overlay through [frdm_rw612_memory.h](../XedgeInit/boards/frdm_rw612_memory.h). EdgeLock 2GO would require another SAU layout.

Flash and PSRAM share the FlexSPI controller. This board allocates BAS thread stacks and their startup semaphores from Zephyr's internal kernel heap, so flash operations can reset the controller safely. Lua and application allocations retain the 70% PSRAM heap. The [secure flash integration](../XedgeInit/tfm/frdm_rw612/flash.c) preserves the controller's flash/PSRAM command table across TF-M ROM storage operations. Keep both settings when changing the memory layout.

`xedgeInitPsa()` initializes the PSA client before Xedge uses cryptography. The existing generic BAS bindings create a persistent device root on first use. This example sets `BA_PSA_GLOBALKEY_USE_DEVICE_ROOT=1`, so both `globalkey()` and `uniquekey()` use that root. Xedge still encrypts `xcfg.bin` through `globalkey()`, but the resulting configuration is device-specific. A missing root is recreated; data encrypted under the previous root cannot be recovered from its name.

Named signing keys use the default PSA key location in TF-M. Lua applications receive key identifiers and public information; the private signing-key bytes stay in the secure environment. Secret derivation uses HKDF-SHA-512. The TF-M configuration enables HKDF Extract and Expand for TLS 1.3 and PBKDF2-HMAC for SharkCA registration proofs. ChaCha20/Poly1305 and Brainpool curves are disabled to match NXP's provider configuration.

## Size the cryptographic resources

`CONFIG_XEDGE_HTTP_CONNECTIONS` defaults to 16 connection slots shared by HTTP and HTTPS, independently of the server's worker-thread count. The [TF-M configuration](../XedgeInit/tfm/frdm_rw612/config.h) allows 64 simultaneous cryptographic operations and 64 loaded key slots. These are separate limits: TLS connections retain hash operations and volatile keys, while NXP's default persistent store allows 10 assets. Size these limits for the required named keys and simultaneous connections. When increasing `CONFIG_XEDGE_MAX_THREADS` or `CONFIG_XEDGE_STACK_SIZE`, also allow enough internal kernel heap for the additional stacks. See [Configuring Xedge for Zephyr](../README.md#configuring-xedge-for-zephyr) for the shared network limits.

## Preserve keys and settings

Keep TF-M's persistent storage and the `/xedge` filesystem across firmware updates. TF-M stores the device root and named signing keys; Xedge stores encrypted settings, certificates, certificate-management state, and application files. Use the combined image with a programmer that erases only the sectors being written. Do not use a whole-chip erase when retaining device identities and settings, and keep the storage layout compatible when changing firmware.

The PSA profile derives both `globalkey()` and `uniquekey()` from the device root. Despite its name, `globalkey()` does not provide a shared fleet key in this configuration. Copying `xcfg.bin` to another device does not transfer the root needed to decrypt it. If the root is lost, creating a replacement cannot recover previously encrypted data. Losing a signing key requires a replacement key and certificate enrollment.

Before switching between softTPM and PSA or changing the key-derivation configuration, recover any required data while its original keys are available and plan its migration, or deliberately reset it. The shared Lua API does not make stored keys and encrypted data interchangeable. There is no automatic migration or fallback to softTPM. See the [BAS PSA integration guide](https://realtimelogic.com/ba/doc/en/C/reference/html/PSAIntegration.html) for provisioning and persistence details.
