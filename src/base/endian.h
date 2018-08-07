#pragma once

void memrev16(void *p);
void memrev32(void *p);
void memrev64(void *p);

/* reverse memory byte by byte for big endian data */
#if (BYTE_ORDER == LITTLE_ENDIAN)
#define memrev16ifbe(p)
#define memrev32ifbe(p)
#define memrev64ifbe(p)
#else
#define memrev16ifbe(p) memrev16(p)
#define memrev32ifbe(p) memrev32(p)
#define memrev64ifbe(p) memrev64(p)
#endif

/* EOF */