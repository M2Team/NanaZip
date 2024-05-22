# NanaZip Release Notes

For preview versions, please read 
[NanaZip Preview Release Notes](ReleaseNotesPreview.md).

**NanaZip 3.0 (3.0.1000.0)**

- Notes
  - NanaZip 3.0 and onwards will have two distribution flavors called NanaZip
    and NanaZip Classic. But NanaZip 3.0 does not have the Classic flavor yet
    because it's not ready. Read
    https://github.com/M2Team/NanaZip#differences-between-nanazip-and-nanazip-classic
    for more information.
  - The 32-bit x86 support for NanaZip packaged version is removed because
    supported 32-bit x86 Windows versions don't support running on 32-bit
    only x86 processors.
  - NanaZip 3.0 and onwards will have NanaZip Sponsor Edition. Read
    https://github.com/M2Team/NanaZip/blob/main/Documents/SponsorEdition.md
    for more information.
  - NanaZip has introduced the Preinstall Support. Read
    https://github.com/M2Team/NanaZip/issues/398 for more information.
- Features
  - Introducing dark mode support for all GUI components.
  - Introducing the Mica support. You will enjoy the full window immersive Mica
    support for all GUI components if you are using dark mode with HDR disabled.
    (Thanks to Andarwinux.)
  - Synchronize the 7-Zip mainline implementations to 24.05.
    (https://github.com/ip7z/7zip/releases/tag/24.05) (Thanks to Igor Pavlov.
    Noticed by AVMLOVER-4885955 and PopuriAO29.)
    - Make NanaZip Self Extracting Executable stubs use 7-Zip mainline
      Zstandard decoder instead of Zstandard official's for reducing the
      size for binaries.
    - Use 7-Zip mainline Zstandard decoder instead of Zstandard official
      decoder for NanaZip.Core project.
    - Remove 7-Zip mainline XXH64 Hash handler for NanaZip.Core because we
      have the xxHash-based implementation in NanaZip.Codecs. It will have
      much better performance, especially for non-x86 targets.
    - Implement the new toolbar and use it to replace the old menubar and old
      toolbar.
    - Refresh the UI layout for About Dialog via following other Nana series
      project design.
    - Add the SM3 HASH algorithm from GmSSL.
      (https://github.com/guanzhi/GmSSL)
- Improvements
  - Rewrite and split the implementation of the Core Library and the Self
    Extracting Executable to the separate NanaZip.Codecs and NanaZip.Core
    projects. Read https://github.com/M2Team/NanaZip/issues/336 for more
    information.
  - Ensure the implementation of Core Library and the Self Extracting Executable
    supports Windows Vista RTM (Build 6000.16386).
  - Reduce the binary size for the Self Extracting Executables.
  - Synchronize the 7-Zip ZS implementations to the latest master branch.
    (https://github.com/mcmilk/7-Zip-zstd/commit/79b2c78e9e7735ddf90147129b75cf2797ff6522)
  - Synchronize Zstandard and builtin xxHash implementations to v1.5.6.
    (https://github.com/facebook/zstd/releases/tag/v1.5.6)
  - Synchronize Brotli implementations to v1.1.0.
    (https://github.com/google/brotli/releases/tag/v1.1.0)
  - Synchronize the RHash implementation to the latest master branch which is
    after v1.4.4.
    (https://github.com/rhash/RHash/commit/d916787590b9dc57eb9d420fd13e469160e0b04c)
  - Synchronize the BLAKE3 implementation to latest master which is after 1.5.1.
    (https://github.com/BLAKE3-team/BLAKE3/commit/0816badf3ada3ec48e712dd4f4cbc2cd60828278)
  - Update to Git submodule version of Mile.Project.Windows.
    (https://github.com/ProjectMile/Mile.Project.Windows)
  - Update Mile.Windows.Helpers to 1.0.558.
    (https://github.com/ProjectMile/Mile.Windows.Helpers/tree/1.0.558.0)
  - Update Mile.Xaml to 2.2.944.
    (https://github.com/ProjectMile/Mile.Xaml/releases/tag/2.2.944.0)
  - Use Mile.Windows.Internal package.
    (https://github.com/ProjectMile/Mile.Windows.Internal)
  - Use Mile.Detours package.
    (https://github.com/ProjectMile/Mile.Detours)
  - Use modern IFileDialog for folder picker dialog. (Contributed by 
    reflectronic.）
  - Launch directly to the settings page of association for NanaZip.
    (Contributed by AndromedaMelody.）
  - Show NanaZip in Drives' ContextMenu. (Contributed by AndromedaMelody.）
  - Sync file extension support from https://github.com/mcmilk/7-Zip-zstd.
  - Add other methods to compression dialog.
    (https://github.com/mcmilk/7-Zip-zstd/commit/cf29d0c1babcd5ddf2c67eda8bb36e11f9c643b9)
  - Reorder initialization in constructor matching to member declaration order.
    (https://github.com/mcmilk/7-Zip-zstd/commit/8b011d230f1ccd8990943bd2eaad38d70e6e3fdf)
  - Fix selectable uppercase / lowercase hash formatting.
    (https://github.com/mcmilk/7-Zip-zstd/commit/4fae369d2d6aa60e2bb45eea1fb05659a2599caa)
  - Update russian translation. (Contributed by Blueberryy.）
  - Update Polish translation. (Contributed by ChuckMichael.)
  - Add mitigation policy of disabling child process creation for command line
    version of NanaZip. (Contributed by dinhngtu.)
  - Add Explorer Patcher DLL blocking for NanaZip File Manager for reenabling
    mitigation policy of blocking loading unexpected libraries from remote
    sources at runtime for the main thread of NanaZip File Manager without the
    stability issues. (Contributed by dinhngtu.)
- Fixes
  - Fix issue in IEnumExplorerCommand::Next for shell extension. (Thanks to
    cnbluefire.)
- Other adjustments for project development.


**NanaZip 2.0 Update 1 (2.0.450.0)**

- Optimize NanaZip binaries via adjusting the WindowsTargetPlatformMinVersion
  to 10.0.19041.0 in all packaged NanaZip binaries projects except the Self
  Extracting Executable stubs projects.
- Opt out of dynamic code mitigation on main NanaZip thread for resolving the
  compatibility issues with Explorer Patcher. (Contributed by dinhngtu.)
- Update Mile.Xaml to 1.1.434.
  (https://github.com/ProjectMile/Mile.Xaml/releases/tag/1.1.434.0)
- Update Mile.Windows.Helpers to 1.0.8.
  (https://github.com/ProjectMile/Mile.Windows.Helpers/commits/main)
- Add dark mode support for context menus.
- Refresh the about dialog with Windows 11 XAML control styles and the 
  immersive Mica effects.
- Fix model dialog style behavior for About dialog.
- Continue to refresh application and file type icons. (Designed by 
  Shomnipotence.)

**NanaZip 2.0 (2.0.396.0)**

- Notes
  - Update the minimum system requirement to Windows 10 Version 2004 (Build 
    19041) or later for solving issues in the XAML Islands.
  - Add instructions for installing NanaZip for all users. (Contributed by
    AndromedaMelody. Suggested by Wolverine1977.)
- Features
  - Integrate the following HASH algorithms to NanaZip from RHash (AICH, 
    BLAKE2b, BTIH, ED2K, EDON-R 224, EDON-R 256, EDON-R 384, EDON-R 512, 
    GOST R 34.11-94, GOST R 34.11-94 CryptoPro, GOST R 34.11-2012 256, 
    GOST R 34.11-2012 512, HAS-160, RIPEMD-160, SHA-224, SHA3-224, SHA3-256, 
    SHA3-384, SHA3-512, Snefru-128, Snefru-256, Tiger, Tiger2, TTH, Whirlpool) 
    and xxHash (XXH3_64bits, XXH3_128bits).
  - Allow NanaZip to be associated with any file type. (Contributed by 
    manfromarce.)
  - Add hfsx to file type association. (Suggested by AndromedaMelody.)
- Improvements
  - Refresh application and file type icons. (Designed by Shomnipotence.)
  - Refresh the about dialog with XAML Islands.
  - Update 7-Zip to 22.01. (https://www.7-zip.org/history.txt) (Thanks to Igor 
    Pavlov. Noticed by HylianSteel, Random-name-hi and DJxSpeedy.)
  - Update Zstandard to 1.5.2. 
    (https://github.com/facebook/zstd/releases/tag/v1.5.2).
  - Update BLAKE3 to 1.3.1.
    (https://github.com/BLAKE3-team/BLAKE3/releases/tag/1.3.1)
  - Update LZ4 to 1.9.4. (https://github.com/lz4/lz4/releases/tag/v1.9.4)
  - Enable Control Flow Guard (CFG) to all target binaries for mitigating ROP 
    attacks. (Contributed by dinhngtu.)
  - Mark all x86 and x64 target binaries as compatible with Control-flow 
    Enforcement Technology (CET) Shadow Stack. (Contributed by dinhngtu.)
  - Strict handle checks at runtime to block the use of invalid handles. 
    (Contributed by dinhngtu.)
  - Disable dynamic code generation in Release builds prevents generating 
    malicious code at runtime. (Contributed by dinhngtu. Thanks to 
    AndromedaMelody.)
  - Block loading unexpected libraries from remote sources at runtime.
    (Contributed by dinhngtu.)
  - Enable Package Integrity Check. (Contributed by AndromedaMelody.)
  - Enable EH Continuation Metadata. (Suggested by dinhngtu. Thanks to 
    mingkuang.)
  - Enable Signed Returns.
  - Add Mile.Xaml to NanaZip project.
  - Start adding prerequisite support for unpackaged mode.
- Fixes
  - Fix the shell extension issue which cause Everything crashed. (Thanks to 
    No5972, startkkkkkk, SakuraNeko, bfgxp and riverar.)
  - Improve the Per-Monitor DPI Awareness support in Windows 10 Version 1607 
    for Self Extracting Executable stubs.
  - Fix line break issue for i18n resource files. (Thanks to ygjsz.)
  - Generate resource identities for package manifest manually. (Suggested by 
    AndromedaMelody.)
  - Add workaround for NanaZip not appearing in classic context menu. 
    (Contributed by dinhngtu.)
  - Check 7z compression parameter validity upon start of compression.
    (Contributed by dinhngtu.)

**NanaZip 1.2 (1.2.252.0)**

- Fix no ordinal 345 was found in the dynamically attached library issue in 
  Self Extracting Executables. (Thanks to FadeMind.)
- Add Per-Monitor DPI-Aware support for all GUI components.
- Adjust and simplify the compiler options for modernizing.
- Fix i18n issue for the About dialog. (Thanks to AndromedaMelody.)
- Update installation tutorial. (Suggested by AndromedaMelody.)
- Fix cannot start editor issue when only store edition of notepad existed. 
  (Thanks to AndromedaMelody.)
- Modernize the i18n implementation via migrating language files from .txt to 
  .resw. (Contributed by AndromedaMelody. Suggested by Maicol Battistini.)
- Update ModernWin32MessageBox for solving the infinite loop issue in some 
  cases. (Thanks to AndromedaMelody.)
- Tweak icons and provide icons for preview versions. (Designed by Alice 
  (四月天). Thanks to StarlightMelody.)
- Fix crash issue when opening archive files. (Thanks to 1human and Maicol 
  Battistini.)
- Remove Language page in Options dialog because NanaZip will follow the 
  language settings from Windows itself.
- Fix the issue of the i18n implementation of File Type Association. 
  (Contributed by AndromedaMelody.)
- Add i18n support for GUI edition of Self Extracting Executable. 
  (Contributed by AndromedaMelody.)

**NanaZip 1.1 (1.1.194.0)**

- Add assembly implementations from 7-Zip back for improving performance.
- Reimplement the about dialog with Task Dialog.
- Modernize the message boxes with Task Dialog. (Thanks to DJxSpeedy.)
- Update 7-Zip to 21.07. (Thanks to Igor Pavlov. Noticed by HylianSteel.)
- Update translations inherited from 7-Zip.
- Update Deutsch translations. (Contributed by Hen Ry.)
- Update Polish translation. (Contributed by ChuckMichael.)
- Improve the multi volume rar file detection for solving 
  https://github.com/M2Team/NanaZip/issues/82. (Thanks to 1human.)
- Simplify the file type association definitions and add the open verb for 
  them. (Thanks to Fabio286.)
- Fix CI issue.
- Update VC-LTL to 5.0.4.
- Update C++/WinRT to 2.0.211028.7.

**NanaZip 1.0 (1.0.95.0)**

- Modernize the build toolchain with MSBuild for using MSIX packaging and 
  parallel compilation support. (Thanks to AndromedaMelody, be5invis, 
  青春永不落幕 and oxygen-dioxide.)
- Use [VC-LTL 5.x](https://github.com/Chuyu-Team/VC-LTL5) toolchain to make the
  binary size even smaller than the official 7-Zip because we can use 
  ucrtbase.dll directly and the optimizations from modern compile toolchain.
  (Thanks to mingkuang.)
- Add the context menu support in Windows 10/11 File Explorer. (Thanks to 
  shiroshan.)
- New icons. (Designed by Alice (四月天), Chi Lei, Kenji Mouri, Rúben Garrido 
  and Sakura Neko. Thanks to AndromedaMelody and 奕然.)
- Minor tweaks. (Thanks to adrianghc, Blueberryy, ChuckMichael, Legna, Maicol 
  Battistini, SakuraNeko and Zbynius.)
- Update 7-Zip from 21.03 to 21.06. (Noticed by Dan, lychichem and sanderdewit.
  Thanks to Igor Pavlov.)
- Enable parsing the NSIS script support in the NSIS archives. (Suggested by 
  alanfox2000. Thanks to myfreeer.)
- Merge features from 7-Zip ZStandard branch. (Suggested by fcharlie. Thanks to
  Tino Reichardt.)
