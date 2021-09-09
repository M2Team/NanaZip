// MyLoadMenu.h

#ifndef __MY_LOAD_MENU_H
#define __MY_LOAD_MENU_H

void OnMenuActivating(HWND hWnd, HMENU hMenu, int position);
// void OnMenuUnActivating(HWND hWnd, HMENU hMenu, int id);
// void OnMenuUnActivating(HWND hWnd);

bool OnMenuCommand(HWND hWnd, unsigned id);
void MyLoadMenu();

struct CFileMenu
{
  bool programMenu;
  bool readOnly;
  bool isFsFolder;
  bool allAreFiles;
  bool isAltStreamsSupported;
  int numItems;
  
  UString FilePath;

  CFileMenu():
      programMenu(false),
      readOnly(false),
      isFsFolder(false),
      allAreFiles(false),
      isAltStreamsSupported(true),
      numItems(0)
    {}

  void Load(HMENU hMenu, unsigned startPos);
};

bool ExecuteFileCommand(unsigned id);

#endif
