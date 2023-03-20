#include <algorithm>
#include <memory>

#include "plugin.h"
#include "parser.h"

using SP_BYTE = std::shared_ptr<BYTE>;
using BYTES_TUPLE = std::tuple<SP_BYTE, size_t>;

BINARY_DATA gBinary = { nullptr, 0, 0, 0 };

void *Malloc(size_t size)
{
    BYTE *mem = (BYTE *) malloc(size);
    if (nullptr != mem)
        memset(mem, 0, size);
    return mem;
}

BOOL ReAlloc(void **ptr, size_t oldSize, size_t newSize)
{
    BYTE *mem = nullptr;
    size_t expansion = newSize - oldSize;

    if (ptr == nullptr)
        return FALSE;

    mem = (BYTE *) realloc(*ptr, newSize);
    if (!mem)
        return FALSE;

    if (expansion > 0)
        memset(mem + oldSize, 0, expansion);

    *ptr = mem;

    return TRUE;
}

void FreeAndNull(void **p)
{
    if (p == nullptr || *p == nullptr)
        return;

    free(*p);
    *p = nullptr;
}

#define ReAllocPtr(p, old, news) ReAlloc( (void **)&(p), (old), (news) )

BOOL PutByteArray(BYTE *ba, size_t size)
{
    if (size > gBinary.size - gBinary.index)
    {
        if (!ReAllocPtr(gBinary.binary, gBinary.size, gBinary.size + size + 100))
        {
            return FALSE;
        }

        gBinary.size += size + 100;
    }

    memcpy(gBinary.binary + gBinary.index, ba, size);
    gBinary.index += size;

    return TRUE;
}

BOOL PutBytes(const BYTES_TUPLE &data)
{
    return PutByteArray(std::get<0>(data).get(), std::get<1>(data));
}

void ResetBinaryObject()
{
    if (!gBinary.binary)
    {
        InitBinaryObject(100);
        return;
    }

    memset(gBinary.binary, 0, gBinary.size);
    gBinary.index = 0;
    gBinary.invalid = FALSE;
}

BOOL InitBinaryObject(size_t initial)
{
    gBinary.binary = (BYTE *) Malloc(initial);
    if (!gBinary.binary)
        return FALSE;

    gBinary.index = 0;
    gBinary.size = initial;

    return TRUE;
}

void DestroyBinaryObject()
{
    Free(gBinary.binary);
    memset(&gBinary, 0, sizeof(BINARY_DATA));
}

BOOL IsDelimiter(CHAR  c)
{
    switch (c)
    {
        case '\r':
        case '\n':
        case '\b':
        case '\t':
        case ' ':
        case ',':
        case '{':
        case '}':
            return TRUE;
    }

    return FALSE;
}

BOOL IsHexChar(int c)
{
    if (c >= '0' && c <= '9')
        return TRUE;
    if ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
        return TRUE;

    return FALSE;
}

#define MAX_HEX_BLOCK 2

bool ValidateHexText(LPCSTR pszText, size_t &nHexChars)
{
    _ASSERT(nullptr != pszText);

    nHexChars = 0;

    if (nullptr == pszText || 0 == *pszText)
        return false;

    LPCSTR pszStart = pszText;
    LPCSTR pszEnd = pszText + strlen(pszText);

    // skip/trim whitespace chars
    while (isspace(*pszStart) && pszStart < pszEnd)
        ++pszStart;
    while (isspace(*pszEnd) && pszEnd > pszStart)
        --pszEnd;

    if (pszStart == pszEnd)
        return true;    // empty string, caller check thought nHexChars

    while (pszStart < pszEnd)
    {
        if ('0' == pszStart[0] || '\\' == pszStart[0])
        {
            if ((pszStart + 2 < pszEnd)
                && ('x' == pszStart[1] || 'X' == pszStart[1])
                && IsHexChar(pszStart[2]))
            {
                pszStart += 2;
                continue;
            }
        }

        if (IsHexChar(*pszStart))
            ++nHexChars;
        else if (!IsDelimiter(*pszStart))
        {
            nHexChars = 0;
            return false;
        }

        ++pszStart;
    }

    return true;
}

BYTES_TUPLE Hex2Bytes(LPSTR sval, size_t len)
{
    std::unique_ptr < BYTE[]> p = std::make_unique<BYTE[]>(len + 2);
    if (len % 2)
    {
        p[0] = '0';
        memcpy(&p[1], sval, len);
        len += 1;
    }
    else
        memcpy(&p[0], sval, len);

    size_t finalLen = len / 2;
    SP_BYTE spb(new BYTE[finalLen], std::default_delete < BYTE[]>());

    for (size_t i = 0, j = 0; j < finalLen; i += 2, ++j)
    {
        spb.get()[j] = (p[i] % 32 + 9) % 25 * 16 + (p[i + 1] % 32 + 9) % 25;
    }

    return { spb, finalLen };
}

BYTES_TUPLE GetBytesValue(LPSTR data, size_t length)
{
    if (length > 2)
    {
        if (data[0] == '0' && data[1] == 'x')
        {
            if (strchr(data + 2, 'x') || strchr(data + 2, '-'))
            {
                gBinary.invalid = TRUE;
                return { nullptr, 0 };
            }

            return Hex2Bytes(data + 2, length - 2);
        }
    }

    if (strchr(data, 'x'))
    {
        gBinary.invalid = TRUE;
        return { nullptr, 0 };
    }

    if (*data == '-')
    {
        if (length == 1)
        {
            gBinary.invalid = TRUE;
            return { nullptr, 0 };
        }

        if (strchr(data + 1, '-'))
        {
            gBinary.invalid = TRUE;
            return { nullptr, 0 };
        }
    }

    return Hex2Bytes(data, length);
}

