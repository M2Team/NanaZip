// UpdatePair.cpp

#include "StdAfx.h"

#include <time.h>

#include "../../../Common/Wildcard.h"

#include "../../../Windows/TimeUtils.h"

#include "SortUtils.h"
#include "UpdatePair.h"

using namespace NWindows;
using namespace NTime;

static int MyCompareTime(NFileTimeType::EEnum fileTimeType, const FILETIME &time1, const FILETIME &time2)
{
  switch (fileTimeType)
  {
    case NFileTimeType::kWindows:
      return ::CompareFileTime(&time1, &time2);
    case NFileTimeType::kUnix:
      {
        UInt32 unixTime1, unixTime2;
        FileTimeToUnixTime(time1, unixTime1);
        FileTimeToUnixTime(time2, unixTime2);
        return MyCompare(unixTime1, unixTime2);
      }
    case NFileTimeType::kDOS:
      {
        UInt32 dosTime1, dosTime2;
        FileTimeToDosTime(time1, dosTime1);
        FileTimeToDosTime(time2, dosTime2);
        return MyCompare(dosTime1, dosTime2);
      }
  }
  throw 4191618;
}

static const char * const k_Duplicate_inArc_Message = "Duplicate filename in archive:";
static const char * const k_Duplicate_inDir_Message = "Duplicate filename on disk:";
static const char * const k_NotCensoredCollision_Message = "Internal file name collision (file on disk, file in archive):";

MY_ATTR_NORETURN
static
void ThrowError(const char *message, const UString &s1, const UString &s2)
{
  UString m (message);
  m.Add_LF(); m += s1;
  m.Add_LF(); m += s2;
  throw m;
}

static int CompareArcItemsBase(const CArcItem &ai1, const CArcItem &ai2)
{
  int res = CompareFileNames(ai1.Name, ai2.Name);
  if (res != 0)
    return res;
  if (ai1.IsDir != ai2.IsDir)
    return ai1.IsDir ? -1 : 1;
  return 0;
}

static int CompareArcItems(const unsigned *p1, const unsigned *p2, void *param)
{
  unsigned i1 = *p1;
  unsigned i2 = *p2;
  const CObjectVector<CArcItem> &arcItems = *(const CObjectVector<CArcItem> *)param;
  int res = CompareArcItemsBase(arcItems[i1], arcItems[i2]);
  if (res != 0)
    return res;
  return MyCompare(i1, i2);
}

