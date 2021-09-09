// RegistryUtils.h

#ifndef __REGISTRY_UTILS_H
#define __REGISTRY_UTILS_H

#include "../../../Common/MyTypes.h"
#include "../../../Common/MyString.h"

void SaveRegLang(const UString &path);
void ReadRegLang(UString &path);

void SaveRegEditor(bool useEditor, const UString &path);
void ReadRegEditor(bool useEditor, UString &path);

void SaveRegDiff(const UString &path);
void ReadRegDiff(UString &path);

void ReadReg_VerCtrlPath(UString &path);

struct CFmSettings
{
  bool ShowDots;
  bool ShowRealFileIcons;
  bool FullRow;
  bool ShowGrid;
  bool SingleClick;
  bool AlternativeSelection;
  // bool Underline;

  bool ShowSystemMenu;

  void Save() const;
  void Load();
};

// void SaveLockMemoryAdd(bool enable);
// bool ReadLockMemoryAdd();

bool ReadLockMemoryEnable();
void SaveLockMemoryEnable(bool enable);

void SaveFlatView(UInt32 panelIndex, bool enable);
bool ReadFlatView(UInt32 panelIndex);

/*
void Save_ShowDeleted(bool enable);
bool Read_ShowDeleted();
*/

#endif
