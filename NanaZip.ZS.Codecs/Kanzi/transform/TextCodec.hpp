/*
Copyright 2011-2026 Frederic Langlet
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
you may obtain a copy of the License at

                http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#pragma once
#ifndef knz_TextCodec
#define knz_TextCodec

#include "../Context.hpp"
#include "../Memory.hpp"
#include "../Transform.hpp"


namespace kanzi {

   class DictEntry FINAL {
   public:
       const byte* _ptr; // text data
       uint _hash; // full word hash
       int _data; // packed word length (8 MSB) + index in dictionary (24 LSB)
   
       DictEntry();

       DictEntry(const byte* ptr, int hash, int idx, int length);

#if __cplusplus < 201103L
       DictEntry(const DictEntry& de);

       DictEntry& operator=(const DictEntry& de);

       ~DictEntry() {}
#else
       DictEntry(const DictEntry& de) = delete;

       DictEntry& operator=(const DictEntry& de) = delete;

       DictEntry(DictEntry&& de) noexcept = default;

       DictEntry& operator=(DictEntry&& de) noexcept = default;

       ~DictEntry() noexcept = default;
#endif
   };

   // Encode word indexes using a token
   class TextCodec1 FINAL : public Transform<byte> {
   public:
       TextCodec1();

       TextCodec1(Context&);

       ~TextCodec1()
       {
           if (_dictList != nullptr) delete[] _dictList;
           if (_dictMap != nullptr) delete[] _dictMap;
       }

       bool forward(SliceArray<byte>& src, SliceArray<byte>& dst, int length);

       bool inverse(SliceArray<byte>& src, SliceArray<byte>& dst, int length);

       // Limit to 1 x srcLength and let the caller deal with
       // a failure when the output is too small
       int getMaxEncodedLength(int srcLen) const { return srcLen; }

   private:
       DictEntry** _dictMap;
       DictEntry* _dictList;
       byte _escapes[2];
       int _staticDictSize;
       int _dictSize;
       int _logHashSize;
       int _hashMask;
       bool _isCRLF; // EOL = CR + LF
       Context* _pCtx;

       bool expandDictionary();

       void reset(int count);

       static int emitWordIndex(byte dst[], int val);

       int emitSymbols(const byte src[], byte dst[], const int srcEnd, const int dstEnd) const;
   };

   // Encode word indexes using a mask (0x80)
   class TextCodec2 FINAL : public Transform<byte> {
   public:
       TextCodec2();

       TextCodec2(Context&);

       ~TextCodec2()
       {
           if (_dictList != nullptr) delete[] _dictList;
           if (_dictMap != nullptr) delete[] _dictMap;
       }

       bool forward(SliceArray<byte>& src, SliceArray<byte>& dst, int length);

       bool inverse(SliceArray<byte>& src, SliceArray<byte>& dst, int length);

       // Limit to 1 x srcLength and let the caller deal with
       // a failure when the output is too small
       int getMaxEncodedLength(int srcLen) const { return srcLen; }

   private:
       DictEntry** _dictMap;
       DictEntry* _dictList;
       int _staticDictSize;
       int _dictSize;
       int _logHashSize;
       int _hashMask;
       int _bsVersion;
       bool _isCRLF; // EOL = CR + LF
       Context* _pCtx;

       bool expandDictionary();

       void reset(int count);

       static int emitWordIndex(byte dst[], int val);

       int emitSymbols(const byte src[], byte dst[], const int srcEnd, const int dstEnd) const;
   };

   // Simple one-pass text codec that replaces words with indexes.
   // Generates a dynamic dictionary.
   class TextCodec FINAL : public Transform<byte> {
       friend class TextCodec1;
       friend class TextCodec2;

   public:
       static const int MAX_DICT_SIZE;
       static const int MAX_WORD_LENGTH;
       static const int MIN_BLOCK_SIZE;
       static const int MAX_BLOCK_SIZE;
       static const byte ESCAPE_TOKEN1;
       static const byte ESCAPE_TOKEN2;
       static const byte MASK_1F;
       static const byte MASK_3F;
       static const byte MASK_20;
       static const byte MASK_40;
       static const byte MASK_80;
       static const byte MASK_FLIP_CASE;

       TextCodec();

       TextCodec(Context& ctx);

       ~TextCodec() { delete _delegate; }

       bool forward(SliceArray<byte>& src, SliceArray<byte>& dst, int length);

       bool inverse(SliceArray<byte>& src, SliceArray<byte>& dst, int length);

       int getMaxEncodedLength(int srcLen) const
       {
           return _delegate->getMaxEncodedLength(srcLen);
       }

       static int8 getType(byte val) { return CHAR_TYPE[uint8(val)]; }

       static bool isText(byte val) { return getType(val) == 0; }

       static bool isLowerCase(byte val) { return (val >= byte('a')) && (val <= byte('z')); }

       static bool isUpperCase(byte val) { return (val >= byte('A')) && (val <= byte('Z')); }

       static bool isDelimiter(byte val) { return getType(val) > 0; }

   private:
       static const int HASH1;
       static const int HASH2;
       static const byte CR;
       static const byte LF;
       static const byte SP;
       static const int THRESHOLD1;
       static const int THRESHOLD2;
       static const int THRESHOLD3;
       static const int THRESHOLD4;
       static const int LOG_HASHES_SIZE;
       static const byte MASK_NOT_TEXT;
       static const byte MASK_CRLF;
       static const byte MASK_XML_HTML;
       static const byte MASK_TEXT_CODEC;
       static const byte MASK_DT;
       static const int MASK_LENGTH;
       static bool init(int8 cType[256]);
       static int8 CHAR_TYPE[256];
       static const bool INIT;

       static uint computeWordHash(const byte src[], int length);
       static bool sameWords(const byte src[], const byte dst[], int length);

       static byte computeStats(const byte block[], int count, uint freqs[], bool strict);

       static byte detectType(const uint freqs0[], const uint freqs1[], int count);
       
       // Common English words.
       static char DICT_EN_1024[];

       // Static dictionary of 1024 entries.
       static DictEntry STATIC_DICTIONARY[1024];
       static int createDictionary(char words[], int dictSize, DictEntry dict[], int maxWords, int startWord);
       static const int STATIC_DICT_WORDS;

       void setEncodingType(int encodingType);

       Transform<byte>* _delegate;
       Context* _pCtx;
       int _encodingType;
       int _bsVersion;
   };

   inline DictEntry::DictEntry()
       : _ptr(nullptr)
       , _hash(0)
       , _data(0)
   { 
   }

   inline DictEntry::DictEntry(const byte* ptr, int hash, int idx, int length = 0)
       : _ptr(ptr)
       , _hash(hash)
       , _data((length << 24) | idx)
   {
   }

#if __cplusplus < 201103L
   inline DictEntry::DictEntry(const DictEntry& de)
   {
       _ptr = de._ptr;
       _hash = de._hash;
       _data = de._data;
   }

   inline DictEntry& DictEntry::operator=(const DictEntry& de)
   {
       _ptr = de._ptr;
       _hash = de._hash;
       _data = de._data;
       return *this;
   }
#endif

   inline uint TextCodec::computeWordHash(const byte src[], int length)
   {
       uint h = HASH1;

       for (int i = 0; i < length; i++)
           h = h * HASH1 ^ uint(src[i]) * HASH2;

       return h;
   }

   inline bool TextCodec::sameWords(const byte src[], const byte dst[], int length)
   {
       while (length >= 4) {
           length -= 4;

           if (!KANZI_MEM_EQ4(&src[length], &dst[length]))
              return false;
       }

       while (length > 0) {
           length--;

           if (dst[length] != src[length])
              return false;
       }

       return true;
   }
}
#endif
