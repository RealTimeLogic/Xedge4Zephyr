# Local names and SharkCA

Start with the [introduction to automatic certificate management](https://realtimelogic.com/services/SharkTrust/), then use the [Xedge certificate manual](https://realtimelogic.com/ba/doc/en/Xedge.html#cert) for setup. Xedge's mDNS support primarily serves SharkCA: it lets browsers resolve the `.local` name covered by the device's private HTTPS certificate. Xedge manages that registration automatically.

This guide covers Zephyr-specific hostname behavior and optional Lua use. Zephyr's native responder publishes the device's IPv4 and IPv6 addresses, and its resolver can look up other `.local` devices.

The responder and resolver are enabled in the shared firmware configuration for the [MIMXRT1020-EVK](mimxrt1020-evk.md), [MIMXRT1060-EVKC](mimxrt1060-evkc.md), [STM32F746G-DISCO](stm32f746g-disco.md), [FRDM-RW612](frdm-rw612.md), and `native_sim`.

## Reach the device by name

The default name is `xedge` followed by the interface's link address as hexadecimal digits, then `.local`. For example, an Ethernet address of `00:11:22:33:44:55` produces `xedge001122334455.local`. Open `http://<name>.local/rtl/` to reach the IDE.

Your computer must support mDNS and share the device's local network. Allow multicast UDP port 5353 on that network. For `native_sim` under WSL2, use its TAP network from inside WSL2; forwarding HTTP to Windows does not forward mDNS traffic.

For SharkCA, use the assigned certificate name and let Xedge manage its registration. Application-selected names are optional; setting one does not create a certificate or install browser trust.

## Select a name from Lua

**Only one active `ba.createmdns` instance is supported per device. Named SharkCA certificates use this instance, so leave it available when using SharkCA with a device name.** SharkCA's IP-only mode leaves it available to applications.

Use the [mDNS Lua API reference](https://realtimelogic.com/ba/doc/en/lua/auxlua.html#mdns) for the hostname argument, validation rules, and application-lifetime example. Zephyr uses the same `ba.createmdns(name)` signature and returns a userdata handle on success. Its native hostname integration has these differences:

| Call | Zephyr behavior |
| --- | --- |
| `handle:status()` | No additional arguments. Returns Boolean `true` while the handle owns the configured hostname; otherwise `nil, "closed"` or `nil, "hostname changed"`. This checks local configuration, not network reachability. |
| `handle:close()` | No additional arguments or return values. Releases the instance and restores the previous hostname if the selected name is still current. |

`ba.createmdns` returns `nil, "invalid name"` for an invalid label, `nil, "mDNS hostname already in use"` when another handle owns the hostname, or `nil, "cannot set hostname"` if Zephyr rejects the update. A non-string argument or a string containing a NUL byte raises a Lua argument error.

Keep the handle referenced for as long as you need the name. Calling `close()` more than once is harmless. Garbage collection and Lua's to-be-closed variables also release the instance. If another native component changes the hostname, closing the handle leaves that newer name intact. Other computers may cache the previous name until its DNS lifetime expires.

## Resolve another device

The resolver works independently of the single advertising handle. Use `ba.socket.toip` with the other device's full `.local` name:

```lua
-- The peer must advertise this name on the same local network.
local address, err = ba.socket.toip("other-device.local")
if address then
   trace("Peer address:", address)
else
   trace("Name lookup failed:", err)
end
```

The required argument is a hostname string. An optional second Boolean argument defaults to `false` for IPv4; pass `true` to request IPv6. On success, `address` is an IP-address string; on failure it is `nil` and `err` is an error string. The lookup blocks while DNS resolves the name, so call it from code that may block, such as application initialization or a BAS worker thread. Ordinary DNS lookups continue to use the configured unicast DNS server.

## Configure SharkCA certificates

Follow the [Xedge certificate manual](https://realtimelogic.com/ba/doc/en/Xedge.html#cert) for the dialog fields, enrollment, and device-versus-portal trust. Use the [SharkCA user manual](https://github.com/RealTimeLogic/SharkCA/blob/main/doc/user-manual.md) for portal administration and CA distribution.

On Zephyr, leave the single mDNS instance available to Xedge for named certificates. Keep `/xedge` during firmware updates to retain saved configuration and certificate state. With the RW612 PSA profile, also preserve [TF-M key storage](frdm-rw612.md#preserve-keys-and-settings).

The [board-porting guide](adding-boards.md#2-add-a-board-configuration-and-overlay) covers the shared mDNS configuration, DNS initialization order, and build compatibility handling.
