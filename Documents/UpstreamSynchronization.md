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

- NanaZip.Core: Commit e4f17d368421568ba9613987d2de8a574a26a472.
- NanaZip.UI.Classic: Unknown because it modifies the old codebase a lot.
- NanaZip.UI.Modern: Unknown because it modifies the old codebase a lot.
- NanaZip.Universal: Commit 507d2c5fc8df7d752f5f8162d66f61662432d75a.

## BLAKE3

- NanaZip.Codecs: 1.8.2

## Brotli

- NanaZip.Codecs: 1.2.0

## FastLZMA2

- NanaZip.Codecs: 1.0.1 with some fixes by NanaZip team.

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

- NanaZip.Codecs: Commit cce6c628f93d9ed332921656aa5e1750d12b8d3e after v1.4.6.

## xxHash

- NanaZip.Codecs: 0.8.3

## Zstandard

- NanaZip.Codecs: 1.5.7

## ZSTDMT

- NanaZip.Codecs: Follow 7-Zip ZS's integration.
