// UpdateProduce.h

#ifndef __UPDATE_PRODUCE_H
#define __UPDATE_PRODUCE_H

#include "UpdatePair.h"

struct CUpdatePair2
{
  bool NewData;
  bool NewProps;
  bool UseArcProps; // if (UseArcProps && NewProps), we want to change only some properties.
  bool IsAnti; // if (!IsAnti) we use other ways to detect Anti status
  
  int DirIndex;
  int ArcIndex;
  int NewNameIndex;

  bool IsMainRenameItem;

  void SetAs_NoChangeArcItem(unsigned arcIndex) // int
  {
    NewData = NewProps = false;
    UseArcProps = true;
    IsAnti = false;
    ArcIndex = (int)arcIndex;
  }

  bool ExistOnDisk() const { return DirIndex != -1; }
  bool ExistInArchive() const { return ArcIndex != -1; }

  CUpdatePair2():
      NewData(false),
      NewProps(false),
      UseArcProps(false),
      IsAnti(false),
      DirIndex(-1),
      ArcIndex(-1),
      NewNameIndex(-1),
      IsMainRenameItem(false)
      {}
};

struct IUpdateProduceCallback
{
  virtual HRESULT ShowDeleteFile(unsigned arcIndex) = 0;
};

void UpdateProduce(
    const CRecordVector<CUpdatePair> &updatePairs,
    const NUpdateArchive::CActionSet &actionSet,
    CRecordVector<CUpdatePair2> &operationChain,
    IUpdateProduceCallback *callback);

#endif
