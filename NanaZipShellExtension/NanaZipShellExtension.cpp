/*
 * PROJECT:   NanaZip
 * FILE:      NanaZipShellExtension.cpp
 * PURPOSE:   Implementation for NanaZip Shell Extension
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: Mouri_Naruto (Mouri_Naruto AT Outlook.com)
 */

#include <Mile.Windows.h>

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
    static const char* const kExtractExludeExtensions =
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
        " wav wma wv"
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
        return !FindExt(kExtractExludeExtensions, name);
    }

    static const char* const kArcExts[] =
    {
        "7z"
      , "bz2"
      , "gz"
      , "rar"
      , "zip"
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
            HashAll
        };
    }
    
    using SubCommandList = std::vector<winrt::com_ptr<IExplorerCommand>>;
    using SubCommandListIterator = SubCommandList::const_iterator;

    struct ExplorerCommandBase : public winrt::implements<
        ExplorerCommandBase,
        IExplorerCommand,
        IObjectWithSite,
        IEnumExplorerCommand>
    {
    private:

        std::vector<winrt::com_ptr<IShellItem>> m_ShellItems;

    private:

        DWORD m_CommandID;
        bool m_IsSeparator;
        CBoolPair m_ElimDup;

        std::wstring m_Icon;
        SubCommandList m_SubCommands;

        SubCommandListIterator m_CurrentSubCommand;
        winrt::com_ptr<IUnknown> m_Site;

    public:

        ExplorerCommandBase(
            DWORD CommandID = CommandID::None,
            CBoolPair const& ElimDup = CBoolPair(),
            std::wstring const& Icon = std::wstring(),
            SubCommandList const& SubCommands = SubCommandList()) :
            m_CommandID(CommandID),
            m_ElimDup(ElimDup),
            m_Icon(Icon),
            m_SubCommands(SubCommands)
        {
            this->m_CurrentSubCommand = this->m_SubCommands.cbegin();
            this->m_IsSeparator = (
                (this->m_CommandID == CommandID::None) &&
                (this->m_SubCommands.empty()));
        }

#pragma region IExplorerCommand

        HRESULT STDMETHODCALLTYPE GetTitle(
            _In_opt_ IShellItemArray* psiItemArray,
            _Outptr_ LPWSTR* ppszName)
        {
            if (this->m_IsSeparator)
            {
                *ppszName = nullptr;
                return S_FALSE;
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

            std::wstring ArchiveName;
            if (FilePaths.size() == 1)
            {
                UStringVector FileNames;
                NWindows::NFile::NFind::CFileInfo FileInfo0;
                FString FolderPrefix;

                for (std::wstring const FilePath : FilePaths)
                {
                    FileNames.Add(FilePath.c_str());
                }

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
                        return Mile::HResultFromLastError();
                    }
                    NWindows::NFile::NDir::GetOnlyDirPrefix(
                        us2fs(FileName),
                        FolderPrefix);
                }

                const UString Name = CreateArchiveName(
                    FileNames,
                    FileNames.Size() == 1 ? &FileInfo0 : NULL);
                ArchiveName = std::wstring(Name.Ptr(), Name.Len());

            }

            std::wstring ArchiveName7z = ArchiveName + L".7z";
            std::wstring ArchiveNameZip = ArchiveName + L".zip";

            std::wstring FinalString;

            switch (this->m_CommandID)
            {
            case CommandID::None:
            {
                FinalString = L"NanaZip";
                break;
            }
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
                
                UString TranslatedString;
                LangString(IDS_CONTEXT_OPEN, TranslatedString);
                FinalString = std::wstring(
                    TranslatedString.Ptr(),
                    TranslatedString.Len());

                break;
            }
            case CommandID::Test:
            {
                if (!NeedExtract)
                {
                    break;
                }

                UString TranslatedString;
                LangString(IDS_CONTEXT_TEST, TranslatedString);
                FinalString = std::wstring(
                    TranslatedString.Ptr(),
                    TranslatedString.Len());
                break;
            }
            case CommandID::Extract:
            {
                if (!NeedExtract)
                {
                    break;
                }

                UString TranslatedString;
                LangString(IDS_CONTEXT_EXTRACT, TranslatedString);
                FinalString = std::wstring(
                    TranslatedString.Ptr(),
                    TranslatedString.Len());
                break;
            }
            case CommandID::ExtractHere:
            {
                if (!NeedExtract)
                {
                    break;
                }

                UString TranslatedString;
                LangString(IDS_CONTEXT_EXTRACT_HERE, TranslatedString);
                FinalString = std::wstring(
                    TranslatedString.Ptr(),
                    TranslatedString.Len());
                break;
            }
            case CommandID::ExtractTo:
            {
                if (!NeedExtract)
                {
                    break;
                }

                UString TranslatedString;
                LangString(IDS_CONTEXT_EXTRACT_TO, TranslatedString);
                MyFormatNew_ReducedName(TranslatedString, SpecFolder.c_str());
                FinalString = std::wstring(
                    TranslatedString.Ptr(),
                    TranslatedString.Len());
                break;
            }
            case CommandID::Compress:
            {
                UString TranslatedString;
                LangString(IDS_CONTEXT_COMPRESS, TranslatedString);
                FinalString = std::wstring(
                    TranslatedString.Ptr(),
                    TranslatedString.Len());
                break;
            }
            case CommandID::CompressTo7z:
            {
                UString TranslatedString;
                LangString(IDS_CONTEXT_COMPRESS_TO, TranslatedString);
                MyFormatNew_ReducedName(TranslatedString, ArchiveName7z.c_str());
                FinalString = std::wstring(
                    TranslatedString.Ptr(),
                    TranslatedString.Len());
                break;
            }
            case CommandID::CompressToZip:
            {
                UString TranslatedString;
                LangString(IDS_CONTEXT_COMPRESS_TO, TranslatedString);
                MyFormatNew_ReducedName(TranslatedString, ArchiveNameZip.c_str());
                FinalString = std::wstring(
                    TranslatedString.Ptr(),
                    TranslatedString.Len());
                break;
            }
            case CommandID::CompressEmail:
            {
                UString TranslatedString;
                LangString(IDS_CONTEXT_COMPRESS_EMAIL, TranslatedString);
                FinalString = std::wstring(
                    TranslatedString.Ptr(),
                    TranslatedString.Len());
                break;
            }
            case CommandID::CompressTo7zEmail:
            {
                UString TranslatedString;
                LangString(IDS_CONTEXT_COMPRESS_TO_EMAIL, TranslatedString);
                MyFormatNew_ReducedName(TranslatedString, ArchiveName7z.c_str());
                FinalString = std::wstring(
                    TranslatedString.Ptr(),
                    TranslatedString.Len());
                break;
            }
            case CommandID::CompressToZipEmail:
            {
                UString TranslatedString;
                LangString(IDS_CONTEXT_COMPRESS_TO_EMAIL, TranslatedString);
                MyFormatNew_ReducedName(TranslatedString, ArchiveNameZip.c_str());
                FinalString = std::wstring(
                    TranslatedString.Ptr(),
                    TranslatedString.Len());
                break;
            }
            case CommandID::HashCRC32:
            {
                FinalString = L"CRC-32";
                break;
            }
            case CommandID::HashCRC64:
            {
                FinalString = L"CRC-64";
                break;
            }
            case CommandID::HashSHA1:
            {
                FinalString = L"SHA-1";
                break;
            }
            case CommandID::HashSHA256:
            {
                FinalString = L"SHA-256";
                break;
            }
            case CommandID::HashAll:
            {
                FinalString = L"*";
                break;
            }
            default:
                break;
            }

            return FinalString.empty()
                ? E_NOTIMPL
                : SHStrDupW(FinalString.c_str(), ppszName);
        }

        HRESULT STDMETHODCALLTYPE GetIcon(
            _In_opt_ IShellItemArray* psiItemArray,
            _Outptr_ LPWSTR* ppszIcon)
        {
            Mile::UnreferencedParameter(psiItemArray);

            *ppszIcon = nullptr;

            return this->m_Icon.empty()
                ? E_NOTIMPL :
                :: SHStrDupW(this->m_Icon.c_str(), ppszIcon);
        }

        HRESULT STDMETHODCALLTYPE GetToolTip(
            _In_opt_ IShellItemArray* psiItemArray,
            _Outptr_ LPWSTR* ppszInfotip)
        {
            Mile::UnreferencedParameter(psiItemArray);
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
            Mile::UnreferencedParameter(psiItemArray);
            Mile::UnreferencedParameter(fOkToBeSlow);
            *pCmdState = ECS_ENABLED;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE Invoke(
            _In_opt_ IShellItemArray* psiItemArray,
            _In_opt_ IBindCtx* pbc)
        {
            Mile::UnreferencedParameter(pbc);

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
                        return Mile::HResultFromLastError();
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

                CalcChecksum(FileNames, MethodName.c_str());
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
            *pFlags = this->m_SubCommands.empty()
                ? this->m_IsSeparator ? ECF_ISSEPARATOR : ECF_DEFAULT
                : ECF_HASSUBCOMMANDS;
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
                this->AddRef();
                return this->QueryInterface(IID_PPV_ARGS(ppEnum));
            }         
        }

