/*
 * PROJECT:   NanaZip
 * FILE:      NanaZipShellExtension.cpp
 * PURPOSE:   Implementation for NanaZip Shell Extension
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: Mouri_Naruto (Mouri_Naruto AT Outlook.com)
 */

#include <Windows.h>

#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

#include <shobjidl_core.h>

#include <winrt/Windows.Foundation.h>

#include "../SevenZip/CPP/Common/Common.h"
#include "../SevenZip/CPP/Windows/DLL.h"
#include "../SevenZip/CPP/Windows/FileDir.h"
#include "../SevenZip/CPP/Windows/FileFind.h"
#include "../SevenZip/CPP/Windows/FileName.h"
#include "../SevenZip/CPP/Windows/ProcessUtils.h"
#include "../SevenZip/CPP/7zip/UI/Common/ArchiveName.h"
#include "../SevenZip/CPP/7zip/UI/Common/CompressCall.h"
#include "../SevenZip/CPP/7zip/UI/Common/ExtractingFilePath.h"
#include "../SevenZip/CPP/7zip/UI/Common/ZipRegistry.h"
#include "../SevenZip/CPP/7zip/UI/FileManager/FormatUtils.h"
#include "../SevenZip/CPP/7zip/UI/FileManager/LangUtils.h"
#include "../SevenZip/CPP/7zip/UI/Explorer/ContextMenuFlags.h"
#include "../SevenZip/CPP/7zip/UI/Explorer/resource.h"

namespace
{
    static const char* const kExtractExcludeExtensions =
        " 3gp"
        " aac ans ape asc asm asp aspx avi awk"
        " bas bat bmp"
        " c cs cls clw cmd cpp csproj css ctl cxx"
        " def dep dlg dsp dsw"
        " eps"
        " f f77 f90 f95 fla flac frm"
        " gif"
        " h hpp hta htm html hxx"
        " ico idl inc ini inl"
        " java jpeg jpg js"
        " la lnk log"
        " mak manifest wmv mov mp3 mp4 mpe mpeg mpg m4a"
        " ofr ogg"
        " pac pas pdf php php3 php4 php5 phptml pl pm png ps py pyo"
        " ra rb rc reg rka rm rtf"
        " sed sh shn shtml sln sql srt swa"
        " tcl tex tiff tta txt"
        " vb vcproj vbs"
        " wav webp wma wv"
        " xml xsd xsl xslt"
        " ";

    static bool FindExt(const char* p, const FString& name)
    {
        int dotPos = name.ReverseFind_Dot();
        if (dotPos < 0 || dotPos == (int)name.Len() - 1)
            return false;

        AString s;

        for (unsigned pos = dotPos + 1;; pos++)
        {
            wchar_t c = name[pos];
            if (c == 0)
                break;
            if (c >= 0x80)
                return false;
            s += (char)MyCharLower_Ascii((char)c);
        }

        for (unsigned i = 0; p[i] != 0;)
        {
            unsigned j;
            for (j = i; p[j] != ' '; j++);
            if (s.Len() == j - i && memcmp(p + i, (const char*)s, s.Len()) == 0)
                return true;
            i = j + 1;
        }

        return false;
    }

    static bool DoNeedExtract(const FString& name)
    {
        return !FindExt(kExtractExcludeExtensions, name);
    }

    static const char* const kArcExts[] =
    {
        "7z"
      , "bz2"
      , "gz"
      , "lz"
      , "liz"
      , "lz4"
      , "lz5"
      , "rar"
      , "zip"
      , "zst"
    };

    static bool IsItArcExt(const UString& ext)
    {
        for (unsigned i = 0; i < ARRAY_SIZE(kArcExts); i++)
            if (ext.IsEqualTo_Ascii_NoCase(kArcExts[i]))
                return true;
        return false;
    }

