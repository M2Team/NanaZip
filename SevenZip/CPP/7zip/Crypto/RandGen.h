// RandGen.h

#ifndef __CRYPTO_RAND_GEN_H
#define __CRYPTO_RAND_GEN_H

#include "../../../C/Sha256.h"

#ifdef _WIN64
// #define USE_STATIC_SYSTEM_RAND
#endif

#ifdef USE_STATIC_SYSTEM_RAND

#ifdef _WIN32
#include <ntsecapi.h>
#define MY_RAND_GEN(data, size) RtlGenRandom(data, size)
#else
#define MY_RAND_GEN(data, size) getrandom(data, size, 0)
#endif

#else

class CRandomGenerator
{
  Byte _buff[SHA256_DIGEST_SIZE];
  bool _needInit;

  void Init();
public:
  CRandomGenerator(): _needInit(true) {};
  void Generate(Byte *data, unsigned size);
};

extern CRandomGenerator g_RandomGenerator;

#define MY_RAND_GEN(data, size) g_RandomGenerator.Generate(data, size)

#endif

#endif
