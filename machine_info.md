# Machine Info

Generated: 2026-05-19
Host: `system76-pc` (System76 Bonobo WS laptop)

---

## 1. Operating System & Kernel

| Field | Value |
|---|---|
| Distro | Ubuntu 24.04.1 LTS (Noble Numbat) |
| Kernel | `6.9.3-76060903-generic` |
| Architecture | `x86_64` |
| Firmware | `2023-10-20_2e4e34b` (Fri 2023-10-20) |

```bash
uname -a
cat /etc/os-release
hostnamectl
```

---

## 2. CPU

| Field | Value |
|---|---|
| Model | 13th Gen Intel(R) Core(TM) i9-13900HX |
| Family / Model / Stepping | 6 / 183 / 1 |
| Sockets | 1 |
| Physical cores | 24 (P-cores + E-cores hybrid) |
| Threads per core | 2 (on P-cores only) |
| Logical CPUs | **32** |
| Base / Max MHz | 800 / 5400 |
| Virtualization | VT-x |
| Address sizes | 39-bit physical, 48-bit virtual |

```bash
lscpu
nproc
cat /proc/cpuinfo
```

---

## 3. NUMA

Single NUMA node — no remote-memory penalty.

| Field | Value |
|---|---|
| Nodes | 1 |
| Node 0 CPUs | 0–31 |
| Node 0 memory | 64141 MB |
| Distances | self=10 |

```bash
numactl --hardware
lscpu | grep -i numa
```

---

## 4. Memory (RAM)

| Field | Value |
|---|---|
| Total | 62 GiB (65,680,660 kB) |
| Used | 5.5 GiB |
| Free | 54 GiB |
| Buff/cache | 4.2 GiB |
| Available | 57 GiB |
| Swap | 4.0 GiB (unused) |

```bash
free -h
cat /proc/meminfo
sudo dmidecode -t memory   # physical DIMM info
```

---

## 5. CPU Cache

| Level | Size | Instances | Line size |
|---|---|---|---|
| L1d | 896 KiB total | 24 | 64 B |
| L1i | 1.3 MiB total | 24 | 64 B |
| L2  | 32 MiB total  | 12 | 64 B |
| L3  | 36 MiB        | 1  | 64 B |

```bash
lscpu | grep -i cache
getconf -a | grep CACHE
lstopo-no-graphics            # if hwloc installed
```

---

## 6. CPU Features / Flags (registers & ISA extensions)

Key extensions supported:

- **SIMD**: SSE, SSE2, SSE3, SSSE3, SSE4.1, SSE4.2, AVX, AVX2, AVX_VNNI, F16C
- **Crypto**: AES-NI, SHA-NI, VAES, VPCLMULQDQ, GFNI
- **Virtualization**: VMX, EPT, VPID
- **Security**: SMEP, SMAP, IBRS_ENHANCED, STIBP, IBT, USER_SHSTK, PKU, OSPKE
- **Misc**: BMI1, BMI2, ADX, RDRAND, RDSEED, RDPID, MOVDIRI, MOVDIR64B, WAITPKG, INTEL_PT
- **Power/perf**: HWP, HWP_EPP, HFI, TURBO (IDA)

```bash
grep -m1 "^flags" /proc/cpuinfo | tr ' ' '\n' | sort -u
sudo rdmsr 0x1a0           # specific MSR (msr-tools + root)
```

---

## 7. PCI Devices

```
00:00.0 Host bridge: Intel a702
00:01.0 PCI bridge -> [01] Samsung NVMe SSD (PM9A1/PM9A3/980 PRO)
00:01.1 PCI bridge -> [02] NVIDIA GN21-X11 (RTX 4080/4090 mobile class) + audio
00:02.0 VGA: Intel Raptor Lake-S UHD Graphics
00:0a.0 Signal proc: Raptor Lake Crashlog & Telemetry
00:14.0 USB 3.2 Gen 2x2 XHCI controller
00:14.3 Wi-Fi: Intel CNVi (AX211)
00:15.0/.1 Serial I/O I2C controllers
00:1c.0 PCI bridge -> [03] Intel Ethernet 3102 (2.5 GbE)
00:1d.0 PCI bridge -> Thunderbolt 4 (Maple Ridge)
00:1f.0 ISA bridge / LPC
00:1f.3 Audio: Raptor Lake HDA
00:1f.4 SMBus
00:1f.5 SPI flash controller
01:00.0 Samsung NVMe SSD
02:00.0 NVIDIA dGPU
02:00.1 NVIDIA HDMI audio
03:00.0 Intel 2.5 GbE NIC
04–28    Thunderbolt 4 bridges, NHI, USB
```

```bash
lspci
lspci -tv          # bus tree
lspci -nnk         # vendor:device IDs + driver in use
lspci -vvv         # verbose
```

---

## 8. USB Devices

```
Bus 001 Device 002  046d:c548  Logitech Logi Bolt Receiver
Bus 001 Device 003  04f2:b7c3  Chicony USB2.0 Camera
Bus 001 Device 004  048d:8910  ITE Device (829x) — usually keyboard/RGB
Bus 001 Device 005  8087:0033  Intel AX211 Bluetooth
```

```bash
lsusb
lsusb -tv
```

---

## 9. Block / Storage Devices

| Device | Size | Type | Mount |
|---|---|---|---|
| `nvme0n1`     | 1.8 TB | NVMe SSD (Samsung PM9A1/980 PRO) | — |
| `nvme0n1p1`   | 512 MB | EFI System | `/boot/efi` |
| `nvme0n1p2`   | 1.8 TB | Linux root | `/` |
| `nvme0n1p3`   | 4 GB   | swap | `[SWAP]` |

```bash
lsblk
sudo fdisk -l
```

---

## 10. Drivers / Kernel Modules

**Loaded modules:** 190

### Key PCI device → driver mapping
| Device | Driver |
|---|---|
| Intel UHD iGPU | `i915` |
| NVIDIA dGPU | `nvidia` (proprietary) |
| NVMe SSD | `nvme` |
| Wi-Fi (CNVi AX211) | `iwlwifi` / `iwlmvm` |
| Ethernet (Intel 2.5G) | `igc` |
| Thunderbolt 4 | `thunderbolt` |
| HDA Audio | `snd_hda_intel` |
| USB 3.2 / TB USB | `xhci_hcd` |
| Bluetooth | `btusb`, `btintel` |
| Virtualization | `kvm`, `kvm_intel` |

### Largest loaded modules (memory)
- `nvidia` (54 MB) — dGPU driver
- `nvidia_uvm` (4.9 MB)
- `i915` (4.3 MB) — Intel iGPU
- `xe` (2.7 MB) — new Intel GPU driver (loaded but unused)
- `mac80211`, `iwlmvm`, `iwlwifi` — Wi-Fi stack
- `kvm` (1.4 MB), `kvm_intel` — virtualization
- `bluetooth` stack — 1.0 MB

```bash
lsmod
lspci -nnk | grep -A2 "Kernel driver"
modinfo <name>
ls /lib/modules/$(uname -r)/kernel/
dmesg | grep -i driver
```

---

## 11. One-shot inventory commands

```bash
sudo lshw -short           # condensed full hardware tree
sudo dmidecode             # BIOS/board/chassis/DIMM
inxi -Fxz                  # human-friendly summary (if installed)
```
