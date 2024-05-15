// LangUtils.cpp

#include "StdAfx.h"

#include "../../../Common/Lang.h"

#include "../../../Windows/DLL.h"
#include "../../../Windows/Synchronization.h"
#include "../../../Windows/Window.h"

#include "LangUtils.h"
#include "RegistryUtils.h"

using namespace NWindows;

#ifndef _UNICODE
extern bool g_IsNT;
#endif

UString g_LangID;

// static
CLang g_Lang;
static bool g_Loaded = false;
static NSynchronization::CCriticalSection g_CriticalSection;

bool LangOpen(CLang &lang, CFSTR fileName);
bool LangOpen(CLang &lang, CFSTR fileName)
{
  // **************** NanaZip Modification Start ****************
  //return lang.Open(fileName, "7-Zip");
  return lang.Open(fileName, "NanaZip");
  // **************** NanaZip Modification End ****************
}

FString GetLangDirPrefix()
{
  // **************** NanaZip Modification Start ****************
  //return NDLL::GetModuleDirPrefix() + FTEXT("Lang") FSTRING_PATH_SEPARATOR;
#ifdef Z7_SFX
  return L"";
#else
  return NDLL::GetModuleDirPrefix() + FTEXT("Lang") FSTRING_PATH_SEPARATOR;
#endif
  // **************** NanaZip Modification End ****************
}

#ifdef Z7_LANG

void LoadLangOneTime()
{
  NSynchronization::CCriticalSectionLock lock(g_CriticalSection);
  if (g_Loaded)
    return;
  g_Loaded = true;
  ReloadLang();
}

void LangSetDlgItemText(HWND dialog, UInt32 controlID, UInt32 langID)
{
  const wchar_t *s = g_Lang.Get(langID);
  if (s)
  {
    CWindow window(GetDlgItem(dialog, (int)controlID));
    window.SetText(s);
  }
}

#ifndef IDCONTINUE
#define IDCONTINUE 11
#endif

static const CIDLangPair kLangPairs[] =
{
  { IDOK,     401 },
  { IDCANCEL, 402 },
  { IDYES,    406 },
  { IDNO,     407 },
  { IDCLOSE,  408 },
  { IDHELP,   409 },
  { IDCONTINUE, 411 }
};


void LangSetDlgItems(HWND dialog, const UInt32 *ids, unsigned numItems)
{
  unsigned i;
  for (i = 0; i < Z7_ARRAY_SIZE(kLangPairs); i++)
  {
    const CIDLangPair &pair = kLangPairs[i];
    CWindow window(GetDlgItem(dialog, (int)pair.ControlID));
    if (window)
    {
      const wchar_t *s = g_Lang.Get(pair.LangID);
      if (s)
        window.SetText(s);
    }
  }

  for (i = 0; i < numItems; i++)
  {
    const UInt32 id = ids[i];
    LangSetDlgItemText(dialog, id, id);
  }
}

void LangSetDlgItems_Colon(HWND dialog, const UInt32 *ids, unsigned numItems)
{
  for (unsigned i = 0; i < numItems; i++)
  {
    const UInt32 id = ids[i];
    const wchar_t *s = g_Lang.Get(id);
    if (s)
    {
      CWindow window(GetDlgItem(dialog, (int)id));
      UString s2 = s;
      s2.Add_Colon();
      window.SetText(s2);
    }
  }
}

void LangSetDlgItems_RemoveColon(HWND dialog, const UInt32 *ids, unsigned numItems)
{
  for (unsigned i = 0; i < numItems; i++)
  {
    const UInt32 id = ids[i];
    const wchar_t *s = g_Lang.Get(id);
    if (s)
    {
      CWindow window(GetDlgItem(dialog, (int)id));
      UString s2 = s;
      if (!s2.IsEmpty() && s2.Back() == ':')
        s2.DeleteBack();
      window.SetText(s2);
    }
  }
}

void LangSetWindowText(HWND window, UInt32 langID)
{
  const wchar_t *s = g_Lang.Get(langID);
  if (s)
    MySetWindowText(window, s);
}

UString LangString(UInt32 langID)
{
  const wchar_t *s = g_Lang.Get(langID);
  if (s)
    return s;
  return MyLoadString(langID);
}

void AddLangString(UString &s, UInt32 langID)
{
  s += LangString(langID);
}

void LangString(UInt32 langID, UString &dest)
{
  const wchar_t *s = g_Lang.Get(langID);
  if (s)
  {
    dest = s;
    return;
  }
  MyLoadString(langID, dest);
}

void LangString_OnlyFromLangFile(UInt32 langID, UString &dest)
{
  dest.Empty();
  const wchar_t *s = g_Lang.Get(langID);
  if (s)
    dest = s;
}

