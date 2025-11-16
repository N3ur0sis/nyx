# Nyx

```
███╗   ██╗██╗   ██╗██╗  ██╗
████╗  ██║╚██╗ ██╔╝╚██╗██╔╝
██╔██╗ ██║ ╚████╔╝  ╚███╔╝ 
██║╚██╗██║  ╚██╔╝   ██╔██╗ 
██║ ╚████║   ██║   ██╔╝ ██╗
╚═╝  ╚═══╝   ╚═╝   ╚═╝  ╚═╝
```

**A comprehensive penetration testing framework inspired by the Greek goddess of the night**  
*Designed as an interconnected suite of modular security tools*

[![Language](https://img.shields.io/badge/language-C-blue.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Status](https://img.shields.io/badge/status-Active%20Development-yellow.svg)](https://github.com/N3ur0sis/nyx)

---

## 📖 Overview

Nyx is a modular penetration testing framework built from the ground up in C, designed for security researchers, penetration testers, and network professionals. Named after the primordial Greek goddess of the night, Nyx embodies the stealth, power, and comprehensive nature of modern offensive security operations.

The framework follows a **state machine-driven workflow** that guides operators through the complete penetration testing lifecycle: from initial reconnaissance and enumeration, through exploitation and privilege escalation, to post-exploitation and pivoting.

### Philosophy

- **Modular by Design** - Independent tools that compose together through shared interfaces
- **Low-Level Control** - Direct hardware access and raw socket manipulation
- **Professional Grade** - Production-ready error handling, logging, and state management
- **Extensible Architecture** - Plugin system allowing easy addition of new modules
- **Educational Focus** - Clean, documented code for learning offensive security techniques

---

## ✨ Features

### Core Framework

- **Unified CLI System** - Standardized command-line interface across all tools
- **Advanced Logging** - Color-coded, level-based logging with verbose mode support
- **Error Management** - Domain-specific error codes with detailed context and suggestions
- **State Persistence** - Session management and operation state tracking
- **Modular Runtime** - Dynamic module loading and composition

### Network Layer (PHOBOS Module)

- **MAC Spoofing** ✅ - Complete MAC address manipulation with randomization and restore
- **ARP Poisoning** 🚧 - Man-in-the-middle attack automation *(In Development)*
- **Port Scanning** 📋 - TCP/UDP service discovery with heuristics *(Planned)*
- **Ping Sweep** 📋 - Network host discovery across subnets *(Planned)*
- **DHCP Starvation** 📋 - IP pool exhaustion for denial-of-service *(Planned)*
- **DNS Spoofing** 📋 - Targeted domain name poisoning *(Planned)*
- **TCP Reset** 📋 - Connection teardown injection *(Planned)*
- **Packet Sniffer** 📋 - Raw packet capture with protocol decoding *(Planned)*

### Reconnaissance & OSINT (AETHER Module)

- Subdomain enumeration and DNS analysis
- HTTP fingerprinting and technology detection
- Web directory brute-forcing
- Banner grabbing and service identification
- Username tracing across platforms
- CIDR expansion and IP extraction

### Exploitation (VULCAN Module)

- FTP traversal and RCE exploit frameworks
- Web shell deployment and management
- SMB relay MITM attacks
- Buffer overflow exploitation tooling
- SQL injection automation
- CMS-specific exploit modules

### Post-Exploitation (KRONOS Module)

- Linux/Windows privilege escalation scanners
- Credential harvesting utilities
- SSH agent forwarding and tunneling
- Port forwarding and SOCKS pivoting
- Internal network mapping and recon

### AI/ML Integration (HELIOS Module)

- AI-based web technology and CVE prediction
- Automated exploit generation
- Attack path planning and optimization
- CTF challenge solving capabilities

---

## 🏗️ Architecture

### System Overview

Nyx follows a layered architecture designed for maintainability, security, and extensibility:

```
┌──────────────────────────────────────────────────────────────┐
│                    Application Layer                          │
│  ┌───────────────┐  ┌──────────────┐  ┌──────────────────┐  │
│  │ Tool CLIs     │  │ Web Dashboard│  │  Script Interface│  │
│  │ (ph_*, ae_*)  │  │   (Future)   │  │    (Future)      │  │
│  └───────────────┘  └──────────────┘  └──────────────────┘  │
└────────────────────────────┬─────────────────────────────────┘
                             │
┌────────────────────────────┴─────────────────────────────────┐
│                     Framework Runtime                         │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  Core Services                                        │   │
│  │  • CLI Parsing & Validation (nyx_cli)                │   │
│  │  • Unified Logging (nyx_logger)                      │   │
│  │  • Error Handling (nyx_error)                        │   │
│  │  • Module Discovery & Loading                        │   │
│  │  • Session Management                                │   │
│  └──────────────────────────────────────────────────────┘   │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  Utility Libraries                                    │   │
│  │  • Network Interface (ph_iface)                      │   │
│  │  • Packet Crafting (ph_packet)                       │   │
│  │  • Socket Management (ph_socket)                     │   │
│  │  • Address Utilities (ph_netaddr)                    │   │
│  └──────────────────────────────────────────────────────┘   │
└────────────────────────────┬─────────────────────────────────┘
                             │
┌────────────────────────────┴─────────────────────────────────┐
│                   Hardware Interface Layer                    │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  • Raw Socket Operations                              │   │
│  │  • Network Interface Control (ioctl)                  │   │
│  │  • Packet Injection & Capture                         │   │
│  │  • System Call Interface                              │   │
│  └──────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────┘
```

### Module Organization

The framework is organized into thematic modules, each containing related tools:

```
nyx/
├── core/                      # Framework runtime & services
│   ├── nyx_cli.{c,h}         # CLI parsing and help system
│   ├── nyx_logger.{c,h}      # Unified logging subsystem
│   └── nyx_error.{c,h}       # Error handling and context
├── utils/                     # Shared utility libraries
│   ├── ph_iface.{c,h}        # Network interface utilities
│   ├── ph_packet.{c,h}       # Packet crafting utilities
│   ├── ph_socket.{c,h}       # Socket management
│   └── ph_netaddr.{c,h}      # Address parsing/validation
├── tools/                     # Security tool modules
│   ├── phobos/               # Network-layer tools
│   │   ├── macspoof/         # MAC address spoofing
│   │   ├── arpspoof/         # ARP poisoning
│   │   ├── pingsweep/        # Host discovery
│   │   └── portscan/         # Port scanning
│   ├── aether/               # Reconnaissance tools
│   ├── vulcan/               # Exploitation tools
│   ├── kronos/               # Post-exploitation tools
│   └── helios/               # AI/ML integration
└── bin/                       # Compiled binaries
```

### Tool Structure

Each tool follows a consistent internal structure:

```c
tool_name/
├── Makefile                   # Build configuration
├── src/
│   ├── tool_api.h            # Public API interface
│   ├── tool_impl.{c,h}       # Core implementation
│   └── tool_cli.c            # CLI wrapper
└── build/                     # Compiled objects
```

### Data Flow

```
User Input → CLI Parser → Input Validator → Core Logic → System Interface
     ↓           ↓              ↓               ↓              ↓
  Logging ← Error Handler ← State Manager ← Result ← Hardware Response
```

---

## 🚀 Getting Started

### Prerequisites

**Operating Systems:**
- Linux (Primary support: kernel 4.x+)
- macOS (Experimental: 11.0+)

**Build Tools:**
- GCC 9.0+ or Clang 10.0+
- GNU Make 4.0+
- Standard C library (glibc or musl)

**Permissions:**
- Root/sudo access required for most network operations
- CAP_NET_RAW capability for packet manipulation

### Quick Start

1. **Clone the repository:**

```bash
git clone https://github.com/N3ur0sis/nyx.git
cd nyx
```

2. **Build a specific tool:**

```bash
cd tools/phobos/macspoof
make
```

3. **Run the tool:**

```bash
# List available interfaces
sudo ../../../bin/nyx-macspoof --list

# Spoof MAC address
sudo ../../../bin/nyx-macspoof -i eth0 -m 00:11:22:33:44:55

# Set random MAC
sudo ../../../bin/nyx-macspoof -i eth0 --random

# Restore original MAC
sudo ../../../bin/nyx-macspoof -i eth0 --restore
```

### Installation

To install tools system-wide:

```bash
# From within a tool directory
make
sudo make install  # Installs to /usr/local/bin (coming soon)
```

---

## 🔨 Building

### Build Individual Tools

Each tool has its own Makefile:

```bash
cd tools/phobos/macspoof
make              # Build the tool
make clean        # Clean build artifacts
```

### Build Configuration

The Makefiles support several options:

```bash
# Debug build with symbols
make DEBUG=1

# Custom compiler
make CC=clang

# Custom installation prefix
make PREFIX=/opt/nyx install
```

### Build Outputs

Compiled binaries are placed in the `bin/` directory at the root:

```
nyx/
└── bin/
    ├── nyx-macspoof      # MAC spoofing tool
    ├── nyx-arpspoof      # ARP poisoning tool
    └── ...               # Other compiled tools
```

---

## 📖 Usage

### MAC Spoofing Tool

The MAC spoofing tool provides complete control over network interface MAC addresses:

**List Available Interfaces:**

```bash
sudo nyx-macspoof --list
```

**Show Current MAC Address:**

```bash
sudo nyx-macspoof -i eth0 --show
```

**Set Custom MAC Address:**

```bash
sudo nyx-macspoof -i eth0 --mac 00:11:22:33:44:55
```

**Set Random MAC Address:**

```bash
# Fully random MAC
sudo nyx-macspoof -i eth0 --random

# Random MAC with realistic vendor OUI
sudo nyx-macspoof -i eth0 --random --vendor-realistic
```

**Restore Original MAC:**

```bash
sudo nyx-macspoof -i eth0 --restore
```

**Enable Debug Output:**

```bash
sudo nyx-macspoof -i eth0 --random --debug
```

### Common Workflow Examples

**Network Reconnaissance:**

```bash
# Change MAC address for anonymity
sudo nyx-macspoof -i eth0 --random

# Discover hosts on the network
sudo nyx-pingsweep -i eth0 -n 192.168.1.0/24

# Scan open ports on discovered hosts
sudo nyx-portscan -t 192.168.1.10 -p 1-1000
```

**ARP Poisoning Attack:**

```bash
# Spoof MAC to impersonate gateway
sudo nyx-macspoof -i eth0 --mac <gateway_mac>

# Launch ARP poisoning
sudo nyx-arpspoof -i eth0 -t 192.168.1.10 -g 192.168.1.1

# Capture and analyze traffic
sudo nyx-sniffer -i eth0 -o capture.pcap
```

---

## 📚 Documentation

### Core APIs

#### CLI System (`nyx_cli.h`)

The CLI system provides standardized argument parsing:

```c
// Define CLI options
static const nyx_cli_opt_def_t options[] = {
    {'i', "interface", "IFACE", "Network interface", 
     NYX_CLI_ARG_REQUIRED, NYX_CLI_FLAG_REQUIRED},
    {'v', "verbose", NULL, "Enable verbose output", 
     NYX_CLI_ARG_NONE, NYX_CLI_FLAG_OPTIONAL},
    {'h', "help", NULL, "Show help", 
     NYX_CLI_ARG_NONE, NYX_CLI_FLAG_OPTIONAL}
};

// Parse arguments
nyx_cli_result_t *result = nyx_cli_parse(argc, argv, options, 3);

// Check for specific options
if (nyx_cli_has_option(result, 'v')) {
    nyx_set_verbose(1);
}

// Get option value
const char *iface = nyx_cli_get_option(result, 'i');
```

#### Logger System (`nyx_logger.h`)

Unified logging across all tools:

```c
// Log levels
nyx_log(NYX_LOG_INFO, "Starting operation...");
nyx_log(NYX_LOG_WARN, "Potential issue detected");
nyx_log(NYX_LOG_ERROR, "Operation failed: %s", error_msg);
nyx_log(NYX_LOG_SUCCESS, "MAC address changed successfully");

// Enable verbose mode
nyx_set_verbose(1);
nyx_log(NYX_LOG_VERBOSE, "Detailed debug information");
```

#### Error System (`nyx_error.h`)

Domain-specific error handling:

```c
// Standard error codes
#define NYX_SUCCESS              0
#define NYX_ERR_PARAM           -1
#define NYX_ERR_PERMISSION      -5
#define NYX_ERR_NOT_FOUND       -6
#define NYX_ERR_NETWORK         -10

// Set error with context
NYX_ERROR_SET_EX(
    NYX_DOMAIN_MACSPOOF,          // Error domain
    PH_ERR_PERMISSION,             // Error code
    NYX_ERROR_SEV_ERROR,           // Severity
    "Permission denied",           // Description
    "Try running with sudo"        // Suggestion
);

// Get last error
const char *error = nyx_error_get_last();
```

#### Network Interface Utilities (`ph_iface.h`)

Interface manipulation and validation:

```c
// Validate interface
if (!ph_iface_is_valid("eth0")) {
    fprintf(stderr, "Invalid interface\n");
    return -1;
}

// Get current MAC address
char mac[18];
if (ph_iface_get_mac("eth0", mac, sizeof(mac)) == PH_IFACE_SUCCESS) {
    printf("Current MAC: %s\n", mac);
}

// Set new MAC address
if (ph_iface_set_mac("eth0", "00:11:22:33:44:55") == PH_IFACE_SUCCESS) {
    printf("MAC address changed\n");
}

// Check interface status
if (ph_iface_is_up("eth0")) {
    printf("Interface is up\n");
}
```

#### Packet Crafting (`ph_packet.h`)

Create and parse network packets:

```c
// Create ICMP packet
ph_icmp_params_t icmp = {
    .type = 8,              // Echo request
    .code = 0,
    .id = 1234,
    .seq = 1,
    .data = "payload",
    .data_len = 7
};

uint8_t packet[1024];
size_t packet_len = sizeof(packet);

if (ph_packet_create_icmp(&icmp, packet, &packet_len) == PH_PACKET_SUCCESS) {
    // Send packet...
}
```

### Design Patterns

**Error Handling Pattern:**

```c
int result = ph_iface_set_mac(iface, mac);
if (result != PH_IFACE_SUCCESS) {
    // Error is already set in error context
    nyx_log(NYX_LOG_ERROR, "Failed to set MAC: %s", nyx_error_get_last());
    return map_iface_error_to_tool_error(result);
}
```

**Resource Cleanup Pattern:**

```c
nyx_cli_result_t *result = nyx_cli_parse(argc, argv, opts, opt_count);
if (!result) {
    return -1;
}

// Use result...

// Always free resources
nyx_cli_free_result(result);
```

---

## 🎯 Roadmap

### Phase 1: Foundation (Current)

- [x] Core framework architecture
- [x] CLI system and logging
- [x] Error handling infrastructure
- [x] Network interface utilities
- [x] MAC spoofing tool (complete)
- [🚧] ARP spoofing tool
- [ ] Packet crafting library
- [ ] Socket management utilities

### Phase 2: Network Layer (PHOBOS)

- [ ] Port scanning (TCP/UDP)
- [ ] ICMP-based host discovery
- [ ] DHCP starvation attacks
- [ ] DNS spoofing
- [ ] TCP connection disruption
- [ ] Raw packet sniffer with filters

### Phase 3: Reconnaissance (AETHER)

- [ ] Subdomain enumeration
- [ ] DNS zone transfer testing
- [ ] HTTP/HTTPS fingerprinting
- [ ] Directory brute-forcing
- [ ] Service banner grabbing
- [ ] OSINT username tracking

### Phase 4: Exploitation (VULCAN)

- [ ] FTP exploitation framework
- [ ] Web shell deployment
- [ ] SMB relay attacks
- [ ] SQL injection engine
- [ ] CMS exploit modules
- [ ] Buffer overflow toolkit

### Phase 5: Post-Exploitation (KRONOS)

- [ ] Linux privilege escalation
- [ ] Windows privilege escalation
- [ ] Credential harvesting
- [ ] Network pivoting tools
- [ ] Persistence mechanisms
- [ ] Internal network mapping

### Phase 6: Intelligence (HELIOS)

- [ ] AI-powered vulnerability analysis
- [ ] Automated exploit generation
- [ ] Attack path optimization
- [ ] Machine learning for evasion
- [ ] CTF challenge automation

### Future Enhancements

- [ ] Web-based control interface
- [ ] Plugin API for third-party tools
- [ ] Report generation system
- [ ] Session recording and replay
- [ ] Distributed operation support
- [ ] Docker containerization
- [ ] Windows native support

---

## 🤝 Contributing

Contributions are welcome! Whether you're fixing bugs, adding features, or improving documentation, your help is appreciated.

### Development Guidelines

1. **Code Style**
   - Follow K&R C style with 4-space indentation
   - Use tabs for indentation, spaces for alignment
   - Maximum line length: 100 characters
   - Comprehensive documentation for all public APIs

2. **Commit Messages**
   - Use imperative mood ("Add feature" not "Added feature")
   - First line: brief summary (50 chars max)
   - Detailed explanation in body if needed

3. **Pull Requests**
   - One feature/fix per PR
   - Include tests if applicable
   - Update documentation
   - Ensure all tools compile without warnings

4. **Security**
   - Never commit credentials or sensitive data
   - Report security vulnerabilities privately
   - Follow responsible disclosure practices

### Areas for Contribution

**High Priority:**
- Completing ARP spoofing implementation
- Port scanning with service detection
- Packet filtering and analysis
- Cross-platform support (macOS, BSD)

**Documentation:**
- Tool usage examples
- API documentation
- Architecture guides
- Video tutorials

**Testing:**
- Unit tests for utility libraries
- Integration tests for tools
- Fuzzing for packet parsing
- Performance benchmarks

**New Features:**
- Additional exploitation modules
- OSINT tools
- Post-exploitation utilities
- AI/ML integration

---

## ⚠️ Legal Disclaimer

**IMPORTANT: READ BEFORE USING**

Nyx is designed for **authorized security testing, research, and educational purposes only**. Users are responsible for complying with all applicable laws and regulations in their jurisdiction.

### Authorized Use Cases

✅ Penetration testing with written authorization  
✅ Security research in controlled environments  
✅ Educational demonstrations in lab settings  
✅ Testing your own systems and networks  
✅ Capture The Flag (CTF) competitions  

### Prohibited Activities

❌ Unauthorized access to computer systems  
❌ Network disruption or denial of service  
❌ Data theft or privacy violations  
❌ Any illegal or malicious activities  

**The authors and contributors of Nyx assume no liability for misuse of this software. By using Nyx, you agree to use it responsibly and ethically.**

---

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

```
MIT License

Copyright (c) 2025 Neur0sis

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
```

---

## 🙏 Acknowledgments

### Inspiration

- **Metasploit Framework** - Modular exploitation architecture
- **Nmap** - Network scanning and service detection
- **Ettercap** - ARP poisoning and MITM techniques
- **Burp Suite** - Professional security testing methodology

### Technologies

- **C Language** - Systems programming foundation
- **Raw Sockets** - Low-level network access
- **ioctl** - Device control interface
- **POSIX** - Cross-platform compatibility

### References

- **RFC 826** - Address Resolution Protocol (ARP)
- **RFC 792** - Internet Control Message Protocol (ICMP)
- **RFC 793** - Transmission Control Protocol (TCP)
- **IEEE 802.3** - Ethernet standards

### Community

Special thanks to the information security community for their research, tools, and knowledge sharing that makes projects like this possible.

---

## 📞 Contact

- **Author:** Neur0sis
- **GitHub:** [@N3ur0sis](https://github.com/N3ur0sis)
- **Project:** [github.com/N3ur0sis/nyx](https://github.com/N3ur0sis/nyx)

---

<div align="center">

**Nyx** - *Born from the primordial darkness, wielding the tools of night*

Made with ❤️ and lots of coffee

[⬆ Back to Top](#nyx)

</div>
