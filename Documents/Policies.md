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

Note that when editing a `REG_MULTI_SZ` value in Registry Editor, each entry
must be on its own line.

- Name: `AllowedHandlers`
- Type: `REG_MULTI_SZ`
- Value: List of allowed archive handlers (case-sensitive).

<!-- -->

- Name: `BlockedHandlers`
- Type: `REG_MULTI_SZ`
- Value: List of blocked archive handlers (case-sensitive).

Known archive handlers (as of NanaZip 6.0 Preview 1):

```
.Electron Archive (asar), .NET Single File Application, 7z, APFS, APM, Ar, Arj,
AVB, Base64, brotli, bzip2, Cab, Chm, COFF, Compound, Cpio, CramFS, Dmg, ELF,
Ext, FAT, FLV, GPT, gzip, HFS, Hxs, IHex, Iso, littlefs, lizard, LP, LVM, lz4,
lz5, Lzh, lzip, lzma, lzma86, MachO, MBR, MsLZ, Mub, Nsis, NTFS, PE, Ppmd, QCOW,
Rar, Rar5, ROMFS, Rpm, Sparse, Split, SquashFS, SWF, SWFc, tar, TE, Udf, UEFIc,
UEFIf, UFS, VDI, VHD, VHDX, VMDK, WebAssembly (WASM), wim, Xar, xz, Z, ZealFS,
zip, zstd
```

Look for `REGISTER_ARC` in NanaZip.Core for the full list of handlers bundled
with NanaZip.

### Codec restrictions

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

Known codecs (as of NanaZip 6.0 Preview 1):

```
7zAES, AES256CBC, ARM, ARM64, ARMT, BCJ, BCJ2, BROTLI, BZip2, Copy, Deflate,
Deflate64, Delta, FLZMA2, IA64, LIZARD, LZ4, LZ5, LZMA, LZMA2, PPC, PPMD, Rar1,
Rar2, Rar3, Rar5, RISCV, SPARC, Swap2, Swap4, ZSTD
```

Look for `REGISTER_CODEC` in NanaZip.Core for the list of codecs bundled with
NanaZip.