void GetUpdatePairInfoList(
    const CDirItems &dirItems,
    const CObjectVector<CArcItem> &arcItems,
    NFileTimeType::EEnum fileTimeType,
    CRecordVector<CUpdatePair> &updatePairs)
{
  CUIntVector dirIndices, arcIndices;
  
  unsigned numDirItems = dirItems.Items.Size();
  unsigned numArcItems = arcItems.Size();
  
  CIntArr duplicatedArcItem(numArcItems);
  {
    int *vals = &duplicatedArcItem[0];
    for (unsigned i = 0; i < numArcItems; i++)
      vals[i] = 0;
  }

  {
    arcIndices.ClearAndSetSize(numArcItems);
    if (numArcItems != 0)
    {
      unsigned *vals = &arcIndices[0];
      for (unsigned i = 0; i < numArcItems; i++)
        vals[i] = i;
    }
    arcIndices.Sort(CompareArcItems, (void *)&arcItems);
    for (unsigned i = 0; i + 1 < numArcItems; i++)
      if (CompareArcItemsBase(
          arcItems[arcIndices[i]],
          arcItems[arcIndices[i + 1]]) == 0)
      {
        duplicatedArcItem[i] = 1;
        duplicatedArcItem[i + 1] = -1;
      }
  }

  UStringVector dirNames;
  {
    dirNames.ClearAndReserve(numDirItems);
    unsigned i;
    for (i = 0; i < numDirItems; i++)
      dirNames.AddInReserved(dirItems.GetLogPath(i));
    SortFileNames(dirNames, dirIndices);
    for (i = 0; i + 1 < numDirItems; i++)
    {
      const UString &s1 = dirNames[dirIndices[i]];
      const UString &s2 = dirNames[dirIndices[i + 1]];
      if (CompareFileNames(s1, s2) == 0)
        ThrowError(k_Duplicate_inDir_Message, s1, s2);
    }
  }
  
  unsigned dirIndex = 0;
  unsigned arcIndex = 0;

  int prevHostFile = -1;
  const UString *prevHostName = NULL;
  
  while (dirIndex < numDirItems || arcIndex < numArcItems)
  {
    CUpdatePair pair;
    
    int dirIndex2 = -1;
    int arcIndex2 = -1;
    const CDirItem *di = NULL;
    const CArcItem *ai = NULL;
    
    int compareResult = -1;
    const UString *name = NULL;
    
    if (dirIndex < numDirItems)
    {
      dirIndex2 = (int)dirIndices[dirIndex];
      di = &dirItems.Items[(unsigned)dirIndex2];
    }

    if (arcIndex < numArcItems)
    {
      arcIndex2 = (int)arcIndices[arcIndex];
      ai = &arcItems[(unsigned)arcIndex2];
      compareResult = 1;
      if (dirIndex < numDirItems)
      {
        compareResult = CompareFileNames(dirNames[(unsigned)dirIndex2], ai->Name);
        if (compareResult == 0)
        {
          if (di->IsDir() != ai->IsDir)
            compareResult = (ai->IsDir ? 1 : -1);
        }
      }
    }
    
    if (compareResult < 0)
    {
      name = &dirNames[(unsigned)dirIndex2];
      pair.State = NUpdateArchive::NPairState::kOnlyOnDisk;
      pair.DirIndex = dirIndex2;
      dirIndex++;
    }
    else if (compareResult > 0)
    {
      name = &ai->Name;
      pair.State = ai->Censored ?
          NUpdateArchive::NPairState::kOnlyInArchive:
          NUpdateArchive::NPairState::kNotMasked;
      pair.ArcIndex = arcIndex2;
      arcIndex++;
    }
    else
    {
      int dupl = duplicatedArcItem[arcIndex];
      if (dupl != 0)
        ThrowError(k_Duplicate_inArc_Message, ai->Name, arcItems[arcIndices[(unsigned)((int)arcIndex + dupl)]].Name);

      name = &dirNames[(unsigned)dirIndex2];
      if (!ai->Censored)
        ThrowError(k_NotCensoredCollision_Message, *name, ai->Name);
      
      pair.DirIndex = dirIndex2;
      pair.ArcIndex = arcIndex2;

      switch (ai->MTimeDefined ? MyCompareTime(
          ai->TimeType != - 1 ? (NFileTimeType::EEnum)ai->TimeType : fileTimeType,
          di->MTime, ai->MTime): 0)
      {
        case -1: pair.State = NUpdateArchive::NPairState::kNewInArchive; break;
        case  1: pair.State = NUpdateArchive::NPairState::kOldInArchive; break;
        default:
          pair.State = (ai->SizeDefined && di->Size == ai->Size) ?
              NUpdateArchive::NPairState::kSameFiles :
              NUpdateArchive::NPairState::kUnknowNewerFiles;
      }
      
      dirIndex++;
      arcIndex++;
    }
    
    if ((di && di->IsAltStream) ||
        (ai && ai->IsAltStream))
    {
      if (prevHostName)
      {
        unsigned hostLen = prevHostName->Len();
        if (name->Len() > hostLen)
          if ((*name)[hostLen] == ':' && CompareFileNames(*prevHostName, name->Left(hostLen)) == 0)
            pair.HostIndex = prevHostFile;
      }
    }
    else
    {
      prevHostFile = (int)updatePairs.Size();
      prevHostName = name;
    }
    
    updatePairs.Add(pair);
  }

  updatePairs.ReserveDown();
}