#define PutValueAndResetBuffer() PutBytes(Hex2Bytes(buf, bufi)); \
                                    written++; \
                                    bufi = 0; \
                                    RtlSecureZeroMemory(buf, sizeof(buf))

size_t TryExtractShellcodeStyle(LPSTR data, size_t length)
{
    size_t i = 0, bufi = 0, written = 0;
    CHAR buf[MAX_HEX_BLOCK + 1] = { 0 };
    BOOL fail = FALSE;

    while (data[i])
    {
        if (gBinary.invalid)
            return 0;

        if (i + 2 >= length)
            break;

        if (data[i] != '\\' && data[i + 1] != 'x')
        {
            fail = TRUE;
            break;
        }

        i += 2;

        while (i < length)
        {
            if (!IsHexChar(data[i]))
            {
                if (data[i] == '\\' || data[i] == 0)
                {
                    PutValueAndResetBuffer();
                    break;
                }

                fail = TRUE;
                break;
            }

            if (!IsHexChar(data[i]))
            {
                fail = TRUE;
                break;
            }

            buf[bufi++] = data[i++];

            if (bufi == MAX_HEX_BLOCK)
            {
                PutValueAndResetBuffer();
                break;
            }
        }

        if (fail)
            break;
    }

    if (fail)
    {
        gBinary.invalid = TRUE;
        written = 0;
    }
    else if (bufi > 0)
    {
        PutValueAndResetBuffer();
    }

    return written;
}

BOOL ParseBytes(LPSTR data, size_t length)
{
    size_t bufi = 0;
    LPSTR p = data, sp = nullptr, supply = nullptr;
    BOOL quoteOn = FALSE, curly = FALSE;

    // HTC: convert hex string to lower case
    CharLowerBuff(data, static_cast<DWORD>(length));

    supply = (LPSTR) Malloc(length + 1);
    if (!supply)
        return FALSE;

    sp = supply;

    while (*p)
    {
        if (gBinary.invalid)
        {
            bufi = 0;
            break;
        }

        if (*p == '"' || *p == '\'')
        {
            quoteOn = !quoteOn;

            if (quoteOn)
            {
                if (bufi > 0)
                    PutBytes(GetBytesValue(supply, bufi));
            }
            else if (bufi > 0)
            {
                TryExtractShellcodeStyle(supply, bufi);
            }

            if (bufi > 0)
            {
                memset(supply, 0, bufi);
                bufi = 0;
                sp = supply;
            }

            p++;
            continue;
        }

        if (*p == '{' || *p == '}')
        {
            curly = *p == '{' ? TRUE : FALSE;
            p++;
            continue;
        }

        if (!quoteOn)
        {
            if (IsDelimiter(*p))
            {
                if (bufi > 0)
                {
                    PutBytes(GetBytesValue(supply, bufi));
                    memset(supply, 0, bufi);
                    bufi = 0;
                    sp = supply;
                }

                p++;
                continue;
            }
        }

        if (IsHexChar(*p) || *p == '-' || *p == 'x' || (quoteOn && *p == '\\'))
        {
            *sp++ = *p++;
            bufi++;
        }
        else
        {
            if (*p == ';' && !curly && !quoteOn)
            {
                p++;
                continue;
            }

            gBinary.invalid = TRUE;
            bufi = 0;
            break;
        }
    }

    if (quoteOn)
        gBinary.invalid = TRUE;
    else if (bufi > 0)
    {
        PutBytes(GetBytesValue(supply, bufi));
    }

    Free(supply);

    return !gBinary.invalid;
}

BINARY_DATA *GetBinaryData()
{
    return &gBinary;
}

struct
{
    LPSTR input;
    BYTE expect[7];
    BOOL willFail;
}Tests[] = {
    { R"({ 0xFA, 0xDE, 0x24, 255 } '\xde\xBF\xf')", {0xfa,0xde,0x24,255,0xde,0xbf,0xf}, FALSE },
    {"\r\n   124, 65 0x95 21,0x44 '\\xaa\\xee'" , {124,65,0x95,21,0x44,0xaa,0xee}, FALSE },
    {R"('\xde\xad\xbe\xef\x41\x41')", {0xde,0xad,0xbe,0xef,0x41,0x41},FALSE},
    { R"({ 0xFA, 0xDEx, 0x24, 255 } '\xde\xBF\xf')", {0xfa,0xde,0x24,255,0xde,0xbf,0xf}, TRUE},
    {"\r\n   124, 65 0x95 21,0x44 '\\xaax\\xee'" , {124,65,0x95,21,0x44,0xaa,0xee}, TRUE},
};

void TestParser()
{
    InitBinaryObject(2);

    for (int i = 0; i < sizeof(Tests) / sizeof(Tests[0]); i++)
    {
        ParseBytes(Tests[i].input, strlen(Tests[i].input));
        if (Tests[i].willFail)
        {
            if (gBinary.invalid)
            {
                printf("Test %d is PASSED\n", i + 1);
            }
            else
            {
                printf("Test %d is FAILED\n", i + 1);
            }
        }
        else
        {
            if (gBinary.invalid)
            {
                printf("Test %d is FAILED\n", i + 1);
            }
            else
            {
                if (memcmp(Tests[i].expect, gBinary.binary, 7) == 0)
                {
                    printf("Test %d is PASSED\n", i + 1);
                }
                else
                {
                    printf("Test %d is FAILED\n", i + 1);
                }
            }
        }

        ResetBinaryObject();
    }

    DestroyBinaryObject();
}
