// RegistryUtils.h

#ifndef ZIP7_INC_REGISTRY_UTILS_H
#define ZIP7_INC_REGISTRY_UTILS_H

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
  // **************** 7-Zip ZS Modification Start ****************
  bool ArcHistory;
  bool PathHistory;
  bool CopyHistory;
  bool FolderHistory;
  bool LowercaseHashes;
  // **************** 7-Zip ZS Modification End ****************
  // bool Underline;

  bool ShowSystemMenu;

  void Save() const;
  void Load();
};

// void SaveLockMemoryAdd(bool enable);
// bool ReadLockMemoryAdd();

bool ReadLockMemoryEnable();
void SaveLockMemoryEnable(bool enable);

// **************** 7-Zip ZS Modification Start ****************
bool WantArcHistory();
bool WantPathHistory();
bool WantCopyHistory();
bool WantFolderHistory();
bool WantLowercaseHashes();

bool WantArcHistory();
bool WantPathHistory();
bool WantCopyHistory();
bool WantFolderHistory();
// **************** 7-Zip ZS Modification End ****************

void SaveFlatView(UInt32 panelIndex, bool enable);
bool ReadFlatView(UInt32 panelIndex);

/*
void Save_ShowDeleted(bool enable);
bool Read_ShowDeleted();
*/

#endif
