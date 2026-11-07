// MultiStream.h

#ifndef ZIP7_INC_MULTI_STREAM_H
#define ZIP7_INC_MULTI_STREAM_H

#include "../../../Common/MyString.h"

struct CSplitVolName
{
  UString Changed;
  UString Name; // Name[0 ... (ChangedPos -1)] - is unchanged part
                // Name[ChangedPos ...]        - is changed part
  unsigned ChangedPos;
#ifndef Z7_SFX
  int SpecPos; //   0 : both cases need to be checked : sequential_mode and skip_mode
               //  -1 : use sequential_mode only : 7zip style
               // > 0 : use skip_mode only : linux_split style
  char PostfixChar; // '0' : 'a' : 'A'
  bool IsComplicated;

  bool IsLatinPostfix() const { return PostfixChar != '0'; }
  void AddPostfix_2Chars()
  {
    // for skip_mode: we add 2 additional postfix characters : linux_split style
    Changed.Add_Char(PostfixChar);
    Changed.Add_Char(PostfixChar);
    SpecPos++;
  }
#endif

  CSplitVolName():
      ChangedPos(0)
#ifndef Z7_SFX
      , SpecPos(0)
#endif
    {}

  bool ParseName()
  {
    if (Name.Len() < 2)
      return false;
    unsigned pos = Name.Len() - 2;
#ifndef Z7_SFX
    if (StringsAreEqualNoCase_Ascii(Name.Ptr(pos), "aa"))
    {
      PostfixChar = (char)Name.Back();
      while (pos)
      {
        if (MyCharLower_Ascii(Name[pos - 1]) != 'a')
          break;
        pos--;
      }
    }
    else
#endif
    {
#ifndef Z7_SFX
      PostfixChar = '0';
#endif
      pos++;
      const wchar_t c = Name[pos];
      if (c != '0' && c != '1')
        return false;
      while (pos)
      {
        if (Name[pos - 1] != '0')
          break;
        pos--;
      }
      if (Name.Len() - pos < 2
          || pos == 0
          || Name[pos - 1] != '.')
        return false;
    }
    ChangedPos = pos;
    Changed = Name.Ptr(pos);
    return true;
  }

  void Generate_SubName(UString &s) const
  {
    unsigned len = ChangedPos;
    if (len && Name[len - 1] == '.')
      len--;
    if (len == 0)
      s = "file";
    else
      s.SetFrom(Name, len);
  }

  bool GetNextName()
  {
#ifndef Z7_SFX
    IsComplicated = false;
#endif
    unsigned i = Changed.Len();
    for (;;)
    {
      wchar_t c = Changed[--i];
#ifndef Z7_SFX
      if (IsLatinPostfix())
      {
        if ((c | 0x20) != 'z')
        {
          c++;
          Changed.ReplaceOneCharAtPos(i, c);
          if ((c | 0x20) != 'z')
            return true;
          break;
        }
        c -= 'z' - 'a';
        Changed.ReplaceOneCharAtPos(i, c);
        if (i == 0)
        {
#if 0  // 1 : for debug
          /* (return_code == false) will stop volume processing in caller.
             But if (return_code) is not checked by caller, we must
             increase length of (Changed) string to prevent infinite loop. */
          SpecPos = -1;
          Changed.InsertAtFront(Changed[0]);
#endif
          return false; // stop volume processing
        }
      }
      else
#endif
      {
        if (c != '9')
        {
          c++;
          Changed.ReplaceOneCharAtPos(i, c);
          if (c != '9')
            return true;
          break;
        }
        Changed.ReplaceOneCharAtPos(i, L'0');
        if (i == 0)
        {
#ifndef Z7_SFX
          SpecPos = -1;
#endif
          Changed.InsertAtFront(L'1');
          return true;
        }
      }
    }

#ifndef Z7_SFX
    if ((int)i != SpecPos)
      return true;
    if (i == 0)
      IsComplicated = true;
    else
      AddPostfix_2Chars();
#endif
    return true;
  }

  void GenerateName()
  {
    Name.DeleteFrom(ChangedPos);
    Name += Changed;
  }
};


/*
Z7_CLASS_IMP_COM_1(
  COutMultiStream,
  IOutStream
)
  Z7_IFACE_COM7_IMP(ISequentialOutStream)

  unsigned _streamIndex; // required stream
  UInt64 _offsetPos; // offset from start of _streamIndex index
  UInt64 _absPos;
  UInt64 _length;

  struct CSubStreamInfo
  {
    CMyComPtr<ISequentialOutStream> Stream;
    UInt64 Size;
    UInt64 Pos;
 };
  CObjectVector<CSubStreamInfo> Streams;
public:
  CMyComPtr<IArchiveUpdateCallback2> VolumeCallback;
  void Init()
  {
    _streamIndex = 0;
    _offsetPos = 0;
    _absPos = 0;
    _length = 0;
  }
};
*/

#endif
