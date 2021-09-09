// SevenZip/Plugin.cpp

#include "StdAfx.h"

#include "Plugin.h"

/*
void CPlugin::AddRealIndexOfFile(const CArchiveFolderItem &aFolder,
    int anIndexInVector, vector<int> &aRealIndexes)
{
  const CArchiveFolderFileItem &anItem = aFolder.m_FileSubItems[anIndexInVector];
  int aHandlerItemIndex = m_ProxyHandler->GetHandlerItemIndex(anItem.m_Properties);
  if (aHandlerItemIndex < 0)
    throw "error";
  aRealIndexes.push_back(aHandlerItemIndex);
}

void CPlugin::AddRealIndexes(const CArchiveFolderItem &anItem,
    vector<int> &aRealIndexes)
{
  int aHandlerItemIndex = m_ProxyHandler->GetHandlerItemIndex(anItem.m_Properties);
  if (aHandlerItemIndex >= 0) // test -1 value
     aRealIndexes.push_back(aHandlerItemIndex);
  for (int i = 0; i < anItem.m_DirSubItems.Size(); i++)
    AddRealIndexes(anItem.m_DirSubItems[i], aRealIndexes);
  for (i = 0; i < anItem.m_FileSubItems.Size(); i++)
    AddRealIndexOfFile(anItem, i , aRealIndexes);
}


void CPlugin::GetRealIndexes(PluginPanelItem *aPanelItems, int anItemsNumber,
    vector<int> &aRealIndexes)
{
  aRealIndexes.clear();
  for (int i = 0; i < anItemsNumber; i++)
  {
    int anIndex = aPanelItems[i].UserData;
    if (anIndex < m_FolderItem->m_DirSubItems.Size())
    {
      const CArchiveFolderItem &anItem = m_FolderItem->m_DirSubItems[anIndex];
      AddRealIndexes(anItem, aRealIndexes);
    }
    else
      AddRealIndexOfFile(*m_FolderItem, anIndex - m_FolderItem->m_DirSubItems.Size(),
          aRealIndexes);
  }
  sort(aRealIndexes.begin(), aRealIndexes.end());
}

*/
