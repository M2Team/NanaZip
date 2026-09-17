# NanaZip Upstream Synchronization Status

## 7-Zip Mainline

- NanaZip.Core: 26.03
- NanaZip.UI.Classic: 22.01
  - Fix for CVE-2025-0411 backported from 7-Zip 24.09.
  - Fix for CVE-2025-11001 and CVE-2025-11002 backported from 7-Zip 25.00.
  - Security enhancements for symbolic link handling backported from 7-Zip
    25.01.
  - Extract callback fix backported from 7-Zip 26.02, including the our simple 
    fix for CVE-2026-58052.
- NanaZip.UI.Modern: 22.01
  - Fix for CVE-2025-0411 backported from 7-Zip 24.09.
  - Fix for CVE-2025-11001 and CVE-2025-11002 backported from 7-Zip 25.00.
  - Security enhancements for symbolic link handling backported from 7-Zip
    25.01.
  - Extract callback fix backported from 7-Zip 26.02, including the our simple 
    fix for CVE-2026-58052.
- NanaZip.Universal: 26.03

## 7-Zip ZS

- NanaZip.Core: Commit 48a3721a41e2ae227c8e4d7814f60f57a6148147 after
  v26.02-v1.5.7-R2.
- NanaZip.UI.Classic: Unknown because it modifies the old codebase a lot.
- NanaZip.UI.Modern: Unknown because it modifies the old codebase a lot.
- NanaZip.Universal: Commit 48a3721a41e2ae227c8e4d7814f60f57a6148147 after
  v26.02-v1.5.7-R2.

## BLAKE3

- NanaZip.Codecs: 1.8.7

## Brotli

- NanaZip.Codecs: 1.2.0

## FastLZMA2

- NanaZip.Codecs: Commit 967306d39daacf9a14ad923c86fa7f9c4552b59b.

## FreeBSD

- NanaZip.Codecs: 14.2.0 with Windows-specific adaptations.

## GmSSL

- NanaZip.Codecs: v3.2.0.

## LittleFS

- NanaZip.Codecs: 2.10.2 with actually has not been integrated yet.

## Lizard

- NanaZip.Codecs: Commit d3becc7e80d3eb01147c2f63bed2e23fac533f19 after v2.1
  with current used 7-Zip ZS modifications.

## LZ4

- NanaZip.Codecs: 1.10.0

## LZ5

- NanaZip.Codecs: Commit 1bc0fef363a44444135badb8e34286d5c56e1d5c after v1.5.

## RHash

- NanaZip.Codecs: Commit 3dbba4baa3cbdc3baf06d3ba086d8094bd98cd88 after v1.4.6.

## xxHash

- NanaZip.Codecs: 0.8.3

## Zstandard

- NanaZip.Codecs: 1.5.7

## ZSTDMT

- NanaZip.Codecs: Follow current used 7-Zip ZS implementation.
