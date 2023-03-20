#pragma once

typedef struct _BINARY_DATA
{
    BYTE *binary;
    size_t size;
    size_t index;
    BOOL invalid;
} BINARY_DATA;

void *Malloc(size_t size);
void FreeAndNull(void **p);
BOOL ReAlloc(void **ptr, size_t oldSize, size_t newSize);

#define Free(p) FreeAndNull((void **)&(p))

void ResetBinaryObject();
BOOL InitBinaryObject(size_t initial);
void DestroyBinaryObject();
bool ValidateHexText(LPCSTR pszText, size_t &nHexChars);
BOOL ParseBytes(LPSTR data, size_t length);
BINARY_DATA *GetBinaryData();
