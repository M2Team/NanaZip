# NanaZip 发布日志

| Language       | Link          |
|----------------|---------------|
| English        | [Read](ReleaseNotes.md)  |

如需预览版的发布日志，请参阅 [NanaZip 预览版发布日志](ReleaseNotesPreview.md)。

**NanaZip 5.0 Update 2 (5.0.1263.0)**

- 新功能
  - 提供 K7 风格的执行别名（K7.exe、K7C.exe 和 K7G.exe），以简化命令行用户体验，并为开发中的 POSIX 版 NanaZip（AptxZip）提供一致的命令名称风格。
- 改进
  - 更新 Mile.Windows.UniCrt 至 1.2.328。
  - 更新 Mile.Xaml 至 2.5.1250。
  - 从 Mile.Project.Windows 迁移至 Mile.Project.Configurations。
- 修复
  - 修复 NanaZip.Codecs 在以“Open Inside #”模式打开 WebAssembly (WASM) 二进制文件（只读支持）时崩溃的问题。
  - 修复 NanaZip.Codecs 在以“Open Inside #”模式打开 .NET 单文件应用包（只读支持）时崩溃的问题。
  - 修复 NanaZip.Codecs 在以“Open Inside #”模式打开 Electron Archive (asar)（只读支持）时崩溃的问题。
  - 修复解压 *.br 压缩包时崩溃的问题。（感谢 HikariCalyx）
  - 修复 XXH3_128bits 打印结果字节序错误的问题。（感谢 fuchanghao）

**NanaZip 5.0 Update 1 (5.0.1252.0)**

- 新功能
  - 引入 .NET 单文件应用程序包的只读支持（当前暂不支持解压包内压缩文件）。
  - 引入 Electron Archive (asar) 的只读支持。
  - 引入 ROMFS 文件系统镜像的只读支持。
  - 引入 ZealFS 文件系统镜像的只读支持。
  - 引入 WebAssembly (WASM) 二进制文件的只读支持。
  - 引入 **开发中** 的 littlefs 文件系统镜像只读支持（当前仅可获取块信息）。
- 改进
  - 更新乌克兰语和俄语翻译（贡献者：SlowDancer011）。
  - 更新匈牙利语翻译（贡献者：JohnFowler58）。
  - 更新维护工具相关依赖包。
- 修复
  - 修复 UFS/UFS2 文件系统镜像只读支持下空文件夹被排除的问题。
  - 修复 UFS/UFS2 文件系统镜像只读支持下取消解压时不可用的问题。

**NanaZip 5.0 (5.0.1250.0)**

