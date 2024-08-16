# Development Roadmap (Archived)

These will be migrated to the new roadmap issue in the future.

- Blue Moon (3.x) Series (Before 2024 Q3)
  - [ ] Continue to modernize the UI with XAML Islands with the Windows 11 
        control style, dark and light mode support.
  - [ ] Full Accessibility support in all UI components.
  - [ ] Migrate configurations from registry to json. (Suggested by 
        AndromedaMelody.)
  - [ ] Continue to modernize the core implementation.
  - [ ] Try to Windows Runtime component for interoperability.
  - [ ] Provide NanaZip Installer for simplify the deployment.
  - [ ] Try to add option for save file names with UTF-8 in 7z archives.
  - [ ] Add batch task support. (Suggested by 刘泪.)
  - [ ] Try to design the new UI layout. (Suggested by wangwenx190.)
  - [ ] Try to add option for using Windows Imaging API (WIMGAPI) backend to 
        make better creation and extraction support for wim archives, also add 
        creation and extraction support for esd archives.
  - [ ] Try to add pri archive extracting support.
  - [ ] Try to add smart extraction.
  - [ ] Try to add language encoding switching support for file names in File
        Manager. (Suggested by 刘泪 and zjkmxy.)
  - [ ] Try to add deleting source after archiving support. (Suggested by 
        OrionGrant.)
  - [ ] Try to add an option when extracting an archive to open the folder 
        where you extracted the files, like WinRAR. (Suggested by maicol07.)
  - [ ] Try to add ISO creation support.
- Sherlock Holmes (5.x) Series (Before 2025 Q3)
  - Currently no new feature plans for this series.
- Unpredictable Future Series (T.B.D.)
  - [ ] Try to create a new archive file format optimized for software 
        distribution and image backup and restore.
    - [ ] Keeping metadata provided by file system.
    - [ ] File referencing support.
    - [ ] Integrity verification support.
    - [ ] Differential support.
    - [ ] Recovery record support.
    - [ ] Provide lightweight SDK for authoring and consuming.
  - [ ] Try to contribute recovery record support for 7z archives to 7-Zip 
        mainline. (Suggested by SakuraNeko.)
  - [ ] Try to port NanaZip to Linux.
    - [ ] Try to port MegaUI (an developing lightweight UI framework created by
          mingkuang, under internal developing at the current stage, will be 
          open source if the work has done) framework to Linux.
    - [ ] Try to create MinLin (a.k.a. Minimum Linux, a distro intended for 
          helping publish distroless binaries and provide some Windows API 
          functions as static libraries for having a lightweight platform 
          abstraction layer) project because I think NanaZip should support
          distroless environment for reducing time wasting for compiling for
          different distros.
  - [ ] After porting NanaZip to Linux, accept contributions from community 
        folks for other POSIX platforms (e.g. FreeBSD) and macOS support.
  - [ ] Try to add extension for Windows File Explorer for give user immersive 
        experience like builtin zip file support in Windows. (Suggested by 
        SakuraNeko and shuax.)
  - [ ] Try to create isolated and portable plugin infrastructure based on
        64-bit RISC-V Unprivileged ISA Specification.
        Runtime Project: https://github.com/ChaosAIOfficial/RaySoul
