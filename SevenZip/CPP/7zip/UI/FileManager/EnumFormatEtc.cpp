// EnumFormatEtc.cpp

#include "StdAfx.h"

#include "EnumFormatEtc.h"
#include "MyCom2.h"

class CEnumFormatEtc :
public IEnumFORMATETC,
public CMyUnknownImp
{
public:
  MY_UNKNOWN_IMP1_MT(IEnumFORMATETC)
    
  STDMETHOD(Next)(ULONG celt, FORMATETC *rgelt, ULONG *pceltFetched);
  STDMETHOD(Skip)(ULONG celt);
  STDMETHOD(Reset)(void);
  STDMETHOD(Clone)(IEnumFORMATETC **ppEnumFormatEtc);
  
  CEnumFormatEtc(const FORMATETC *pFormatEtc, ULONG numFormats);
  ~CEnumFormatEtc();
  
private:
  LONG m_RefCount;
  ULONG m_NumFormats;
  FORMATETC *m_Formats;
  ULONG m_Index;
};

static void DeepCopyFormatEtc(FORMATETC *dest, const FORMATETC *src)
{
  *dest = *src;
  if (src->ptd)
  {
    dest->ptd = (DVTARGETDEVICE*)CoTaskMemAlloc(sizeof(DVTARGETDEVICE));
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

STDMETHODIMP CEnumFormatEtc::Next(ULONG celt, FORMATETC *pFormatEtc, ULONG *pceltFetched)
{
  ULONG copied  = 0;
  if (celt == 0 || pFormatEtc == 0)
    return E_INVALIDARG;
  while (m_Index < m_NumFormats && copied < celt)
  {
    DeepCopyFormatEtc(&pFormatEtc[copied], &m_Formats[m_Index]);
    copied++;
    m_Index++;
  }
  if (pceltFetched != 0)
    *pceltFetched = copied;
  return (copied == celt) ? S_OK : S_FALSE;
}

STDMETHODIMP CEnumFormatEtc::Skip(ULONG celt)
{
  m_Index += celt;
  return (m_Index <= m_NumFormats) ? S_OK : S_FALSE;
}

STDMETHODIMP CEnumFormatEtc::Reset(void)
{
  m_Index = 0;
  return S_OK;
}

STDMETHODIMP CEnumFormatEtc::Clone(IEnumFORMATETC ** ppEnumFormatEtc)
{
  HRESULT hResult = CreateEnumFormatEtc(m_NumFormats, m_Formats, ppEnumFormatEtc);
  if (hResult == S_OK)
    ((CEnumFormatEtc *)*ppEnumFormatEtc)->m_Index = m_Index;
  return hResult;
}

// replacement for SHCreateStdEnumFmtEtc
HRESULT CreateEnumFormatEtc(UINT numFormats, const FORMATETC *formats, IEnumFORMATETC **enumFormat)
{
  if (numFormats == 0 || formats == 0 || enumFormat == 0)
    return E_INVALIDARG;
  *enumFormat = new CEnumFormatEtc(formats, numFormats);
  return (*enumFormat) ? S_OK : E_OUTOFMEMORY;
}
