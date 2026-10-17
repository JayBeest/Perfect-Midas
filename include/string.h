#if !defined(TARGET_N64) && defined(__cplusplus)
/* PC port: this header shadows the toolchain's <string.h>; C++ TUs (fast3d)
 * defer to the real one, which libstdc++ needs. C TUs keep the minimal stub,
 * because the hosted header's bcopy/bzero/bcmp conflict with PR/os.h. */
#include_next <string.h>
#else

#ifndef _STRING_H_
#define _STRING_H_
#include <PR/ultratypes.h>

extern void *memcpy(void *, const void *, size_t);
extern unsigned char *strchr(const unsigned char *, int);
extern size_t strlen(const unsigned char *);

#endif
#endif /* TARGET_N64 */
