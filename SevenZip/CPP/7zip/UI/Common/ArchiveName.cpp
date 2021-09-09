// ArchiveName.cpp

#include "StdAfx.h"

#include "../../../Common/Wildcard.h"

#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileName.h"

#include "ExtractingFilePath.h"
#include "ArchiveName.h"

using namespace NWindows;
using namespace NFile;

static UString CreateArchiveName(const NFind::CFileInfo &fi, bool keepName)
{
  FString resultName = fi.Name;
  if (!fi.IsDir() && !keepName)
  {
    int dotPos = resultName.ReverseFind_Dot();
    if (dotPos > 0)
    {
      FString archiveName2 = resultName.Left((unsigned)dotPos);
      if (archiveName2.ReverseFind_Dot() < 0)
        resultName = archiveName2;
    }
  }
  return Get_Correct_FsFile_Name(fs2us(resultName));
}

static FString CreateArchiveName2(const FString &path, bool fromPrev, bool keepName)
{
  FString resultName ("Archive");
  if (fromPrev)
  {
    FString dirPrefix;
    if (NDir::GetOnlyDirPrefix(path, dirPrefix))
    {
      if (!dirPrefix.IsEmpty() && IsPathSepar(dirPrefix.Back()))
      {
        #if defined(_WIN32) && !defined(UNDER_CE)
        if (NName::IsDriveRootPath_SuperAllowed(dirPrefix))
          resultName = dirPrefix[dirPrefix.Len() - 3]; // only letter
        else
        #endif
        {
          dirPrefix.DeleteBack();
          NFind::CFileInfo fi;
          if (fi.Find(dirPrefix))
            resultName = fi.Name;
        }
      }
    }
  }
  else
  {
    NFind::CFileInfo fi;
    if (fi.Find(path))
    {
      resultName = fi.Name;
      if (!fi.IsDir() && !keepName)
      {
        int dotPos = resultName.ReverseFind_Dot();
        if (dotPos > 0)
        {
          FString name2 = resultName.Left((unsigned)dotPos);
          if (name2.ReverseFind_Dot() < 0)
            resultName = name2;
        }
      }
    }
  }
  return resultName;
}


UString CreateArchiveName(const UStringVector &paths, const NFind::CFileInfo *fi)
{
  bool keepName = false;
  /*
  if (paths.Size() == 1)
  {
    const UString &name = paths[0];
    if (name.Len() > 4)
      if (CompareFileNames(name.RightPtr(4), L".tar") == 0)
        keepName = true;
  }
  */

  UString name;
  if (fi)
    name = CreateArchiveName(*fi, keepName);
  else
  {
    if (paths.IsEmpty())
      return L"archive";
    bool fromPrev = (paths.Size() > 1);
    name = Get_Correct_FsFile_Name(fs2us(CreateArchiveName2(us2fs(paths.Front()), fromPrev, keepName)));
  }

  UStringVector names;

  {
    FOR_VECTOR (i, paths)
    {
      NFind::CFileInfo fi2;
      const NFind::CFileInfo *fp;
      if (fi && paths.Size() == 1)
        fp = fi;
      else
      {
        if (!fi2.Find(us2fs(paths[i])))
          continue;
        fp = &fi2;
      }
      names.Add(fs2us(fp->Name));
    }
  }

  UString postfix;
  UInt32 index = 1;

  for (;;)
  {
    // we don't want cases when we include archive to itself.
    // so we find first available name for archive
    const UString name2 = name + postfix;
    const UString name2_zip = name2 + L".zip";
    const UString name2_7z = name2 + L".7z";
    const UString name2_tar = name2 + L".tar";
    const UString name2_wim = name2 + L".wim";
    
    unsigned i = 0;

    for (i = 0; i < names.Size(); i++)
    {
      const UString &fname = names[i];
      if (   0 == CompareFileNames(fname, name2_zip)
          || 0 == CompareFileNames(fname, name2_7z)
          || 0 == CompareFileNames(fname, name2_tar)
          || 0 == CompareFileNames(fname, name2_wim))
        break;
    }
    
    if (i == names.Size())
      break;
    index++;
    postfix = "_";
    postfix.Add_UInt32(index);
  }
  
  name += postfix;
  return name;
}
