// 7zCompressionMode.h

#ifndef ZIP7_INC_7Z_COMPRESSION_MODE_H
#define ZIP7_INC_7Z_COMPRESSION_MODE_H

#include "../../Common/MethodId.h"
#include "../../Common/MethodProps.h"

namespace NArchive {
namespace N7z {

struct CMethodFull: public CMethodProps
{
  CMethodId Id;
  UInt32 NumStreams;
  int CodecIndex;
  UInt32 NumThreads;
  bool Set_NumThreads;

  CMethodFull(): CodecIndex(-1), NumThreads(1), Set_NumThreads(false) {}
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

  #ifndef Z7_ST
  UInt32 NumThreads;
  bool NumThreads_WasForced;
  bool MultiThreadMixer;
  #endif

  UInt64 MemoryUsageLimit;
  bool MemoryUsageLimit_WasSet;
  
  bool PasswordIsDefined;
  UString Password; // _Wipe

  bool IsEmpty() const { return (Methods.IsEmpty() && !PasswordIsDefined); }
  CCompressionMethodMode():
        DefaultMethod_was_Inserted(false)
      , Filter_was_Inserted(false)
      #ifndef Z7_ST
      , NumThreads(1)
      , NumThreads_WasForced(false)
      , MultiThreadMixer(true)
      #endif
      , MemoryUsageLimit((UInt64)1 << 30)
      , MemoryUsageLimit_WasSet(false)
      , PasswordIsDefined(false)
  {}

#ifdef Z7_CPP_IS_SUPPORTED_default
  CCompressionMethodMode(const CCompressionMethodMode &) = default;
  CCompressionMethodMode& operator =(const CCompressionMethodMode &) = default;
#endif
  ~CCompressionMethodMode() { Password.Wipe_and_Empty(); }
};

}}

#endif
