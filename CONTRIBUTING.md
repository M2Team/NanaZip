# Contributing to NanaZip

## How to become a contributor

- Direct contributions
  - **Create pull requests directly and follow the code style and conventions
    mentioned in this document.**
- Feedback suggestions and bugs.
  - We use GitHub issues to track bugs and features.
  - For bugs and general issues please 
    [file a new issue](https://github.com/M2Team/NanaZip/issues/new).

## Code contribution guidelines

### Prerequisites

- Visual Studio 2022 or later.
  - You also need install ARM64 components (MSVC Toolchain and ATL/MFC) if you
    want to compile the ARM64 version of NanaZip.
- Windows 11 SDK or later.
  - You also need to install ARM64 components if you want to compile the ARM64
    version of NanaZip.

### How to build all targets of NanaZip

Run `BuildAllTargets.bat` in the root of the repository.

### How to modify or debugging NanaZip

Open `NanaZip.sln` in the root of the repository.

### Code style and conventions

Read Kenji Mouri's [MD24: The coding style for all my open-source projects] for
more information.

[MD24: The coding style for all my open-source projects]: https://github.com/MouriNaruto/MouriDocs/tree/main/docs/24

**It's important to read that coding style carefully word by word. If you don't
follow that, your PR will be rejected and closed without any notice because all
NanaZip Benevolent Dictators For Life don't want to act as Dave Culter and Linus
Torvalds a.k.a. the angry code reviewers.**

Note: NanaZip have multiple Benevolent Dictators For Life. Read
https://github.com/M2Team/NanaZip/blob/main/Documents/People.md for more
information.

For all languages respect the [.editorconfig](https://editorconfig.org/) file 
specified in the source tree. Many IDEs natively support this or can with a 
plugin.

#### Notice for "Mile." prefix contents

You are forbidden to modify any content in any files and folders starting 
with the "Mile." prefix, or your PR won't be merged and closed immediately,
unless you get the permission from Kenji Mouri.

#### Modifications for inherited 7-Zip mainline source code

**Read https://github.com/M2Team/NanaZip/blob/main/License.md first for knowing
which files whether belong to inherited 7-Zip mainline source code.**

For simplifying the synchronization from 7-Zip mainline, the modification mark
is necessary, which the original 7-Zip mainline code should be commented as
original, Here is the format:

```
// **************** NanaZip Modification Start **************** 
// xzProps.numTotalThreads = (int)(prop.ulVal); 
xzProps.numTotalThreads = ((int)prop.ulVal) > 1 ? (int)prop.ulVal : 1; 
// **************** NanaZip Modification End **************** 
```

For multiple lines, you can also use the following format:

```
// **************** 7-Zip ZS Modification Start ****************
#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
kBZip2 = 12,

kLZMA = 14,

kTerse = 18,
kLz77 = 19,
kZstdPk = 20,

kZstdWz = 93,
kMP3 = 94,
kXz = 95,
kJpeg = 96,
kWavPack = 97,
kPPMd = 98,
kWzAES = 99
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
kBZip2 = 12,   // File is compressed using BZIP2 algorithm

kLZMA = 14,    // LZMA

kTerse = 18,   // File is compressed using IBM TERSE (new)
kLz77 = 19,    // IBM LZ77 z Architecture
kZstdPk = 20,  // deprecated (use method 93 for zstd)

kZstd = 93,    // Zstandard (zstd) Compression
kMP3 = 94,     // MP3 Compression
kXz = 95,      // XZ Compression
kJpeg = 96,    // JPEG variant
kWavPack = 97, // WavPack compressed data
kPPMd = 98,    // PPMd version I, Rev 1
kWzAES = 99    // AE-x encryption marker (see APPENDIX E)
// **************** 7-Zip ZS Modification End ****************
```

#### Translation contribution notice

All `comment` in `resw` files should be kept English for make it better to 
maintenance in the future.
