# Xedge with PSA on the FRDM-RW612

Build browser-managed applications on the NXP Freedom FRDM-RW612 with hardware-backed protection for HTTPS private keys. Enable **Automatic TLS Certificates** to use that protection with automatic certificate issuance and renewal. See the README's [private-key protection overview](../README.md#protect-your-devices-tls-private-key) for the softTPM and PSA security models and certificate setup.

This example runs Xedge over the board's Ethernet connection, uses external PSRAM for the Lua heap, and calls the Platform Security Architecture (PSA) Crypto API through Trusted Firmware-M (TF-M). TF-M runs separately from the nonsecure Zephyr application and owns the persistent signing keys. No external security chip or custom cryptography provider is required.

This is a development configuration. Hardware bring-up, key persistence across power cycles, and concurrent filesystem/TLS operation must be verified when the board is available. The example retains NXP's development boot keys; production boot provisioning and debug restrictions are separate requirements. It does not establish FIPS validation.

## Prepare the workspace

Use Linux or WSL2 with the tools in the [common prerequisites](../README.md#prerequisites). Use a separate workspace for [NXP ZSDK nxp-v4.4.1.1](https://github.com/nxp-zephyr/nxp-zsdk/releases/tag/nxp-v4.4.1.1), which includes the RW612 TF-M/PSRAM fixes:

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

The build generates the PSA resources automatically, builds TF-M and Xedge, and produces `build-xedge-rw612/zephyr/tfm_merged.hex`. Use this combined image for initial programming. `zephyr.hex` alone does not contain the secure firmware and bootloader.

The [board configuration](../XedgeInit/boards/frdm_rw612_rw612_ns.conf) selects Ethernet, DHCP, PSA, assertions, and the PSRAM initialization order. The [overlay](../XedgeInit/boards/frdm_rw612_rw612_ns.overlay) sets PSRAM pins, heap placement, and the filesystem. Wi-Fi is not enabled in this example.

## Connect the board

1. Connect the MCU-Link USB port to the programming host and Ethernet to a LAN with DHCP and DNS. Internet access provides clock synchronization and online certificate services.
2. Open the serial console at **115200 baud, 8 data bits, no parity, one stop bit**.
3. Install NXP LinkServer and make the debug probe available to the host running `west`. Follow the [NXP Zephyr board instructions](https://github.com/nxp-zephyr/nxp-zephyr/blob/e5a841432a6262089eabb237e6f0f2eb87f81091/boards/nxp/frdm_rw612/doc/index.rst) for the probe and boot settings.
4. Program the combined image:

```bash
# The board runner selects tfm_merged.hex for a TF-M build.
west flash -d build-xedge-rw612 --runner linkserver
```

Find the DHCP address in the console and open `http://<board-ip>/rtl/`. Continue with [Use Xedge](../README.md#use-xedge). HTTPS initially uses a self-signed certificate created through the PSA TPM.

Ordinary firmware updates must preserve TF-M's persistent storage and Xedge's filesystem. Do not use a whole-chip erase when testing persistence.

## Memory and keys

| Region | Address / flash offset | Capacity | Purpose |
| --- | --- | ---: | --- |
| Nonsecure firmware | Flash offset `0x000c0000` | 3 MiB | Xedge, Zephyr, embedded resources |
| Internal nonsecure SRAM | `0x20040000` | 960 KiB | Zephyr static data, networking, stacks |
| External PSRAM | `0x0c000000` | 8 MiB | 5,872,024-byte Xedge heap, placed in a NOLOAD section |
| Xedge filesystem | Flash offset `0x01000000` | 16 MiB | LittleFS mounted at `/xedge` |

The filesystem occupies flash offsets 16-32 MiB. The [secure mapping hook](../XedgeInit/tfm/frdm_rw612/storage.c) grants nonsecure access to only that additional region, using NXP's flash security helper and Security Attribution Unit (SAU) region 6. It leaves the bootloader, update slots, radio firmware, and TF-M key storage in their existing regions. The layout is shared with the overlay through [frdm_rw612_memory.h](../XedgeInit/boards/frdm_rw612_memory.h). EdgeLock 2GO would require another SAU layout.

`xedgeInitPsa()` initializes the PSA client before Xedge uses cryptography. The existing generic BAS bindings create a persistent device root on first use. This example sets `BA_PSA_GLOBALKEY_USE_DEVICE_ROOT=1`, so both `globalkey()` and `uniquekey()` use that root. Xedge still encrypts `xcfg.bin` through `globalkey()`, but the resulting configuration is device-specific. A missing root is recreated; data encrypted under the previous root cannot be recovered from its name.

Named signing keys use the default PSA key location in TF-M. The Lua application receives key identifiers and public information, rather than private signing-key bytes. Secret derivation uses HKDF-SHA-512. NXP's provider supplies the cryptographic operations; `lpsa.c` has no RW612-specific changes. ChaCha20/Poly1305 and Brainpool curves are disabled to match the selected provider configuration.

## First hardware checks

- Confirm PSRAM initialization, LittleFS mounting, DHCP, time synchronization, and the browser IDE.
- Confirm `ba.psa == true`, create a named signing key, and exercise signing, certificate creation, and HTTPS.
- Save an application and encrypted configuration, power-cycle the board, and verify the files, certificate identity, and named key remain usable.
- Exercise simultaneous HTTPS connections while saving files. Check for PSA slot exhaustion, stack faults, and flash/PSRAM interference.

The [TF-M configuration](../XedgeInit/tfm/frdm_rw612/config.h) allows 64 simultaneous cryptographic operations and 64 loaded key slots. These are separate limits: TLS connections retain hash operations and volatile keys, while NXP's default persistent store allows 10 assets. Size these limits for the required named keys and simultaneous connections after measuring the hardware workload; a successful link does not verify those limits.
