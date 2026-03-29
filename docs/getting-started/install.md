# Installation

## Requirements

- **OS:** Linux (tested on Ubuntu, Kali, Parrot, Debian, Arch)
- **Compiler:** GCC 9+ or Clang 10+
- **Build system:** CMake 3.16+
- **C library:** glibc or musl
- **Privileges:** root or `CAP_NET_RAW` / `CAP_NET_ADMIN` for network tools

## Quick Install from Release

Download the latest release tarball and run the installer:

```bash
# Download and extract
curl -fsSL https://github.com/N3ur0sis/nyx/releases/latest/download/nyx-1.0.0-linux-x86_64.tar.gz \
  | tar xz
cd nyx-1.0.0-linux-x86_64

# Install (default: /usr/local)
sudo bash install.sh

# Or install to ~/.local (no root needed, but capabilities won't be set)
bash install.sh --prefix ~/.local
```

The installer copies binaries, man pages, and optionally sets Linux capabilities so network tools work without `sudo`.

## Build from Source

```bash
git clone https://github.com/N3ur0sis/nyx.git
cd nyx
cmake -B build
cmake --build build
```

Binaries are placed in `bin/` at the project root.

### Install System-Wide

```bash
sudo cmake --install build
```

This installs binaries to `/usr/local/bin/` and man pages to `/usr/local/share/man/`.

### Build Options

```bash
# Debug build with symbols
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Use Clang instead of GCC
cmake -B build -DCMAKE_C_COMPILER=clang

# Custom binary output directory
cmake -B build -DNYX_BIN_DIR=/opt/nyx/bin
```

### Build a Single Tool

```bash
cmake --build build --target nyx-portscan
```

## Verify the Build

```bash
./bin/nyx version
./bin/nyx info
```

`nyx info` lists all detected tools, their versions, and binary sizes.

## Binary Hardening

All NYX binaries are compiled with security hardening flags:

- `-fstack-protector-strong` -- stack buffer overflow detection
- `-fstack-clash-protection` -- stack clash mitigation
- `-D_FORTIFY_SOURCE=2` -- runtime buffer overflow checks
- `-fPIE` / `-pie` -- position-independent executables (ASLR)
- `-Wl,-z,relro,-z,now` -- full RELRO (GOT protection)
- `-Wl,-z,noexecstack` -- non-executable stack
- `-fcf-protection` -- control-flow integrity (where supported)

Verify with:

```bash
readelf -l bin/nyx | grep GNU_RELRO
readelf -d bin/nyx | grep BIND_NOW
```
