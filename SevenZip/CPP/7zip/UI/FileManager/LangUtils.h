// LangUtils.h

#ifndef __LANG_UTILS_H
#define __LANG_UTILS_H

#include "../../../Windows/ResourceString.h"

#ifdef LANG

extern UString g_LangID;

struct CIDLangPair
{
  UInt32 ControlID;
  UInt32 LangID;
};

void ReloadLang();
void LoadLangOneTime();
FString GetLangDirPrefix();

void LangSetDlgItemText(HWND dialog, UInt32 controlID, UInt32 langID);
void LangSetDlgItems(HWND dialog, const UInt32 *ids, unsigned numItems);
void LangSetDlgItems_Colon(HWND dialog, const UInt32 *ids, unsigned numItems);
void LangSetWindowText(HWND window, UInt32 langID);

UString LangString(UInt32 langID);
void AddLangString(UString &s, UInt32 langID);
void LangString(UInt32 langID, UString &dest);
void LangString_OnlyFromLangFile(UInt32 langID, UString &dest);
 
#else

inline UString LangString(UInt32 langID) { return NWindows::MyLoadString(langID); }
inline void LangString(UInt32 langID, UString &dest) { NWindows::MyLoadString(langID, dest); }
inline void AddLangString(UString &s, UInt32 langID) { s += NWindows::MyLoadString(langID); }

#endif

#endif
