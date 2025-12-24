# NanaZip Policies

Starting with NanaZip 6.0, users can configure system‑wide policies under the
Windows registry key `HKLM\Software\Policies\M2Team\NanaZip`.

These system-wide policies override user settings and are intended for use by
system administrators to enforce specific configurations across multiple users
or systems.

Here are the currently provided policies and their descriptions.

## Available policies since NanaZip 6.0

### Allow dynamic code generation for all NanaZip components

NanaZip disables dynamic code generation for its components by default in
Release builds to reduce the risk of running potentially malicious code at
runtime. However, some third‑party software that relies on dynamic code
generation and injects code into NanaZip processes may require this feature
to be enabled for compatibility.

- Name: `AllowDynamicCodeGeneration`
- Type: `REG_DWORD`
- Value:
  - `0`: Disabled (Default)
  - `1`: Enabled

### Allow child processes creation for all NanaZip components

NanaZip blocks child process creation by default in its command line components
and self‑extracting executables to reduce the risk of process abuse by malicious
archives. However, some third‑party input method editors (IMEs) and assistive
technologies (ATs) may require child process creation to function correctly. In
such cases, this policy can be used to allow child process creation.

- Name: `AllowChildProcessCreation`
- Type: `REG_DWORD`
- Value:
  - `0`: Disabled (Default)
  - `1`: Enabled

# Legacy Policies in NanaZip 6.0 Preview 1

NanaZip supports setting system-wide policies via creating Registry values in
the key `HKLM\Software\NanaZip\Policies`.
These policies override user settings.

Following is the list of currently-supported policy options.

## Supported policy values

### Propagate Zone.Id stream

*Availability: NanaZip 6.0 Preview 1 (6.0.1461.0) and later.*

This value controls Mark-of-the-Web (MOTW) propagation of archive files.

- Name: `WriteZoneIdExtract`
- Type: `REG_DWORD`
- Value:
    - `0`: No
    - `1`: Yes (all files)
    - `2`: Only for unsafe extensions (does not support all nested archives)

### Disable mitigations

*Availability: NanaZip 6.0 Preview 1 (6.0.1461.0) and later.*

This value controls which security mitigations should not be applied by NanaZip.

- Name: `DisableMitigations`
- Type: `REG_DWORD`
- Value:
    - `0`: Don't disable mitigations
    - `1`: Disable all mitigations
    - Other values are reserved.

### Archive handler restrictions

*Availability: NanaZip 6.0 Preview 2 and later.*

These values control which archive handlers can be loaded by NanaZip.

With this policy, you can significantly limit the attack surface of NanaZip by
blocking the parsing of unusual archive formats.

If AllowedHandlers is present, only the handlers specified in said list will be
allowed.

If BlockedHandlers is present, the handlers specified in said list will be
blocked. BlockedHandlers takes precedence over AllowedHandlers; i.e. handlers
specified in BlockedHandlers will still be blocked even if they're specified in
AllowedHandlers.

Note that when editing a `REG_MULTI_SZ` value in Registry Editor, each entry
must be on its own line.

- Name: `AllowedHandlers`
- Type: `REG_MULTI_SZ`
- Value: List of allowed archive handlers (case-sensitive).

<!-- -->

- Name: `BlockedHandlers`
- Type: `REG_MULTI_SZ`
- Value: List of blocked archive handlers (case-sensitive).

Known archive handlers (as of NanaZip 6.0 Preview 2):

```
.Electron Archive (asar), .NET Single File Application, 7z, APFS, APM, Ar, Arj,
AVB, Base64, brotli, bzip2, Cab, Chm, COFF, Compound, Cpio, CramFS, Dmg, ELF,
Ext, FAT, FLV, GPT, gzip, HFS, Hxs, IHex, Iso, littlefs, lizard, LP, LVM, lz4,
lz5, Lzh, lzip, lzma, lzma86, MachO, MBR, MsLZ, Mub, Nsis, NTFS, PE, Ppmd, QCOW,
Rar, Rar5, ROMFS, Rpm, Sparse, Split, SquashFS, SWF, SWFc, tar, TE, Udf, UEFIc,
UEFIf, UFS, VDI, VHD, VHDX, VMDK, WebAssembly (WASM), wim, Xar, xz, Z, ZealFS,
zip, zstd
```

Run `NanaZipC --version` to see the full list of currently-enabled handlers.

### Codec restrictions

*Availability: NanaZip 6.0 Preview 2 and later.*

Similarly to archive handler restrictions, these values control which codecs can
be loaded by NanaZip.

**Note:** Some single-codec archive formats (e.g. Brotli) may call their
associated codecs without respecting codec restrictions. If you want to fully
block a codec from being used, make sure that its associated formats are also
blocked.

- Name: `AllowedCodecs`
- Type: `REG_MULTI_SZ`
- Value: List of allowed codecs (case-sensitive).

<!-- -->

- Name: `BlockedCodecs`
- Type: `REG_MULTI_SZ`
- Value: List of blocked codecs (case-sensitive).

Known codecs (as of NanaZip 6.0 Preview 2):

```
7zAES, AES256CBC, ARM, ARM64, ARMT, BCJ, BCJ2, BROTLI, BZip2, Copy, Deflate,
Deflate64, Delta, FLZMA2, IA64, LIZARD, LZ4, LZ5, LZMA, LZMA2, PPC, PPMD, Rar1,
Rar2, Rar3, Rar5, RISCV, SPARC, Swap2, Swap4, ZSTD
```

Run `NanaZipC --version` to see the full list of currently-enabled codecs.
