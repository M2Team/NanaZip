# NanaZip Upstream Synchronization Status

## 7-Zip Mainline

- NanaZip.Core: 26.02
- NanaZip.UI.Classic: 22.01
    - Fix for CVE-2025-0411 backported from 7-Zip 24.09.
    - Fix for CVE-2025-11001 and CVE-2025-11002 backported from 7-Zip 25.00.
    - Security enhancements for symbolic link handling backported from 7-Zip 25.01.
    - Extract callback fix backported from 7-Zip 26.02.
- NanaZip.UI.Modern: 22.01
    - Fix for CVE-2025-0411 backported from 7-Zip 24.09.
    - Fix for CVE-2025-11001 and CVE-2025-11002 backported from 7-Zip 25.00.
    - Security enhancements for symbolic link handling backported from 7-Zip 25.01.
    - Extract callback fix backported from 7-Zip 26.02.
- NanaZip.Universal: 26.02

## 7-Zip ZS

- NanaZip.Core: Commit d8d651b72a6a85353a23d3f19e0fd2d96c0f36b4 equals to
  v26.01-v1.5.7-R1.
- NanaZip.UI.Classic: Unknown because it modifies the old codebase a lot.
- NanaZip.UI.Modern: Unknown because it modifies the old codebase a lot.
- NanaZip.Universal: Commit d8d651b72a6a85353a23d3f19e0fd2d96c0f36b4 equals to
  v26.01-v1.5.7-R1.

## BLAKE3

- NanaZip.Codecs: 1.8.5

## Brotli

- NanaZip.Codecs: 1.2.0

## FastLZMA2

- NanaZip.Codecs: Commit a793db99fade2957d2453035390f97e573acecb2.

## FreeBSD

- NanaZip.Codecs: 14.2.0 with Windows-specific adaptations.

## GmSSL

- NanaZip.Codecs: v3.2.0.

## LittleFS

- NanaZip.Codecs: 2.10.2 with actually has not been integrated yet.

## Lizard

- NanaZip.Codecs: 2.1 with current used 7-Zip ZS modifications.

## LZ4

- NanaZip.Codecs: 1.10.0

## LZ5

- NanaZip.Codecs: 1.5

## RHash

- NanaZip.Codecs: Commit 3dbba4baa3cbdc3baf06d3ba086d8094bd98cd88 after v1.4.6.

## xxHash

- NanaZip.Codecs: 0.8.3

## Zstandard

- NanaZip.Codecs: 1.5.7

## ZSTDMT

- NanaZip.Codecs: Follow current used 7-Zip ZS implementation.
