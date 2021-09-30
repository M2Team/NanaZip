# Changelog

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
