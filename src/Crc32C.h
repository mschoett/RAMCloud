/* Copyright (c) 2010 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

// RAMCloud pragma [CPPLINT=0]

#ifndef RAMCLOUD_CRC32C_H
#define RAMCLOUD_CRC32C_H

#include "Common.h"

/// Lookup tables for software CRC32C implementation.
namespace Crc32CSlicingBy8 {
    extern const uint32_t crc_tableil8_o32[256];
    extern const uint32_t crc_tableil8_o40[256];
    extern const uint32_t crc_tableil8_o48[256];
    extern const uint32_t crc_tableil8_o56[256];
    extern const uint32_t crc_tableil8_o64[256];
    extern const uint32_t crc_tableil8_o72[256];
    extern const uint32_t crc_tableil8_o80[256];
    extern const uint32_t crc_tableil8_o88[256];
}

namespace RAMCloud {

/*
 * Note that we use hex opcodes to avoid issues with older versions
 * of the GNU assembler that do not recognise the instruction.
 */

// crc32q %rdx, %rcx
#define CRC32Q(_crc, _p64, _off) \
    __asm__ __volatile__(".byte 0xf2, 0x48, 0x0f, 0x38, 0xf1, 0xca" : \
        "=c"(_crc) : "c"(_crc), "d"(*(_p64 + _off)))

// crc32l %edx, %ecx
#define CRC32L(_crc, _p32, _off) \
    __asm__ __volatile__(".byte 0xf2, 0x0f, 0x38, 0xf1, 0xca" : \
        "=c"(_crc) : "c"(_crc), "d"(*(_p32 + _off)))

// crc32w %dx, %ecx
#define CRC32W(_crc, _p16, _off) \
    __asm__ __volatile__(".byte 0x66, 0xf2, 0x0f, 0x38, 0xf1, 0xca" : \
        "=c"(_crc) : "c"(_crc), "d"(*(_p16 + _off)))

// crc32b %dl, %ecx
#define CRC32B(_crc, _p8, _off) \
    __asm__ __volatile__(".byte 0xf2, 0x0f, 0x38, 0xf0, 0xca" : \
        "=c"(_crc) : "c"(_crc), "d"(*(_p8 + _off)))

/// See #Crc32C().
static inline uint32_t
intelCrc32C(uint32_t crc, const void* buffer, uint64_t bytes)
{
    const uint64_t* p64 = static_cast<const uint64_t*>(buffer);
    uint64_t remainder = bytes;
    uint64_t chunk32 = 0;
    uint64_t chunk8 = 0;

    // Do unrolled 32-byte chunks first, 8-bytes at a time.
    chunk32 = remainder >> 5;
    remainder &= 31;
    while (chunk32-- > 0) {
        CRC32Q(crc, p64, 0);
        CRC32Q(crc, p64, 1);
        CRC32Q(crc, p64, 2);
        CRC32Q(crc, p64, 3);
        p64 += 4;
    }

    // Next, do any remaining 8-byte chunks.
    chunk8 = remainder >> 3;
    remainder &= 7;
    while (chunk8-- > 0) {
        CRC32Q(crc, p64, 0);
        p64++;
    }

    // Next, do any remaining 2-byte chunks.
    const uint16_t* p16 = reinterpret_cast<const uint16_t*>(p64);
    uint64_t chunk2 = remainder >> 1;
    remainder &= 1;
    while (chunk2-- > 0) {
        CRC32W(crc, p16, 0);
        p16++;
    }

    // Finally, do any remaining byte.
    if (remainder) {
        const uint8_t* p8 = reinterpret_cast<const uint8_t*>(p16);
        CRC32B(crc, p8, 0);
    }
    return crc;
}

/// See #Crc32C().
static inline uint32_t
softwareCrc32C(uint32_t crc, const void* data, uint64_t length)
{
    // This following header applies to this function only. The LICENSE file
    // referred to in the first header block was not found in the archive.
    // This is from http://evanjones.ca/crc32c.html.

    // Copyright 2008,2009,2010 Massachusetts Institute of Technology.
    // All rights reserved. Use of this source code is governed by a
    // BSD-style license that can be found in the LICENSE file.
    //
    // Implementations adapted from Intel's Slicing By 8 Sourceforge Project
    // http://sourceforge.net/projects/slicing-by-8/

    // Copyright (c) 2004-2006 Intel Corporation - All Rights Reserved
    //
    // This software program is licensed subject to the BSD License, 
    // available at http://www.opensource.org/licenses/bsd-license.html

    using namespace Crc32CSlicingBy8; // NOLINT
    const char* p_buf = (const char*) data;

    // Handle leading misaligned bytes
    size_t initial_bytes = (sizeof(int32_t) - (intptr_t)p_buf) & (sizeof(int32_t) - 1);
    if (length < initial_bytes) initial_bytes = length;
    for (size_t li = 0; li < initial_bytes; li++) {
        crc = crc_tableil8_o32[(crc ^ *p_buf++) & 0x000000FF] ^ (crc >> 8);
    }

    length -= initial_bytes;
    size_t running_length = length & ~(sizeof(uint64_t) - 1);
    size_t end_bytes = length - running_length; 

    for (size_t li = 0; li < running_length/8; li++) {
        crc ^= *(uint32_t*) p_buf;
        p_buf += 4;
        uint32_t term1 = crc_tableil8_o88[crc & 0x000000FF] ^
                crc_tableil8_o80[(crc >> 8) & 0x000000FF];
        uint32_t term2 = crc >> 16;
        crc = term1 ^
              crc_tableil8_o72[term2 & 0x000000FF] ^ 
              crc_tableil8_o64[(term2 >> 8) & 0x000000FF];
        term1 = crc_tableil8_o56[(*(uint32_t *)p_buf) & 0x000000FF] ^
                crc_tableil8_o48[((*(uint32_t *)p_buf) >> 8) & 0x000000FF];

        term2 = (*(uint32_t *)p_buf) >> 16;
        crc = crc ^ term1 ^
                crc_tableil8_o40[term2  & 0x000000FF] ^
                crc_tableil8_o32[(term2 >> 8) & 0x000000FF];
        p_buf += 4;
    }

    for (size_t li=0; li < end_bytes; li++) {
        crc = crc_tableil8_o32[(crc ^ *p_buf++) & 0x000000FF] ^ (crc >> 8);
    }

    return crc;
}

/**
 * Compute a CRC32C (i.e. CRC32 with the Castagnoli polynomial, which
 * is used in iSCSI, among other protocols).
 *
 * This function uses the "crc32" instruction found in Intel Nehalem and later
 * processors. On processors without that instruction, it calculates the same
 * function much more slowly in software (just under 400 MB/sec in software vs
 * just under 2000 MB/sec in hardware on Westmere boxes).
 *
 * \param[in] crc
 *      CRC to accumulate. The return value of this function can be
 *      passed to future calls as this parameter to update a CRC
 *      with multiple invocations.
 * \param[in] buffer
 *      A pointer to the memory to be checksummed.
 * \param[in] bytes
 *      The number of bytes of memory to checksum.
 * \return
 *      The CRC32C associated with the input parameters.
 */
static inline uint32_t
Crc32C(uint32_t crc, const void* buffer, uint64_t bytes)
{
    extern bool haveSse42();
    static bool hardware = haveSse42();
#ifdef PERF_DEBUG_RECOVERY_SOFTWARE_CRC32
    hardware = false;
#endif
    return hardware ? intelCrc32C(crc, buffer, bytes)
                    : softwareCrc32C(crc, buffer, bytes);
}

} // namespace RAMCloud

#endif // !RAMCLOUD_CRC32C_H
