// EnumFormatEtc.cpp

#include "StdAfx.h"

#include "EnumFormatEtc.h"
#include "../../IDecl.h"
#include "MyCom2.h"

class CEnumFormatEtc Z7_final:
  public IEnumFORMATETC,
  public CMyUnknownImp
{
  Z7_COM_UNKNOWN_IMP_1_MT(IEnumFORMATETC)

  STDMETHOD(Next)(ULONG celt, FORMATETC *rgelt, ULONG *pceltFetched) Z7_override;
  STDMETHOD(Skip)(ULONG celt) Z7_override;
  STDMETHOD(Reset)(void) Z7_override;
  STDMETHOD(Clone)(IEnumFORMATETC **ppEnumFormatEtc) Z7_override;

  LONG m_RefCount;
  ULONG m_NumFormats;
  FORMATETC *m_Formats;
  ULONG m_Index;
public:
  CEnumFormatEtc(const FORMATETC *pFormatEtc, ULONG numFormats);
  ~CEnumFormatEtc();
};

static void DeepCopyFormatEtc(FORMATETC *dest, const FORMATETC *src)
{
  *dest = *src;
  if (src->ptd)
  {
    dest->ptd = (DVTARGETDEVICE*)CoTaskMemAlloc(sizeof(DVTARGETDEVICE));
    if (!dest->ptd)
      throw 2026; // CNewException()
    *(dest->ptd) = *(src->ptd);
  }
}

CEnumFormatEtc::CEnumFormatEtc(const FORMATETC *pFormatEtc, ULONG numFormats)
{
  m_RefCount = 1;
  m_Index = 0;
  m_NumFormats = 0;
  m_Formats = new FORMATETC[numFormats];
  // if (m_Formats)
  {
    m_NumFormats = numFormats;
    for (ULONG i = 0; i < numFormats; i++)
      DeepCopyFormatEtc(&m_Formats[i], &pFormatEtc[i]);
  }
}

CEnumFormatEtc::~CEnumFormatEtc()
{
  if (m_Formats)
  {
    for (ULONG i = 0; i < m_NumFormats; i++)
      if (m_Formats[i].ptd)
        CoTaskMemFree(m_Formats[i].ptd);
    delete []m_Formats;
  }
}

Z7_COMWF_B CEnumFormatEtc::Next(ULONG celt, FORMATETC *pFormatEtc, ULONG *pceltFetched)
{
  ULONG copied  = 0;
  if (celt == 0 || !pFormatEtc)
    return E_INVALIDARG;
  while (m_Index < m_NumFormats && copied < celt)
  {
    DeepCopyFormatEtc(&pFormatEtc[copied], &m_Formats[m_Index]);
    copied++;
    m_Index++;
  }
  if (pceltFetched)
    *pceltFetched = copied;
  return (copied == celt) ? S_OK : S_FALSE;
}

Z7_COMWF_B CEnumFormatEtc::Skip(ULONG celt)
{
  m_Index += celt;
  return (m_Index <= m_NumFormats) ? S_OK : S_FALSE;
}

Z7_COMWF_B CEnumFormatEtc::Reset(void)
{
  m_Index = 0;
  return S_OK;
}

Z7_COMWF_B CEnumFormatEtc::Clone(IEnumFORMATETC ** ppEnumFormatEtc)
{
  HRESULT hResult = CreateEnumFormatEtc(m_NumFormats, m_Formats, ppEnumFormatEtc);
  if (hResult == S_OK)
    ((CEnumFormatEtc *)*ppEnumFormatEtc)->m_Index = m_Index;
  return hResult;
}

// replacement for SHCreateStdEnumFmtEtc
HRESULT CreateEnumFormatEtc(UINT numFormats, const FORMATETC *formats, IEnumFORMATETC **enumFormat)
{
  if (numFormats == 0 || !formats || !enumFormat)
    return E_INVALIDARG;
  *enumFormat = new CEnumFormatEtc(formats, numFormats);
  return (*enumFormat) ? S_OK : E_OUTOFMEMORY;
}
