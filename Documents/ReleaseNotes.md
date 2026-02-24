# NanaZip Release Notes

For preview versions, please read 
[NanaZip Preview Release Notes](ReleaseNotesPreview.md).

**NanaZip 6.0 Update 1 (6.0.1638.0)**

- Fix several issues for the UFS/UFS2 file system image readonly support.
  (Cooperated with HO-9.)
- Fix several issues for the .NET Single File Application bundle readonly
  support. (Cooperated with HO-9.)
- Disable text wrapping for textbox control in XAML information dialog to show
  information better.
- Make the hash calculation result output text format more compact to improve
  the copy and paste experience.
- Fix the Win32 TaskDialog-based Folders History dialog not functioning when
  users navigate to the desktop virtual folder.
- Update Japanese translation. (Contributed by noangel.)
- Update Brazilian Portuguese translation. (Contributed by maisondasilva.)

**NanaZip 6.0 (6.0.1632.0)**

- Notes
  - Start to transition to community driven development model.
  - Update system requirement to Windows 10, version 2004 (Build 19041) or
    later, and remove the x86 (32-bit) for all components.
  - Start to provide the official portable binaries package of NanaZip with both
    Modern and Classic flavors, except the NanaZip File Manager.
  - Introduce formal version of contributing guide and security policies.
    (Cooperated with dinhngtu and dongle-the-gadget.)
  - Add Release Tags to NanaZip Versioning.
