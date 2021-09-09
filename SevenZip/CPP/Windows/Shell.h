// Windows/Shell.h

#ifndef __WINDOWS_SHELL_H
#define __WINDOWS_SHELL_H

#include "../Common/MyWindows.h"
#include <ShlObj.h>

#include "../Common/MyString.h"

#include "Defs.h"

namespace NWindows{
namespace NShell{

/////////////////////////
// CItemIDList
#ifndef UNDER_CE

class CItemIDList
{
  LPITEMIDLIST m_Object;
public:
  CItemIDList(): m_Object(NULL) {}
  // CItemIDList(LPCITEMIDLIST itemIDList);
  // CItemIDList(const CItemIDList& itemIDList);
  ~CItemIDList() { Free(); }
  void Free();
  void Attach(LPITEMIDLIST object)
  {
    Free();
    m_Object = object;
  }
  LPITEMIDLIST Detach()
  {
    LPITEMIDLIST object = m_Object;
    m_Object = NULL;
    return object;
  }
  operator LPITEMIDLIST() { return m_Object;}
  operator LPCITEMIDLIST() const { return m_Object;}
  LPITEMIDLIST* operator&() { return &m_Object; }
  LPITEMIDLIST operator->() { return m_Object; }

  // CItemIDList& operator=(LPCITEMIDLIST object);
  // CItemIDList& operator=(const CItemIDList &object);
};

/////////////////////////////
// CDrop

class CDrop
{
  HDROP m_Object;
  bool m_MustBeFinished;
  bool m_Assigned;
  void Free();
public:
  CDrop(bool mustBeFinished) : m_MustBeFinished(mustBeFinished), m_Assigned(false) {}
  ~CDrop() { Free(); }

  void Attach(HDROP object);
  operator HDROP() { return m_Object;}
  bool QueryPoint(LPPOINT point)
    { return BOOLToBool(::DragQueryPoint(m_Object, point)); }
  void Finish() {  ::DragFinish(m_Object); }
  UINT QueryFile(UINT fileIndex, LPTSTR fileName, UINT fileNameSize)
    { return ::DragQueryFile(m_Object, fileIndex, fileName, fileNameSize); }
  #ifndef _UNICODE
  UINT QueryFile(UINT fileIndex, LPWSTR fileName, UINT fileNameSize)
    { return ::DragQueryFileW(m_Object, fileIndex, fileName, fileNameSize); }
  #endif
  UINT QueryCountOfFiles();
  UString QueryFileName(UINT fileIndex);
  void QueryFileNames(UStringVector &fileNames);
};

#endif

/////////////////////////////
// Functions

bool GetPathFromIDList(LPCITEMIDLIST itemIDList, CSysString &path);
bool BrowseForFolder(LPBROWSEINFO lpbi, CSysString &resultPath);
bool BrowseForFolder(HWND owner, LPCTSTR title, LPCTSTR initialFolder, CSysString &resultPath);

#ifndef _UNICODE
bool GetPathFromIDList(LPCITEMIDLIST itemIDList, UString &path);
bool BrowseForFolder(LPBROWSEINFO lpbi, UString &resultPath);
bool BrowseForFolder(HWND owner, LPCWSTR title, LPCWSTR initialFolder, UString &resultPath);
#endif
}}

#endif
