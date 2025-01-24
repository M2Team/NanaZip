/*
 * PROJECT:   NanaZip
 * FILE:      Sha1Wrapper.h
 * PURPOSE:   Definition for SHA-1 wrapper for 7-Zip
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef ZIP7_INC_SHA1_H
#define ZIP7_INC_SHA1_H

#include "../SevenZip/C/7zTypes.h"

#include <K7Pal.h>

EXTERN_C_BEGIN

#define SHA1_NUM_BLOCK_WORDS  16
#define SHA1_NUM_DIGEST_WORDS  5

#define SHA1_BLOCK_SIZE   (SHA1_NUM_BLOCK_WORDS * 4)
#define SHA1_DIGEST_SIZE  (SHA1_NUM_DIGEST_WORDS * 4)

typedef struct CSha1
{
    K7_PAL_HASH_HANDLE HashHandle;

#ifdef __cplusplus
    CSha1& operator=(
        CSha1 const& Source);
#endif // __cplusplus
} CSha1;

#define SHA1_ALGO_DEFAULT 0
#define SHA1_ALGO_SW      1
#define SHA1_ALGO_HW      2

/*
Sha1_SetFunction()
return:
  0 - (algo) value is not supported, and func_UpdateBlocks was not changed
  1 - func_UpdateBlocks was set according (algo) value.
*/
BoolInt Sha1_SetFunction(
    CSha1* p,
    unsigned algo);

void Sha1_InitState(
    CSha1* p);

void Sha1_Init(
    CSha1* p);

void Sha1_Update(
    CSha1* p,
    const Byte* data,
    size_t size);

void Sha1_Final(
    CSha1* p,
    Byte* digest);

void Sha1_PrepareBlock(
    const CSha1* p,
    Byte* block,
    unsigned size);

void Sha1_GetBlockDigest(
    const CSha1* p,
    const Byte* data,
    Byte* destDigest);

EXTERN_C_END

#endif