- 新功能
  - 引入 UFS/UFS2 文件系统镜像的只读支持。（感谢 NishiOwO）
  - 引入开发中的 NanaZip 平台抽象层（K7Pal），用于封装平台相关基础设施。（感谢 RuesanG 的反馈）(https://github.com/M2Team/NanaZip/tree/main/K7Pal)
    - 提供基于 Windows CNG API 实现的哈希函数接口，NanaZip 从 K7Pal 使用如下哈希函数：
      - MD2
      - MD4
      - MD5
      - SHA-1
      - SHA-256
      - SHA-384
      - SHA-512
      - ED2K（在 NanaZip.Codecs 中实现为 K7Pal MD4 封装器）
  - 更新 NanaZip.Specification.SevenZip 头文件。
  - 引入智能解压（Smart Extraction）。（贡献者：R-YaTian）
  - 新增解压后打开文件夹的设置选项。（贡献者：DaxDupont）
- 改进
  - 主线 7-Zip 实现同步至 24.09。（https://github.com/ip7z/7zip/releases/tag/24.09，感谢 Igor Pavlov，FadeMind 与 peashooter2 提供的信息）
  - BLAKE3 实现同步至 1.5.5。（https://github.com/BLAKE3-team/BLAKE3/releases/tag/1.5.5）
  - RHash 实现同步至最新 master（v1.4.5 之后）。（https://github.com/rhash/RHash/commit/cf2adf22ae7c39d9b8e5e7b87222046a8f42b3dc）
  - xxHash 实现同步至 v0.8.3。（https://github.com/Cyan4973/xxHash/releases/tag/v0.8.3）
  - 更新 Mile.Windows.Helpers 至 1.0.671。（https://github.com/ProjectMile/Mile.Windows.Helpers/tree/1.0.671.0）
  - 更新巴西葡萄牙语翻译（贡献者：maisondasilva）。
  - 更新波兰语翻译（贡献者：ChuckMichael）。
  - 更新简体中文和繁体中文翻译中的 'Want * History' 字符串。（贡献者：R-YaTian）（此项在 NanaZip 5.0 Preview 1 中遗漏提及）
  - 确保 NanaZip Core（除自解压文件外）和 NanaZip Classic 均使用 10.0.19041.0 及以上版本的 ucrtbase.dll。
  - NanaZip 控制台版迁移至 NanaZip.Core 项目。
  - NanaZip.Codecs 和 NanaZip.Frieren 移除对 C++/WinRT 的依赖。
  - 所有组件移除 VC-LTL 依赖，非自解压组件均改用 Mile.Windows.UniCrt（https://github.com/ProjectMile/Mile.Windows.UniCrt）。
  - 调整自解压组件的编译配置以优化二进制体积。
  - 开始简化 NanaZip 特有的解码器和编码器实现。
  - 启用 NanaZip 自解压文件禁止创建子进程功能（自解压安装模式除外，相关二进制不包含在 NanaZip MSIX 包中）。
- 修复
  - NanaZip.Frieren.DarkMode 新增 GetDpiForWindowWrapper 以修复旧版 Windows 兼容性问题。
  - 修复 ModernSHBrowseForFolderW 因 DefaultFolder 无法设置而导致失败的问题。（贡献者：dinhngtu）
  - 修复从右键菜单调用 NanaZip 时窗口和对话框会被置于后台的问题。（贡献者：R-YaTian）

**NanaZip 3.1 (3.1.1080.0)**

- 尝试探索新的赞助按钮设计，但最终回退至旧设计以获得更自然的外观。（贡献者：dongle-the-gadget、Gaoyifei1011）
- 优化简体中文翻译。（贡献者：DeepChirp）
- 改进赞助版文档说明。（贡献者：sirredbeard）
- 改进阿尔巴尼亚语翻译。（贡献者：RDN000）
- 改进德语翻译。（贡献者：CennoxX）
- 修复多个深色模式下的界面样式问题。（贡献者：rounk-ctrl）
- 忽略文本缩放系数，解决 UI 布局问题。
- 主线 7-Zip 实现同步至 24.08。（https://github.com/ip7z/7zip/releases/tag/24.08，感谢 Igor Pavlov，atplsx 和 wallrik 的反馈）
- LZ4 实现同步至 1.10.0。（https://github.com/lz4/lz4/releases/tag/v1.10.0）
- Mile.Project.Windows 子模块同步至 2024 年 8 月 12 日最新版。
- VC-LTL 升级至 5.1.1。
- YY-Thunks 升级至 1.1.2。
- Mile.Windows.Helpers 升级至 1.0.645。
- Mile.Xaml 升级至 2.3.1064。
- Mile.Windows.Internal 升级至 1.0.2971。
- 在 Windows 11 23H2 及以上系统中，应用运行时将延后包更新。（建议者：AndromedaMelody）
- 优化维护工具，支持 NanaZip 全目标自动打包。

**NanaZip 3.0 Update 1 (3.0.1000.0)**

- Mile.Windows.Internal 升级至 1.0.2889。
- 使 7-Zip Zstandard 分支特有选项支持多语言翻译。（贡献者：ChuckMichael）
- 赞助弹窗添加波兰语翻译。（贡献者：ChuckMichael）
- 修复与讯飞输入法、搜狗拼音和透明浮窗的兼容性问题。（贡献者：dinhngtu）
- 更新赞助按钮的界面布局。（建议者：namazso）
- NanaZip 现仅在首次启动文件管理器或点击赞助按钮时检查赞助版授权状态，以优化用户体验。
- 更新 NanaZip 安装文档说明。（贡献者：dongle-the-gadget）
- 未选择文件时，使用“解压”对话框进行操作。（贡献者：dinhngtu）
- 修复 XAML 控件的提示气泡无法透明的问题。
- 修复部分 Windows 10 环境下深色模式字体渲染异常的问题。
- 调整深色模式下的文字颜色以提升用户体验。（建议者：userzzzq）
- 主线 7-Zip 实现同步至 24.06。（https://github.com/ip7z/7zip/releases/tag/24.06，感谢 Igor Pavlov，KsZAO 的反馈）

**NanaZip 3.0 (3.0.996.0)**

- 说明
  - NanaZip 3.0 及以后将有两种发行版本：NanaZip 和 NanaZip Classic。但 NanaZip 3.0 尚未提供 Classic 版本，因为还未准备好。详见：https://github.com/M2Team/NanaZip#differences-between-nanazip-and-nanazip-classic
  - NanaZip 打包版不再支持 32 位 x86，因为受支持的 32 位 x86 Windows 已不支持仅运行于 32 位 x86 处理器。
  - NanaZip 3.0 及以后将有 NanaZip 赞助版（Sponsor Edition），详见：https://github.com/M2Team/NanaZip/blob/main/Documents/SponsorEdition.md
  - NanaZip 引入了预装支持，详见：https://github.com/M2Team/NanaZip/issues/398
- 新功能
  - 全部 GUI 组件支持深色模式。
  - 引入 Mica 支持。若开启深色模式且关闭 HDR，所有 GUI 组件可享受全窗口沉浸式 Mica 效果。（感谢 Andarwinux）
  - 主线 7-Zip 实现同步至 24.05（https://github.com/ip7z/7zip/releases/tag/24.05，感谢 Igor Pavlov，AVMLOVER-4885955 和 PopuriAO29 提供信息）
    - NanaZip 自解压执行文件存根使用 7-Zip 主线 Zstandard 解码器替换官方 Zstandard 解码器，以减小二进制体积。
    - NanaZip.Core 项目同样使用主线 Zstandard 解码器。
    - NanaZip.Core 移除主线 XXH64 Hash 处理器，因 NanaZip.Codecs 已有 xxHash 实现，且非 x86 性能更佳。
  - 实现新工具栏，替换旧菜单栏和旧工具栏。
  - 按照其他 Nana 系列项目设计，刷新关于对话框界面布局。
  - 新增 GmSSL 提供的 SM3 哈希算法（https://github.com/guanzhi/GmSSL）。
- 改进
  - 重写并拆分核心库与自解压执行文件实现，分别为 NanaZip.Codecs 和 NanaZip.Core 项目。详见：https://github.com/M2Team/NanaZip/issues/336
  - 确保核心库和自解压执行文件实现支持 Windows Vista RTM（Build 6000.16386）。
  - 缩小自解压执行文件的二进制体积。
  - 7-Zip ZS 实现同步至最新 master（https://github.com/mcmilk/7-Zip-zstd/commit/79b2c78e9e7735ddf90147129b75cf2797ff6522）
  - Zstandard 及内置 xxHash 实现同步至 v1.5.6（https://github.com/facebook/zstd/releases/tag/v1.5.6）
  - Brotli 实现同步至 v1.1.0（https://github.com/google/brotli/releases/tag/v1.1.0）
  - RHash 实现同步至 v1.4.4 之后的最新 master（https://github.com/rhash/RHash/commit/d916787590b9dc57eb9d420fd13e469160e0b04c）
  - BLAKE3 实现同步至 1.5.1 之后的最新 master（https://github.com/BLAKE3-team/BLAKE3/commit/0816badf3ada3ec48e712dd4f4cbc2cd60828278）
  - Mile.Project.Windows 更新为 Git 子模块版（https://github.com/ProjectMile/Mile.Project.Windows）
  - Mile.Windows.Helpers 升级至 1.0.558（https://github.com/ProjectMile/Mile.Windows.Helpers/tree/1.0.558.0）
  - Mile.Xaml 升级至 2.2.944（https://github.com/ProjectMile/Mile.Xaml/releases/tag/2.2.944.0）
  - 使用 Mile.Windows.Internal 包（https://github.com/ProjectMile/Mile.Windows.Internal）
  - 使用 Mile.Detours 包（https://github.com/ProjectMile/Mile.Detours）
  - 文件夹选择器对话框改用现代 IFileDialog 实现（贡献者：reflectronic）
  - 可直接跳转到 NanaZip 文件关联设置页（贡献者：AndromedaMelody）
  - 驱动器右键菜单集成 NanaZip（贡献者：AndromedaMelody）
  - 文件扩展名支持同步自 https://github.com/mcmilk/7-Zip-zstd
  - 压缩对话框新增其他方法（https://github.com/mcmilk/7-Zip-zstd/commit/cf29d0c1babcd5ddf2c67eda8bb36e11f9c643b9）
  - 构造函数按成员声明顺序重排初始化顺序（https://github.com/mcmilk/7-Zip-zstd/commit/8b011d230f1ccd8990943bd2eaad38d70e6e3fdf）
  - 修正哈希结果可选大写/小写输出（https://github.com/mcmilk/7-Zip-zstd/commit/4fae369d2d6aa60e2bb45eea1fb05659a2599caa）
  - 更新俄语翻译（贡献者：Blueberryy）
  - 更新波兰语翻译（贡献者：ChuckMichael）
  - 命令行版本增加禁止创建子进程的防护策略（贡献者：dinhngtu）
  - NanaZip 文件管理器启用 Explorer Patcher DLL 阻断，主线程运行时重新启用远程库加载防护，无副作用（贡献者：dinhngtu）
- 修复
  - 修复 shell 扩展中 IEnumExplorerCommand::Next 的问题（感谢 cnbluefire）
- 其它开发相关调整

**NanaZip 2.0 Update 1 (2.0.450.0)**

- 通过将 WindowsTargetPlatformMinVersion 调整为 10.0.19041.0，优化除自解压执行文件存根项目外所有 NanaZip 打包二进制体积。
- 为解决与 Explorer Patcher 兼容性问题，主线程不再启用动态代码防护（贡献者：dinhngtu）。
- Mile.Xaml 升级至 1.1.434（https://github.com/ProjectMile/Mile.Xaml/releases/tag/1.1.434.0）
- Mile.Windows.Helpers 升级至 1.0.8（https://github.com/ProjectMile/Mile.Windows.Helpers/commits/main）
- 右键菜单支持深色模式。
- 关于对话框采用 Windows 11 XAML 控件风格和沉浸式 Mica 效果。
- 修正 About 对话框的模态弹窗交互行为。
- 持续刷新应用与文件类型图标（设计：Shomnipotence）

**NanaZip 2.0 (2.0.396.0)**

- 说明
  - 将最低系统要求提升至 Windows 10 Version 2004（Build 19041）及以上，以解决 XAML Islands 相关问题。
  - 增加了为所有用户安装 NanaZip 的说明。（贡献者：AndromedaMelody，建议者：Wolverine1977）
- 新功能
  - 集成 RHash（AICH、BLAKE2b、BTIH、ED2K、EDON-R 224、EDON-R 256、EDON-R 384、EDON-R 512、GOST R 34.11-94、GOST R 34.11-94 CryptoPro、GOST R 34.11-2012 256、GOST R 34.11-2012 512、HAS-160、RIPEMD-160、SHA-224、SHA3-224、SHA3-256、SHA3-384、SHA3-512、Snefru-128、Snefru-256、Tiger、Tiger2、TTH、Whirlpool）与 xxHash（XXH3_64bits、XXH3_128bits）算法。
  - 允许 NanaZip 与任意文件类型关联。（贡献者：manfromarce）
  - 文件类型关联新增对 hfsx 的支持。（建议者：AndromedaMelody）
- 改进
  - 刷新应用程序及文件类型图标。（设计：Shomnipotence）
  - 使用 XAML Islands 刷新关于对话框。
  - 7-Zip 升级至 22.01。（https://www.7-zip.org/history.txt，感谢 Igor Pavlov，HylianSteel、Random-name-hi和DJxSpeedy 提供信息）
  - Zstandard 升级至 1.5.2（https://github.com/facebook/zstd/releases/tag/v1.5.2）
  - BLAKE3 升级至 1.3.1（https://github.com/BLAKE3-team/BLAKE3/releases/tag/1.3.1）
  - LZ4 升级至 1.9.4（https://github.com/lz4/lz4/releases/tag/v1.9.4）
  - 所有目标二进制启用控制流保护（CFG），以缓解 ROP 攻击。（贡献者：dinhngtu）
  - 标记所有 x86 和 x64 目标二进制兼容 CET Shadow Stack。（贡献者：dinhngtu）
  - 运行时严格句柄检查，阻止使用无效句柄。（贡献者：dinhngtu）
  - Release 版本禁用动态代码生成，防止运行时生成恶意代码。（贡献者：dinhngtu，感谢 AndromedaMelody）
  - 阻止运行时从远程源加载意外库。（贡献者：dinhngtu）
  - 启用包完整性检查。（贡献者：AndromedaMelody）
  - 启用 EH Continuation Metadata（建议者：dinhngtu，感谢 mingkuang）
  - 启用签名返回保护。
  - 项目新增 Mile.Xaml。
  - 开始添加对解包模式的前置支持。
- 修复
  - 修复导致 Everything 崩溃的外壳扩展问题。（感谢 No5972、startkkkkkk、SakuraNeko、bfgxp、riverar）
  - 提升 Windows 10 Version 1607 下自解压执行文件存根的每显示器 DPI 感知支持。
  - 修复 i18n 资源文件换行符问题。（感谢 ygjsz）
  - 手动为包清单生成资源标识。（建议者：AndromedaMelody）
  - 解决 NanaZip 未出现在经典右键菜单的兼容问题。（贡献者：dinhngtu）
  - 启动压缩时检查 7z 压缩参数合法性。（贡献者：dinhngtu）

**NanaZip 1.2 (1.2.252.0)**

- 修复自解压执行文件“未找到序号 345”问题。（感谢 FadeMind）
- 所有 GUI 组件支持每显示器 DPI 感知。
- 调整并简化编译器选项以实现现代化。
- 修复关于对话框的 i18n 问题。（感谢 AndromedaMelody）
- 更新安装教程。（建议者：AndromedaMelody）
- 修复仅存在应用商店版 notepad 时无法启动编辑器的问题。（感谢 AndromedaMelody）
- 通过将语言文件从 .txt 迁移到 .resw 现代化 i18n 实现。（贡献者：AndromedaMelody，建议者：Maicol Battistini）
- ModernWin32MessageBox 更新，解决部分情况下的无限循环问题。（感谢 AndromedaMelody）
- 微调图标并为预览版提供专用图标。（设计：Alice（四月天），感谢 StarlightMelody）
- 修复打开压缩包时的崩溃问题。（感谢 1human 和 Maicol Battistini）
- 选项对话框移除语言页，NanaZip 现跟随 Windows 的语言设置。
- 修复文件类型关联的 i18n 实现问题。（贡献者：AndromedaMelody）
- 自解压执行文件 GUI 版添加 i18n 支持。（贡献者：AndromedaMelody）

**NanaZip 1.1 (1.1.194.0)**

- 重新引入 7-Zip 的汇编实现以提升性能。
- 使用 Task Dialog 重新实现关于对话框。
- 消息框现代化，采用 Task Dialog。（感谢 DJxSpeedy）
- 7-Zip 升级至 21.07。（感谢 Igor Pavlov，HylianSteel 提供信息）
- 继承自 7-Zip 的翻译更新。
- 德语翻译更新。（贡献者：Hen Ry）
- 波兰语翻译更新。（贡献者：ChuckMichael）
- 改进多卷 RAR 文件检测，修复 https://github.com/M2Team/NanaZip/issues/82。（感谢 1human）
- 简化文件类型关联定义，并为其添加“打开”操作。（感谢 Fabio286）
- 修复 CI 问题。
- VC-LTL 升级至 5.0.4。
- C++/WinRT 升级至 2.0.211028.7。

**NanaZip 1.0 (1.0.95.0)**

- 使用 MSBuild 现代化构建工具链，支持 MSIX 打包和并行编译。（感谢 AndromedaMelody、be5invis、青春永不落幕 和 oxygen-dioxide）
- 采用 [VC-LTL 5.x](https://github.com/Chuyu-Team/VC-LTL5) 工具链，使二进制体积比官方 7-Zip 更小，因为可以直接使用 ucrtbase.dll 及现代编译链优化。（感谢 mingkuang）
- 在 Windows 10/11 文件资源管理器中加入右键菜单支持。（感谢 shiroshan）
- 新图标设计。（设计：Alice（四月天）、Chi Lei、Kenji Mouri、Rúben Garrido、Sakura Neko。感谢 AndromedaMelody 和 奕然）
- 细节调整和优化。（感谢 adrianghc、Blueberryy、ChuckMichael、Legna、Maicol Battistini、SakuraNeko 和 Zbynius）
- 7-Zip 从 21.03 升级至 21.06。（感谢 Dan、lychichem、sanderdewit 提供信息，感谢 Igor Pavlov）
- 支持解析 NSIS 压缩包中的 NSIS 脚本。（建议：alanfox2000，感谢 myfreeer）
- 合并 7-Zip ZStandard 分支的功能。（建议：fcharlie，感谢 Tino Reichardt）