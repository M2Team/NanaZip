## Contributing to NanaZip

### How to become a contributor

- Direct contributions
  - **Create pull requests directly.**
  - Please send e-mails to KurikoMouri@outlook.jp if you have any questions.
  - You are forbidden to modify any content in any files and folders starting 
    with the "Mile." prefix, or your PR won't be merged and closed immediately.
- Feedback suggestions and bugs.
  - We use GitHub issues to track bugs and features.
  - For bugs and general issues please 
    [file a new issue](https://github.com/M2Team/NanaZip/issues/new).

### Code contribution guidelines

#### Prerequisites

- Visual Studio 2022 or later.
  - You also need install ARM64 components (MSVC Toolchain and ATL/MFC) if you
    want to compile the ARM64 version of NanaZip.
- Windows 11 SDK or later.
  - You also need to install ARM64 components if you want to compile the ARM64
    version of NanaZip.

#### How to build all targets of NanaZip

Run `BuildAllTargets.bat` in the root of the repository.

#### How to modify or debugging NanaZip

Open `NanaZip.sln` in the root of the repository.

#### Code style and conventions

- C++: [C++ Core Guidelines](https://github.com/isocpp/CppCoreGuidelines/blob/master/CppCoreGuidelines.md)
- C#: Follow the .NET Core team's [C# coding style](https://github.com/dotnet/corefx/blob/master/Documentation/coding-guidelines/coding-style.md)

For all languages respect the [.editorconfig](https://editorconfig.org/) file 
specified in the source tree. Many IDEs natively support this or can with a 
plugin.
