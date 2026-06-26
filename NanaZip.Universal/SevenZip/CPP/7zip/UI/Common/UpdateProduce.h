// UpdateProduce.h

#ifndef ZIP7_INC_UPDATE_PRODUCE_H
#define ZIP7_INC_UPDATE_PRODUCE_H

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
  bool IsSameTime;

  void Construct()
  {
    NewData = false;
    NewProps = false;
    UseArcProps = false;
    IsAnti = false;
    DirIndex = -1;
    ArcIndex = -1;
    NewNameIndex = -1;
    IsMainRenameItem = false;
    IsSameTime = false;
  }

  void SetAs_NoChangeArcItem(unsigned arcIndex) // int
  {
    Construct();
    UseArcProps = true;
    ArcIndex = (int)arcIndex;
  }

  bool ExistOnDisk() const { return DirIndex != -1; }
  bool ExistInArchive() const { return ArcIndex != -1; }
};

Z7_PURE_INTERFACES_BEGIN

DECLARE_INTERFACE(IUpdateProduceCallback)
{
  virtual HRESULT ShowDeleteFile(unsigned arcIndex) = 0;
};
Z7_PURE_INTERFACES_END

void UpdateProduce(
    const CRecordVector<CUpdatePair> &updatePairs,
    const NUpdateArchive::CActionSet &actionSet,
    CRecordVector<CUpdatePair2> &operationChain,
    IUpdateProduceCallback *callback);

#endif
