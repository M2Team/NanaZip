// 7zCompressionMode.h

#ifndef __7Z_COMPRESSION_MODE_H
#define __7Z_COMPRESSION_MODE_H

#include "../../Common/MethodId.h"
#include "../../Common/MethodProps.h"

namespace NArchive {
namespace N7z {

struct CMethodFull: public CMethodProps
{
  CMethodId Id;
  UInt32 NumStreams;
  int CodecIndex;

  CMethodFull(): CodecIndex(-1) {}
  bool IsSimpleCoder() const { return NumStreams == 1; }
};

struct CBond2
{
  UInt32 OutCoder;
  UInt32 OutStream;
  UInt32 InCoder;
};

struct CCompressionMethodMode
{
  /*
    if (Bonds.Empty()), then default bonds must be created
    if (Filter_was_Inserted)
    {
      Methods[0] is filter method
      Bonds don't contain bonds for filter (these bonds must be created)
    }
  */

  CObjectVector<CMethodFull> Methods;
  CRecordVector<CBond2> Bonds;

  bool IsThereBond_to_Coder(unsigned coderIndex) const
  {
    FOR_VECTOR(i, Bonds)
      if (Bonds[i].InCoder == coderIndex)
        return true;
    return false;
  }

  bool DefaultMethod_was_Inserted;
  bool Filter_was_Inserted;

  #ifndef _7ZIP_ST
  UInt32 NumThreads;
  bool MultiThreadMixer;
  #endif
  
  bool PasswordIsDefined;
  UString Password; // _Wipe

  bool IsEmpty() const { return (Methods.IsEmpty() && !PasswordIsDefined); }
  CCompressionMethodMode():
        DefaultMethod_was_Inserted(false)
      , Filter_was_Inserted(false)
      #ifndef _7ZIP_ST
      , NumThreads(1)
      , MultiThreadMixer(true)
      #endif
      , PasswordIsDefined(false)
  {}

  ~CCompressionMethodMode() { Password.Wipe_and_Empty(); }
};

}}

#endif
