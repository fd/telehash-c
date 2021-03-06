#ifndef util_h
#define util_h

#include <stdint.h>
#include <stdlib.h>

// make sure out is 2*len + 1
char *util_hex(uint8_t *in, uint32_t len, char *out);
// out must be len/2
uint8_t *util_unhex(char *in, uint32_t len, uint8_t *out);
// hex string validator, NULL is invalid, else returns str
char *util_ishex(char *str, uint32_t len);

// safer string comparison (0 == same)
int util_cmp(char *a, char *b);

// portable sort
void util_sort(void *base, int nel, int width, int (*comp)(void *, const void *, const void *), void *arg);

// portable reallocf
void *util_reallocf(void *ptr, size_t size);

#endif