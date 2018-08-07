#include "intset.h"

#include <stdio.h> 
#include <string.h> 

#include "../endian.h"

uint8_t
intset_get(intset_t *is, uint32_t pos, int64_t *v)
{
    int64_t v64;
    int32_t v32;
    int16_t v16;

    if(pos >= is->length) return 0;

    if (is->encoding == sizeof(int64_t)) {
        v64 = 0;
        memcpy(&v64, (int64_t*)is->contents + pos, sizeof(int64_t));
        memrev64ifbe(&v64);
        *v = v64;
    } else if (is->encoding == sizeof(int32_t)) {
        v32 = 0;
        memcpy(&v32, (int32_t*)is->contents + pos, sizeof(int32_t));
        memrev32ifbe(&v32);
        *v = v32;
    } else {
        v16 = 0;
        memcpy(&v16, (int16_t*)is->contents + pos, sizeof(int16_t));
        memrev16ifbe(&v16);
        *v = v16;
    }
    return 1;
}


void
intset_dump(intset_t *is)
{
    printf("encoding: %d\n", is->encoding);
    printf("length: %d\n", is->length);

    uint32_t i;
    int64_t v;
    printf("element { ");
    for (i = 0; i < is->length; i++) {
        intset_get(is, i, &v);
        printf("%lld\t", v);
    }
    printf("}\n");
}

