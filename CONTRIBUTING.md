# Contributing to NanaZip

## How to become a contributor

- Direct contributions
  - We use a code style similar but not identical to that of Windows NT kernel
    drivers. You must read the code style guidelines carefully, word by word,
    before submitting your pull request. To maintain NanaZip's source code
    quality, and to respect our reviewers' time, we will not accept PRs that
    don't follow these guidelines.
  - We expect all contributions to match our existing style WITHOUT EXCEPTION.
    If you have any questions about our coding standards, please open an issue
    for discussion before submitting your pull request.
  - By submitting a pull request, you agree to license your contribution under
    the MIT license. We reserve the right to reuse and rewrite contributors' PRs
    as needed.
  - You are forbidden to modify any content in any files and folders starting
    with the "Mile." prefix because those implementations are shared across many
    projects, or your pull requests won't be merged and closed immediately,
    unless you get the permission from Kenji Mouri.
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

For all languages respect the [.editorconfig](https://editorconfig.org/) file 
specified in the source tree. Many IDEs natively support this or can with a 
plugin.

#### Modifications for inherited 7-Zip mainline source code

> [!NOTE]
> Read https://github.com/M2Team/NanaZip/blob/main/License.md first for knowing
> which files whether belong to inherited 7-Zip mainline source code.**

> [!NOTE]
> For adding something to inherited 7-Zip code, don't add extra empty line if
> there is no empty line in the existed inherited 7-Zip methods and functions.

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

All `comment` in `resw` files should be kept English to make it better for 
maintenance in the future.
