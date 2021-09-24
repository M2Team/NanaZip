# NanaZip

![ContextMenu](Documents/ContextMenu.png)

**Warning: This project is in early stages of development, the final product 
may differ from what you see now.**

NanaZip is an open source file archiver intended for the modern Windows 
experience, forked from the source code of well-known open source file archiver
7-Zip 21.03.

We need help to translate NanaZip into native languages that have not been 
supported.

# Development Roadmap

- 1.x Series
  - [x] Modernize the build toolchain with MSBuild for the MSIX packaging and 
        parallel compilation support.
  - [x] Use [VC-LTL 5.x](https://github.com/Chuyu-Team/VC-LTL5) toolchain to 
        make the binary size even smaller than the official 7-Zip because we
        can use ucrtbase.dll directly and the optimizations from modern compile
        toolchain.
  - [x] Add support for the new context menu in the Windows 11 File Explorer.
  - [ ] New icons and minor UI tweaks.
  - [ ] Add Per-Monitor DPI-Aware support for Self Extracting Executables.
  - [ ] Try to merge Brotli, Fast-LZMA2, Lizard, LZ4, LZ5 and Zstandard support
        from [7-Zip ZS](https://github.com/mcmilk/7-Zip-zstd) (Suggested by 
        fcharlie.)
  - [ ] Try to add option for save file names with UTF-8 in 7z archives.
- 2.x Series
  - [ ] Modernize the UI with XAML Islands.
  - [ ] Full High DPI and Accessibility support in all UI components.
- 3.x Series
  - [ ] Modernize the core implementation.
  - [ ] Add Windows Runtime component for interoperability.
  - [ ] Try to add extension for Windows File Explorer for give user immersive 
        experience. (Suggested by SakuraNeko and shuax.)
  - [ ] Add batch task support. (Suggested by 刘泪.)
  - [ ] Try to design the new UI layout. (Suggested by wangwenx190.)
  - [ ] Add option for using Windows Imaging API (WIMGAPI) backend to make 
        better creation and extraction support for wim archives, also add 
        creation and extraction support for esd archives.
- 4.x Series
  - [ ] Try to add recovery record support for 7z archives. (Suggested by 
        SakuraNeko.)
  - [ ] Try to create a new archive format for Windows imaging.

**All kinds of contributions will be appreciated. All suggestions, pull 
requests and issues are welcome.**

# System Requirements

- Supported OS: Windows 10, version 1809 or later
- Supported Platforms: x86, x86-64(AMD64) and ARM64.

# Features

- Packaging with MSIX for modern deployment experience.
- Support the new context menu in the Windows 11 File Explorer.

# Documents

- [License](License.md)
- [Changelog]()
- [Relevant People](Documents/People.md)
- [Privacy Policy](Documents/Privacy.md)
- [Windows Store]()
- [Code of Conduct]()
- [Contributing Guide]()
- [Versioning](Documents/Versioning.md)
