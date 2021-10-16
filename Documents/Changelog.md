# Changelog

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

- Remove IObjectWithSite in shell extension  implementation to reduce the 
  complexity and crashes.
- Add altform-lightunplated assets for display the contrast standard mode icon
  in the taskbar instead of contrast white icon. (Thanks to StarlightMelody.)
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
