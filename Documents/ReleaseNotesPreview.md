# NanaZip Preview Release Notes

For stable versions, please read [NanaZip Release Notes](ReleaseNotes.md).

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
