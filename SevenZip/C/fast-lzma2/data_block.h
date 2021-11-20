#include "mem.h"

#ifndef FL2_DATA_BLOCK_H_
#define FL2_DATA_BLOCK_H_

#if defined (__cplusplus)
extern "C" {
#endif

typedef struct {
    const BYTE* data;
    size_t start;
    size_t end;
} FL2_dataBlock;

#if defined (__cplusplus)
}
#endif

#endif /* FL2_DATA_BLOCK_H_ */