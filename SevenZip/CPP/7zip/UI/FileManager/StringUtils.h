// StringUtils.h

#ifndef __STRING_UTILS_H
#define __STRING_UTILS_H

#include "../../../Common/MyString.h"

void SplitStringToTwoStrings(const UString &src, UString &dest1, UString &dest2);

void SplitString(const UString &srcString, UStringVector &destStrings);
UString JoinStrings(const UStringVector &srcStrings);

#endif
