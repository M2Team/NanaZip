/*
 * PROJECT:   NanaZip
 * FILE:      Sha256Wrapper.h
 * PURPOSE:   Definition for SHA-256 wrapper for 7-Zip
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef ZIP7_INC_SHA256_H
#define ZIP7_INC_SHA256_H

#include "../SevenZip/C/7zTypes.h"

#include <K7Pal.h>

EXTERN_C_BEGIN

#define SHA256_NUM_BLOCK_WORDS  16
#define SHA256_NUM_DIGEST_WORDS  8

#define SHA256_BLOCK_SIZE   (SHA256_NUM_BLOCK_WORDS * 4)
#define SHA256_DIGEST_SIZE  (SHA256_NUM_DIGEST_WORDS * 4)

typedef struct CSha256
{
    K7_PAL_HASH_HANDLE HashHandle;

#ifdef __cplusplus
    CSha256& operator=(
        CSha256 const& Source);
#endif // __cplusplus
} CSha256;

#define SHA256_ALGO_DEFAULT 0
#define SHA256_ALGO_SW 1
#define SHA256_ALGO_HW 2

/*
Sha256_SetFunction()
return:
  0 - (algo) value is not supported, and func_UpdateBlocks was not changed
  1 - func_UpdateBlocks was set according (algo) value.
*/
BoolInt Sha256_SetFunction(
    CSha256* p,
    unsigned algo);

void Sha256_InitState(
    CSha256* p);

void Sha256_Init(
    CSha256* p);

void Sha256_Update(
    CSha256* p,
    const Byte* data,
    size_t size);

void Sha256_Final(
    CSha256* p,
    Byte* digest);

EXTERN_C_END

#endif
