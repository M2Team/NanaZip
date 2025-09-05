# NanaZip Policies

NanaZip supports setting system-wide policies via creating Registry values in
the key `HKLM\Software\NanaZip\Policies`.
These policies override user settings.

Following is the list of currently-supported policy options.

## Supported policy values

### Propagate Zone.Id stream

This value controls Mark-of-the-Web (MOTW) propagation of archive files.

- Name: `WriteZoneIdExtract`
- Type: `REG_DWORD`
- Value:
    - `0`: No
    - `1`: Yes (all files)
    - `2`: Only for unsafe extensions (does not support all nested archives)

### Disable mitigations

This value controls which security mitigations should not be applied by NanaZip.

- Name: `DisableMitigations`
- Type: `REG_DWORD`
- Value:
    - `0`: Don't disable mitigations
    - `1`: Disable all mitigations
    - Other values are reserved.

### Archive handler restrictions

These values control which archive handlers can be loaded by NanaZip.

With this policy, you can significantly limit the attack surface of NanaZip by
blocking the parsing of unusual archive formats.

If AllowedHandlers is present, only the handlers specified in said list will be
allowed.

If BlockedHandlers is present, the handlers specified in said list will be
blocked. BlockedHandlers takes precedence over AllowedHandlers; i.e. handlers
specified in BlockedHandlers will still be blocked even if they're specified in
AllowedHandlers.

Look for `REGISTER_ARC` in NanaZip.Core for the list of handlers bundled with
NanaZip.

- Name: `AllowedHandlers`
- Type: `REG_MULTI_SZ`
- Value: List of allowed archive handlers (case-sensitive).

- Name: `BlockedHandlers`
- Type: `REG_MULTI_SZ`
- Value: List of blocked archive handlers (case-sensitive).
