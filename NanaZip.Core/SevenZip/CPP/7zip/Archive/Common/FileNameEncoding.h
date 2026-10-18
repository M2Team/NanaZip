/*
 * PROJECT:    NanaZip
 * FILE:       FileNameEncoding.h
 * PURPOSE:    Filename encoding detection for raw archive headers
 *
 * LICENSE:    The MIT License
 */

#ifndef ZIP7_INC_FILE_NAME_ENCODING_H
#define ZIP7_INC_FILE_NAME_ENCODING_H

#include "../../../Common/StringConvert.h"
#include "../../../Common/StringToInt.h"
#include "../../../Common/UTFConvert.h"
#include "../../Common/MethodProps.h"

#ifdef _WIN32
#include <icu.h>
#endif

namespace NArchive
{
    static const UInt32 FileNameCodePageAuto = static_cast<UInt32>(-1);

    inline HRESULT ParseFileNameCodePage(
        PROPVARIANT const& Property,
        UInt32& CodePage)
    {
        if (Property.vt == VT_BSTR && Property.bstrVal &&
            ::StringsAreEqualNoCase_Ascii(Property.bstrVal, "auto"))
        {
            CodePage = FileNameCodePageAuto;
            return S_OK;
        }
        CodePage = CP_OEMCP;
        RINOK(::ParsePropToUInt32(L"", Property, CodePage))
#ifdef _WIN32
        if (CodePage != CP_ACP && CodePage != CP_OEMCP &&
            !::IsValidCodePage(CodePage))
        {
            return E_INVALIDARG;
        }
#else
        if (CodePage == FileNameCodePageAuto)
        {
            return E_INVALIDARG;
        }
#endif
        return S_OK;
    }

    class CFileNameEncodingSample
    {
    private:
        AString m_Bytes;

        UInt32 GetRegionalCodePage() const
        {
#ifdef _WIN32
            DWORD CodePage = 0;
            if (::GetLocaleInfoW(
                LOCALE_USER_DEFAULT,
                LOCALE_IDEFAULTANSICODEPAGE | LOCALE_RETURN_NUMBER,
                reinterpret_cast<LPWSTR>(&CodePage),
                sizeof(CodePage) / sizeof(WCHAR)) &&
                CodePage > CP_OEMCP && CodePage != CP_UTF8 &&
                ::IsValidCodePage(CodePage) &&
                ::MultiByteToWideChar(
                    CodePage,
                    MB_ERR_INVALID_CHARS,
                    this->m_Bytes.Ptr(),
                    static_cast<int>(this->m_Bytes.Len()),
                    nullptr,
                    0) != 0)
            {
                return CodePage;
            }
#endif
            return FileNameCodePageAuto;
        }

    public:
        void Add(AString const& Name)
        {
            const unsigned MaxBytes = 32768;
            if (Name.IsAscii() || Name.Len() >= MaxBytes ||
                this->m_Bytes.Len() + Name.Len() + 1 > MaxBytes ||
                ::CheckUTF8_AString(Name))
            {
                return;
            }
            this->m_Bytes += Name;
            this->m_Bytes += '\n';
        }

        UInt32 Detect() const
        {
            if (this->m_Bytes.IsEmpty())
            {
                return FileNameCodePageAuto;
            }
#ifdef _WIN32
            UErrorCode Error = U_ZERO_ERROR;
            struct CDetector
            {
                UCharsetDetector* Handle = nullptr;

                ~CDetector()
                {
                    if (this->Handle)
                    {
                        ::ucsdet_close(this->Handle);
                    }
                }
            };
            CDetector Detector;
            Detector.Handle = ::ucsdet_open(&Error);
            if (U_FAILURE(Error) || !Detector.Handle)
            {
                return FileNameCodePageAuto;
            }
            ::ucsdet_setText(
                Detector.Handle,
                this->m_Bytes.Ptr(),
                static_cast<int32_t>(this->m_Bytes.Len()),
                &Error);
            int32_t Count = 0;
            const UCharsetMatch** Matches = ::ucsdet_detectAll(
                Detector.Handle,
                &Count,
                &Error);
            if (U_FAILURE(Error) || !Matches || Count <= 0)
            {
                return FileNameCodePageAuto;
            }
            const int32_t Confidence = ::ucsdet_getConfidence(
                Matches[0], &Error);
            const bool Ambiguous = Confidence < 50 ||
                (Count > 1 &&
                    ::ucsdet_getConfidence(Matches[1], &Error) >= Confidence);
            if (U_FAILURE(Error))
            {
                return FileNameCodePageAuto;
            }
            if (Ambiguous)
            {
                return this->GetRegionalCodePage();
            }
            const char* Name = ::ucsdet_getName(Matches[0], &Error);
            if (U_FAILURE(Error) || !Name)
            {
                return FileNameCodePageAuto;
            }
            const char* WindowsName = ::ucnv_getStandardName(
                Name, "WINDOWS", &Error);
            if (U_FAILURE(Error))
            {
                return FileNameCodePageAuto;
            }
            UInt32 CodePage = FileNameCodePageAuto;
            if (WindowsName && ::strncmp(WindowsName, "windows-", 8) == 0)
            {
                const char* End;
                CodePage = ::ConvertStringToUInt32(WindowsName + 8, &End);
                if (*End != 0)
                {
                    return FileNameCodePageAuto;
                }
            }
            else if (::strcmp(Name, "EUC-JP") == 0)
            {
                CodePage = 20932;
            }
            return ::IsValidCodePage(CodePage) ? CodePage : FileNameCodePageAuto;
#else
            return FileNameCodePageAuto;
#endif
        }
    };
}

#endif