    UString GetSubFolderNameForExtract(const UString& arcName)
    {
        int dotPos = arcName.ReverseFind_Dot();
        if (dotPos < 0)
            return Get_Correct_FsFile_Name(arcName) + L'~';

        const UString ext = arcName.Ptr(dotPos + 1);
        UString res = arcName.Left(dotPos);
        res.TrimRight();
        dotPos = res.ReverseFind_Dot();
        if (dotPos > 0)
        {
            const UString ext2 = res.Ptr(dotPos + 1);
            if ((ext.IsEqualTo_Ascii_NoCase("001") && IsItArcExt(ext2))
                || (ext.IsEqualTo_Ascii_NoCase("rar") &&
                    (ext2.IsEqualTo_Ascii_NoCase("part001")
                        || ext2.IsEqualTo_Ascii_NoCase("part01")
                        || ext2.IsEqualTo_Ascii_NoCase("part1"))))
                res.DeleteFrom(dotPos);
            res.TrimRight();
        }
        return Get_Correct_FsFile_Name(res);
    }

    static void ReduceString(UString& s)
    {
        const unsigned kMaxSize = 64;
        if (s.Len() <= kMaxSize)
            return;
        s.Delete(kMaxSize / 2, s.Len() - kMaxSize);
        s.Insert(kMaxSize / 2, L" ... ");
    }

    static UString GetQuotedReducedString(const UString& s)
    {
        UString s2 = s;
        ReduceString(s2);
        s2.Replace(L"&", L"&&");
        return GetQuotedString(s2);
    }

    static void MyFormatNew_ReducedName(UString& s, const UString& name)
    {
        s = MyFormatNew(s, GetQuotedReducedString(name));
    }

    static UString GetNanaZipPath()
    {
        return fs2us(NWindows::NDLL::GetModuleDirPrefix()) + L"NanaZip.exe";
    }
}

namespace NanaZip::ShellExtension
{
    namespace CommandID
    {
        enum
        {
            None,

            Open,
            Test,

            Extract,
            ExtractHere,
            ExtractTo,

            Compress,
            CompressTo7z,
            CompressToZip,

            CompressEmail,
            CompressTo7zEmail,
            CompressToZipEmail,

            HashCRC32,
            HashCRC64,
            HashSHA1,
            HashSHA256,
            HashAll,

            Maximum
        };
    }

    using SubCommandList = std::vector<winrt::com_ptr<IExplorerCommand>>;
    using SubCommandListIterator = SubCommandList::const_iterator;

