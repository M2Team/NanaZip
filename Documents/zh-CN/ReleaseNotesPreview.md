# NanaZip 预览版发布日志

如需稳定版发布日志，请参阅 [NanaZip 发布日志](ReleaseNotes.md)。

**NanaZip 5.1 预览 0 (5.1.1263.0)**

本版本包含 NanaZip 5.0 Update 2 (5.0.1263.0) 的全部改进内容。

**NanaZip 5.1 预览 0 (5.1.1252.0)**

本版本包含 NanaZip 5.0 Update 1 (5.0.1252.0) 的全部改进内容。

**NanaZip 5.0 预览 2 (5.0.1243.0)**

- 持续完善开发中的 NanaZip 平台抽象层（K7Pal）。(https://github.com/M2Team/NanaZip/tree/main/K7Pal)
  - 修复 7-Zip 哈希算法封装器在加密 RAR5 格式解压时崩溃及密码错误问题，详见 https://github.com/M2Team/NanaZip/issues/542。（感谢 RuesanG）
  - 新增 K7PalHashDuplicate API。
  - 移除 K7PalHashReset API 以提升安全性。
  - 精简哈希上下文中保存的信息以提升安全性。
  - 7-Zip 的 SHA-1 哈希算法通过 NanaZip 平台抽象层（K7Pal）实现。
- 更新 zh-Hans 与 zh-Hant 的 'Want * History' 相关翻译。（贡献者：R-YaTian，5.0 Preview 1 未提及）
- 新增解压后打开文件夹的设置选项。（贡献者：DaxDupont）
- 引入智能解压（Smart Extraction）。（贡献者：R-YaTian）
- 修复通过右键菜单启动 NanaZip 时窗口与对话框被置于后台的问题。（贡献者：R-YaTian）
- xxHash 升级至 v0.8.3。（https://github.com/Cyan4973/xxHash/releases/tag/v0.8.3）

**NanaZip 5.0 预览 1 (5.0.1215.0)**

- 引入 NanaZip 平台抽象层（K7Pal），用于封装平台相关基础设施。（https://github.com/M2Team/NanaZip/tree/main/K7Pal，开发中）
  - 提供基于 Windows CNG API 实现的哈希函数接口，NanaZip 使用如下哈希函数：
    - MD2
    - MD4
    - MD5
    - SHA-1
    - SHA-256
    - SHA-384
    - SHA-512
    - ED2K（NanaZip.Codecs 内实现为 K7Pal MD4 封装器）
- 主线 7-Zip 实现同步至 24.09。（https://github.com/ip7z/7zip/releases/tag/24.09，感谢 Igor Pavlov，FadeMind 与 peashooter2 提供信息）
- NanaZip 控制台版最终迁移至 NanaZip.Core 项目。
- 修复 ModernSHBrowseForFolderW 因 DefaultFolder 无法设置而失败的问题。（贡献者：dinhngtu）
- Mile.Windows.UniCrt 升级至 1.1.278。

**NanaZip 5.0 预览 0 (5.0.1188.0)**

- 本版本包含 NanaZip 3.1 (3.1.1080.0) 的全部改进内容。（https://github.com/M2Team/NanaZip/releases/tag/3.1.1080.0）
- 巴西葡萄牙语翻译更新。（贡献者：maisondasilva）
- 确保 NanaZip Core（除自解压文件外）和 NanaZip Classic 使用 10.0.19041.0 及以上版本的 ucrtbase.dll。
- Mile.Windows.Helpers 升级至 1.0.671。（https://github.com/ProjectMile/Mile.Windows.Helpers/tree/1.0.671.0）
- NanaZip 控制台版迁移至 NanaZip.Core 项目（MSIX 包未收录，需等待下个预览版以包含 NanaZip 3.1 中 CVE-2024-11477 修复）。
- NanaZip.Codecs 与 NanaZip.Frieren 移除 C++/WinRT 依赖。
- NanaZip.Frieren.DarkMode 新增 GetDpiForWindowWrapper，修复旧版 Windows 兼容性问题。
- 自解压相关项目移除 VC-LTL 依赖。
- 调整自解压项目编译配置以优化二进制体积。
- 非自解压组件改用 Mile.Windows.UniCrt（https://github.com/ProjectMile/Mile.Windows.UniCrt）。
- 更新 NanaZip.Specification.SevenZip 头文件。
- 开始简化 NanaZip 特有的解码器和编码器实现。
- BLAKE3 实现同步至 1.5.5。（https://github.com/BLAKE3-team/BLAKE3/releases/tag/1.5.5）
- RHash 实现同步至 v1.4.5 之后的最新 master。（https://github.com/rhash/RHash/commit/cf2adf22ae7c39d9b8e5e7b87222046a8f42b3dc）
- 自解压文件支持禁止创建子进程（自解压安装模式除外，相关二进制未包含在 NanaZip MSIX 包）。

**NanaZip 3.5 预览 0 (3.5.1000.0)**

本版本包含 NanaZip 3.0 Update 1 (3.0.1000.0) 的全部改进内容。

**NanaZip 3.5 预览 0 (3.5.996.0)**

本版本包含 NanaZip 3.0 (3.0.996.0) 的全部改进内容。

**NanaZip 3.0 预览 0 (3.0.756.0)**

- NanaZip 核心库与自解压执行文件实现重写并拆分，核心库为 NanaZip.Core 项目。
- 核心库与自解压执行文件兼容 Windows Vista RTM（Build 6000.16386）。
- 自解压执行文件二进制体积进一步缩小。
- 主线 7-Zip 实现同步至 23.01。（https://www.7-zip.org/history.txt）
- 7-Zip ZS 实现同步至最新 master。（https://github.com/mcmilk/7-Zip-zstd/commit/ce27b4a0d3a94313d256c3d077f1784baffb9eee）
- 新增 GmSSL 提供的 SM3 哈希算法。（https://github.com/guanzhi/GmSSL）
- Zstandard 及内置 xxHash 实现同步至 v1.5.5。（https://github.com/facebook/zstd/releases/tag/v1.5.5）
- Brotli 实现同步至 v1.1.0。（https://github.com/google/brotli/releases/tag/v1.1.0）
- RHash 实现同步至最新 master。（https://github.com/rhash/RHash/commit/b8c91ea6551e99e10352386cd46ea26973bb4a4d）
- Mile.Project.Windows 更新为 Git 子模块版。（https://github.com/ProjectMile/Mile.Project.Windows）
- Mile.Windows.Helpers 升级至 1.0.15。（https://github.com/ProjectMile/Mile.Windows.Helpers/commit/b522a956f7dd42dc205869d362f96a777bcb2aa0）
- Mile.Xaml 升级至 2.1.661。（https://github.com/ProjectMile/Mile.Xaml/releases/tag/2.1.661.0）
- 俄语翻译更新。（贡献者：Blueberryy）
- 修复 About 对话框文本换行问题。（感谢 MenschenToaster）
- 文件夹选择器对话框改用现代 IFileDialog 实现。（贡献者：reflectronic）
- 可直接跳转到 NanaZip 文件关联设置页。（贡献者：AndromedaMelody）
- 驱动器右键菜单集成 NanaZip。（贡献者：AndromedaMelody）
- 文件扩展名支持同步自 https://github.com/mcmilk/7-Zip-zstd
- 压缩对话框新增其他方法。（https://github.com/mcmilk/7-Zip-zstd/commit/cf29d0c1babcd5ddf2c67eda8bb36e11f9c643b9）
- 构造函数按成员声明顺序重排初始化顺序。（https://github.com/mcmilk/7-Zip-zstd/commit/8b011d230f1ccd8990943bd2eaad38d70e6e3fdf）
- 修正哈希结果可选大写/小写输出。（https://github.com/mcmilk/7-Zip-zstd/commit/4fae369d2d6aa60e2bb45eea1fb05659a2599caa）
- 其它开发相关调整。

**NanaZip 2.1 预览 0 (2.1.451.0)**

- 图标进一步更新。（设计：Shomnipotence，NanaZip 2.0 正式版已更新）
- 增加为所有用户安装 NanaZip 的说明。（NanaZip 2.0 正式版已更新）
- 项目新增 Mile.Xaml。（NanaZip 2.0 正式版已更新）
- 关于对话框采用 XAML Islands 刷新界面。（NanaZip 2.0 正式版已更新）
- 开始添加对解包模式的前置支持。（NanaZip 2.0 正式版已更新）
- 除自解压存根项目外，所有打包二进制将 WindowsTargetPlatformMinVersion 提升至 10.0.19041.0 以优化体积。（NanaZip 2.0 Update 1 已更新）
- 主线程禁用动态代码防护以解决与 Explorer Patcher 的兼容性问题。（贡献者：dinhngtu，NanaZip 2.0 Update 1 已更新）
- Mile.Xaml 升级至 1.1.434。（NanaZip 2.0 Update 1 已更新，https://github.com/ProjectMile/Mile.Xaml/releases/tag/1.1.434.0）
- Mile.Windows.Helpers 升级至 1.0.8。（NanaZip 2.0 Update 1 已更新，https://github.com/ProjectMile/Mile.Windows.Helpers/commits/main）
- 右键菜单支持深色模式。（NanaZip 2.0 Update 1 已更新）
- 关于对话框采用 Windows 11 XAML 控件风格和 Mica 效果。（NanaZip 2.0 Update 1 已更新）
- 修正 About 对话框的模态弹窗交互行为。（NanaZip 2.0 Update 1 已更新）
- 持续刷新应用与文件类型图标。（设计：Shomnipotence，NanaZip 2.0 Update 1 已更新）

**NanaZip 2.0 预览 2 (2.0.376.0)**

- 7-Zip 升级至 22.01。（感谢 Igor Pavlov，HylianSteel、Random-name-hi 和 DJxSpeedy 提供信息）
- 文件类型关联新增对 hfsx 的支持。（建议者：AndromedaMelody）
- 将最低系统要求提升至 Windows 10 Version 2004（Build 19041）及以上，以解决 XAML Islands 相关问题。
- LZ4 升级至 v1.9.4。
- 启用包完整性检查。（贡献者：AndromedaMelody）
- Debug 版本不启用“禁用动态代码生成”防护，解决编解码器加载错误。（感谢 AndromedaMelody）
- 持续启用多项安全防护：
  - 启用 EH Continuation Metadata。
  - 启用签名返回保护。
- 手动为包清单生成资源标识。（建议者：AndromedaMelody）
- 解决 NanaZip 未出现在经典右键菜单的兼容问题。（贡献者：dinhngtu）
- 启动压缩时检查 7z 压缩参数合法性。（贡献者：dinhngtu）
- 图标进一步更新。（设计：Shomnipotence）

**NanaZip 2.0 预览 1 (2.0.313.0)**

- 修复导致 Everything 崩溃的外壳扩展问题。（感谢 No5972、startkkkkkk、SakuraNeko、bfgxp、riverar）
- 允许 NanaZip 与任意文件类型关联。（贡献者：manfromarce）
- 7-Zip 升级至 22.00。（感谢 Igor Pavlov，HylianSteel 提供信息）
- 集成 RHash 与 xxHash 哈希算法：
  - AICH
  - BLAKE2b
  - BTIH
  - ED2K
  - EDON-R 224、EDON-R 256、EDON-R 384、EDON-R 512
  - GOST R 34.11-94、GOST R 34.11-94 CryptoPro
  - GOST R 34.11-2012 256、GOST R 34.11-2012 512
  - HAS-160、RIPEMD-160
  - SHA-224
  - SHA3-224、SHA3-256、SHA3-384、SHA3-512
  - Snefru-128、Snefru-256
  - Tiger、Tiger2
  - TTH
  - Whirlpool
  - XXH3_64bits、XXH3_128bits
- Zstandard 升级至 1.5.2。
- BLAKE3 升级至 1.3.1。
- Windows 10 Version 1607 下自解压执行文件存根提升每显示器 DPI 感知支持。
- 修复 i18n 资源文件换行符问题。（感谢 ygjsz）
- 启用多项安全防护（贡献者：dinhngtu）：
  - 所有目标二进制启用控制流保护（CFG），以缓解 ROP 攻击
  - 标记所有 x86 和 x64 目标二进制兼容 CET Shadow Stack
  - 运行时严格句柄检查，阻止使用无效句柄
  - 禁用动态代码生成，防止运行时生成恶意代码
  - 阻止运行时从远程源加载意外库

**NanaZip 1.2 Update 1 预览 1 (1.2.253.0)**

- 修复文件类型关联的 i18n 实现问题。（贡献者：AndromedaMelody，NanaZip 1.2 正式版已修复）
- 自解压 GUI 版添加 i18n 支持。（贡献者：AndromedaMelody，NanaZip 1.2 正式版已修复）

**NanaZip 1.2 预览 4 (1.2.225.0)**

- ModernWin32MessageBox 持续优化，解决部分情况下的无限循环问题。（感谢 AndromedaMelody）
- 修复打开压缩包时的崩溃问题。（感谢 1human 和 Maicol Battistini）
- 选项对话框移除语言页，NanaZip 现跟随 Windows 的语言设置。

**NanaZip 1.1 Servicing Update 1 预览 3 (1.1.220.0)**

- 通过将语言文件从 .txt 迁移到 .resw 现代化 i18n 实现。（贡献者：AndromedaMelody，建议者：Maicol Battistini）
- ModernWin32MessageBox 优化，解决部分情况下的无限循环问题。（感谢 AndromedaMelody）
- 微调图标并为预览版提供专用图标。（设计：Alice（四月天），感谢 StarlightMelody）

**NanaZip 1.1 Servicing Update 1 预览 2 (1.1.201.0)**

- 修复自解压执行文件出现“未找到序号 345”问题。（感谢 FadeMind）
- 所有 GUI 组件支持每显示器 DPI 感知。
- 调整并简化编译器选项以实现现代化。
- 修复关于对话框的 i18n 问题。（感谢 AndromedaMelody）
- 更新安装教程。（建议者：AndromedaMelody）
- 修复仅存在应用商店版 notepad 时无法启动编辑器的问题。（感谢 AndromedaMelody）

**NanaZip 1.1 Servicing Update 1 预览 1 (1.1.196.0)**

- 简化文件类型关联定义并为其添加 open 动词支持。（感谢 Fabio286，NanaZip 1.1 正式版已修复）
- VC-LTL 升级至 5.0.4。（NanaZip 1.1 正式版已更新）

**NanaZip 1.1 预览 2 (1.1.153.0)**

- 修复无法正常加载右键菜单的问题。（感谢 DJxSpeedy）

**NanaZip 1.1 预览 2 (1.1.152.0)**

- 关于对话框重写为 Task Dialog。
- 德语翻译更新。（贡献者：Hen Ry）
- 重新引入 7-Zip 的汇编实现以提升性能。
- 继承自 7-Zip 的翻译更新。
- 7-Zip 升级至 21.07。（感谢 Igor Pavlov，HylianSteel 提供信息）
- 改进多卷 RAR 文件检测，修复 https://github.com/M2Team/NanaZip/issues/82。（感谢 1human）
- 消息框现代化，采用 Task Dialog。

**NanaZip 1.1 预览 1 (1.1.101.0)**

- 归档文件类型列表中排除 .webp，解决 https://github.com/M2Team/NanaZip/issues/57。（感谢 Zbynius，NanaZip 1.0 正式版已修复）
- 波兰语翻译更新。（贡献者：ChuckMichael）
- 修复 CI 问题。
- VC-LTL 升级至 5.0.3。
- C++/WinRT 升级至 2.0.211028.7。

**NanaZip 1.0 预览 4 (1.0.88.0)**

- 意大利语、俄语与波兰语翻译更新。（贡献者：Blueberryy、Maicol Battistini、ChuckMichael）
- 提供 7-Zip 执行别名，方便用户迁移至 NanaZip。（建议者：AndromedaMelody）
- 调整文件关联图标。（建议者：奕然）
- 合并 7-Zip ZStandard 分支功能。（建议者：fcharlie，感谢 Tino Reichardt）
- 7-Zip 升级至 21.06。（感谢 Dan、lychichem、sanderdewit 提供信息，感谢 Igor Pavlov）
- 修复压缩对话框压缩等级显示问题。（感谢 SakuraNeko）
- 文件关联定义中每个扩展名独立类型，解决 https://github.com/M2Team/NanaZip/issues/53。（感谢 oxygen-dioxide）
- 关闭 virtualization:ExcludedDirectories，解决 https://github.com/M2Team/NanaZip/issues/34。（感谢 AndromedaMelody）
- 编译警告减少。
- NanaZipPackage 项目配置调整，解决引用 WinRT 组件时的问题。
- 更新 Mile.Cpp。

**NanaZip 1.0 预览 3 (1.0.46.0)**

- 支持解析 NSIS 压缩包中的 NSIS 脚本。（建议者：alanfox2000，感谢 myfreeer）
- 简化右键菜单分隔符布局实现。
- 修复应用依然显示在文件夹右键菜单且无可用操作时产生空项的问题。（感谢 shiroshan）
- 修复 VC-LTL 5.x 异常处理器实现导致的某些情况下的崩溃问题。（感谢 mingkuang）
- 新图标设计。（设计：Alice（四月天）、Chi Lei、Kenji Mouri、Rúben Garrido、Sakura Neko）
- 主 NanaZip 包含全部资源。
- 修正命令行帮助字符串。（感谢 adrianghc）

**NanaZip 1.0 预览 2 (1.0.31.0)**

- Shell 扩展实现中移除 IObjectWithSite，降低复杂性并减少崩溃。
- 添加 altform-lightunplated 资源，任务栏对比度标准模式下显示专用图标而非白色图标。（感谢 AndromedaMelody）
- 移除 Windows.Universal TargetDeviceFamily，解决最低系统要求显示问题。（感谢 青春永不落幕）
- AppX 清单启用 NanaZipC 与 NanaZipG。（感谢 be5invis）
- “操作可能需要大量内存”错误弹窗调整为警告弹窗。（感谢 Legna）

**NanaZip 1.0 预览 1 (1.0.25.0)**

- 使用 MSBuild 现代化构建工具链，支持 MSIX 打包和并行编译。
- 采用 [VC-LTL 5.x](https://github.com/Chuyu-Team/VC-LTL5) 工具链，使二进制体积比官方 7-Zip 更小，因为可以直接使用 ucrtbase.dll 及现代编译链优化。
- 在 Windows 10/11 文件资源管理器中加入右键菜单支持。
- 新图标与界面微调。