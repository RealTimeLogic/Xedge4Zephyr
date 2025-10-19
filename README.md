# Xedge for Zephyr

*A Lua-powered IoT and web framework for Zephyr and embedded systems.*

**Xedge** is a robust IoT and web framework designed specifically for microcontrollers.  
It’s built on the **industrial-grade Barracuda Application Server (BAS)** and engineered for seamless OEM integration.

Xedge accelerates embedded firmware development by providing:

- A **flexible, Lua-based environment**
- A **full stack of industrial-strength protocols**, including:
  - OPC UA  
  - Modbus  
  - MQTT  
  - SMQ  
  - WebSockets  
  - HTTP / HTTPS  

**Product pages:**

- [Xedge Toolkit](https://realtimelogic.com/products/xedge/)
- [Barracuda Application Server C Source Code Library](https://realtimelogic.com/products/barracuda-application-server/)

---

## About This Guide

This guide explains how to **build and run Xedge on the Zephyr host simulator**.  
You can reuse the same build files for your own target and project, but we recommend first following this tutorial **exactly as written** to verify your setup and test Xedge using the **host simulator**.

## Prerequisites

Before building Xedge, ensure that you have installed the **Zephyr SDK** and **West** according to the official Zephyr installation instructions.

Once installed, verify that you can activate the Zephyr environment by running:

```bash
$ source ~/zephyrproject/.venv/bin/activate
(.venv) $
```

### File System Support

The Xedge build configuration includes **file system support** by default.  
While Xedge can be built **without** a file system, having one makes it easier to develop and test Lua code directly on a running Zephyr system.

If your target hardware does not support a file system, you can exclude it in your build configuration later.  
For now, ensure file system support **remains enabled**.

When running Xedge on the **Zephyr host simulator**, Zephyr relies on **FUSE (Filesystem in Userspace)** under Linux.  
Because the simulator is compiled as a **32-bit application**, you must install the **32-bit FUSE developer libraries**:

```bash
sudo dpkg --add-architecture i386
sudo apt-get update
sudo apt-get install -y libfuse-dev:i386 gcc-multilib g++-multilib
```

## Creating a Workspace

For this guide, we'll create a new **Zephyr workspace** dedicated to building Xedge.  
Later, you can integrate the Xedge module into your own existing workspace.

We'll name the workspace **`xedgews`**, but you can choose any name, just remember to adjust the commands accordingly.

### Step 1: Create the workspace

```bash
mkdir ~/xedgews
west init ~/xedgews
cd ~/xedgews
west update
# Install Python dependencies
pip3 install -r ~/xedgews/zephyr/scripts/requirements.txt
```

## Step 2: Clone the Xedge4Zephyr Repository

Clone the Xedge4Zephyr repository into the `modules` directory of your workspace.

```bash
cd ~/xedgews/modules
git clone --recursive --recurse-submodules https://github.com/RealTimeLogic/Xedge4Zephyr
cd Xedge4Zephyr
git submodule update --init --remote
```

## Step 3: Build the Xedge Resource File

A significant portion of Xedge is implemented in **Lua**.  
To include these Lua resources in your Zephyr build, we must:

1. Build the **resource file (ZIP)**
2. Convert the ZIP file to **C source code**
3. Copy the generated file into the **Xedge directory**

Run the following commands:

```bash
cd ~/xedgews/modules/Xedge4Zephyr/BAS-Resources/build/
printf "n\nl\nn\n" | bash Xedge.sh
cp XedgeZip.c ../../BAS/examples/xedge/
```

The printf command is used here to automate the prompts in the Xedge.sh build script. As you become more familiar with Xedge, you’ll have the flexibility to choose which modules and resources to include in the generated resource file.

### Step 4: Register the Xedge4Zephyr Module in the West Manifest

Edit `~/xedgews/zephyr/west.yml` and add this entry in the `projects:` section:

```yaml
manifest:
  projects:
    # ... other projects ...
    
    - name: xedge
      path: modules/Xedge4Zephyr
      revision: main
      url: .
```

**Important**: The `url: .` means local module (not from GitHub or remote repo)

### Step 5: Update West

Now update west to register the module:

```bash
cd ~/xedgews
west update
```

You should see: `=== updating xedge (modules/Xedge4Zephyr):`

## Building Xedge

To build Xedge for the first time (or after a **pristine** clean), run:

```bash
# Ensure pkg-config can find the 32-bit FUSE .pc files
export PKG_CONFIG_PATH="/usr/lib/i386-linux-gnu/pkgconfig:${PKG_CONFIG_PATH}"
# From your workspace root (~/xedgews)
west build -b native_sim modules/Xedge4Zephyr/XedgeInit \
  -- -DDTC_OVERLAY_FILE="$PWD/modules/Xedge4Zephyr/XedgeInit/boards/native_sim.overlay"
```

**Note:** The Zephyr native simulator is built as a 32-bit app; the FUSE developer package you installed (libfuse-dev:i386) places its fuse.pc in /usr/lib/i386-linux-gnu/pkgconfig. Exporting PKG_CONFIG_PATH allows pkg-config (invoked during the build) to locate FUSE, avoiding link/compile errors.

The FUSE/file system initialization for the simulator is configured in
Xedge4Zephyr/XedgeInit/boards/native_sim.overlay.

If the build completes without errors, subsequent incremental builds are simply:

```bash
west build
```

## Running Xedge

After a successful build, run Xedge with:

```bash
west build -t run
```

On the first launch in the simulator, you’ll likely see LittleFS report an error and then auto-format:

```
<wrn> littlefs: can't mount (LFS -84); formatting
```

This is expected the first time the simulated flash is empty or uninitialized.
After formatting, the file system will mount normally and subsequent runs won’t show this warning.

You will also see the following error:

```
<err> xedge_main: NTP sync failed: -101. Did you configure your network?
```

The reason you’re seeing the NTP sync error is because the file `modules/Xedge4Zephyr/XedgeInit/src/main.c` tries to fetch time from the Internet, but the zeth interface hasn’t been started and NAT isn’t configured yet.

### Configure `zeth`

The zeth interface is a virtual Ethernet device that connects the Zephyr simulator to the host’s network. It lets the simulated system exchange IP traffic with external networks through the host. Because the simulator runs in a virtual environment without direct Internet access, zeth works together with NAT (Network Address Translation) on the host to route and translate packets so Zephyr can communicate with the outside world as if it were real hardware.

Our setup requires modifications to the default `zeth.conf` configuration file. Do the following only one time:

```bash
mv modules/Xedge4Zephyr/zeth.conf tools/net-tools/zeth.conf
```

#### Start the zeth interface:




```bash
sudo ~/xedgews/tools/net-tools/net-setup.sh start
```

#### Create two routing tables

Edit `/etc/iproute2/rt_tables` and add the two lines:

```
100 zeth
200 eth0    or enp0s3, wlp2s0, etc. (details below)
```

#### Configure NAT:

```
chmod +x ~/xedgews/modules/Xedge4Zephyr/setup-gateway.sh
export WAN_IFACE=<iface>
export LAN_DEFAULT_GW=<ip-addr>
sudo ~/xedgews/modules/Xedge4Zephyr/setup-gateway.sh
```

To find your network interface, run: `ip a`. Look for something like enp0s3 or wlp2s0.

To find the gateway for that interface, run: `ip route`

**Note:** The script **defaults** to settings for Windows Subsystem for Linux (**WSL2**), so you don’t need to set the two environment variables when using WSL2.

## Running Xedge with Configured NAT Network

```
west build -t run

### Printouts you should see

FUSE mounting flash in host flash/
WARNING: Using a test - not safe - entropy source
uart connected to pseudotty: /dev/pts/1
[00:00:00.000,000] <inf> littlefs: littlefs partition at /xedge
[00:00:00.000,000] <inf> littlefs: LittleFS version 2.11, disk version 2.1
[00:00:00.000,000] <inf> littlefs: FS at flash-controller@0:0x0 is 256 0x1000-byte blocks with 512 cycle
[00:00:00.000,000] <inf> littlefs: partition sizes: rd 16 ; pr 16 ; ca 64 ; la 32
[00:00:00.000,000] <inf> littlefs: Automount /xedge succeeded
*** Booting Zephyr OS build v4.2.0-6017-g86a46c48bee5 ***
[00:00:00.000,000] <inf> net_config: Initializing network
[00:00:00.000,000] <inf> net_config: IPv4 address: 10.200.200.2
[00:00:00.000,000] <inf> xedge_main: Xedge application starting...
[00:00:00.000,000] <inf> xedge_main: Board: native_sim
[00:00:00.000,000] <inf> xedge_main: Xedge stack size: 16384 bytes
[00:00:00.000,000] <inf> xedge_main: Xedge - dlmalloc initialized - heap size: 2097152 bytes
[00:00:00.000,000] <inf> xedge_main: Starting the Xedge main loop
[00:00:00.000,000] <inf> xedge_main: main.c - xedgeInitDiskIo: Mounting /xedge
[00:00:00.000,000] <inf> xedge_main: Xedge: Server listening on IPv6 port 80
[00:00:00.000,000] <inf> xedge_main: Xedge: Server listening on IPv4 port 80
[00:00:00.000,000] <inf> xedge_main: Xedge: SharkSSL server listening on IPv6 port 443
[00:00:00.000,000] <inf> xedge_main: Xedge: SharkSSL server listening on IPv4 port 443
[00:00:00.000,000] <err> fs: file open error (-2)
[00:00:00.000,000] <inf> xedge_main: Xedge: Configuration file: /xedge/xcfg.bin: noaccess
[00:00:30.250,000] <inf> xedge_main: Uptime: 30 seconds
[00:01:00.260,000] <inf> xedge_main: Uptime: 60 seconds
```

**Note:** You see the xcfg.bin error since you have so far not created this file using the Xedge IDE.

To verify that Xedge is running and accessible, open a **separate terminal** and run:

```bash
wget http://10.200.200.2/rtl/
```

This command downloads the Xedge IDE (HTML code), confirming that the server is up and responding.


## Accessing the Xedge IDE

If you are running **standard Linux (not WSL2)**, simply open your browser and go to: `http://10.200.200.2`

You should see the **Xedge IDE** load in your browser.

## Accessing Xedge via WSL2

When using **WSL2**, networking is slightly more complex because it runs behind a **double NAT**.  
The Zephyr simulator creates a virtual interface named **`zeth`**, which is not directly reachable from Windows.  
To fix this, you can connect to Xedge in one of two ways:

---

### **Option 1: Set Up Routing (Recommended)**

1. Open **PowerShell** as an Administrator.  
2. Run the following command (replace the IP address with your current WSL2 host IP if different):

   ```powershell
   route add 10.200.200.0 mask 255.255.255.252 172.29.54.156
   ```

This creates a route from Windows to the zeth network interface inside WSL2. You should now be able to directly access Xedge from your Windows browser at: `http://10.200.200.2`


### Option 2: Set Up a Tunnel

If routing doesn’t work or you prefer a quick alternative, you can create a TCP tunnel using socat.

Open a new WSL2 terminal. Run the following command (install socat if you don’t already have it):

```bash
sudo socat -d -d TCP-LISTEN:8090,fork,reuseaddr TCP:10.200.200.2:80
```

This forwards traffic from port 8090 on the WSL2 host to the Xedge simulator’s port 80.

You can now access Xedge from Windows via your WSL2 IP (for example, 172.29.54.156) - e.g. `http://172.29.54.156:8090/rtl/`


**Tip:** You can find your WSL2 IP address by running the following in a WSL2 console:

```
ip addr show eth0
```

## What's Next

Learn how to use the [Xedge IDE](https://realtimelogic.com/ba/doc/en/Xedge.html) and explore the built-in Lua environment.  
Start by copying example code from the [online Lua tutorials](https://tutorial.realtimelogic.com/) and pasting it into the IDE.
