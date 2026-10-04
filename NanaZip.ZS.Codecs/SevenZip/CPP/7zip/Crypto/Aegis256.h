// Aegis256.h
// Copyright (C) fzxx   Contributor: https://github.com/fzxx
// License: GNU LGPL v2.1+
//
// AEGIS-256 authenticated encryption (RFC 10032), without any 7z specific code.

#ifndef ZIP7_INC_CRYPTO_AEGIS256_H
#define ZIP7_INC_CRYPTO_AEGIS256_H

#include "../../Common/MyCom.h"

namespace NCrypto {
namespace NAegis256 {

const unsigned kKeySize   = 32;
const unsigned kNonceSize = 32;
const unsigned kTagSize   = 32;
const unsigned kBlockSize = 16;
const unsigned kNumStateBlocks = 6;
const unsigned kStateSize = kNumStateBlocks * kBlockSize;

// the state update function: AES-NI / NEON-AES / portable software
typedef void (Z7_FASTCALL *AEGIS256_UPDATE_FUNC)(Byte *state, const Byte *m);
// encrypt or decrypt a sequence of full blocks
typedef void (Z7_FASTCALL *AEGIS256_BLOCKS_FUNC)(Byte *state, Byte *data, size_t numBlocks);

// these pointers are set to the fastest available implementation
extern AEGIS256_UPDATE_FUNC g_Aegis256_Update;
extern AEGIS256_BLOCKS_FUNC g_Aegis256_EncBlocks;
extern AEGIS256_BLOCKS_FUNC g_Aegis256_DecBlocks;

void Aegis256_InitFuncs(void);

class CCipher
{
  Byte _state[kStateSize];
  UInt64 _adLen;
  UInt64 _msgLen;

  void Absorb(const Byte *block) { g_Aegis256_Update(_state, block); }
  void EncBlock(Byte *dest, const Byte *src);
public:
  CCipher();
  ~CCipher() { Clear(); }

  void Clear();
  void Init(const Byte *key, const Byte *nonce);

  // ad can be absorbed only before the message data
  void AbsorbAad(const Byte *data, size_t size);

  // size of the last call can be not a multiple of the block size
  void EncryptData(Byte *data, size_t size);
  void DecryptData(Byte *data, size_t size);

  void FinalTag(Byte *tag);

#ifdef Z7_CPP_IS_SUPPORTED_default
  CCipher(const CCipher &) = delete;
  CCipher &operator=(const CCipher &) = delete;
#endif
};

}}

#endif
