# NanaZip Release Notes

For preview versions, please read 
[NanaZip Preview Release Notes](ReleaseNotesPreview.md).

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