static const char * const kLangs =
  "ar.bg.ca.zh.-tw.-cn.cs.da.de.el.en.es.fi.fr.he.hu.is."
  "it.ja.ko.nl.no.=nb.=nn.pl.pt.-br.rm.ro.ru.sr.=hr.-spl.-spc.=hr.=bs.sk.sq.sv.th.tr."
  "ur.id.uk.be.sl.et.lv.lt.tg.fa.vi.hy.az.eu.hsb.mk."
  "st.ts.tn.ve.xh.zu.af.ka.fo.hi.mt.se.ga.yi.ms.kk."
  "ky.sw.tk.uz.-latn.-cyrl.tt.bn.pa.-in.gu.or.ta.te.kn.ml.as.mr.sa."
  "mn.=mn.=mng.bo.cy.kh.lo.my.gl.kok..sd.syr.si..iu.am.tzm."
  "ks.ne.fy.ps.tl.dv..ff.ha..yo.qu.st.ba.lb.kl."
  "ig.kr.om.ti.gn..la.so.ii..arn..moh..br.."
  "ug.mi.oc.co."
  // "gsw.sah.qut.rw.wo....prs...."
  // ".gd."
  ;

static void FindShortNames(UInt32 primeLang, AStringVector &names)
{
  UInt32 index = 0;
  for (const char *p = kLangs; *p != 0;)
  {
    const char *p2 = p;
    for (; *p2 != '.'; p2++);
    bool isSub = (p[0] == '-' || p[0] == '=');
    if (!isSub)
      index++;
    if (index >= primeLang)
    {
      if (index > primeLang)
        break;
      AString s;
      if (isSub)
      {
        if (p[0] == '-')
          s = names[0];
        else
          p++;
      }
      while (p != p2)
        s.Add_Char((char)(Byte)*p++);
      names.Add(s);
    }
    p = p2 + 1;
  }
}

/*
#include "../../../Common/IntToString.h"

static struct CC1Lang
{
  CC1Lang()
  {
    for (int i = 1; i < 150; i++)
    {
      UString s;
      char ttt[32];
      ConvertUInt32ToHex(i, ttt);
      s += ttt;
      UStringVector names;
      FindShortNames(i, names);

      FOR_VECTOR (k, names)
      {
        s.Add_Space();
        s += names[k];
      }
      OutputDebugStringW(s);
    }
  }
} g_cc1;
*/

// typedef LANGID (WINAPI *GetUserDefaultUILanguageP)();

void Lang_GetShortNames_for_DefaultLang(AStringVector &names, unsigned &subLang)
{
  names.Clear();
  subLang = 0;
  // Region / Administative / Language for non-Unicode programs:
  const LANGID sysLang = GetSystemDefaultLangID();

  // Region / Formats / Format:
  const LANGID userLang = GetUserDefaultLangID();

  if (PRIMARYLANGID(sysLang) !=
      PRIMARYLANGID(userLang))
    return;
  const LANGID langID = userLang;

  // const LANGID langID = MAKELANGID(0x1a, 1); // for debug
  
  /*
  LANGID sysUILang; // english  in XP64
  LANGID userUILang; // english  in XP64

  GetUserDefaultUILanguageP fn = (GetUserDefaultUILanguageP)GetProcAddress(
      GetModuleHandle("kernel32"), "GetUserDefaultUILanguage");
  if (fn)
    userUILang = fn();
  fn = (GetUserDefaultUILanguageP)GetProcAddress(
      GetModuleHandle("kernel32"), "GetSystemDefaultUILanguage");
  if (fn)
    sysUILang = fn();
  */

  const WORD primLang = (WORD)(PRIMARYLANGID(langID));
  subLang = SUBLANGID(langID);
  FindShortNames(primLang, names);
}


static void OpenDefaultLang()
{
  AStringVector names;
  unsigned subLang;
  Lang_GetShortNames_for_DefaultLang(names, subLang);
  {
    const FString dirPrefix (GetLangDirPrefix());
    for (unsigned i = 0; i < 2; i++)
    {
      const unsigned index = (i == 0 ? subLang : 0);
      if (index < names.Size())
      {
        const AString &name = names[index];
        if (!name.IsEmpty())
        {
          FString path (dirPrefix);
          path += name;
          path += ".txt";
          if (LangOpen(g_Lang, path))
          {
            g_LangID = name;
            return;
          }
        }
      }
    }
  }
}

void ReloadLang()
{
  g_Lang.Clear();
  ReadRegLang(g_LangID);
  #ifndef _UNICODE
  if (g_IsNT)
  #endif
  {
    if (g_LangID.IsEmpty())
    {
      OpenDefaultLang();
      return;
    }
  }
  if (g_LangID.Len() > 1 || g_LangID[0] != L'-')
  {
    FString s = us2fs(g_LangID);
    if (s.ReverseFind_PathSepar() < 0)
    {
      if (s.ReverseFind_Dot() < 0)
        s += ".txt";
      s.Insert(0, GetLangDirPrefix());
      LangOpen(g_Lang, s);
    }
  }
}

#endif
