#!/usr/bin/env bash
# Portable zeth <-> WAN gateway for WSL2 and regular Linux
# Env vars (optional): WAN_IFACE, LAN_DEFAULT_GW
# - WAN_IFACE: outward-facing NIC (default: auto-detect the iface with a default route)
# - LAN_DEFAULT_GW: upstream GW IP (default: auto-detect)

set -euo pipefail

# ---------- Helpers ----------
err() { echo "ERROR: $*" >&2; exit 1; }
have() { command -v "$1" >/dev/null 2>&1; }

require_root() { [[ $EUID -eq 0 ]] || err "Run as root (sudo $0)"; }
iface_exists() { ip link show "$1" &>/dev/null; }

rule_present() { ip rule show | grep -qE "$1"; }
route_present() { ip route show table "$1" | grep -qE "$2"; }

ipt_present() { sudo iptables -C "$1" $2 >/dev/null 2>&1; } # usage: ipt_present CHAIN "args..."

# ---------- Detect ----------
require_root

# Pick WAN_IFACE if not provided
WAN_IFACE="${WAN_IFACE:-$(ip -o route show default 2>/dev/null | awk '/default/ {print $5; exit}')}"

[[ -n "${WAN_IFACE:-}" ]] || err "Could not auto-detect WAN_IFACE; set WAN_IFACE env var."
iface_exists "$WAN_IFACE" || err "Interface '$WAN_IFACE' not found (run: net-setup.sh start)."

# LAN side is zeth (required)
iface_exists zeth || err "Interface 'zeth' not found."

# Upstream GW autodetect (works on both WSL2 and regular Linux)
LAN_DEFAULT_GW="${LAN_DEFAULT_GW:-$(ip -o route show default dev "$WAN_IFACE" 2>/dev/null | awk '{print $3; exit}')}"
[[ -n "${LAN_DEFAULT_GW:-}" ]] || err "Could not auto-detect upstream GW; set LAN_DEFAULT_GW env var."

echo "WAN_IFACE=$WAN_IFACE  LAN_DEFAULT_GW=$LAN_DEFAULT_GW"

# ---------- Enable forwarding ----------
sysctl -w net.ipv4.ip_forward=1 >/dev/null

# ---------- Policy routing (tables 100=zeth, 200=wan) ----------
LAN_TABLE=100
WAN_TABLE=200

# Routes: replace ensures idempotence
ip route replace default via "$LAN_DEFAULT_GW" dev "$WAN_IFACE" table "$LAN_TABLE"
ip route replace default dev zeth table "$WAN_TABLE"

# Rules: prefer replace, but some iproute2 versions lack it for rules; fall back to add-if-missing
rule_present "iif zeth lookup $LAN_TABLE" || ip rule add iif zeth lookup "$LAN_TABLE"
rule_present "iif $WAN_IFACE lookup $WAN_TABLE" || ip rule add iif "$WAN_IFACE" lookup "$WAN_TABLE"

# ---------- NAT + FORWARD ----------
# MASQUERADE on WAN_IFACE
if ! iptables -t nat -C POSTROUTING -o "$WAN_IFACE" -j MASQUERADE 2>/dev/null; then
  iptables -t nat -A POSTROUTING -o "$WAN_IFACE" -j MASQUERADE
fi

# Forward LAN->WAN
if ! iptables -C FORWARD -i zeth -o "$WAN_IFACE" -j ACCEPT 2>/dev/null; then
  iptables -A FORWARD -i zeth -o "$WAN_IFACE" -j ACCEPT
fi

# Forward WAN->LAN for established flows
if ! iptables -C FORWARD -i "$WAN_IFACE" -o zeth -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT 2>/dev/null; then
  iptables -A FORWARD -i "$WAN_IFACE" -o zeth -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT
fi

echo "Configured routing and NAT: zeth <-> $WAN_IFACE (GW: $LAN_DEFAULT_GW)."
echo "Tables: $LAN_TABLE(zeth-in), $WAN_TABLE(wan-in)"
