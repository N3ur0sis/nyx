# Security Policy

## Scope

This policy covers vulnerabilities in the NYX framework itself -- the shared libraries, build system, workflow engine, and tool implementations. It does **not** cover weaknesses in target systems that NYX is designed to test.

## Reporting a Vulnerability

If you discover a security vulnerability in NYX (e.g. a buffer overflow in packet parsing, a privilege escalation in the installer, or an expression injection in the workflow engine), please report it responsibly.

**Do not open a public GitHub issue for security vulnerabilities.**

Instead, use one of these channels:

- **GitHub Security Advisories**: open a [private security advisory](https://github.com/N3ur0sis/nyx/security/advisories/new) on this repository
- **Email**: contact the maintainer directly at the email listed in the GitHub profile

Please include:

- Description of the vulnerability
- Steps to reproduce
- Affected version(s)
- Potential impact assessment
- Suggested fix (if you have one)

## Response Timeline

- **Acknowledgment**: within 72 hours
- **Initial assessment**: within 1 week
- **Fix or mitigation**: coordinated with the reporter before public disclosure

## Supported Versions

Only the latest release is actively maintained with security patches.

| Version | Supported |
|---------|-----------|
| latest | Yes |
| older | No |

## Hardening

NYX binaries are compiled with security hardening flags including stack protectors, FORTIFY_SOURCE, PIE/ASLR, full RELRO, non-executable stack, and control-flow integrity where supported. See `CMakeLists.txt` for the full flag set.
