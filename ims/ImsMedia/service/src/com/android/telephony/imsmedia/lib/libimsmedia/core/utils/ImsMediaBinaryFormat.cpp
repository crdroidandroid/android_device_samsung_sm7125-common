/**
 * Copyright (C) 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <string.h>
#include <ImsMediaBinaryFormat.h>
#include <ImsMediaTrace.h>

// Carriage-Return (\r)
#define CR         0x0D
// Line-Feed (\n)
#define LF         0x0A
// Padding character for Base64
#define BASE64_PAD '='

// Constant table for Base64 value encoding / decoding
static const char BASE64_ENCODING_TABLE[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static int DecodeBase16Char(char value)
{
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    if (value >= 'A' && value <= 'F')
        return value - 'A' + 10;
    return -1;
}

static int DecodeBase64Char(char value)
{
    if (value >= 'A' && value <= 'Z')
        return value - 'A';
    if (value >= 'a' && value <= 'z')
        return value - 'a' + 26;
    if (value >= '0' && value <= '9')
        return value - '0' + 52;
    if (value == '+')
        return 62;
    if (value == '/')
        return 63;
    return -1;
}

static bool BinaryToBase16(char* pszDst, uint32_t nDstBuffSize, uint8_t* pbSrc, uint32_t nSrcSize)
{
    const uint64_t requiredSize = static_cast<uint64_t>(nSrcSize) * 2 + 1;
    if (pszDst == nullptr || (nSrcSize > 0 && pbSrc == nullptr) || requiredSize > nDstBuffSize)
        return false;

    uint32_t dstPos = 0;
    for (uint32_t srcPos = 0; srcPos < nSrcSize; ++srcPos)
    {
        const uint8_t value = pbSrc[srcPos];
        pszDst[dstPos++] = "0123456789ABCDEF"[value >> 4];
        pszDst[dstPos++] = "0123456789ABCDEF"[value & 0x0F];
    }
    pszDst[dstPos] = '\0';

    return true;
}

static bool Base16ToBinary(uint8_t* pbDst, uint32_t* pnDstSize, uint32_t nDstBuffSize, char* pszSrc)
{
    if (pnDstSize == nullptr || pszSrc == nullptr)
        return false;

    const size_t srcLen = strlen(pszSrc);
    if ((srcLen & 1) != 0 || srcLen / 2 > nDstBuffSize ||
            (srcLen > 0 && pbDst == nullptr))
        return false;

    for (size_t srcPos = 0; srcPos < srcLen; srcPos += 2)
    {
        const int high = DecodeBase16Char(pszSrc[srcPos]);
        const int low = DecodeBase16Char(pszSrc[srcPos + 1]);
        if (high < 0 || low < 0)
            return false;
    }

    for (size_t srcPos = 0, dstPos = 0; srcPos < srcLen; srcPos += 2, ++dstPos)
    {
        pbDst[dstPos] = static_cast<uint8_t>(
                DecodeBase16Char(pszSrc[srcPos]) << 4 |
                DecodeBase16Char(pszSrc[srcPos + 1]));
    }

    *pnDstSize = static_cast<uint32_t>(srcLen / 2);

    return true;
}

static bool BinaryToBase64(
        char* pszDst, uint32_t nDstBuffSize, const uint8_t* pbSrc, uint32_t nSrcSize)
{
    const uint64_t encodedSize = (static_cast<uint64_t>(nSrcSize) + 2) / 3 * 4;
    if (pszDst == nullptr || (nSrcSize > 0 && pbSrc == nullptr) ||
            encodedSize + 1 > nDstBuffSize)
        return false;

    uint32_t srcPos = 0;
    uint64_t dstPos = 0;
    while (srcPos < nSrcSize)
    {
        const uint32_t remaining = nSrcSize - srcPos;
        const uint8_t first = pbSrc[srcPos++];
        const uint8_t second = remaining > 1 ? pbSrc[srcPos++] : 0;
        const uint8_t third = remaining > 2 ? pbSrc[srcPos++] : 0;

        pszDst[dstPos++] = BASE64_ENCODING_TABLE[first >> 2];
        pszDst[dstPos++] = BASE64_ENCODING_TABLE[(first & 0x03) << 4 | second >> 4];
        pszDst[dstPos++] = remaining > 1
                ? BASE64_ENCODING_TABLE[(second & 0x0F) << 2 | third >> 6]
                : BASE64_PAD;
        pszDst[dstPos++] = remaining > 2 ? BASE64_ENCODING_TABLE[third & 0x3F] : BASE64_PAD;
    }

    pszDst[dstPos] = '\0';

    return true;
}

static bool Base64ToBinary(uint8_t* pbDst, uint32_t* pnDstSize, uint32_t nDstBuffSize, char* pszSrc)
{
    if (pnDstSize == nullptr || pszSrc == nullptr)
        return false;

    size_t encodedSize = 0;
    uint32_t padding = 0;
    bool paddingStarted = false;
    for (const char* source = pszSrc; *source != '\0'; ++source)
    {
        if (*source == CR || *source == LF)
        {
            continue;
        }

        const size_t quartetPosition = encodedSize % 4;
        if (*source == BASE64_PAD)
        {
            if (quartetPosition < 2 || ++padding > 2)
                return false;
            paddingStarted = true;
        }
        else if (paddingStarted || DecodeBase64Char(*source) < 0)
        {
            return false;
        }
        ++encodedSize;
    }

    if (encodedSize % 4 != 0)
        return false;

    const uint64_t decodedSize = encodedSize / 4 * 3 - padding;
    if (decodedSize > nDstBuffSize || (decodedSize > 0 && pbDst == nullptr))
        return false;

    char quartet[4];
    uint32_t quartetSize = 0;
    uint32_t dstPos = 0;
    for (const char* source = pszSrc; *source != '\0'; ++source)
    {
        if (*source == CR || *source == LF)
            continue;

        quartet[quartetSize++] = *source;
        if (quartetSize != 4)
            continue;

        const uint32_t first = static_cast<uint32_t>(DecodeBase64Char(quartet[0]));
        const uint32_t second = static_cast<uint32_t>(DecodeBase64Char(quartet[1]));
        const uint32_t third = quartet[2] == BASE64_PAD
                ? 0
                : static_cast<uint32_t>(DecodeBase64Char(quartet[2]));
        const uint32_t fourth = quartet[3] == BASE64_PAD
                ? 0
                : static_cast<uint32_t>(DecodeBase64Char(quartet[3]));

        pbDst[dstPos++] = static_cast<uint8_t>(first << 2 | second >> 4);
        if (quartet[2] != BASE64_PAD)
            pbDst[dstPos++] = static_cast<uint8_t>(second << 4 | third >> 2);
        if (quartet[3] != BASE64_PAD)
            pbDst[dstPos++] = static_cast<uint8_t>(third << 6 | fourth);
        quartetSize = 0;
    }

    *pnDstSize = dstPos;

    return true;
}

bool ImsMediaBinaryFormat::BinaryToBase00(
        char* pszDst, uint32_t nDstBuffSize, uint8_t* pbSrc, uint32_t nSrcSize, uint32_t eFormat)
{
    switch (eFormat)
    {
        case BINARY_FORMAT_BASE16:
            return BinaryToBase16(pszDst, nDstBuffSize, pbSrc, nSrcSize);
        case BINARY_FORMAT_BASE64:
            return BinaryToBase64(pszDst, nDstBuffSize, pbSrc, nSrcSize);
        case BINARY_FORMAT_BASE32:
        default:
            IMLOGE1("[BinaryToBase00] not supported binary format %d", eFormat);
            return false;
    }
}

bool ImsMediaBinaryFormat::Base00ToBinary(
        uint8_t* pbDst, uint32_t* pnDstSize, uint32_t nDstBuffSize, char* pszSrc, uint32_t eFormat)
{
    switch (eFormat)
    {
        case BINARY_FORMAT_BASE16:
            return Base16ToBinary(pbDst, pnDstSize, nDstBuffSize, pszSrc);
        case BINARY_FORMAT_BASE64:
            return Base64ToBinary(pbDst, pnDstSize, nDstBuffSize, pszSrc);
        case BINARY_FORMAT_BASE32:
        default:
            IMLOGE1("[Base00ToBinary] not supported binary format %d", eFormat);
            return false;
    }
}
