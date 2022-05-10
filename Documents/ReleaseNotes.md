# NanaZip Release Notes

For preview versions, please read 
[NanaZip Preview Release Notes](ReleaseNotesPreview.md).

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
