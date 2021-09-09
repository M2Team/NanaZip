// OverwriteDialogFar.cpp

#include "StdAfx.h"

#include <stdio.h>

#include "../../../Common/StringConvert.h"
#include "../../../Common/IntToString.h"

#include "../../../Windows/FileName.h"
#include "../../../Windows/PropVariantConv.h"

#include "FarUtils.h"
#include "Messages.h"

#include "OverwriteDialogFar.h"

using namespace NWindows;
using namespace NFar;

namespace NOverwriteDialog {

struct CFileInfoStrings
{
  AString Size;
  AString Time;
};

static void SetFileInfoStrings(const CFileInfo &fileInfo,
    CFileInfoStrings &fileInfoStrings)
{
  char buffer[256];

  if (fileInfo.SizeIsDefined)
  {
    ConvertUInt64ToString(fileInfo.Size, buffer);
    fileInfoStrings.Size = buffer;
    fileInfoStrings.Size += ' ';
    fileInfoStrings.Size += g_StartupInfo.GetMsgString(NMessageID::kOverwriteBytes);
  }
  else
  {
    fileInfoStrings.Size = "";
  }

  fileInfoStrings.Time.Empty();
  if (fileInfo.TimeIsDefined)
  {
    char timeString[32];
    ConvertUtcFileTimeToString(fileInfo.Time, timeString);
    fileInfoStrings.Time = g_StartupInfo.GetMsgString(NMessageID::kOverwriteModifiedOn);
    fileInfoStrings.Time += ' ';
    fileInfoStrings.Time += timeString;
  }
}

static void ReduceString2(UString &s, unsigned size)
{
  if (!s.IsEmpty() && s.Back() == ' ')
  {
    // s += (wchar_t)(0x2423);
    s.InsertAtFront(L'\"');
    s += L'\"';
  }
  ReduceString(s, size);
}

NResult::EEnum Execute(const CFileInfo &oldFileInfo, const CFileInfo &newFileInfo)
{
  const int kYSize = 22;
  const int kXSize = 76;
  
  CFileInfoStrings oldFileInfoStrings;
  CFileInfoStrings newFileInfoStrings;

  SetFileInfoStrings(oldFileInfo, oldFileInfoStrings);
  SetFileInfoStrings(newFileInfo, newFileInfoStrings);

  const UString &oldName2 = oldFileInfo.Name;
  const UString &newName2 = newFileInfo.Name;

  int slashPos = oldName2.ReverseFind_PathSepar();
  UString pref1 = oldName2.Left(slashPos + 1);
  UString name1 = oldName2.Ptr(slashPos + 1);

  slashPos = newName2.ReverseFind_PathSepar();
  UString pref2 = newName2.Left(slashPos + 1);
  UString name2 = newName2.Ptr(slashPos + 1);

  const unsigned kNameOffset = 2;
  {
    const unsigned maxNameLen = kXSize - 9 - 2;
    ReduceString(pref1, maxNameLen);
    ReduceString(pref2, maxNameLen);
    ReduceString2(name1, maxNameLen - kNameOffset);
    ReduceString2(name2, maxNameLen - kNameOffset);
  }

  AString pref1A (UnicodeStringToMultiByte(pref1, CP_OEMCP));
  AString pref2A (UnicodeStringToMultiByte(pref2, CP_OEMCP));
  AString name1A (UnicodeStringToMultiByte(name1, CP_OEMCP));
  AString name2A (UnicodeStringToMultiByte(name2, CP_OEMCP));

  struct CInitDialogItem initItems[]={
    { DI_DOUBLEBOX, 3, 1, kXSize - 4, kYSize - 2, false, false, 0, false, NMessageID::kOverwriteTitle, NULL, NULL },
    { DI_TEXT, 5, 2, 0, 0, false, false, 0, false, NMessageID::kOverwriteMessage1, NULL, NULL },
    
    { DI_TEXT, 3, 3, 0, 0, false, false, DIF_BOXCOLOR|DIF_SEPARATOR, false, -1, "", NULL  },
    
    { DI_TEXT, 5, 4, 0, 0, false, false, 0, false, NMessageID::kOverwriteMessageWouldYouLike, NULL, NULL },

    { DI_TEXT, 7, 6, 0, 0, false, false, 0, false,  -1, pref1A, NULL },
    { DI_TEXT, 7 + kNameOffset, 7, 0, 0, false, false, 0, false,  -1, name1A, NULL },
    { DI_TEXT, 7, 8, 0, 0, false, false, 0, false,  -1, oldFileInfoStrings.Size, NULL },
    { DI_TEXT, 7, 9, 0, 0, false, false, 0, false,  -1, oldFileInfoStrings.Time, NULL },

    { DI_TEXT, 5, 11, 0, 0, false, false, 0, false, NMessageID::kOverwriteMessageWithtTisOne, NULL, NULL },
    
    { DI_TEXT, 7, 13, 0, 0, false, false, 0, false,  -1, pref2A, NULL },
    { DI_TEXT, 7 + kNameOffset, 14, 0, 0, false, false, 0, false,  -1, name2A, NULL },
    { DI_TEXT, 7, 15, 0, 0, false, false, 0, false,  -1, newFileInfoStrings.Size, NULL },
    { DI_TEXT, 7, 16, 0, 0, false, false, 0, false,  -1, newFileInfoStrings.Time, NULL },

    { DI_TEXT, 3, kYSize - 5, 0, 0, false, false, DIF_BOXCOLOR|DIF_SEPARATOR, false, -1, "", NULL  },
        
    { DI_BUTTON, 0, kYSize - 4, 0, 0, true, false, DIF_CENTERGROUP, true, NMessageID::kOverwriteYes, NULL, NULL  },
    { DI_BUTTON, 0, kYSize - 4, 0, 0, false, false, DIF_CENTERGROUP, false, NMessageID::kOverwriteYesToAll, NULL, NULL  },
    { DI_BUTTON, 0, kYSize - 4, 0, 0, false, false, DIF_CENTERGROUP, false, NMessageID::kOverwriteNo, NULL, NULL  },
    { DI_BUTTON, 0, kYSize - 4, 0, 0, false, false, DIF_CENTERGROUP, false, NMessageID::kOverwriteNoToAll, NULL, NULL  },
    { DI_BUTTON, 0, kYSize - 3, 0, 0, false, false, DIF_CENTERGROUP, false, NMessageID::kOverwriteAutoRename, NULL, NULL  },
    { DI_BUTTON, 0, kYSize - 3, 0, 0, false, false, DIF_CENTERGROUP, false, NMessageID::kOverwriteCancel, NULL, NULL  }
  };
  
  const int kNumDialogItems = ARRAY_SIZE(initItems);
  FarDialogItem aDialogItems[kNumDialogItems];
  g_StartupInfo.InitDialogItems(initItems, aDialogItems, kNumDialogItems);
  int anAskCode = g_StartupInfo.ShowDialog(kXSize, kYSize,
      NULL, aDialogItems, kNumDialogItems);
  const int kButtonStartPos = kNumDialogItems - 6;
  if (anAskCode >= kButtonStartPos && anAskCode < kNumDialogItems)
    return NResult::EEnum(anAskCode - kButtonStartPos);
  return NResult::kCancel;
}

}