#pragma endregion

#pragma region IObjectWithSite

        HRESULT STDMETHODCALLTYPE SetSite(
            _In_opt_ IUnknown* pUnkSite)
        {
            *this->m_Site.put() = pUnkSite;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE GetSite(
            _In_ REFIID riid,
            _Outptr_ void** ppvSite)
        {
            return this->m_Site->QueryInterface(riid, ppvSite);
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
            Mile::UnreferencedParameter(celt);
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
            Mile::UnreferencedParameter(pUnkOuter);

            try
            {
                using NanaZip::ShellExtension::ExplorerCommandBase;
                using NanaZip::ShellExtension::SubCommandList;

                CContextMenuInfo ContextMenuInfo;
                ContextMenuInfo.Load();
                DWORD ContextMenuFlags = ContextMenuInfo.Flags;
                CBoolPair ContextMenuElimDup = ContextMenuInfo.ElimDup;

                LoadLangOneTime();

                SubCommandList NanaZipMenu;

                if (ContextMenuFlags & NContextMenuFlags::kOpen)
                {
                    NanaZipMenu.push_back(
                        winrt::make<ExplorerCommandBase>(
                            CommandID::Open,
                            ContextMenuElimDup));
                }

                if (ContextMenuFlags & NContextMenuFlags::kTest)
                {
                    NanaZipMenu.push_back(
                        winrt::make<ExplorerCommandBase>(
                            CommandID::Test,
                            ContextMenuElimDup));
                }

                if ((ContextMenuFlags & NContextMenuFlags::kOpen) ||
                    (ContextMenuFlags & NContextMenuFlags::kTest))
                {
                    NanaZipMenu.push_back(
                        winrt::make<ExplorerCommandBase>());
                }

                if (ContextMenuFlags & NContextMenuFlags::kExtract)
                {
                    NanaZipMenu.push_back(
                        winrt::make<ExplorerCommandBase>(
                            CommandID::Extract,
                            ContextMenuElimDup));
                }

                if (ContextMenuFlags & NContextMenuFlags::kExtractHere)
                {
                    NanaZipMenu.push_back(
                        winrt::make<ExplorerCommandBase>(
                            CommandID::ExtractHere,
                            ContextMenuElimDup));
                }

                if (ContextMenuFlags & NContextMenuFlags::kExtractTo)
                {
                    NanaZipMenu.push_back(
                        winrt::make<ExplorerCommandBase>(
                            CommandID::ExtractTo,
                            ContextMenuElimDup));
                }

                if ((ContextMenuFlags & NContextMenuFlags::kExtract) ||
                    (ContextMenuFlags & NContextMenuFlags::kExtractHere) ||
                    (ContextMenuFlags & NContextMenuFlags::kExtractTo))
                {
                    NanaZipMenu.push_back(
                        winrt::make<ExplorerCommandBase>());
                }

                if (ContextMenuFlags & NContextMenuFlags::kCompress)
                {
                    NanaZipMenu.push_back(
                        winrt::make<ExplorerCommandBase>(
                            CommandID::Compress,
                            ContextMenuElimDup));
                }

                if (ContextMenuFlags & NContextMenuFlags::kCompressTo7z)
                {
                    NanaZipMenu.push_back(
                        winrt::make<ExplorerCommandBase>(
                            CommandID::CompressTo7z,
                            ContextMenuElimDup));
                }

                if (ContextMenuFlags & NContextMenuFlags::kCompressToZip)
                {
                    NanaZipMenu.push_back(
                        winrt::make<ExplorerCommandBase>(
                            CommandID::CompressToZip,
                            ContextMenuElimDup));
                }

                if ((ContextMenuFlags & NContextMenuFlags::kCompress) ||
                    (ContextMenuFlags & NContextMenuFlags::kCompressTo7z) ||
                    (ContextMenuFlags & NContextMenuFlags::kCompressToZip))
                {
                    NanaZipMenu.push_back(
                        winrt::make<ExplorerCommandBase>());
                }

                if (ContextMenuFlags & NContextMenuFlags::kCompressEmail)
                {
                    NanaZipMenu.push_back(
                        winrt::make<ExplorerCommandBase>(
                            CommandID::CompressEmail,
                            ContextMenuElimDup));
                }

                if (ContextMenuFlags & NContextMenuFlags::kCompressTo7zEmail)
                {
                    NanaZipMenu.push_back(
                        winrt::make<ExplorerCommandBase>(
                            CommandID::CompressTo7zEmail,
                            ContextMenuElimDup));
                }           

                if (ContextMenuFlags & NContextMenuFlags::kCompressToZipEmail)
                {
                    NanaZipMenu.push_back(
                        winrt::make<ExplorerCommandBase>(
                            CommandID::CompressToZipEmail,
                            ContextMenuElimDup));
                }

                if ((ContextMenuFlags & NContextMenuFlags::kCompressEmail) ||
                    (ContextMenuFlags & NContextMenuFlags::kCompressTo7zEmail)||
                    (ContextMenuFlags & NContextMenuFlags::kCompressToZipEmail))
                {
                    NanaZipMenu.push_back(
                        winrt::make<ExplorerCommandBase>());
                }

                if (ContextMenuFlags & NContextMenuFlags::kCRC)
                {
                    NanaZipMenu.push_back(
                        winrt::make<ExplorerCommandBase>(
                            CommandID::HashCRC32,
                            ContextMenuElimDup));
                    NanaZipMenu.push_back(
                        winrt::make<ExplorerCommandBase>(
                            CommandID::HashCRC64,
                            ContextMenuElimDup));
                    NanaZipMenu.push_back(
                        winrt::make<ExplorerCommandBase>(
                            CommandID::HashSHA1,
                            ContextMenuElimDup));
                    NanaZipMenu.push_back(
                        winrt::make<ExplorerCommandBase>(
                            CommandID::HashSHA256,
                            ContextMenuElimDup));
                    NanaZipMenu.push_back(
                        winrt::make<ExplorerCommandBase>(
                            CommandID::HashAll,
                            ContextMenuElimDup));
                }

                return winrt::make<ExplorerCommandBase>(
                    CommandID::None,
                    ContextMenuElimDup,
                    std::wstring(),
                    NanaZipMenu)->QueryInterface(riid, ppvObject);
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