- Features
  - Introduce extract-on-open feature. (Contributed by dinhngtu.)
    - To disable this temporarily, hold Shift while opening the file.
    - To disable this permanently, go to Settings, Integration tab, and disable
      "Extract on open" at the bottom.
  - Introduce registry settings for Mark of the Web (MoTW) enforcement policies.
    (Contributed by dinhngtu.)
  - Improve the dynamic library block list for resolving compatibility issues.
    (Contributed by dinhngtu.)
  - Introduce registry settings for disabling security mitigation policies,
    which should be helpful for debugging and resolving compatibility issues.
    (Contributed by dinhngtu.)
  - Introduce registry settings for the archive handler and codec handler
    restriction. (Contributed by dinhngtu.)
  - Introduce Group Policy Administrative Template (ADMX/ADML) for NanaZip.
    (Contributed by dinhngtu.)
  - Set the LOAD_LIBRARY_SEARCH_SYSTEM32 dependent load flag on Release builds
    of Self Extracting Executables stubs to mitigate static imports level DLL
    planting attack on Windows 10 build 14393 and later. (Contributed by
    dinhngtu.)
  - Introduce the XAML-based address bar. (Contributed by dongle-the-gadget.)
  - Introduce the XAML-based status bar. (Contributed by dongle-the-gadget.)
  - Introduce the XAML-based properties and information dialog. (Contributed by
    dongle-the-gadget.)
  - Introduce the XAML-based progress dialog. (Contributed by 
    dongle-the-gadget.)
  - Introduce extract all automatically policy when opening executable files in
    archives. (Contributed by MajThomas.)
  - Introduce the environment variable parsing support for address bar. (Thanks
    to dongle-the-gadget's huge help.)
  - Add support for CBR and CBZ file associations. (Contributed by dinhngtu.)
  - Add support for ASAR file association. (Contributed by dongle-the-gadget.)
  - Display file system version in archive property window for the UFS/UFS2 file
    system image readonly support.
  - Display bundle version in archive property for the .NET Single File
    Application bundle readonly support.
  - Display file system version in archive property for the littlefs file system
    image readonly support.
  - Allow to associate more file types to NanaZip. (Contributed by manfromarce.)
  - Enable Ctrl+Backspace on edit fields using SHAutoComplete. (Contributed by
    dinhngtu.)
  - Introduce NanaZip.Universal.Windows project for making NanaZip Command Line
    Interface (Windows) for both NanaZip (Modern) and NanaZip Classic a.k.a.
    NanaZipG or K7G to synchronize to the latest 7-Zip mainline and 7-Zip ZS
    implementations.
  - Introduce NanaZip Platform Base Library (K7Base) and NanaZip Platform User
    Library (K7User) to replace NanaZip Platform Abstraction Layer (K7Pal) and
    NanaZip.Frieren for better code sharing and maintainability.
  - Create NanaZip.Modern project for the modern user experience development.
  - Introduce Mile.Mobility dependence to NanaZip.Codecs project for making
    better portability between NanaZip and AptxZip (a.k.a. NanaZip for POSIX).
  - Introduce NanaZip.ExtensionPackage project for future development of
    features which need to be distributed as legacy installer. (Contributed by
    dinhngtu.)
  - Introduce Mile.Helpers.Portable.Base.Unstaged as the temporary
    infrastructure to standardize some portable things in NanaZip.Codecs.
  - Introduce NanaZip.Codecs.Specification.Fat for adding definitions of FAT12,
    FAT16 and FAT32.
  - Introduce NanaZip.Codecs.Specification.Zealfs for adding definitions of
    ZealFS series file system. (Cooperated with Zeal 8-bit.)
  - Add littlefs v1 on-disk definitions to the littlefs file system image
    readonly support.
  - Move NanaZip.Core's NanaZip.Core.Console project to NanaZip.Universal's
    NanaZip.Universal.Console project.
- Improvements
  - Start to use Zstandard decoder instead of 7-Zip mainline's implementation
    for better reliability, especially it can reduce more potential
    vulnerabilities.
  - Use x-generate to define language resources in AppX manifest to solve the
    Windows AppX language fallback issue. (Suggested by dongle-the-gadget.)
  - Make texts have better contrast in dark mode.
  - Improve the Smart Extraction feature. (Contributed by R-YaTian.)
  - Improve the NanaZip startup performance by initializing the StoreContext
    later. (Contributed by dongle-the-gadget.)
  - Move open folder checkbox to extract dialog. (Contributed by dinhngtu.)
  - Update Albanian translation. (Contributed by F1219R.)
  - Update Hungarian translation. (Contributed by John Fowler.)
  - Update Greek translation. (Contributed by Lefteris T.)
  - Update German translation. (Contributed by HackZy01.)
  - Update Bengali translation. (Contributed by Sumon Kayal.)
  - Update Dutch translation. (Contributed by Stephan-P.)
  - Localize Open Inside menu text. (Contributed by dinhngtu.)
  - Remove Help button from compress option dialogs. (Contributed by
    peashooter2.)
  - Update the icon assets with optimized assets to reduce the binary size.
  - Use Win32 TaskDialog to implement the Folders History dialog to simplify the
    implementation.
  - Synchronize 7-Zip mainline implementations to 26.00. (Except the NanaZip
    File Manager.) (Thanks to Igor Pavlov. Noticed by FadeMind and Pinguin2001.)
  - Synchronize the 7-Zip ZS implementations to
    https://github.com/mcmilk/7-Zip-zstd/tree/5766dd7745f6517f7ea42f6de9a190dfd92aa25f.
    (Except the NanaZip File Manager.) (Thanks to Sergey G. Brester and Tino
    Reichardt.)
  - Make FastLZMA2 and Lizard reuse partial Zstandard implementations.
  - Update Zstandard to v1.5.7. (Noticed by dcog989.)
  - Update Lizard to v2.1.
  - Synchronize Brotli implementations to v1.2.0.
    (https://github.com/google/brotli/releases/tag/v1.2.0)
  - Synchronize the FastLZMA2 implementations to
    https://github.com/conor42/fast-lzma2/tree/a793db99fade2957d2453035390f97e573acecb2,
    which can fix some issues. (Contributed by dinhngtu.)
  - Synchronize the BLAKE3 implementation to 1.8.3. (Noticed by peashooter2.)
    (https://github.com/BLAKE3-team/BLAKE3/releases/tag/1.8.3)
  - Use assembly hardware acceleration for BLAKE3 implementation.
  - Synchronize RHash to the latest master
    (https://github.com/rhash/RHash/tree/b76c6a3312422c09817c2cef40442b2f2d9d4689)
    which is after v1.4.6.
  - Update Mile.Project.Configurations to 1.0.1917, which solve some issues for
    using NuGet Package References Support in Visual Studio 2026 version 18.3 or
    later. (Cooperated by AndromedaMelody.)
  - Update Mile.Project.Helpers to 1.0.975.
  - Update Mile.Windows.Helpers to 1.0.1171.
  - Update Mile.Windows.Internal to 1.0.3515.
  - Update Mile.Xaml to 2.5.1616.
  - Update Mile.Detours to 1.0.2180.
  - Update Mile.Json to 1.0.1057.
  - Update .NET projects to .NET 10 and update NuGet package dependencies.
  - Add littlefs v2.10.2 to NanaZip.Codecs for future development of work in
    progress littlefs archive format readonly support.
  - Update build binary logs when failed to build in GitHub Actions.
    (Contributed by dongle-the-gadget.)
  - Introduce legacy string migrator. (Contributed by dongle-the-gadget.)
  - Add more targeted editorconfig rules. (Contributed by dongle-the-gadget.)
  - Move NanaZip.Core's NanaZip.Core.Console project to NanaZip.Universal's
    NanaZip.Universal.Console project.
  - Disable WinRT metadata generation for all WinRT component consumers.
  - Move NanaZip.Modern as the first item to workaround the AppX toolchain
    manifest generation issues.
  - Improve the GitHub Actions artifacts generation.
  - Update to Windows 11 SDK Build 26100 for NanaZipPackage.
  - Introduce reproducible build support for the whole project.
  - Migrate solution from sln to slnx, and start to build with MSVC 14.50 
    toolset.
  - Support opening NanaZip Visual Studio solution without installing WinUI
    application and/or Universal Windows Platform development workload.
    (Contributed by AndromedaMelody.)
  - Add RestoreNuGetPackages.cmd script to workaround some issues for people who
    want to use Visual Studio to build NanaZip without running
    BuildAllTargets.bat script.
  - Make the precompiled build tools generated by GitHub Actions workflow
    automatically.
  - Improve several implementations.
- Fixes
  - Fix dead loop issue when compressing files with Brotli, Lizard, LZ4 and LZ5
    mentioned in https://github.com/M2Team/NanaZip/issues/639. (Thanks to
    gigano01, InfiniteLoopGameDev and iOrange.)
  - Backport CVE-2025-0411 and CVE-2025-11001 for the NanaZip File Manager which
    still use the old 7-Zip mainline codebase. (Contributed by dinhngtu.)
  - Fix several crash issues for the .NET Single File Application bundle
    readonly support. (Thanks to haaeein.)
  - Fix several hang issues for the ROMFS file system image readonly support
    support. (Contributed by dinhngtu. Thanks to haaeein.)
  - Fix the flickering issues when selecting list view in dark mode.
  - Fix the unable to return the processed bytes count issue for
    NanaZipCodecsReadInputStream.
  - Fix the parsing padding issue for Electron Archive (asar) readonly support.
    (Contributed by Vlad-Andrei Popescu).
  - Improve the ZealFS file system image readonly support implementation to fix
    several issues. (Cooperated with Zeal 8-bit.)
  - Try to bring dialog window to the foreground to resolve user experience
    issues when opened from context menu. (Contributed by dinhngtu.)
  - Fix flickering XAML dialogs. (Contributed by dongle-the-gadget.)
  - Fix crash issue from XAML address bar. (Contributed by dongle-the-gadget.)
  - Use a low name as the shell extension name prefix to work around our context
    menu not appearing in the classic context menu. (Contributed by dinhngtu.)
  - Don't reset compression method after changing level. (Contributed by
    dongle-the-gadget.)
  - Fix text overflow in German translation. (Contributed by Pinguin2001.)
  - Fix UI assignment of WriteZone setting. (Contributed by dinhngtu.)
  - Handle Add button when inside archives. (Contributed by dinhngtu.)
  - Fix issues for NanaZip Preview SVG icon assets for Contrast Black mode.
  - Fix several issues for context menu support. (Contributed by dinhngtu.)
  - Fix some potential issues for the dark mode support.
  - Try to partially improve the NanaZip File Manager main window keyboard
    navigation experience.
  - Fix some string resources issues. (Contributed by dinhngtu.)
  - Fix issues in compression levels combobox. (Contributed by dongle-the-gadget.)
  - Fix the source code file header comment format.
  - Use C++ zero initialization to reduce potential issues.
  - Fix several issues.

**NanaZip 5.0 Update 2 (5.0.1263.0)**

- Features
  - Provide K7 style execution aliases (K7.exe, K7C.exe, and K7G.exe) for
    simplifying the command line user experience and providing the same command
    name style in work-in-progress NanaZip for POSIX a.k.a. AptxZip.
- Improvements
  - Update Mile.Windows.UniCrt to 1.2.328.
  - Update Mile.Xaml to 2.5.1250.
  - Migrate from Mile.Project.Windows to Mile.Project.Configurations.
- Fixes
  - Fix crash issue when using Open Inside # mode for the WebAssembly (WASM)
    binary file readonly support for NanaZip.Codecs.
  - Fix crash issue when using Open Inside # mode for the .NET Single File
    Application bundle readonly support for NanaZip.Codecs.
  - Fix crash issue when using Open Inside # mode for the Electron Archive
    (asar) readonly support for NanaZip.Codecs.
  - Fix the crash issue when extracting *.br archives. (Thanks to HikariCalyx.)
  - Fix the XXH3_128bits printable results with wrong byte order. (Thanks to
    fuchanghao.)

**NanaZip 5.0 Update 1 (5.0.1252.0)**

- Features
  - Introduce the .NET Single File Application bundle readonly support which
    currently extracting compressed files in the bundle are not supported.
  - Introduce the Electron Archive (asar) readonly support.
  - Introduce the ROMFS file system image readonly support.
  - Introduce the ZealFS file system image readonly support.
  - Introduce the WebAssembly (WASM) binary file readonly support.
  - Introduce the **Work In Progress** littlefs file system image readonly
    support which currently only block information can acquired.
- Improvements
  - Update Ukrainian and Russian translation. (Contributed by SlowDancer011.)
  - Update Hungarian translation. (Contributed by JohnFowler58.)
  - Update packages for maintainer tools.
- Fixes
  - Fix the empty folders are excluded issue for the UFS/UFS2 file system image
    readonly support.
  - Fix the unavailable issue when cancelling the extraction for the UFS/UFS2
    file system image readonly support.

**NanaZip 5.0 (5.0.1250.0)**

- Features
  - Introduce the UFS/UFS2 file system image readonly support. (Thanks to
    NishiOwO.)
  - Introduce work-in-progress NanaZip Platform Abstraction Layer (K7Pal) for
    wrapping the platform specific infrastructures. (Thanks to RuesanG's 
    feedback.) (https://github.com/M2Team/NanaZip/tree/main/K7Pal)
    - Provide hash functions interfaces implemented with Windows CNG API.
      NanaZip uses these hash functions from K7Pal:
      - MD2
      - MD4
      - MD5
      - SHA-1
      - SHA-256
      - SHA-384
      - SHA-512
      - ED2K (Implemented as the K7Pal MD4 wrapper in NanaZip.Codecs.)
  - Update NanaZip.Specification.SevenZip header file.
  - Introduce the Smart Extraction. (Contributed by R-YaTian.)
  - Adds a setting for opening the folder after extracting from archive.
    (Contributing by DaxDupont.)
- Improvements
  - Synchronize the 7-Zip mainline implementations to 24.09.
    (https://github.com/ip7z/7zip/releases/tag/24.09) (Thanks to Igor Pavlov.
    Noticed by FadeMind and peashooter2.)
  - Synchronize the BLAKE3 implementation to 1.5.5.
    (https://github.com/BLAKE3-team/BLAKE3/releases/tag/1.5.5)
  - Synchronize the RHash implementation to the latest master branch which is
    after v1.4.5.
    (https://github.com/rhash/RHash/commit/cf2adf22ae7c39d9b8e5e7b87222046a8f42b3dc)
  - Synchronize the xxHash implementation to v0.8.3.
    (https://github.com/Cyan4973/xxHash/releases/tag/v0.8.3)
  - Update Mile.Windows.Helpers to 1.0.671.
    (https://github.com/ProjectMile/Mile.Windows.Helpers/tree/1.0.671.0)
  - Update Brazilian Portuguese translation. (Contributed by maisondasilva.)
  - Update Polish translation. (Contributed by ChuckMichael.)
  - Update zh-Hans and zh-Hant translations for 'Want * History' strings. 
    (Contributed by R-YaTian.) (Forget to mentioned in NanaZip 5.0 Preview 1.)
  - Make sure NanaZip Core (except the Self Extracting Executables) and NanaZip
    Classic using ucrtbase.dll with 10.0.19041.0 version or later.
  - Move NanaZip console version to NanaZip.Core project.
  - Remove C++/WinRT dependency for NanaZip.Codecs and NanaZip.Frieren.
  - Remove VC-LTL dependency for all components, and also use 
    Mile.Windows.UniCrt (https://github.com/ProjectMile/Mile.Windows.UniCrt)
    instead of VC-LTL for non Self Extracting Executables stub components.
  - Adjust the compilation configurations to optimize the binary size for the
    Self Extracting Executables.
  - Start to simplify the NanaZip specific decoders and encoders implementation.
  - Enable disabling child process creation for NanaZip Self Extracting
    Executables. (Except installer mode of Self Extracting Executables, which
    compiled binaries is not provided in the NanaZip MSIX package.)
- Fixes
  - Add GetDpiForWindowWrapper for NanaZip.Frieren.DarkMode to fix the legacy
    Windows compatibility issues.
  - Don't fail ModernSHBrowseForFolderW when DefaultFolder cannot be set.
    (Contributed by dinhngtu.)
  - Fix the issue that which NanaZip windows and dialogs will be opened in the
    background when using NanaZip from context menu. (Contributed by R-YaTian.)

**NanaZip 3.1 (3.1.1080.0)**

- Try to discover the new Sponsor button design but finally reverted to the old
  design for more natural looking. (Contributed by dongle-the-gadget and
  Gaoyifei1011.)
- Improve Simplified Chinese translation. (Contributed by DeepChirp.)
- Improve the Sponsor Edition documentation. (Contributed by sirredbeard.)
- Improve Albanian translation. (Contributed by RDN000.)
- Improve German translation. (Contributed by CennoxX.)
- Fix several dark mode UI style issues. (Contributed by rounk-ctrl.)
- Ignore the text scale factor for solving the UI layout issues.
- Synchronize the 7-Zip mainline implementations to 24.08.
  (https://github.com/ip7z/7zip/releases/tag/24.08) (Thanks to Igor Pavlov.
  Noticed by atplsx and wallrik.)
- Synchronize the LZ4 implementations to 1.10.0.
  (https://github.com/lz4/lz4/releases/tag/v1.10.0)
- Update Mile.Project.Windows submodule to the August 12, 2024 latest.
- Update VC-LTL to 5.1.1.
- Update YY-Thunks to 1.1.2.
- Update Mile.Windows.Helpers to 1.0.645.
- Update Mile.Xaml to 2.3.1064.
- Update Mile.Windows.Internal to 1.0.2971.
- Defer package updates while the app is running in Windows 11 23H2 or later.
  (Suggested by AndromedaMelody.)
- Improve maintainer tools for introducing automatic packaging support when
  building all targets for NanaZip.

**NanaZip 3.0 Update 1 (3.0.1000.0)**

- Update Mile.Windows.Internal to 1.0.2889.
- Make the 7-Zip Zstandard branch's specific options translatable. (Contributed
  by ChuckMichael.)
- Polish translation for Sponsor dialog. (Contributed by ChuckMichael.)
- Fix compatibility issues with iFlyIME, Sogou Pinyin, and Transparent Flyout.
  (Contributed by dinhngtu.)
- Update the UI layout for the sponsor button. (Suggested by namazso.)
- NanaZip will only check the Sponsor Edition addon licensing status the first
  time you launch NanaZip File Manager or click the sponsor button to optimize
  the user experience.
- Update NanaZip installation documentation. (Contributed by dongle-the-gadget.)
- Use Extract dialog when extracting without selection. (Contributed by 
  dinhngtu.)
- Fix tooltips from XAML controls cannot be transparent.
- Fix dark mode UI font rendering issues in some Windows 10 environments.
- Adjust the dark mode text color for improving the user experience. (Suggested
  by userzzzq.)
- Synchronize the 7-Zip mainline implementations to 24.06.
  (https://github.com/ip7z/7zip/releases/tag/24.06) (Thanks to Igor Pavlov.
  Noticed by KsZAO.)

**NanaZip 3.0 (3.0.996.0)**

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
  - Add the SM3 HASH algorithm from GmSSL. (https://github.com/guanzhi/GmSSL)
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
    reflectronic.)
  - Launch directly to the settings page of association for NanaZip.
    (Contributed by AndromedaMelody.)
  - Show NanaZip in Drives' ContextMenu. (Contributed by AndromedaMelody.)
  - Sync file extension support from https://github.com/mcmilk/7-Zip-zstd.
  - Add other methods to compression dialog.
    (https://github.com/mcmilk/7-Zip-zstd/commit/cf29d0c1babcd5ddf2c67eda8bb36e11f9c643b9)
  - Reorder initialization in constructor matching to member declaration order.
    (https://github.com/mcmilk/7-Zip-zstd/commit/8b011d230f1ccd8990943bd2eaad38d70e6e3fdf)
  - Fix selectable uppercase / lowercase hash formatting.
    (https://github.com/mcmilk/7-Zip-zstd/commit/4fae369d2d6aa60e2bb45eea1fb05659a2599caa)
  - Update russian translation. (Contributed by Blueberryy.)
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
    1)     or later for solving issues in the XAML Islands.
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
