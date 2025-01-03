// utf8.c

#include "utf8.h"

void utf8_decoder_init(struct utf8_decoder* decoder) {
	decoder->state = 0;
	decoder->seqLength = 0;
	decoder->c = 0;
}

ucp* utf8_decoder_decode(struct utf8_decoder* decoder, byte b, ucp* c, int* error) {
	*error = 0;
	switch (decoder->state) {
	case 0: // First byte of character
		if (b < 0x80) { // US-ASCII character
			*(c++) = b;
		}
		// First byte of multi-byte character
		else if ((b & 0xe0) == 0xc0) {
			decoder->c = b & 0x1f;
			decoder->seqLength = 2;
			decoder->state = 1;
		}
		else if ((b & 0xf0) == 0xe0) {
			decoder->c = b & 0x0f;
			decoder->seqLength = 3;
			decoder->state = 2;
		}
		else if ((b & 0xf8) == 0xf0) {
			decoder->c = b & 0x07;
			decoder->seqLength = 4;
			decoder->state = 3;
		}
		else {
			*error = UTF8_ERROR_INVALID_START_BYTE;
		}
		break;
	default: // Next byte of multi-byte character
		if ((b & 0xc0) != 0x80) {
			*error = UTF8_ERROR_INVALID_CONTINUATION_BYTE;
		}
		else {
			decoder->c = decoder->c << 6 | (b & 0x3f);
			decoder->state--;
			if (decoder->state == 0) { // Multi-byte character complete
				if (utf8_bytes_for_char(decoder->c) == decoder->seqLength) {
					*(c++) = decoder->c;
				} else {
					*error = UTF8_ERROR_OVERLONG_SEQUENCE;
				}
			}
		}
	}
	return c;
}

int utf8_decoder_can_terminate(struct utf8_decoder* decoder) {
	return decoder->state == 0;
}

ucp* utf8_decode_array(byte* ba, size_t bc, ucp* ca, size_t cc, int* error) {
	struct utf8_decoder decoder;
	utf8_decoder_init(&decoder);
	byte* bend = ba + bc;
	ucp* cend = ca + cc;
	for (; ba < bend && ca < cend; ba++) {
		ca = utf8_decoder_decode(&decoder, *ba, ca, error);
		if (*error) { // A decoder error occurred
			return ca;
		}
	}
	// Handle array-based errors
	if (ba < bend && ca >= cend) {
		// We ran out of room to store characters
		*error = UTF8_ERROR_CHAR_OVERFLOW;
	} else if (!utf8_decoder_can_terminate(&decoder)) {
		// Input bytes did not end on a character boundary
		*error = UTF8_ERROR_UNEXPECTED_TERMINATION;
	} else {
		*error = 0;
	}
	return ca;
}

ucp* utf8_decode_string(const char* str, ucp* ca, size_t cc, int* error) {
	struct utf8_decoder decoder;
	utf8_decoder_init(&decoder);
	ucp* cend = ca + cc;
	for (; *str && ca < cend; str++) {
		ca = utf8_decoder_decode(&decoder, (byte)*str, ca, error);
		if (*error) { // A decoder error occurred
			return ca;
		}
	}
	// Handle array-based errors
	if (*str) {
		*error = UTF8_ERROR_CHAR_OVERFLOW;
	} else if (!utf8_decoder_can_terminate(&decoder)) {
		*error = UTF8_ERROR_UNEXPECTED_TERMINATION;
	} else {
		*error = 0;
	}
	return ca;
}

int utf8_bytes_for_char(ucp c) {
	if (c < 0x80) return 1;
	if (c < 0x800) return 2;
	if (c < 0x10000) return 3;
	return 4;
}

byte* utf8_encode_char(ucp c, byte* b, size_t len, int *error) {
	if (c < 0x80) {
		if (len < 1) {
			*error = UTF8_ERROR_BYTE_OVERFLOW;
		}
		else {
			*(b++) = c;
		}
	}
	else if (c < 0x800) {
		if (len < 2) {
			*error = UTF8_ERROR_BYTE_OVERFLOW;
		}
		else {
			*(b++) = (c & 0x7ff) >> 6 | 0xc0;
			*(b++) = (c & 0x3f) | 0x80;
		}
	}
	else if (c < 0x10000) {
		if (len < 3) {
			*error = UTF8_ERROR_BYTE_OVERFLOW;
		}
		else {
			*(b++) = (c & 0xffff) >> 12 | 0xe0;
			*(b++) = (c & 0xfff) >> 6 | 0x80;
			*(b++) = (c & 0x3f) | 0x80;
		}
	}
	else {
		if (len < 4) {
			*error = UTF8_ERROR_BYTE_OVERFLOW;
		}
		else {
			*(b++) = (c & 0x1fffff) >> 18 | 0xf0;
			*(b++) = (c & 0x3ffff) >> 12 | 0x80;
			*(b++) = (c & 0xfff) >> 6 | 0x80;
			*(b++) = (c & 0x3f) | 0x80;
		}
	}
	return b;
}

byte* utf8_encode_array(ucp* ca, size_t cc, byte* ba, size_t bc, int* error) {
	ucp* cend = ca + cc;
	byte* bend = ba + bc;
	*error = 0;
	for (; ca < cend; ca++) {
		ba = utf8_encode_char(*ca, ba, bend - ba, error);
		if (*error) {
			return ba;
		}
	}
	if (ca < cend) {
		// We did not encode all characters
		*error = UTF8_ERROR_BYTE_OVERFLOW;
	} else {
		*error = 0;
	}
	return ba;
}
