// SystemPage.h
 
#ifndef __SYSTEM_PAGE_H
#define __SYSTEM_PAGE_H

#include "../../../Windows/Control/ImageList.h"
#include "../../../Windows/Control/ListView.h"
#include "../../../Windows/Control/PropertyPage.h"

#include "FilePlugins.h"
#include "RegistryAssociations.h"

enum EExtState
{
  kExtState_Clear = 0,
  kExtState_Other,
  kExtState_7Zip
};

struct CModifiedExtInfo: public NRegistryAssoc::CShellExtInfo
{
  int OldState;
  int State;
  int ImageIndex;
  bool Other;
  bool Other7Zip;

  CModifiedExtInfo(): ImageIndex(-1) {}

  CSysString GetString() const;

  void SetState(const UString &iconPath)
  {
    State = kExtState_Clear;
    Other = false;
    Other7Zip = false;
    if (!ProgramKey.IsEmpty())
    {
      State = kExtState_Other;
      Other = true;
      if (IsIt7Zip())
      {
        Other7Zip = !iconPath.IsEqualTo_NoCase(IconPath);
        if (!Other7Zip)
        {
          State = kExtState_7Zip;
          Other = false;
        }
      }
    }
    OldState = State;
  };
};

struct CAssoc
{
  CModifiedExtInfo Pair[2];
  int SevenZipImageIndex;

  int GetIconIndex() const
  {
    for (unsigned i = 0; i < 2; i++)
    {
      const CModifiedExtInfo &pair = Pair[i];
      if (pair.State == kExtState_Clear)
        continue;
      if (pair.State == kExtState_7Zip)
        return SevenZipImageIndex;
      if (pair.ImageIndex != -1)
        return pair.ImageIndex;
    }
    return -1;
  }
};

#ifdef UNDER_CE
  #define NUM_EXT_GROUPS 1
#else
  #define NUM_EXT_GROUPS 2
#endif

class CSystemPage: public NWindows::NControl::CPropertyPage
{
  CExtDatabase _extDB;
  CObjectVector<CAssoc> _items;

  unsigned _numIcons;
  NWindows::NControl::CImageList _imageList;
  NWindows::NControl::CListView _listView;

  bool _needSave;

  HKEY GetHKey(unsigned
      #if NUM_EXT_GROUPS != 1
        group
      #endif
      ) const
  {
    #if NUM_EXT_GROUPS == 1
      return HKEY_CLASSES_ROOT;
    #else
      return group == 0 ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
    #endif
  }

  int AddIcon(const UString &path, int iconIndex);
  unsigned GetRealIndex(unsigned listIndex) const { return listIndex; }
  void RefreshListItem(unsigned group, unsigned listIndex);
  void ChangeState(unsigned group, const CUIntVector &indices);
  void ChangeState(unsigned group);

  bool OnListKeyDown(LPNMLVKEYDOWN keyDownInfo);
  
public:
  bool WasChanged;
  
  CSystemPage(): WasChanged(false) {}

  virtual bool OnInit();
  virtual void OnNotifyHelp();
  virtual bool OnNotify(UINT controlID, LPNMHDR lParam);
  virtual LONG OnApply();
  virtual bool OnButtonClicked(int buttonID, HWND buttonHWND);
};

#endif
