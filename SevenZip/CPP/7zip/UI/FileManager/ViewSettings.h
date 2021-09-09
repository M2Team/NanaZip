// ViewSettings.h

#ifndef __VIEW_SETTINGS_H
#define __VIEW_SETTINGS_H

#include "../../../Common/MyTypes.h"
#include "../../../Common/MyString.h"

struct CColumnInfo
{
  PROPID PropID;
  bool IsVisible;
  UInt32 Width;

  bool IsEqual(const CColumnInfo &a) const
  {
    return PropID == a.PropID && IsVisible == a.IsVisible && Width == a.Width;
  }
};

struct CListViewInfo
{
  CRecordVector<CColumnInfo> Columns;
  PROPID SortID;
  bool Ascending;
  bool IsLoaded;

  void Clear()
  {
    SortID = 0;
    Ascending = true;
    IsLoaded = false;
    Columns.Clear();
  }

  CListViewInfo():
    SortID(0),
    Ascending(true),
    IsLoaded(false)
    {}

  /*
  int FindColumnWithID(PROPID propID) const
  {
    FOR_VECTOR (i, Columns)
      if (Columns[i].PropID == propID)
        return i;
    return -1;
  }
  */

  bool IsEqual(const CListViewInfo &info) const
  {
    if (Columns.Size() != info.Columns.Size() ||
        SortID != info.SortID ||
        Ascending != info.Ascending)
      return false;
    FOR_VECTOR (i, Columns)
      if (!Columns[i].IsEqual(info.Columns[i]))
        return false;
    return true;
  }

  void Save(const UString &id) const;
  void Read(const UString &id);
};


struct CWindowInfo
{
  RECT rect;
  bool maximized;

  UInt32 numPanels;
  UInt32 currentPanel;
  UInt32 splitterPos;

  void Save() const;
  void Read(bool &windowPosDefined, bool &panelInfoDefined);
};

void SaveToolbarsMask(UInt32 toolbarMask);
UInt32 ReadToolbarsMask();

const UInt32 kListMode_Report = 3;

struct CListMode
{
  UInt32 Panels[2];
  
  void Init() { Panels[0] = Panels[1] = kListMode_Report; }
  CListMode() { Init(); }
  
  void Save() const ;
  void Read();
};



void SavePanelPath(UInt32 panel, const UString &path);
bool ReadPanelPath(UInt32 panel, UString &path);


void SaveFolderHistory(const UStringVector &folders);
void ReadFolderHistory(UStringVector &folders);

void SaveFastFolders(const UStringVector &folders);
void ReadFastFolders(UStringVector &folders);

void SaveCopyHistory(const UStringVector &folders);
void ReadCopyHistory(UStringVector &folders);

void AddUniqueStringToHeadOfList(UStringVector &list, const UString &s);

#endif
