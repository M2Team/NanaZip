// Windows/ResourceString.h

#ifndef __WINDOWS_RESOURCE_STRING_H
#define __WINDOWS_RESOURCE_STRING_H

#include "../Common/MyString.h"

namespace NWindows {

UString MyLoadString(UINT resourceID);
void MyLoadString(HINSTANCE hInstance, UINT resourceID, UString &dest);
void MyLoadString(UINT resourceID, UString &dest);

}

#endif
