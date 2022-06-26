/* hex.c - conversion for hexadecimal and base32 strings.
 *
 * Copyright (c) 2008, Aleksey Kravchenko <rhash.admin@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE  INCLUDING ALL IMPLIED WARRANTIES OF  MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT,  OR CONSEQUENTIAL DAMAGES  OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE,  DATA OR PROFITS,  WHETHER IN AN ACTION OF CONTRACT,  NEGLIGENCE
 * OR OTHER TORTIOUS ACTION,  ARISING OUT OF  OR IN CONNECTION  WITH THE USE  OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
#include "hex.h"
#include "util.h"
#include <ctype.h>
#include <string.h>

/**
 * Store hexadecimal representation of a binary string to given buffer.
 *
 * @param dst the buffer to receive hexadecimal representation
 * @param src binary string
 * @param length string length
 * @param upper_case flag to print string in uppercase
 */
void rhash_byte_to_hex(char* dst, const unsigned char* src, size_t length, int upper_case)
{
	const char hex_add = (upper_case ? 'A' - 10 : 'a' - 10);
	for (; length > 0; src++, length--) {
		const unsigned char hi = (*src >> 4) & 15;
		const unsigned char lo = *src & 15;
		*dst++ = (hi > 9 ? hi + hex_add : hi + '0');
		*dst++ = (lo > 9 ? lo + hex_add : lo + '0');
	}
	*dst = '\0';
}

/**
 * Encode a binary string to base32.
 *
 * @param dst the buffer to store result
 * @param src binary string
 * @param length string length
 * @param upper_case flag to print string in uppercase
 */
void rhash_byte_to_base32(char* dst, const unsigned char* src, size_t length, int upper_case)
{
	const char a = (upper_case ? 'A' : 'a');
	unsigned shift = 0;
	unsigned char word;
	const unsigned char* e = src + length;
	while (src < e) {
		if (shift > 3) {
			word = (*src & (0xFF >> shift));
			shift = (shift + 5) % 8;
			word <<= shift;
			if (src + 1 < e)
				word |= *(src + 1) >> (8 - shift);
			++src;
		} else {
			shift = (shift + 5) % 8;
			word = ( *src >> ( (8 - shift) & 7 ) ) & 0x1F;
			if (shift == 0) src++;
		}
		*dst++ = ( word < 26 ? word + a : word + '2' - 26 );
	}
	*dst = '\0';
}

/**
 * Encode a binary string to base64.
 * Encoded output length is always a multiple of 4 bytes.
 *
 * @param dst the buffer to store result
 * @param src binary string
 * @param length string length
 */
void rhash_byte_to_base64(char* dst, const unsigned char* src, size_t length)
{
	static const char* tail = "0123456789+/";
	unsigned shift = 0;
	unsigned char word;
	const unsigned char* e = src + length;
	while (src < e) {
		if (shift > 2) {
			word = (*src & (0xFF >> shift));
			shift = (shift + 6) % 8;
			word <<= shift;
			if (src + 1 < e)
				word |= *(src + 1) >> (8 - shift);
			++src;
		} else {
			shift = (shift + 6) % 8;
			word = ( *src >> ( (8 - shift) & 7 ) ) & 0x3F;
			if (shift == 0) src++;
		}
		*dst++ = ( word < 52 ? (word < 26 ? word + 'A' : word - 26 + 'a') : tail[word - 52]);
	}
	if (shift > 0) {
		*dst++ = '=';
		if (shift == 4) *dst++ = '=';
	}
	*dst = '\0';
}

size_t rhash_base64_url_encoded_helper(char* dst, const unsigned char* src, size_t length, int url_encode, int upper_case)
{
#define B64_CHUNK_SIZE 120
	char buffer[164];
	RHASH_ASSERT((BASE64_LENGTH(B64_CHUNK_SIZE) + 4) <= sizeof(buffer));
	RHASH_ASSERT((B64_CHUNK_SIZE % 6) == 0);
	if (url_encode) {
		size_t result_length = 0;
		for (; length > 0; src += B64_CHUNK_SIZE) {
			size_t chunk_size = (length < B64_CHUNK_SIZE ? length : B64_CHUNK_SIZE);
			size_t encoded_length;
			rhash_byte_to_base64(buffer, src, chunk_size);
			encoded_length = rhash_urlencode(dst, buffer, BASE64_LENGTH(chunk_size), upper_case);
			result_length += encoded_length;
			dst += encoded_length;
			length -= chunk_size;
		}
		return result_length;
	}
	rhash_byte_to_base64(dst, src, length);
	return BASE64_LENGTH(length);
}

/* RFC 3986: safe url characters are ascii alpha-numeric and "-._~", other characters should be percent-encoded */
static unsigned url_safe_char_mask[4] = { 0, 0x03ff6000, 0x87fffffe, 0x47fffffe };
#define IS_URL_GOOD_CHAR(c) ((unsigned)(c) < 128 && (url_safe_char_mask[c >> 5] & (1 << (c & 31))))

/**
 * URL-encode specified binary string.
 *
 * @param dst (nullable) buffer to output encoded string to,
 *            NULL to just calculate the lengths of encoded string
 * @param src binary string to encode
 * @param size size of the binary string
 * @param upper_case flag to output hex-codes in uppercase
 * @return the length of the result string
 */
size_t rhash_urlencode(char* dst, const char* src, size_t size, int upper_case)
{
	const char* start;
	size_t i;
	if (!dst) {
		size_t length = size;
		for (i = 0; i < size; i++)
			if (!IS_URL_GOOD_CHAR(src[i]))
				length += 2;
		return length;
	} else {
		const char hex_add = (upper_case ? 'A' - 10 : 'a' - 10);
		start = dst;
		/* percent-encode all but unreserved URL characters */
		for (i = 0; i < size; i++) {
			if (IS_URL_GOOD_CHAR(src[i])) {
				*dst++ = src[i];
			} else {
				unsigned char hi = ((unsigned char)(src[i]) >> 4) & 0x0f;
				unsigned char lo = (unsigned char)(src[i]) & 0x0f;
				*dst++ = '%';
				*dst++ = (hi > 9 ? hi + hex_add : hi + '0');
				*dst++ = (lo > 9 ? lo + hex_add : lo + '0');
			}
		}
		*dst = 0;
	}
	return dst - start;
}

/**
 * Print 64-bit number with trailing '\0' to a string buffer.
 * if dst is NULL, then just return the length of the number.
 *
 * @param dst output buffer
 * @param number the number to print
 * @return length of the printed number (without trailing '\0')
 */
int rhash_sprintI64(char* dst, uint64_t number)
{
	/* The biggest number has 20 digits: 2^64 = 18 446 744 073 709 551 616 */
	char buf[24];
	char* p;
	size_t length;

	if (dst == NULL) {
		/* just calculate the length of the number */
		if (number == 0) return 1;
		for (length = 0; number != 0; number /= 10) length++;
		return (int)length;
	}

	p = buf + 23;
	*p = '\0'; /* last symbol should be '\0' */
	if (number == 0) {
		*(--p) = '0';
	} else {
		for (; p >= buf && number != 0; number /= 10) {
			*(--p) = '0' + (char)(number % 10);
		}
	}
	length = buf + 23 - p;
	memcpy(dst, p, length + 1);
	return (int)length;
}
