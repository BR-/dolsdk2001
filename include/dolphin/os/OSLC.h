#ifndef _DOLPHIN_OSLC_H_
#define _DOLPHIN_OSLC_H_

void LCAllocOneTag(BOOL invalidate, void *tag);
void LCAllocTags(BOOL invalidate, void *startTag, u32 numBlocks);
void LCAlloc(void *addr, u32 nBytes);
void LCAllocNoInvalidate(void *addr, u32 nBytes);

#endif