    struct ExplorerCommandBase : public winrt::implements<
        ExplorerCommandBase,
        IExplorerCommand>
    {
    private:

        std::wstring m_Title;

        DWORD m_CommandID;
        bool m_IsSeparator;
        CBoolPair m_ElimDup;

    public:

        ExplorerCommandBase(
            std::wstring const& Title = std::wstring(),
            DWORD CommandID = CommandID::None,
            CBoolPair const& ElimDup = CBoolPair()) :
            m_Title(Title),
            m_CommandID(CommandID),
            m_ElimDup(ElimDup)
        {
            this->m_IsSeparator = (this->m_CommandID == CommandID::None);
        }

#pragma region IExplorerCommand

        HRESULT STDMETHODCALLTYPE GetTitle(
            _In_opt_ IShellItemArray* psiItemArray,
            _Outptr_ LPWSTR* ppszName)
        {
            UNREFERENCED_PARAMETER(psiItemArray);

            if (this->m_IsSeparator)
            {
                *ppszName = nullptr;
                return S_FALSE;
            }

            return ::SHStrDupW(this->m_Title.c_str(), ppszName);
        }

        HRESULT STDMETHODCALLTYPE GetIcon(
            _In_opt_ IShellItemArray* psiItemArray,
            _Outptr_ LPWSTR* ppszIcon)
        {
            UNREFERENCED_PARAMETER(psiItemArray);

            *ppszIcon = nullptr;
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE GetToolTip(
            _In_opt_ IShellItemArray* psiItemArray,
            _Outptr_ LPWSTR* ppszInfotip)
        {
            UNREFERENCED_PARAMETER(psiItemArray);
            *ppszInfotip = nullptr;
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE GetCanonicalName(
            _Out_ GUID* pguidCommandName)
        {
            *pguidCommandName = GUID_NULL;
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE GetState(
            _In_opt_ IShellItemArray* psiItemArray,
            _In_ BOOL fOkToBeSlow,
            _Out_ EXPCMDSTATE* pCmdState)
        {
            UNREFERENCED_PARAMETER(psiItemArray);
            UNREFERENCED_PARAMETER(fOkToBeSlow);
            *pCmdState = ECS_ENABLED;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE Invoke(
            _In_opt_ IShellItemArray* psiItemArray,
            _In_opt_ IBindCtx* pbc)
        {
            UNREFERENCED_PARAMETER(pbc);

            if (this->m_IsSeparator)
            {
                return E_NOTIMPL;
            }

            std::vector<std::wstring> FilePaths;
            if (psiItemArray)
            {
                DWORD Count = 0;
                if (SUCCEEDED(psiItemArray->GetCount(&Count)))
                {
                    for (DWORD i = 0; i < Count; ++i)
                    {
                        winrt::com_ptr<IShellItem> Item;
                        if (SUCCEEDED(psiItemArray->GetItemAt(
                            i,
                            Item.put())))
                        {
                            LPWSTR DisplayName = nullptr;
                            if (SUCCEEDED(Item->GetDisplayName(
                                SIGDN_FILESYSPATH,
                                &DisplayName)))
                            {
                                FilePaths.push_back(std::wstring(DisplayName));
                                ::CoTaskMemFree(DisplayName);
                            }
                        }
                    }
                }
            }

            bool NeedExtract = false;
            if (FilePaths.size() > 0)
            {
                for (std::wstring const FilePath : FilePaths)
                {
                    DWORD FileAttributes = ::GetFileAttributesW(
                        FilePath.c_str());
                    if (FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    {
                        continue;
                    }

                    if (DoNeedExtract(::PathFindFileNameW(FilePath.c_str())))
                    {
                        NeedExtract = true;
                        break;
                    }
                }
            }

            std::wstring SpecFolder = L"*";
            if (NeedExtract)
            {
                if (FilePaths.size() == 1)
                {
                    SpecFolder = GetSubFolderNameForExtract(
                        ::PathFindFileNameW(FilePaths[0].c_str()));
                }
                SpecFolder += L'\\';
            }

            UStringVector FileNames;
            for (std::wstring const FilePath : FilePaths)
            {
                FileNames.Add(FilePath.c_str());
            }

            FString FolderPrefix;

            std::wstring ArchiveName;
            if (FilePaths.size() > 0)
            {
                NWindows::NFile::NFind::CFileInfo FileInfo0;

                const UString& FileName = FileNames.Front();

                if (NWindows::NFile::NName::IsDevicePath(us2fs(FileName)))
                {
                    // CFileInfo::Find can be slow for device files. So we
                    // don't call it.
                    // we need only name here.
                    // change it 4 - must be constant
                    FileInfo0.Name = us2fs(FileName.Ptr(
                        NWindows::NFile::NName::kDevicePathPrefixSize));
                    FolderPrefix = "C:\\";
                }
                else
                {
                    if (!FileInfo0.Find(us2fs(FileName)))
                    {
                        return ::HRESULT_FROM_WIN32(::GetLastError());
                    }
                    NWindows::NFile::NDir::GetOnlyDirPrefix(
                        us2fs(FileName),
                        FolderPrefix);
                }

                const UString Name = CreateArchiveName(
                    FileNames,
                    FileNames.Size() == 1 ? &FileInfo0 : nullptr);
                ArchiveName = std::wstring(Name.Ptr(), Name.Len());

            }

            std::wstring BaseFolder = std::wstring(
                FolderPrefix.Ptr(),
                FolderPrefix.Len());

            std::wstring ArchiveName7z = ArchiveName + L".7z";
            std::wstring ArchiveNameZip = ArchiveName + L".zip";

            switch (this->m_CommandID)
            {
            case CommandID::Open:
            {
                if (FilePaths.size() != 1)
                {
                    break;
                }

                DWORD FileAttributes = ::GetFileAttributesW(
                    FilePaths[0].c_str());
                if (FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    break;
                }

                if (!DoNeedExtract(FilePaths[0].c_str()))
                {
                    break;
                }

                UString params;
                params = GetQuotedString(FilePaths[0].c_str());
                NWindows::MyCreateProcess(::GetNanaZipPath(), params);

                break;
            }
            case CommandID::Test:
            {
                if (!NeedExtract)
                {
                    break;
                }

                TestArchives(FileNames);
                break;
            }
            case CommandID::Extract:
            case CommandID::ExtractHere:
            case CommandID::ExtractTo:
            {
                if (!NeedExtract)
                {
                    break;
                }

                std::wstring Folder = BaseFolder;
                if (this->m_CommandID != CommandID::ExtractHere)
                {
                    Folder += SpecFolder;
                }

                ExtractArchives(
                    FileNames,
                    Folder.c_str(),
                    (this->m_CommandID == CommandID::Extract),
                    ((this->m_CommandID == CommandID::ExtractTo)
                    && this->m_ElimDup.Val));

                break;
            }
            case CommandID::Compress:
            case CommandID::CompressTo7z:
            case CommandID::CompressToZip:
            case CommandID::CompressEmail:
            case CommandID::CompressTo7zEmail:
            case CommandID::CompressToZipEmail:
            {
                bool Email =(
                    (this->m_CommandID == CommandID::CompressEmail) ||
                    (this->m_CommandID == CommandID::CompressTo7zEmail) ||
                    (this->m_CommandID == CommandID::CompressToZipEmail));
                bool ShowDialog = (
                    (this->m_CommandID == CommandID::Compress) ||
                    (this->m_CommandID == CommandID::CompressEmail));
                bool AddExtension = (
                    (this->m_CommandID == CommandID::Compress) ||
                    (this->m_CommandID == CommandID::CompressEmail));
                bool Is7z = (
                    (this->m_CommandID == CommandID::CompressTo7z) ||
                    (this->m_CommandID == CommandID::CompressTo7zEmail));

                std::wstring Name = (
                    AddExtension
                    ? ArchiveName
                    : (Is7z ? ArchiveName7z : ArchiveNameZip));

                CompressFiles(
                    BaseFolder.c_str(),
                    Name.c_str(),
                    Is7z ? L"7z" : L"zip",
                    AddExtension,
                    FileNames,
                    Email,
                    ShowDialog,
                    false);

                break;
            }
            case CommandID::HashCRC32:
            case CommandID::HashCRC64:
            case CommandID::HashSHA1:
            case CommandID::HashSHA256:
            case CommandID::HashAll:
            {
                std::wstring MethodName;
                switch (this->m_CommandID)
                {
                case CommandID::HashCRC32:
                    MethodName = L"CRC32";
                    break;
                case CommandID::HashCRC64:
                    MethodName = L"CRC64";
                    break;
                case CommandID::HashSHA1:
                    MethodName = L"SHA1";
                    break;
                case CommandID::HashSHA256:
                    MethodName = L"SHA256";
                    break;
                case CommandID::HashAll:
                    MethodName = L"*";
                    break;
                default:
                    break;
                }

                CalcChecksum(FileNames, MethodName.c_str(), L"", L"");
                break;
            }
            default:
                break;
            }

            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE GetFlags(
            _Out_ EXPCMDFLAGS* pFlags)
        {
            *pFlags =
                this->m_IsSeparator
                ? ECF_ISSEPARATOR
                : ECF_DEFAULT;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE EnumSubCommands(
            _Outptr_ IEnumExplorerCommand** ppEnum)
        {
            *ppEnum = nullptr;
            return E_NOTIMPL;
        }

#pragma endregion
    };


    struct ExplorerCommandRoot : public winrt::implements<
        ExplorerCommandRoot,
        IExplorerCommand,
        IEnumExplorerCommand>
    {
    private:

        DWORD m_ContextMenuFlags;
        CBoolPair m_ContextMenuElimDup;

        SubCommandList m_SubCommands;
        SubCommandListIterator m_CurrentSubCommand;

    public:

        ExplorerCommandRoot()
        {
            CContextMenuInfo ContextMenuInfo;
            ContextMenuInfo.Load();
            this->m_ContextMenuFlags = ContextMenuInfo.Flags;
            this->m_ContextMenuElimDup = ContextMenuInfo.ElimDup;
        }

#pragma region IExplorerCommand

        HRESULT STDMETHODCALLTYPE GetTitle(
            _In_opt_ IShellItemArray* psiItemArray,
            _Outptr_ LPWSTR* ppszName)
        {
            UNREFERENCED_PARAMETER(psiItemArray);

            std::vector<std::wstring> FilePaths;
            if (psiItemArray)
            {
                DWORD Count = 0;
                if (SUCCEEDED(psiItemArray->GetCount(&Count)))
                {
                    for (DWORD i = 0; i < Count; ++i)
                    {
                        winrt::com_ptr<IShellItem> Item;
                        if (SUCCEEDED(psiItemArray->GetItemAt(
                            i,
                            Item.put())))
                        {
                            LPWSTR DisplayName = nullptr;
                            if (SUCCEEDED(Item->GetDisplayName(
                                SIGDN_FILESYSPATH,
                                &DisplayName)))
                            {
                                FilePaths.push_back(std::wstring(DisplayName));
                                ::CoTaskMemFree(DisplayName);
                            }
                        }
                    }
                }
            }

            if (FilePaths.empty())
            {
                return E_NOTIMPL;
            }

            UStringVector FileNames;
            for (std::wstring const FilePath : FilePaths)
            {
                FileNames.Add(FilePath.c_str());
            }

            bool NeedExtract = false;
            for (std::wstring const FilePath : FilePaths)
            {
                DWORD FileAttributes = ::GetFileAttributesW(
                    FilePath.c_str());
                if (FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    continue;
                }

                if (DoNeedExtract(::PathFindFileNameW(FilePath.c_str())))
                {
                    NeedExtract = true;
                    break;
                }
            }

            std::wstring SpecFolder = L"*";
            if (NeedExtract)
            {
                if (FilePaths.size() == 1)
                {
                    SpecFolder = GetSubFolderNameForExtract(
                        ::PathFindFileNameW(FilePaths[0].c_str()));
                }
                SpecFolder += L'\\';
            }

            FString FolderPrefix;

            std::wstring ArchiveName;
            {
                NWindows::NFile::NFind::CFileInfo FileInfo0;

                const UString& FileName = FileNames.Front();

                if (NWindows::NFile::NName::IsDevicePath(us2fs(FileName)))
                {
                    // CFileInfo::Find can be slow for device files. So we
                    // don't call it.
                    // we need only name here.
                    // change it 4 - must be constant
                    FileInfo0.Name = us2fs(FileName.Ptr(
                        NWindows::NFile::NName::kDevicePathPrefixSize));
                    FolderPrefix = "C:\\";
                }
                else
                {
                    if (!FileInfo0.Find(us2fs(FileName)))
                    {
                        return ::HRESULT_FROM_WIN32(::GetLastError());
                    }
                    NWindows::NFile::NDir::GetOnlyDirPrefix(
                        us2fs(FileName),
                        FolderPrefix);
                }

                const UString Name = CreateArchiveName(
                    FileNames,
                    FileNames.Size() == 1 ? &FileInfo0 : nullptr);
                ArchiveName = std::wstring(Name.Ptr(), Name.Len());

            }

            std::wstring BaseFolder = std::wstring(
                FolderPrefix.Ptr(),
                FolderPrefix.Len());

            std::wstring ArchiveName7z = ArchiveName + L".7z";
            std::wstring ArchiveNameZip = ArchiveName + L".zip";

            using NanaZip::ShellExtension::ExplorerCommandBase;

            CContextMenuInfo ContextMenuInfo;
            ContextMenuInfo.Load();
            DWORD ContextMenuFlags = ContextMenuInfo.Flags;
            CBoolPair ContextMenuElimDup = ContextMenuInfo.ElimDup;

            LoadLangOneTime();

            if (ContextMenuFlags & NContextMenuFlags::kOpen)
            {
                DWORD FileAttributes = ::GetFileAttributesW(
                    FilePaths[0].c_str());
                if ((FilePaths.size() == 1) &&
                    !(FileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                    DoNeedExtract(FilePaths[0].c_str()))
                {
                    UString TranslatedString;
                    LangString(IDS_CONTEXT_OPEN, TranslatedString);
                    this->m_SubCommands.push_back(
                        winrt::make<ExplorerCommandBase>(
                            std::wstring(
                                TranslatedString.Ptr(),
                                TranslatedString.Len()),
                            CommandID::Open,
                            ContextMenuElimDup));
                }
            }

            if (NeedExtract)
            {
                if (ContextMenuFlags & NContextMenuFlags::kTest)
                {
                    UString TranslatedString;
                    LangString(IDS_CONTEXT_TEST, TranslatedString);
                    this->m_SubCommands.push_back(
                        winrt::make<ExplorerCommandBase>(
                            std::wstring(
                                TranslatedString.Ptr(),
                                TranslatedString.Len()),
                            CommandID::Test,
                            ContextMenuElimDup));
                }

                if (ContextMenuFlags & NContextMenuFlags::kExtract)
                {
                    UString TranslatedString;
                    LangString(IDS_CONTEXT_EXTRACT, TranslatedString);
                    this->m_SubCommands.push_back(
                        winrt::make<ExplorerCommandBase>(
                            std::wstring(
                                TranslatedString.Ptr(),
                                TranslatedString.Len()),
                            CommandID::Extract,
                            ContextMenuElimDup));
                }

                if (ContextMenuFlags & NContextMenuFlags::kExtractHere)
                {
                    UString TranslatedString;
                    LangString(IDS_CONTEXT_EXTRACT_HERE, TranslatedString);
                    this->m_SubCommands.push_back(
                        winrt::make<ExplorerCommandBase>(
                            std::wstring(
                                TranslatedString.Ptr(),
                                TranslatedString.Len()),
                            CommandID::ExtractHere,
                            ContextMenuElimDup));
                }

                if (ContextMenuFlags & NContextMenuFlags::kExtractTo)
                {
                    UString TranslatedString;
                    LangString(IDS_CONTEXT_EXTRACT_TO, TranslatedString);
                    MyFormatNew_ReducedName(TranslatedString, SpecFolder.c_str());
                    this->m_SubCommands.push_back(
                        winrt::make<ExplorerCommandBase>(
                            std::wstring(
                                TranslatedString.Ptr(),
                                TranslatedString.Len()),
                            CommandID::ExtractTo,
                            ContextMenuElimDup));
                }
            }

            if (ContextMenuFlags & NContextMenuFlags::kCompress)
            {
                UString TranslatedString;
                LangString(IDS_CONTEXT_COMPRESS, TranslatedString);
                this->m_SubCommands.push_back(
                    winrt::make<ExplorerCommandBase>(
                        std::wstring(
                            TranslatedString.Ptr(),
                            TranslatedString.Len()),
                        CommandID::Compress,
                        ContextMenuElimDup));
            }

            if (ContextMenuFlags & NContextMenuFlags::kCompressTo7z)
            {
                UString TranslatedString;
                LangString(IDS_CONTEXT_COMPRESS_TO, TranslatedString);
                MyFormatNew_ReducedName(TranslatedString, ArchiveName7z.c_str());
                this->m_SubCommands.push_back(
                    winrt::make<ExplorerCommandBase>(
                        std::wstring(
                            TranslatedString.Ptr(),
                            TranslatedString.Len()),
                        CommandID::CompressTo7z,
                        ContextMenuElimDup));
            }

            if (ContextMenuFlags & NContextMenuFlags::kCompressToZip)
            {
                UString TranslatedString;
                LangString(IDS_CONTEXT_COMPRESS_TO, TranslatedString);
                MyFormatNew_ReducedName(TranslatedString, ArchiveNameZip.c_str());
                this->m_SubCommands.push_back(
                    winrt::make<ExplorerCommandBase>(
                        std::wstring(
                            TranslatedString.Ptr(),
                            TranslatedString.Len()),
                        CommandID::CompressToZip,
                        ContextMenuElimDup));
            }

            if (ContextMenuFlags & NContextMenuFlags::kCompressEmail)
            {
                UString TranslatedString;
                LangString(IDS_CONTEXT_COMPRESS_EMAIL, TranslatedString);
                this->m_SubCommands.push_back(
                    winrt::make<ExplorerCommandBase>(
                        std::wstring(
                            TranslatedString.Ptr(),
                            TranslatedString.Len()),
                        CommandID::CompressEmail,
                        ContextMenuElimDup));
            }

            if (ContextMenuFlags & NContextMenuFlags::kCompressTo7zEmail)
            {
                UString TranslatedString;
                LangString(IDS_CONTEXT_COMPRESS_TO_EMAIL, TranslatedString);
                MyFormatNew_ReducedName(TranslatedString, ArchiveName7z.c_str());
                this->m_SubCommands.push_back(
                    winrt::make<ExplorerCommandBase>(
                        std::wstring(
                            TranslatedString.Ptr(),
                            TranslatedString.Len()),
                        CommandID::CompressTo7zEmail,
                        ContextMenuElimDup));
            }

            if (ContextMenuFlags & NContextMenuFlags::kCompressToZipEmail)
            {
                UString TranslatedString;
                LangString(IDS_CONTEXT_COMPRESS_TO_EMAIL, TranslatedString);
                MyFormatNew_ReducedName(TranslatedString, ArchiveNameZip.c_str());
                this->m_SubCommands.push_back(
                    winrt::make<ExplorerCommandBase>(
                        std::wstring(
                            TranslatedString.Ptr(),
                            TranslatedString.Len()),
                        CommandID::CompressToZipEmail,
                        ContextMenuElimDup));
            }

            if (ContextMenuFlags & NContextMenuFlags::kCRC)
            {
                if (!this->m_SubCommands.empty())
                {
                    this->m_SubCommands.push_back(
                        winrt::make<ExplorerCommandBase>());
                }

                this->m_SubCommands.push_back(
                    winrt::make<ExplorerCommandBase>(
                        L"CRC-32",
                        CommandID::HashCRC32,
                        ContextMenuElimDup));

                this->m_SubCommands.push_back(
                    winrt::make<ExplorerCommandBase>(
                        L"CRC-64",
                        CommandID::HashCRC64,
                        ContextMenuElimDup));

                this->m_SubCommands.push_back(
                    winrt::make<ExplorerCommandBase>(
                        L"SHA-1",
                        CommandID::HashSHA1,
                        ContextMenuElimDup));

                this->m_SubCommands.push_back(
                    winrt::make<ExplorerCommandBase>(
                        L"SHA-256",
                        CommandID::HashSHA256,
                        ContextMenuElimDup));

                this->m_SubCommands.push_back(
                    winrt::make<ExplorerCommandBase>(
                        L"*",
                        CommandID::HashAll,
                        ContextMenuElimDup));
            }

            if (this->m_SubCommands.empty())
            {
                *ppszName = nullptr;
                return E_NOTIMPL;
            }

            return ::SHStrDupW(L"NanaZip", ppszName);
        }

        HRESULT STDMETHODCALLTYPE GetIcon(
            _In_opt_ IShellItemArray* psiItemArray,
            _Outptr_ LPWSTR* ppszIcon)
        {
            UNREFERENCED_PARAMETER(psiItemArray);
            UString Path = ::GetNanaZipPath();
            std::wstring Icon = std::wstring(Path.Ptr(), Path.Len());
            Icon += L",-1";
            return ::SHStrDupW(Icon.c_str(), ppszIcon);
        }

        HRESULT STDMETHODCALLTYPE GetToolTip(
            _In_opt_ IShellItemArray* psiItemArray,
            _Outptr_ LPWSTR* ppszInfotip)
        {
            UNREFERENCED_PARAMETER(psiItemArray);
            *ppszInfotip = nullptr;
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE GetCanonicalName(
            _Out_ GUID* pguidCommandName)
        {
            *pguidCommandName = GUID_NULL;
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE GetState(
            _In_opt_ IShellItemArray* psiItemArray,
            _In_ BOOL fOkToBeSlow,
            _Out_ EXPCMDSTATE* pCmdState)
        {
            UNREFERENCED_PARAMETER(psiItemArray);
            UNREFERENCED_PARAMETER(fOkToBeSlow);
            *pCmdState = ECS_ENABLED;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE Invoke(
            _In_opt_ IShellItemArray* psiItemArray,
            _In_opt_ IBindCtx* pbc)
        {
            UNREFERENCED_PARAMETER(psiItemArray);
            UNREFERENCED_PARAMETER(pbc);
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE GetFlags(
            _Out_ EXPCMDFLAGS* pFlags)
        {
            *pFlags = ECF_HASSUBCOMMANDS;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE EnumSubCommands(
            _Outptr_ IEnumExplorerCommand** ppEnum)
        {
            if (this->m_SubCommands.empty())
            {
                *ppEnum = nullptr;
                return E_NOTIMPL;
            }
            else
            {
                this->m_CurrentSubCommand = this->m_SubCommands.cbegin();
                this->AddRef();
                return this->QueryInterface(IID_PPV_ARGS(ppEnum));
            }
        }

#pragma endregion

#pragma region IEnumExplorerCommand

        HRESULT STDMETHODCALLTYPE Next(
            _In_ ULONG celt,
            _Out_ IExplorerCommand** pUICommand,
            _Out_opt_ ULONG* pceltFetched)
        {
            ULONG Fetched = 0;

            for (
                ULONG i = 0;
                (i < celt) &&
                (this->m_CurrentSubCommand != this->m_SubCommands.cend());
                ++i)
            {
                this->m_CurrentSubCommand->copy_to(&pUICommand[0]);
                ++this->m_CurrentSubCommand;
                ++Fetched;
            }

            if (pceltFetched)
            {
                *pceltFetched = Fetched;
            }

            return (Fetched == celt) ? S_OK : S_FALSE;
        }

        HRESULT STDMETHODCALLTYPE Skip(
            _In_ ULONG celt)
        {
            UNREFERENCED_PARAMETER(celt);
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE Reset()
        {
            this->m_CurrentSubCommand = this->m_SubCommands.cbegin();
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE Clone(
            _Out_ IEnumExplorerCommand** ppenum)
        {
            *ppenum = nullptr;
            return E_NOTIMPL;
        }

#pragma endregion
    };

    struct DECLSPEC_UUID("CAE3F1D4-7765-4D98-A060-52CD14D56EAB")
        ClassFactory : public winrt::implements<
        ClassFactory, IClassFactory>
    {
    public:

        HRESULT STDMETHODCALLTYPE CreateInstance(
            _In_opt_ IUnknown* pUnkOuter,
            _In_ REFIID riid,
            _COM_Outptr_ void** ppvObject) noexcept override
        {
            UNREFERENCED_PARAMETER(pUnkOuter);

            try
            {
                return winrt::make<ExplorerCommandRoot>()->QueryInterface(
                    riid, ppvObject);
            }
            catch (...)
            {
                return winrt::to_hresult();
            }
        }

        HRESULT STDMETHODCALLTYPE LockServer(
            _In_ BOOL fLock) noexcept override
        {
            if (fLock)
            {
                ++winrt::get_module_lock();
            }
            else
            {
                --winrt::get_module_lock();
            }

            return S_OK;
        }
    };
}

EXTERN_C HRESULT STDAPICALLTYPE DllCanUnloadNow()
{
    if (winrt::get_module_lock())
    {
        return S_FALSE;
    }

    winrt::clear_factory_cache();
    return S_OK;
}

EXTERN_C HRESULT STDAPICALLTYPE DllGetClassObject(
    _In_ REFCLSID rclsid,
    _In_ REFIID riid,
    _Outptr_ LPVOID* ppv)
{
    if (!ppv)
    {
        return E_POINTER;
    }

    if (riid != IID_IClassFactory && riid != IID_IUnknown)
    {
        return E_NOINTERFACE;
    }

    if (rclsid != __uuidof(NanaZip::ShellExtension::ClassFactory))
    {
        return E_INVALIDARG;
    }

    try
    {
        return winrt::make<NanaZip::ShellExtension::ClassFactory>(
            )->QueryInterface(riid, ppv);
    }
    catch (...)
    {
        return winrt::to_hresult();
    }
}

long g_DllRefCount = 0;
HWND g_HWND = nullptr;
HINSTANCE g_hInstance = nullptr;

BOOL WINAPI DllMain(
    _In_ HINSTANCE hinstDLL,
    _In_ DWORD fdwReason,
    _In_ LPVOID lpvReserved)
{
    UNREFERENCED_PARAMETER(lpvReserved);

    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
    {
        g_hInstance = hinstDLL;
        break;
    }
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
