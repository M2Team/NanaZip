/*
 * PROJECT:   NanaZip
 * FILE:      Sha512Wrapper.h
 * PURPOSE:   Definition for SHA-384/SHA-512 wrapper for 7-Zip
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef ZIP7_INC_SHA512_H
#define ZIP7_INC_SHA512_H

#include "../SevenZip/C/7zTypes.h"

#include <K7Pal.h>

EXTERN_C_BEGIN

#define SHA512_NUM_DIGEST_WORDS  8

#define SHA512_DIGEST_SIZE  (SHA512_NUM_DIGEST_WORDS * 8)
#define SHA512_384_DIGEST_SIZE  (384 / 8)

typedef struct CSha512
{
    K7_PAL_HASH_HANDLE HashHandle;

#ifdef __cplusplus
    CSha512& operator=(
        CSha512 const& Source);
#endif // __cplusplus
} CSha512;

#define SHA512_ALGO_DEFAULT 0
#define SHA512_ALGO_SW 1
#define SHA512_ALGO_HW 2

/*
Sha512_SetFunction()
return:
  0 - (algo) value is not supported, and func_UpdateBlocks was not changed
  1 - func_UpdateBlocks was set according (algo) value.
*/
BoolInt Sha512_SetFunction(
    CSha512* p,
    unsigned algo);

// we support only these (digestSize) values: 384/8, 512/8

void Sha512_InitState(
    CSha512* p,
    unsigned digestSize);

void Sha512_Init(
    CSha512* p,
    unsigned digestSize);

void Sha512_Update(
    CSha512* p,
    const Byte* data,
    size_t size);

void Sha512_Final(
    CSha512* p,
    Byte* digest,
    unsigned digestSize);

EXTERN_C_END

#endif
