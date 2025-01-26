// test.c

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "utf8.h"

int main()
{
	struct utf8_decoder decoder;
	utf8_decoder_init(&decoder);

	ucp c;
	ucp *cp;
	byte b;
	byte *bp;
	int error;

	// Test encoding individual ASCII characters
	for (c = 0; c < 0x80; c++) {
		error = 1;
		assert(utf8_bytes_for_char(c, &error) == 1);
		assert(error == 0);
		error = 1;
		b = c + 1;
		bp = utf8_encode_char(c, &b, 1, &error);
		assert(bp == &b + 1);
		assert(error == 0);
		assert(b == c);
	}

	// Test decoding individual ASCII characters
	for (b = 0; b < 128; b++) {
		error = 1;
		c = b + 1;
		cp = utf8_decoder_decode(&decoder, b, &c, &error);
		assert(error == 0);
		assert(c == b);
		assert(cp == &c + 1);
	}

	// Test encoding an array of ASCII characters
	ucp ca[128];
	byte ba[512];
	for (int i = 0; i < 128; i++) {
		ca[i] = i;
		ba[i] = i + 1; // Intentional mismatch
	}
	error = 1;
	bp = utf8_encode_array(ca, 128, ba, 128, &error);
	assert(error == 0);
	assert(bp == ba + 128);
	for (int i = 0; i < 128; i++) {
		assert(ca[i] == (ucp)i);
		assert(ba[i] == i); // Assert changed
	}

	// Test decoding an array of ASCII characters
	for (int i = 0; i < 128; i++) {
		ca[i] = i + 1; // Intentional mismatch
		ba[i] = i;
	}
	error = 1;
	cp = utf8_decode_array(ba, 128, ca, 128, &error);
	assert(error == 0);
	assert(cp == ca + 128);
	for (int i = 0; i < 128; i++) {
		assert(ca[i] == (ucp)i); // Assert changed
		assert(ba[i] == i);
	}

	// Test boundaries for byte counts
	error = 1;
	assert(utf8_bytes_for_char(0x7f, &error) == 1);
	assert(error == 0);
	error = 1;
	assert(utf8_bytes_for_char(0x80, &error) == 2);
	assert(error == 0);
	error = 1;
	assert(utf8_bytes_for_char(0x7ff, &error) == 2);
	assert(error == 0);
	error = 1;
	assert(utf8_bytes_for_char(0x800, &error) == 3);
	assert(error == 0);
	error = 1;
	assert(utf8_bytes_for_char(0xffff, &error) == 3);
	assert(error == 0);
	error = 1;
	assert(utf8_bytes_for_char(0x10000, &error) == 4);
	assert(error == 0);
	error = 1;
	assert(utf8_bytes_for_char(0x1fffff, &error) == 4);
	assert(error == 0);
	assert(utf8_bytes_for_char(0x200000, &error) == 0);
	assert(error == UTF8_ERROR_INVALID_CHARACTER);

	// Get current time
	struct timeval tv;
	int ret = gettimeofday(&tv, NULL);
	if (ret != 0) {
		fprintf(stderr, "error: failed to get current time\n");
		return 1;
	}

	// Seed random number generator with current time in microseconds
	srandom(tv.tv_usec + tv.tv_sec * 1000000);

	// Test that encoding/decoding is symmetric.
	// Use random characters in three ranges.
	const int masks[3] = { 0x7ff, 0xffff, 0x1fffff };
	ucp ca2[128];
	for (int mi = 0; mi < 3; mi++) {
		for (int i = 0; i < 128; i++) {
			ca[i] = random() & masks[mi];
			ca2[i] = ca[i] + 1; // Intentional mismatch
		}
		error = 1;
		bp = utf8_encode_array(ca, 128, ba, 512, &error);
		assert(error == 0);
		assert(bp >= ba + 128 && bp <= ba + 512);
		error = 1;
		cp = utf8_decode_array(ba, bp - ba, ca2, 128, &error);
		assert(error == 0);
		assert(cp == ca2 + 128);
		for (int i = 0; i < 128; i++) {
			assert(ca[i] == ca2[i]);
		}
	}

	// Test overflow detection in utf8_decode_string()
	error = 0;
	cp = utf8_decode_string("abc", ca, 1, &error);
	assert(cp == ca + 1);
	assert(error == UTF8_ERROR_CHARACTER_OVERFLOW);
	error = 0;
	cp = utf8_decode_string("abc", ca, 2, &error);
	assert(cp == ca + 2);
	assert(error == UTF8_ERROR_CHARACTER_OVERFLOW);
	cp = utf8_decode_string("abc", ca, 3, &error);
	assert(cp == ca + 3);
	assert(error == 0);

	// Test overflow detection in utf8_decode_array()
	ba[0] = 'a';
	ba[1] = 'b';
	ba[2] = 'c';
	error = 0;
	cp = utf8_decode_array(ba, 3, ca, 1, &error);
	assert(cp == ca + 1);
	assert(error == UTF8_ERROR_CHARACTER_OVERFLOW);
	error = 0;
	cp = utf8_decode_array(ba, 3, ca, 2, &error);
	assert(cp == ca + 2);
	assert(error == UTF8_ERROR_CHARACTER_OVERFLOW);
	cp = utf8_decode_array(ba, 3, ca, 3, &error);
	assert(cp == ca + 3);
	assert(error == 0);

	// Test overflow detection in utf8_encode_array()
	ca[0] = 1; // 1 byte
	ca[1] = 0x80; // 2 byte
	ca[2] = 0x800; // 3 byte
	ca[3] = 0x10000; // 4 byte
	error = 0;
	bp = utf8_encode_array(ca, 1, ba, 0, &error);
	assert(bp == ba);
	assert(error == UTF8_ERROR_BYTE_OVERFLOW);
	error = 1;
	bp = utf8_encode_array(ca, 1, ba, 1, &error);
	assert(bp == ba + 1);
	assert(error == 0);
	error = 0;
	bp = utf8_encode_array(ca, 2, ba, 2, &error);
	assert(bp == ba + 1);
	assert(error == UTF8_ERROR_BYTE_OVERFLOW);
	error = 1;
	bp = utf8_encode_array(ca, 2, ba, 3, &error);
	assert(bp == ba + 3);
	assert(error == 0);
	error = 0;
	bp = utf8_encode_array(ca, 3, ba, 4, &error);
	assert(bp == ba + 3);
	assert(error == UTF8_ERROR_BYTE_OVERFLOW);
	error = 1;
	bp = utf8_encode_array(ca, 3, ba, 6, &error);
	assert(bp == ba + 6);
	assert(error == 0);
	error = 0;
	bp = utf8_encode_array(ca, 4, ba, 7, &error);
	assert(bp == ba + 6);
	assert(error == UTF8_ERROR_BYTE_OVERFLOW);
	error = 0;
	bp = utf8_encode_array(ca, 4, ba, 9, &error);
	assert(bp == ba + 6);
	assert(error == UTF8_ERROR_BYTE_OVERFLOW);
	error = 1;
	bp = utf8_encode_array(ca, 4, ba, 10, &error);
	assert(bp == ba + 10);
	assert(error == 0);

	// Test invalid start byte
	ba[0] = 0x80;
	ba[1] = 0;
	error = 0;
	cp = utf8_decode_array(ba, 2, ca, 1, &error);
	assert(cp == ca);
	assert(error == UTF8_ERROR_INVALID_START_BYTE);

	// Test invalid continuation byte
	ba[0] = 0xd0;
	ba[1] = 0;
	error = 0;
	cp = utf8_decode_array(ba, 2, ca, 1, &error);
	assert(cp == ca);
	assert(error == UTF8_ERROR_INVALID_CONTINUATION_BYTE);

	// Test overlong sequences and unexpected termination
	ba[0] = 0xc1;
	ba[1] = 0x80;
	error = 0;
	cp = utf8_decode_array(ba, 2, ca, 1, &error);
	assert(cp == ca);
	assert(error == UTF8_ERROR_OVERLONG_SEQUENCE);
	cp = utf8_decode_array(ba, 1, ca, 1, &error);
	assert(cp == ca);
	assert(error == UTF8_ERROR_UNEXPECTED_TERMINATION);
	ba[0] = 0xe0;
	ba[1] = 0x90;
	ba[2] = 0x80;
	error = 0;
	cp = utf8_decode_array(ba, 3, ca, 1, &error);
	assert(cp == ca);
	assert(error == UTF8_ERROR_OVERLONG_SEQUENCE);
	cp = utf8_decode_array(ba, 2, ca, 1, &error);
	assert(cp == ca);
	assert(error == UTF8_ERROR_UNEXPECTED_TERMINATION);
	ba[0] = 0xf0;
	ba[1] = 0x88;
	ba[2] = 0x80;
	ba[3] = 0x80;
	error = 0;
	cp = utf8_decode_array(ba, 4, ca, 1, &error);
	assert(cp == ca);
	assert(error == UTF8_ERROR_OVERLONG_SEQUENCE);
	cp = utf8_decode_array(ba, 3, ca, 1, &error);
	assert(cp == ca);
	assert(error == UTF8_ERROR_UNEXPECTED_TERMINATION);

	return 0;
}
