# Contributing to NanaZip

## How to become a contributor

- Direct contributions
  - We use a code style similar but not identical to that of Windows NT kernel
    drivers. You must read the code style guidelines carefully, word by word,
    before submitting your pull request. To maintain NanaZip's source code
    quality, and to respect our reviewers' time, we will not accept pull
    requests that don't follow these guidelines.
  - We expect all contributions to match our existing style WITHOUT EXCEPTION.
    If you have questions about our coding standards, ask in an issue or in your
    pull request description.
  - By submitting a pull request, you agree to license your contribution under
    the MIT license. We reserve the right to reuse and rewrite contributors'
    pull requests as needed.
  - You are forbidden to modify any content in any files and folders starting
    with the "Mile." prefix because those implementations are shared across many
    projects, or your pull request will not be merged and will be closed
    immediately, unless you get the permission from Kenji Mouri.
- Feedback suggestions and bugs.
  - We use GitHub issues to track bugs and features.
  - For bugs and general issues please 
    [file a new issue](https://github.com/M2Team/NanaZip/issues/new).

## AI/LLM use when contributing

- Before opening a pull request, read the basic guidance yourself, word for 
  word. AI/LLM assistance, including translation, must not replace your own
  reading and understanding. Readability-first requires human judgment.
- Ask about unclear points in an issue or the pull request description. Your
  questions must show that you have read the guidance; unstated uncertainty does
  not excuse style violations.
- Before committing, personally refine the draft so you can explain your
  reasoning for any added line when asked during review. This includes i18n
  entries and sources for translations outside your native language.
- Before pushing, review the changes yourself, then build and test locally.
- Write commit messages, pull request descriptions, and review comments
  yourself. Keep them concise, with no emojis or unnecessary tables.
  AI/LLM-assisted polishing must preserve your own reasoning.
- Commit messages and pull request descriptions must be in English. Keep commit
  messages to one sentence, adding lines only for essential context or
  attribution.

Pull requests will be closed unless you have personally refined and reviewed the
changes and successfully built and tested them locally.

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

Recommend to run `RestoreNuGetPackages.cmd` in the root of the repository to
restore NuGet packages if you have not run this script or `BuildAllTargets.bat`
before. (Recent versions of Visual Studio 2026 has some issues to restore NuGet
packages automatically.)

Open `NanaZip.slnx` in the root of the repository.

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
> which files whether belong to inherited 7-Zip mainline source code.

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

#### Synchronization between components

To minimize the work when synchronizing with upstream code, the following should
be done when proposing changes to NanaZip:

- Synchronize between Core and Universal.
- Synchronize between UI.Classic and UI.Modern.
- Do not synchronize between Core/Universal and UI.Classic/UI.Modern.
