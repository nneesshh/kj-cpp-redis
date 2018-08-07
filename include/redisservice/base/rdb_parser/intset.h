#pragma once

#include <stdint.h>

typedef struct intset_s intset_t;

struct intset_s {
    uint32_t encoding; /* int16, int32 or int64 */
    uint32_t length;
    uint8_t contents[0];
};

uint8_t intset_get(intset_t *is, uint32_t pos, int64_t *v);

void intset_dump(intset_t *is);

/* EOF */