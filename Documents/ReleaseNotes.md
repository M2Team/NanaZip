# NanaZip Release Notes

For preview versions, please read 
[NanaZip Preview Release Notes](ReleaseNotesPreview.md).

**NanaZip 1.1 (1.1.193.0)**

- Add assembly implementations from 7-Zip back for improving performance.
- Reimplement the about dialog with TaskDialog.
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
- Update VC-LTL to 5.0.4-Beta3.
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
