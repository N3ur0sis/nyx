# TODO: Stealth and Security Enhancements for `ph_macspoof`

---

## Stealth Mode Enhancements

### Add `--silent` / `--quiet` CLI Flag
- Suppress all stdout and stderr messages
- Redirect `nyx_log` output to `/dev/null` or memory buffer
- Useful when running in automated scripts or evasion contexts

---

### Add `--no-backup` Mode
- Skip creating `.bak` files when spoofing
- Avoids leaving traces on disk (e.g., `/tmp`, `$HOME/.nyx`)
- Clearly warn the user this disables restoration
- Optional: also auto-delete old backup on new spoof

---

###  Optional: `--ephemeral` or `--memory-only` Mode
- Use temporary RAM-based directory (e.g., `/dev/shm`, `tmpfs`)
- Ensures all data is wiped on reboot or exit
- Useful for non-persistent spoofing in high-opsec environments

---

## Security & Reliability Hardening

###  Add Stronger Random MAC Generator
- Use `/dev/urandom` or `arc4random()` instead of `rand()`
- Optional: allow specifying custom vendor prefix or seed source
- Add `--oui <prefix>` for targeted spoofing (e.g., Apple, Intel)

---

### Add Exit Code Consistency + Failure Logging
- Ensure **any MAC change failure returns non-zero**
- Do not silently continue on `ph_macspoof_change_mac()` failure
- Suggest exit code map (e.g., `1 = perm error`, `2 = spoof fail`, `3 = iface invalid`)

---

###  Add `--check` Mode (Validation Only)
- Check if interface is valid and spoofable
- Optionally dry-run MAC spoof without applying changes
- Logs diagnostic output without touching the system state

---

###  Backup File Integrity Verification
- Add checksum or hash (e.g., SHA-1) to `.bak` file content
- Validate before restoring to detect tampering or corruption

---

##  Clean Execution & Anti-Forensics

###  Add `--cleanup` Option
- Delete all existing `.bak` files and metadata
- Securely wipe if possible (e.g., overwrite with nulls)

---

###  Zero Sensitive Buffers
- Use `explicit_bzero()` or similar to wipe memory storing MACs
- Prevent memory scraping attacks on live process memory

---

###  Remove or Obfuscate Banner in Stealth Mode
- Do not print ASCII art/banner in `--silent` mode
- Optionally randomize version string for less signature risk

---

###  CLI Obfuscation (optional)
- Allow tool to be renamed or aliased (`mv ph_macspoof something`)
- Avoid hardcoded strings that could be signatured (e.g., "NYX OFFENSIVE SUITE")

---

##  Optional Advanced Ideas

###  Syscall-level Obfuscation (Advanced)
- Consider using `LD_PRELOAD` to hijack system calls stealthily
- Or, reimplement spoofing logic via netlink instead of ioctl (less commonly monitored)

---

###  Interface Downtime Minimization
- Add optional flag to avoid toggling interface down/up if already up
- Some monitoring tools alert on interface state changes

---

##  Summary Checklist

| Feature                     | Status        |
|----------------------------|---------------|
| `--silent` flag            | ☐ To Do       |
| `--no-backup` flag         | ☐ To Do       |
| Secure random MAC          | ☐ To Do       |
| Reliable exit codes        | ☐ To Do       |
| `.bak` checksum/hash       | ☐ To Do       |
| Optional `--check` dry-run | ☐ To Do       |
| Secure cleanup             | ☐ To Do       |
| Ephemeral mode             | ☐ To Do       |
| Memory zeroing             | ☐ To Do       |
| Banner suppression         | ☐ To Do       |
| Interface downtime toggle  | ☐ To Do       |
| Signature obfuscation      | ☐ To Do       |

---

