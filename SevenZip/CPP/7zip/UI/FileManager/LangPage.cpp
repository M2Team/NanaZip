// LangPage.cpp

#include "StdAfx.h"

#include "../../../Common/Lang.h"

#include "../../../Windows/FileFind.h"
#include "../../../Windows/ResourceString.h"

#include "HelpUtils.h"
#include "LangPage.h"
#include "LangPageRes.h"
#include "LangUtils.h"
#include "RegistryUtils.h"

using namespace NWindows;

static const UInt32 kLangIDs[] =
{
  IDT_LANG_LANG
};

#define kLangTopic "fm/options.htm#language"

static void NativeLangString(UString &dest, const wchar_t *s)
{
  dest += " (";
  dest += s;
  dest += ')';
}

bool LangOpen(CLang &lang, CFSTR fileName);

bool CLangPage::OnInit()
{
  LangSetDlgItems(*this, kLangIDs, ARRAY_SIZE(kLangIDs));

  _langCombo.Attach(GetItem(IDC_LANG_LANG));

  UString temp = MyLoadString(IDS_LANG_ENGLISH);
  NativeLangString(temp, MyLoadString(IDS_LANG_NATIVE));
  int index = (int)_langCombo.AddString(temp);
  _langCombo.SetItemData(index, _paths.Size());
  _paths.Add(L"-");
  _langCombo.SetCurSel(0);

  const FString dirPrefix = GetLangDirPrefix();
  NFile::NFind::CEnumerator enumerator;
  enumerator.SetDirPrefix(dirPrefix);
  NFile::NFind::CFileInfo fi;
  CLang lang;
  UString error;
  
  while (enumerator.Next(fi))
  {
    if (fi.IsDir())
      continue;
    const unsigned kExtSize = 4;
    if (fi.Name.Len() < kExtSize)
      continue;
    const unsigned pos = fi.Name.Len() - kExtSize;
    if (!StringsAreEqualNoCase_Ascii(fi.Name.Ptr(pos), ".txt"))
    {
      // if (!StringsAreEqualNoCase_Ascii(fi.Name.Ptr(pos), ".ttt"))
      continue;
    }

    if (!LangOpen(lang, dirPrefix + fi.Name))
    {
      error.Add_Space_if_NotEmpty();
      error += fs2us(fi.Name);
      continue;
    }
    
    const UString shortName = fs2us(fi.Name.Left(pos));
    UString s = shortName;
    const wchar_t *eng = lang.Get(IDS_LANG_ENGLISH);
    if (eng)
      s = eng;
    const wchar_t *native = lang.Get(IDS_LANG_NATIVE);
    if (native)
      NativeLangString(s, native);
    index = (int)_langCombo.AddString(s);
    _langCombo.SetItemData(index, _paths.Size());
    _paths.Add(shortName);
    if (g_LangID.IsEqualTo_NoCase(shortName))
      _langCombo.SetCurSel(index);
  }
  
  if (!error.IsEmpty())
    MessageBoxW(0, error, L"Error in Lang file", MB_ICONERROR);
  return CPropertyPage::OnInit();
}

LONG CLangPage::OnApply()
{
  int pathIndex = (int)_langCombo.GetItemData_of_CurSel();
  if (_needSave)
    SaveRegLang(_paths[pathIndex]);
  _needSave = false;
  ReloadLang();
  LangWasChanged = true;
  return PSNRET_NOERROR;
}

void CLangPage::OnNotifyHelp()
{
  ShowHelpWindow(kLangTopic);
}

bool CLangPage::OnCommand(int code, int itemID, LPARAM param)
{
  if (code == CBN_SELCHANGE && itemID == IDC_LANG_LANG)
  {
    _needSave = true;
    Changed();
    return true;
  }
  return CPropertyPage::OnCommand(code, itemID, param);
}
