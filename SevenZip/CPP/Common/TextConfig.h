// Common/TextConfig.h

#ifndef __COMMON_TEXT_CONFIG_H
#define __COMMON_TEXT_CONFIG_H

#include "MyString.h"

struct CTextConfigPair
{
  UString ID;
  UString String;
};

bool GetTextConfig(const AString &text, CObjectVector<CTextConfigPair> &pairs);

int FindTextConfigItem(const CObjectVector<CTextConfigPair> &pairs, const char *id) throw();
UString GetTextConfigValue(const CObjectVector<CTextConfigPair> &pairs, const char *id);

#endif
