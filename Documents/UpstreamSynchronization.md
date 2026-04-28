# NanaZip Upstream Synchronization Status

## 7-Zip Mainline

- NanaZip.Core: 26.01
- NanaZip.UI.Classic: 22.01
    - Fix for CVE-2025-0411 backported from 7-Zip 24.09.
    - Fix for CVE-2025-11001 and CVE-2025-11002 backported from 7-Zip 25.00.
    - Security enhancements for symbolic link handling backported from 7-Zip 25.01.
- NanaZip.UI.Modern: 22.01
    - Fix for CVE-2025-0411 backported from 7-Zip 24.09.
    - Fix for CVE-2025-11001 and CVE-2025-11002 backported from 7-Zip 25.00.
    - Security enhancements for symbolic link handling backported from 7-Zip 25.01.
- NanaZip.Universal: 26.01

## 7-Zip ZS

- NanaZip.Core: Commit 6146959af008acfb7e92c7d38ef9e43bf9f6afbb.
- NanaZip.UI.Classic: Unknown because it modifies the old codebase a lot.
- NanaZip.UI.Modern: Unknown because it modifies the old codebase a lot.
- NanaZip.Universal: Commit 6146959af008acfb7e92c7d38ef9e43bf9f6afbb.

## BLAKE3

- NanaZip.Codecs: 1.8.4

## Brotli

- NanaZip.Codecs: 1.2.0

## FastLZMA2

- NanaZip.Codecs: Commit a793db99fade2957d2453035390f97e573acecb2.

## FreeBSD

- NanaZip.Codecs: 14.2.0 with Windows-specific adaptations.

## GmSSL

- NanaZip.Codecs: Commit 0bcffd37347aa9e1799cf81e6eaeb1e76562dc6c after v3.1.1.

## LittleFS

- NanaZip.Codecs: 2.10.2 with actually has not been integrated yet.

## Lizard

- NanaZip.Codecs: 2.1

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

- NanaZip.Codecs: Follow 7-Zip ZS's integration.
