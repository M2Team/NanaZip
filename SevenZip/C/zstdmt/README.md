
# Multithreading Library for [Brotli], [Lizard], [LZ4], [LZ5] and [Zstandard]

## Description
- works with skippables frame id 0x184D2A50 (12 bytes per compressed frame)
- brotli is supported the same way, it will encapsulate the real brotli stream
  within an 16 byte frame header

## Generic skippable frame definition

- the frame header for [Lizard], [LZ4], [LZ5] and [Zstandard] is like this:

size    | value             | description
--------|-------------------|------------
4 bytes | 0x184D2A50U       | magic for skippable frame
4 bytes | 4                 | size of skippable frame
4 bytes | compressed size   | size of the following frame (compressed data)


## [Brotli] frame definition

- the frame header for brotli is defined a bit different:

size    | value             | description
--------|-------------------|------------
4 bytes | 0x184D2A50U       | magic for skippable frame (like zstd)
4 bytes | 8                 | size of skippable frame
4 bytes | compressed size   | size of the following frame (compressed data)
2 bytes | 0x5242U           | magic for brotli "BR"
2 bytes | uncompressed size | allocation hint for decompressor (64KB * this size)


## Usage of the Testutils
- see [programs](https://github.com/mcmilk/zstdmt/tree/master/programs)

## Usage of the Library

- see [lib](https://github.com/mcmilk/zstdmt/tree/master/lib)


[Brotli]:https://github.com/google/brotli/
[LZ4]:https://github.com/lz4/lz4/
[LZ5]:https://github.com/inikep/lz5/
[Zstandard]:https://github.com/facebook/zstd/
[Lizard]:https://github.com/inikep/lizard/


/TR 2017-05-24
