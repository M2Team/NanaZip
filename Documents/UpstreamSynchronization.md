# NanaZip Upstream Synchronization Status

## 7-Zip Mainline

- NanaZip.Core: 25.01
- NanaZip.UI.Classic: 22.01
    - Fix for CVE-2025-0411 backported from 7-Zip 24.09.
    - Fix for CVE-2025-11001 and CVE-2025-11002 backported from 7-Zip 25.00.
    - Security enhancements for symbolic link handling backported from 7-Zip 25.01.
- NanaZip.UI.Modern: 22.01
    - Fix for CVE-2025-0411 backported from 7-Zip 24.09.
    - Fix for CVE-2025-11001 and CVE-2025-11002 backported from 7-Zip 25.00.
    - Security enhancements for symbolic link handling backported from 7-Zip 25.01.
- NanaZip.Universal: 25.01

## 7-Zip ZS

- NanaZip.Core: Commit a18a29fa4cd5a71cae81c618923205d0631fb16c.
- NanaZip.UI.Classic: Unknown because it modifies the old codebase a lot.
- NanaZip.UI.Modern: Unknown because it modifies the old codebase a lot.
- NanaZip.Universal: Commit a18a29fa4cd5a71cae81c618923205d0631fb16c.

## BLAKE3

- NanaZip.Codecs: 1.8.2

## Brotli

- NanaZip.Codecs: 1.2.0

## FastLZMA2

- NanaZip.Codecs: Commit a793db99fade2957d2453035390f97e573acecb2.

## FreeBSD

- NanaZip.Codecs: 14.2.0 with Windows-specific adaptations.

## GmSSL

- NanaZip.Codecs: 3.1.1

## LittleFS

- NanaZip.Codecs: 2.10.2 with actually has not been integrated yet.

## Lizard

- NanaZip.Codecs: 2.1

## LZ4

- NanaZip.Codecs: 1.10.0

## LZ5

- NanaZip.Codecs: 1.5

## RHash

- NanaZip.Codecs: Commit b76c6a3312422c09817c2cef40442b2f2d9d4689 after v1.4.6.

## xxHash

- NanaZip.Codecs: 0.8.3

## Zstandard

- NanaZip.Codecs: 1.5.7

## ZSTDMT

- NanaZip.Codecs: Follow 7-Zip ZS's integration.
