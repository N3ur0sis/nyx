# macspoof

MAC address spoofing, randomization, and restoration.

**Module:** phobos
**Privileges:** root (`CAP_NET_ADMIN`)

## What It Does

Changes the MAC address of a network interface. Supports setting a specific address, generating a random locally-administered unicast MAC, and restoring the original hardware address.

## Interactive Commands

### show

```
show -i <iface>
```

Display the current MAC address of an interface.

### random

```
random -i <iface>
```

Generate and apply a random MAC address. The original MAC is backed up for later restore.

### set

```
set -i <iface> -m <mac>
```

Set a specific MAC address in `XX:XX:XX:XX:XX:XX` format.

### restore

```
restore -i <iface>
```

Restore the original (permanent) MAC address from backup.

### list

```
list
```

Show all network interfaces with their MAC, IP, netmask, and state.

## Workflow Contract

### Accepted Parameters

| Key | Aliases | Type | Required | Description |
|-----|---------|------|----------|-------------|
| `operation` | -- | string | no | `show`, `random`, `custom`, `restore`, or `list` (default: `show`) |
| `i` | `interface` | string | yes* | Network interface (*not needed for `list`) |
| `m` | `mac` | string | for `custom` | MAC address in `XX:XX:XX:XX:XX:XX` format |

### Result Fields

Results vary by operation:

#### list

```json
{
  "results": {
    "interfaces": [
      {
        "name": "eth0",
        "mac": "00:11:22:33:44:55",
        "ipv4": "192.168.1.10",
        "netmask": "255.255.255.0",
        "up": true
      }
    ]
  }
}
```

#### show

```json
{
  "results": {
    "interface": "eth0",
    "mac": "00:11:22:33:44:55",
    "up": true
  }
}
```

#### random / custom

```json
{
  "results": {
    "interface": "eth0",
    "old_mac": "00:11:22:33:44:55",
    "new_mac": "a6:b2:3c:d4:e5:f6",
    "verified": true
  }
}
```

#### restore

```json
{
  "results": {
    "interface": "eth0",
    "restored_mac": "00:11:22:33:44:55",
    "verified": true
  }
}
```

### Workflow Example

Randomize a MAC before starting a scan:

```json
{
  "id": "spoof",
  "tool": "macspoof",
  "params": {
    "operation": "random",
    "i": "${vars.interface}"
  }
}
```

### Error Conditions

| Error | Cause | Suggestion |
|-------|-------|------------|
| No interface specified | Missing `i` / `interface` | Add `"i": "eth0"` to params |
| No MAC address provided | `custom` operation without `m` | Add `"m": "XX:XX:XX:XX:XX:XX"` |
| Unknown operation | Invalid `operation` value | Use: `show`, `random`, `custom`, `restore`, or `list` |
| Permission denied | Not running as root | Run with `sudo` or set `CAP_NET_ADMIN` |
