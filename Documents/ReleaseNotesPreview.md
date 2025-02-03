# NanaZip Preview Release Notes

For stable versions, please read [NanaZip Release Notes](ReleaseNotes.md).

**NanaZip 5.1 Preview 0 (5.1.1252.0)**

This release includes all the improvements from NanaZip 5.0 Update 1
(5.0.1252.0).

**NanaZip 5.0 Preview 2 (5.0.1243.0)**

- Continue to improve the work-in-progress NanaZip Platform Abstraction Layer
  (K7Pal). (https://github.com/M2Team/NanaZip/tree/main/K7Pal)
  - Fix the crash and encrypted RAR5 format extraction wrong password issue for
    7-Zip's hash algorithms wrappers which mentioned in
    https://github.com/M2Team/NanaZip/issues/542. (Thanks to RuesanG.)
  - Introduce K7PalHashDuplicate API.
  - Remove K7PalHashReset API for improving the security.
  - Reduce the saved information in hash contexts to improve the security.
  - Use NanaZip Platform Abstraction Layer (K7Pal) to implement 7-Zip's SHA-1
    hash algorithms.
- Update zh-Hans and zh-Hant translations for 'Want * History' strings. 
  (Contributed by R-YaTian.) (Forget to mentioned in NanaZip 5.0 Preview 1.)
- Adds a setting for opening the folder after extracting from archive.
  (Contributing by DaxDupont.)
- Introduce the Smart Extraction. (Contributed by R-YaTian.)
- Fix the issue that which NanaZip windows and dialogs will be opened in the
  background when using NanaZip from context menu. (Contributed by R-YaTian.)
- Update xxHash to v0.8.3.
  (https://github.com/Cyan4973/xxHash/releases/tag/v0.8.3)

**NanaZip 5.0 Preview 1 (5.0.1215.0)**

- Introduce NanaZip Platform Abstraction Layer (K7Pal) for wrapping the platform
  specific infrastructures. (https://github.com/M2Team/NanaZip/tree/main/K7Pal)
  (Work In Progress)
  - Provide hash functions interfaces implemented with Windows CNG API. NanaZip
    uses these hash functions from K7Pal:
    - MD2
    - MD4
    - MD5
    - SHA-1
    - SHA-256
    - SHA-384
    - SHA-512
    - ED2K (Implemented as the K7Pal MD4 wrapper in NanaZip.Codecs.)
- Synchronize the 7-Zip mainline implementations to 24.09.
  (https://github.com/ip7z/7zip/releases/tag/24.09) (Thanks to Igor Pavlov.
  Noticed by FadeMind and peashooter2.)
- Finally move NanaZip console version to NanaZip.Core project.
- Don't fail ModernSHBrowseForFolderW when DefaultFolder cannot be set.
  (Contributed by dinhngtu.)
- Update Mile.Windows.UniCrt to 1.1.278.

**NanaZip 5.0 Preview 0 (5.0.1188.0)**

- This release includes all the improvements from NanaZip 3.1 (3.1.1080.0)
  (https://github.com/M2Team/NanaZip/releases/tag/3.1.1080.0).
- Update Brazilian Portuguese translation. (Contributed by maisondasilva.)
- Make sure NanaZip Core (except the Self Extracting Executables) and NanaZip
  Classic using ucrtbase.dll with 10.0.19041.0 version or later.
- Update Mile.Windows.Helpers to 1.0.671.
    (https://github.com/ProjectMile/Mile.Windows.Helpers/tree/1.0.671.0)
- Move NanaZip console version to NanaZip.Core project. (Not used in NanaZip
  MSIX package because we need to release the next preview to contain the
  CVE-2024-11477 fix in NanaZip 3.1.)
- Remove C++/WinRT dependency for NanaZip.Codecs and NanaZip.Frieren.
- Add GetDpiForWindowWrapper for NanaZip.Frieren.DarkMode to fix the legacy
  Windows compatibility issues.
- Remove VC-LTL dependency for the Self Extracting Executables related projects.
- Adjust the compilation configurations to optimize the binary size for the Self
  Extracting Executables.
- Use Mile.Windows.UniCrt (https://github.com/ProjectMile/Mile.Windows.UniCrt)
  instead of VC-LTL.
- Update NanaZip.Specification.SevenZip header file.
- Start to simplify the NanaZip specific decoders and encoders implementation.
- Synchronize the BLAKE3 implementation to 1.5.5.
  (https://github.com/BLAKE3-team/BLAKE3/releases/tag/1.5.5)
- Synchronize the RHash implementation to the latest master branch which is
  after v1.4.5.
  (https://github.com/rhash/RHash/commit/cf2adf22ae7c39d9b8e5e7b87222046a8f42b3dc)
- Enable disabling child process creation for NanaZip Self Extracting
  Executables. (Except installer mode of Self Extracting Executables, which
  compiled binaries is not provided in the NanaZip MSIX package.)

**NanaZip 3.5 Preview 0 (3.5.1000.0)**

This release includes all the improvements from NanaZip 3.0 Update 1
(3.0.1000.0).

**NanaZip 3.5 Preview 0 (3.5.996.0)**

This release includes all the improvements from NanaZip 3.0 (3.0.996.0).

**NanaZip 3.0 Preview 0 (3.0.756.0)**

- Rewrite and split the implementation of the Core Library and the Self
  Extracting Executable to the separate NanaZip.Core project.
- Ensure the implementation of Core Library and the Self Extracting Executable
  supports Windows Vista RTM (Build 6000.16386).
- Reduce the binary size for the Self Extracting Executables.
- Synchronize the 7-Zip mainline implementations to 23.01.
  (https://www.7-zip.org/history.txt)
- Synchronize the 7-Zip ZS implementations to the latest master branch.
  (https://github.com/mcmilk/7-Zip-zstd/commit/ce27b4a0d3a94313d256c3d077f1784baffb9eee)
- Add the SM3 HASH algorithm from GmSSL.
  (https://github.com/guanzhi/GmSSL)
- Synchronize Zstandard and builtin xxHash implementations to v1.5.5.
  (https://github.com/facebook/zstd/releases/tag/v1.5.5)
- Synchronize Brotli implementations to v1.1.0.
  (https://github.com/google/brotli/releases/tag/v1.1.0)
- Synchronize the RHash implementation to the latest master branch.
  (https://github.com/rhash/RHash/commit/b8c91ea6551e99e10352386cd46ea26973bb4a4d)
- Update to Git submodule version of Mile.Project.Windows.
  (https://github.com/ProjectMile/Mile.Project.Windows)
- Update Mile.Windows.Helpers to 1.0.15.
  (https://github.com/ProjectMile/Mile.Windows.Helpers/commit/b522a956f7dd42dc205869d362f96a777bcb2aa0)
- Update Mile.Xaml to 2.1.661.
  (https://github.com/ProjectMile/Mile.Xaml/releases/tag/2.1.661.0)
- Update russian translation. (Contributed by Blueberryy.)
- Fix the text wrapping issue in the About dialog. (Thanks to MenschenToaster.)
- Use modern IFileDialog for folder picker dialog. (Contributed by 
  reflectronic.)
- Launch directly to the settings page of association for NanaZip. (Contributed
  by AndromedaMelody.)
- Show NanaZip in Drives' ContextMenu. (Contributed by AndromedaMelody.)
- Sync file extension support from https://github.com/mcmilk/7-Zip-zstd.
- Add other methods to compression dialog.
  (https://github.com/mcmilk/7-Zip-zstd/commit/cf29d0c1babcd5ddf2c67eda8bb36e11f9c643b9)
- Reorder initialization in constructor matching to member declaration order.
  (https://github.com/mcmilk/7-Zip-zstd/commit/8b011d230f1ccd8990943bd2eaad38d70e6e3fdf)
- Fix selectable uppercase / lowercase hash formatting.
  (https://github.com/mcmilk/7-Zip-zstd/commit/4fae369d2d6aa60e2bb45eea1fb05659a2599caa)
- Other adjustments for project development.

**NanaZip 2.1 Preview 0 (2.1.451.0)**

- Update icons. (Designed by Shomnipotence. Updated in NanaZip 2.0 Stable.)
- Add instructions for installing NanaZip for all users. (Updated in NanaZip
  2.0 Stable.)
- Add Mile.Xaml to NanaZip project. (Updated in NanaZip 2.0 Stable.)
- Refresh the about dialog with XAML Islands. (Updated in NanaZip 2.0 Stable.)
- Start adding prerequisite support for unpackaged mode. (Updated in NanaZip 
  2.0 Stable.)
- Optimize NanaZip binaries via adjusting the WindowsTargetPlatformMinVersion
  to 10.0.19041.0 in all packaged NanaZip binaries projects except the Self
  Extracting Executable stubs projects. (Updated in NanaZip 2.0 Update 1.)
- Opt out of dynamic code mitigation on main NanaZip thread for resolving the
  compatibility issues with Explorer Patcher. (Contributed by dinhngtu. Updated
  in NanaZip 2.0 Update 1.)
- Update Mile.Xaml to 1.1.434. (Updated in NanaZip 2.0 Update 1.)
  (https://github.com/ProjectMile/Mile.Xaml/releases/tag/1.1.434.0)
- Update Mile.Windows.Helpers to 1.0.8. (Updated in NanaZip 2.0 Update 1.)
  (https://github.com/ProjectMile/Mile.Windows.Helpers/commits/main)
- Add dark mode support for context menus. (Updated in NanaZip 2.0 Update 1.)
- Refresh the about dialog with Windows 11 XAML control styles and the 
  immersive Mica effects. (Updated in NanaZip 2.0 Update 1.)
- Fix model dialog style behavior for About dialog. (Updated in NanaZip 2.0
  Update 1.)
- Continue to refresh application and file type icons. (Designed by 
  Shomnipotence. Updated in NanaZip 2.0 Update 1.)

**NanaZip 2.0 Preview 2 (2.0.376.0)**

- Update 7-Zip to 22.01. (Thanks to Igor Pavlov. Noticed by HylianSteel, 
  Random-name-hi and DJxSpeedy.)
- Add hfsx to file type association. (Suggested by AndromedaMelody.)
- Update the minimum system requirement to Windows 10 Version 2004 (Build 19041)
  or later for solving issues in the XAML Islands.
- Update LZ4 to v1.9.4.
- Enable Package Integrity. (Contributed by AndromedaMelody.)
- Don't enable "Disable dynamic code generation" mitigation in Debug builds for
  solving codec load error issue. (Thanks to AndromedaMelody.)
- Continue to enable several security mitigations.
  - Enable EH Continuation Metadata.
  - Enable Signed Returns.
- Generate resource identities for package manifest manually. (Suggested by 
  AndromedaMelody.)
- Add workaround for NanaZip not appearing in classic context menu. 
  (Contributed by dinhngtu.)
- Check 7z compression parameter validity upon start of compression.
  (Contributed by dinhngtu.)
- Update icons. (Designed by Shomnipotence.)

**NanaZip 2.0 Preview 1 (2.0.313.0)**

- Fix the shell extension issue which cause Everything crashed. (Thanks to 
  No5972, startkkkkkk, SakuraNeko, bfgxp and riverar.)
- Allow NanaZip to be associated with any file type. (Contributed by 
  manfromarce.)
- Update 7-Zip to 22.00. (Thanks to Igor Pavlov. Noticed by HylianSteel.)
- Integrate the following HASH algorithms to NanaZip from RHash and xxHash.
  - AICH
  - BLAKE2b
  - BTIH
  - ED2K
  - EDON-R 224, EDON-R 256, EDON-R 384, EDON-R 512
  - GOST R 34.11-94, GOST R 34.11-94 CryptoPro
  - GOST R 34.11-2012 256, GOST R 34.11-2012 512
  - HAS-160, RIPEMD-160
  - SHA-224
  - SHA3-224, SHA3-256, SHA3-384, SHA3-512
  - Snefru-128, Snefru-256
  - Tiger, Tiger2
  - TTH
  - Whirlpool
  - XXH3_64bits, XXH3_128bits
- Update Zstandard to 1.5.2.
- Update BLAKE3 to 1.3.1.
- Improve the Per-Monitor DPI Awareness support in Windows 10 Version 1607 for
  Self Extracting Executable stubs.
- Fix line break issue for i18n resource files. (Thanks to ygjsz.)
- Enable several security mitigations. (Contributed by dinhngtu.)
  - Enable Control Flow Guard (CFG) to all target binaries for mitigating ROP 
    attacks.
  - Mark all x86 and x64 target binaries as compatible with Control-flow 
    Enforcement Technology (CET) Shadow Stack.
  - Strict handle checks at runtime to block the use of invalid handles.
  - Disable dynamic code generation prevents generating malicious code at
    runtime.
  - Block loading unexpected libraries from remote sources at runtime.

**NanaZip 1.2 Update 1 Preview 1 (1.2.253.0)**

- Fix the issue of the i18n implementation of File Type Association. 
  (Contributed by AndromedaMelody. Updated in NanaZip 1.2 Stable.)
- Add i18n support for GUI edition of Self Extracting Executable. 
  (Contributed by AndromedaMelody. Updated in NanaZip 1.2 Stable.)

**NanaZip 1.2 Preview 4 (1.2.225.0)**

- Continue to update ModernWin32MessageBox for solving the infinite loop issue
  in some cases. (Thanks to AndromedaMelody.)
- Fix crash issue when opening archive files. (Thanks to 1human and Maicol 
  Battistini.)
- Remove Language page in Options dialog because NanaZip will follow the 
  language settings from Windows itself.

**NanaZip 1.1 Servicing Update 1 Preview 3 (1.1.220.0)**

- Modernize the i18n implementation via migrating language files from .txt to 
  .resw. (Contributed by AndromedaMelody. Suggested by Maicol Battistini.)
- Update ModernWin32MessageBox for solving the infinite loop issue in some 
  cases. (Thanks to AndromedaMelody.)
- Tweak icons and provide icons for preview versions. (Designed by Alice 
  (四月天). Thanks to StarlightMelody.)

**NanaZip 1.1 Servicing Update 1 Preview 2 (1.1.201.0)**

- Fix no ordinal 345 was found in the dynamically attached library issue in 
  Self Extracting Executables. (Thanks to FadeMind.)
- Add Per-Monitor DPI-Aware support for all GUI components.
- Adjust and simplify the compiler options for modernizing.
- Fix i18n issue for the About dialog. (Thanks to AndromedaMelody.)
- Update installation tutorial. (Suggested by AndromedaMelody.)
- Fix cannot start editor issue when only store edition of notepad existed. 
  (Thanks to AndromedaMelody.)

**NanaZip 1.1 Servicing Update 1 Preview 1 (1.1.196.0)**

- Simplify the file type association definitions and add the open verb for 
  them. (Thanks to Fabio286. Fixed in NanaZip 1.1 Stable.)
- Update VC-LTL to 5.0.4. (Updated in NanaZip 1.1 Stable.)

**NanaZip 1.1 Preview 2 (1.1.153.0)**

- Fix the issue which can't load context menu properly. (Thanks to DJxSpeedy.)

**NanaZip 1.1 Preview 2 (1.1.152.0)**

- Reimplement the about dialog with Task Dialog.
- Update Deutsch translations. (Contributed by Hen Ry.)
- Add assembly implementations from 7-Zip back for improving performance.
- Update translations inherited from 7-Zip.
- Update 7-Zip to 21.07. (Thanks to Igor Pavlov. Noticed by HylianSteel.)
- Improve the multi volume rar file detection for solving 
  https://github.com/M2Team/NanaZip/issues/82. (Thanks to 1human.)
- Modernize the message boxes with Task Dialog.

**NanaZip 1.1 Preview 1 (1.1.101.0)**

- Exclude .webp in the archive file type list for solving 
  https://github.com/M2Team/NanaZip/issues/57. (Thanks to Zbynius. Fixed in 
  NanaZip 1.0 Stable.)
- Update Polish translation. (Contributed by ChuckMichael.)
- Fix CI issue.
- Update VC-LTL to 5.0.3.
- Update C++/WinRT to 2.0.211028.7.

**NanaZip 1.0 Preview 4 (1.0.88.0)**

- Update Italian, Russian and Polish Translations. (Contributed by Blueberryy, 
  Maicol Battistini and ChuckMichael.)
- Provide 7-Zip execution alias for helping users to migrate to NanaZip. 
  (Suggested by AndromedaMelody.)
- Adjust file association icon. (Suggested by 奕然.)
- Merge features from 7-Zip ZStandard branch. (Suggested by fcharlie. Thanks to
  Tino Reichardt.)
- Update 7-Zip to 21.06. (Noticed by Dan, lychichem and sanderdewit. Thanks to 
  Igor Pavlov.)
- Fix compression level display issue in the compress dialog. (Thanks to 
  SakuraNeko.)
- Make every file extension have own file type in file type association
  definitions for solving https://github.com/M2Team/NanaZip/issues/53. (Thanks 
  to oxygen-dioxide.)
- Disable virtualization:ExcludedDirectories for resolve 
  https://github.com/M2Team/NanaZip/issues/34. (Thanks to AndromedaMelody.)
- Reduce the compilation warnings.
- Change the configuration for NanaZipPackage project for solve the issue when
  referencing the Windows Runtime Components.
- Update Mile.Cpp.

**NanaZip 1.0 Preview 3 (1.0.46.0)**

- Enable parsing the NSIS script support in the NSIS archives. (Suggested by 
  alanfox2000. Thanks to myfreeer.)
- Simplify the separator layout in the context menu implementation.
- Fix app still displays in folder context menu, resulting in empty entry 
  that doesn't do anything when no options that could interact. (Thanks to 
  shiroshan.)
- Fix the application crash in some cases caused by some issues in the 
  exception handler implementation from VC-LTL 5.x. (Thanks to mingkuang.)
- Update new icons. (Designed by Alice (四月天), Chi Lei, Kenji Mouri, Rúben
  Garrido and Sakura Neko.)
- Make main NanaZip package contains all resources.
- Fix the command line help string. (Thanks to adrianghc.)

**NanaZip 1.0 Preview 2 (1.0.31.0)**

- Remove IObjectWithSite in shell extension implementation to reduce the 
  complexity and crashes.
- Add altform-lightunplated assets for display the contrast standard mode icon
  in the taskbar instead of contrast white icon. (Thanks to AndromedaMelody.)
- Remove Windows.Universal TargetDeviceFamily for solving the minimum OS 
  requirements display issue. (Thanks to 青春永不落幕.)
- Enable NanaZipC and NanaZipG in AppX manifest. (Thanks to be5invis.)
- Change "The operation can require big amount of RAM (memory)" error dialog to
  warning dialog. (Thanks to Legna.)

**NanaZip 1.0 Preview 1 (1.0.25.0)**

- Modernize the build toolchain with MSBuild for using MSIX packaging and 
  parallel compilation support.
- Use [VC-LTL 5.x](https://github.com/Chuyu-Team/VC-LTL5) toolchain to make the
  binary size even smaller than the official 7-Zip because we can use 
  ucrtbase.dll directly and the optimizations from modern compile toolchain.
- Add the context menu support in Windows 10/11 File Explorer.
- New icons and minor UI tweaks.
