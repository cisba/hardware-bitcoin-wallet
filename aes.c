// ***********************************************************************
// aes.c
// ***********************************************************************
//
// A byte-oriented AES (Rijndael) implementation. The emphasis is on having
// small code size. As a result, performance (time taken per byte encrypted
// or decrypted) may not be very good.
// This implementation is for 128-bit keys (10 rounds).
//
// This is based on "aestable.c", by Karl Malbrain (malbrain@yahoo.com).
// Significant changes from original:
// - Reduced the number of lookup tables
// - Rolled up loops in [Inv]ShiftRows() and [Inv]MixSubColumns()
// - Combined ShiftRows() and InvShiftRows() into one function
//
// This file is licensed as described by the file LICENCE.

// Defining this will facilitate testing
//#define TEST

#ifdef TEST
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <string.h>
#endif // #ifdef TEST

#include "common.h"
#include "aes.h"

// Forward s-box
static const uint8_t sbox[256] PROGMEM = {
0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16};

// Inverse s-box
static const uint8_t inv_sbox[256] PROGMEM = {
0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d};

// All the xTimes?InGF() functions implement modular multiplication of their
// argument by some constant under the field GF(2 ^ 8) with the reducing
// polynomial x ^ 8 + x ^ 4 + x ^ 3 + x + 1.

static uint8_t xTimes2InGF(uint8_t x)
{
	// ((unsigned int)(-(int)(x >> 7)) & 0x1b) is equivalent to
	// (x & 0x80 ? 0x1b : 0) but is more timing attack resistant.
	return (uint8_t)(((unsigned int)(-(int)(x >> 7)) & 0x1b) ^ (x + x));
}

static uint8_t xTimes3InGF(uint8_t x)
{
	return (uint8_t)(xTimes2InGF(x) ^ x);
}

static uint8_t xTimes4InGF(uint8_t x)
{
	return xTimes2InGF(xTimes2InGF(x));
}

static uint8_t xTimes8InGF(uint8_t x)
{
	return xTimes2InGF(xTimes4InGF(x));
}

static uint8_t xTimes9InGF(uint8_t x)
{
	return (uint8_t)(xTimes8InGF(x) ^ x);
}

static uint8_t xTimesBInGF(uint8_t x)
{
	return (uint8_t)(xTimes9InGF(x) ^ xTimes2InGF(x));
}

static uint8_t xTimesDInGF(uint8_t x)
{
	// Note that x * 13 is not the same as x * 11 + x * 2 under GF(2 ^ 8)
	return (uint8_t)(xTimes9InGF(x) ^ xTimes4InGF(x));
}

static uint8_t xTimesEInGF(uint8_t x)
{
	return (uint8_t)(xTimes8InGF(x) ^ xTimes4InGF(x) ^ xTimes2InGF(x));
}

static void copy16(uint8_t *out, uint8_t *in)
{
	uint8_t i;

	for (i = 0; i < 16; i++)
	{
		out[i] = in[i];
	}
}

// Exchanges (or restores) columns in each of 4 rows.
// To exchange, use 5 for shift_or_inv. To restore, use 13 for shift_or_inv.
// Why 5 and 13? 5 = 1 + 4 (mod 16), and 13 = 1 - 4 (mod 16). shift_or_inv
// controls how much rows are shifted.
// row0 - unchanged
// row1 - shifted left (or right) 1
// row2 - shifted left (or right) 2
// row3 - shifted left (or right) 3
static void shiftOrInvShiftRows(uint8_t *state, uint8_t shift_or_inv)
{
	uint8_t tmp[16];
	uint8_t i, j;
	uint8_t o1, o2;

	o1 = 0;
	o2 = 0;
	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 4; j++)
		{
			if (shift_or_inv == 5)
			{
				tmp[o1] = LOOKUP_BYTE(&(sbox[state[o2]]));
			}
			else
			{
				tmp[o1] = LOOKUP_BYTE(&(inv_sbox[state[o2]]));
			}
			o1 = (uint8_t)((o1 + 4) & 15);
			o2 = (uint8_t)((o2 + 4) & 15);
		}
		o1 = (uint8_t)((o1 + 1) & 15);
		o2 = (uint8_t)((o2 + shift_or_inv) & 15);
	}

	copy16(state, tmp);
}

// Recombine and mix each row in a column.
static void mixSubColumns(uint8_t *state)
{
	uint8_t tmp[16];
	uint8_t i;
	uint8_t o1, o2, o3, o4, otemp;

	o1 = 0;
	o2 = 5;
	o3 = 10;
	o4 = 15;
	for (i = 0; i < 16; i++)
	{
		tmp[i] = (uint8_t)(
			xTimes2InGF(LOOKUP_BYTE(&(sbox[state[o1]])))
			^ xTimes3InGF(LOOKUP_BYTE(&(sbox[state[o2]])))
			^ LOOKUP_BYTE(&(sbox[state[o3]]))
			^ LOOKUP_BYTE(&(sbox[state[o4]])));
		otemp = o1;
		o1 = o2;
		o2 = o3;
		o3 = o4;
		o4 = otemp;
		if ((i & 3) == 3)
		{
			o1 = (uint8_t)((o1 + 4) & 15);
			o2 = (uint8_t)((o2 + 4) & 15);
			o3 = (uint8_t)((o3 + 4) & 15);
			o4 = (uint8_t)((o4 + 4) & 15);
		}
	}

	copy16(state, tmp);
}

// Restore and un-mix each row in a column.
static void invMixSubColumns(uint8_t *state)
{
	uint8_t tmp[16];
	uint8_t i;
	uint8_t idx;
	uint8_t o1, o2, o3, o4, otemp;

	o1 = 0;
	o2 = 1;
	o3 = 2;
	o4 = 3;
	idx = 0;
	for (i = 0; i < 16; i++)
	{
		tmp[idx] = (uint8_t)(
			xTimesEInGF(state[o1])
			^ xTimesBInGF(state[o2])
			^ xTimesDInGF(state[o3])
			^ xTimes9InGF(state[o4]));
		idx = (uint8_t)((idx + 5) & 15);
		otemp = o1;
		o1 = o2;
		o2 = o3;
		o3 = o4;
		o4 = otemp;
		if ((i & 3) == 3)
		{
			o1 = (uint8_t)((o1 + 4) & 15);
			o2 = (uint8_t)((o2 + 4) & 15);
			o3 = (uint8_t)((o3 + 4) & 15);
			o4 = (uint8_t)((o4 + 4) & 15);
		}
	}

	for (i = 0; i < 16; i++)
	{
		state[i] = LOOKUP_BYTE(&(inv_sbox[tmp[i]]));
	}
}

// Encrypt/decrypt columns of the key.
static void addRoundKey(uint32_t *state, uint32_t *key)
{
	uint8_t idx;

	for (idx = 0; idx < 4; idx++)
	{
		state[idx] ^= key[idx];
	}
}

static const uint8_t r_con[11] = {
0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36};

// Expand the key by 16 bytes for each round.
// Input (key): 16 bytes
// Output (expanded_key): 176 bytes
void aesExpandKey(uint8_t *expanded_key, uint8_t *key)
{
	uint8_t tmp0, tmp1, tmp2, tmp3, tmp4;
	uint8_t idx;

	copy16(expanded_key, key);

	for (idx = 16; idx < 176; idx = (uint8_t)(idx + 4))
	{
		tmp0 = expanded_key[idx - 4];
		tmp1 = expanded_key[idx - 3];
		tmp2 = expanded_key[idx - 2];
		tmp3 = expanded_key[idx - 1];
		if ((idx & 15) == 0)
		{
			tmp4 = tmp3;
			tmp3 = LOOKUP_BYTE(&(sbox[tmp0]));
			tmp0 = (uint8_t)(LOOKUP_BYTE(&(sbox[tmp1])) ^ r_con[idx >> 4]);
			tmp1 = LOOKUP_BYTE(&(sbox[tmp2]));
			tmp2 = LOOKUP_BYTE(&(sbox[tmp4]));
		}

		expanded_key[idx + 0] = (uint8_t)(expanded_key[idx - 16 + 0] ^ tmp0);
		expanded_key[idx + 1] = (uint8_t)(expanded_key[idx - 16 + 1] ^ tmp1);
		expanded_key[idx + 2] = (uint8_t)(expanded_key[idx - 16 + 2] ^ tmp2);
		expanded_key[idx + 3] = (uint8_t)(expanded_key[idx - 16 + 3] ^ tmp3);
	}
}

// Encrypt one 128 bit block.
// in is the plaintext, a 16-byte array. The ciphertext will be placed in
// out, which should also be a 16-byte array. expanded_key should point to a
// 176-byte array containing the expanded key (see aesExpandKey()).
void aesEncrypt(uint8_t *out, uint8_t *in, uint8_t *expanded_key)
{
	uint8_t round;

	copy16(out, in);

	addRoundKey((uint32_t *)out, (uint32_t *)expanded_key);

	for (round = 1; round < 11; round++)
	{
		if (round < 10)
		{
			mixSubColumns(out);
		}
		else
		{
			shiftOrInvShiftRows(out, 5);
		}

		addRoundKey((uint32_t *)out, ((uint32_t *)expanded_key) + round * 4);
	}
}

// Decrypt one 128-bit block.
// in is the ciphertext, a 16-byte array. The plaintext will be placed in
// out, which should also be a 16-byte array. expanded_key should point to a
// 176-byte array containing the expanded key (see aesExpandKey()).
void aesDecrypt(uint8_t *out, uint8_t *in, uint8_t *expanded_key)
{
	uint8_t round;

	copy16(out, in);

	addRoundKey((uint32_t *)out, ((uint32_t *)expanded_key) + 40);
	shiftOrInvShiftRows(out, 13);

	for (round = 10; round--; )
	{
		addRoundKey((uint32_t *)out, ((uint32_t *)expanded_key) + round * 4);
		if (round != 0)
		{
			invMixSubColumns(out);
		}
	}
}

#ifdef TEST

static int succeeded;
static int failed;

static void skipWhiteSpace(FILE *f)
{
	int one_char;
	do
	{
		one_char = fgetc(f);
	} while ((one_char == ' ') || (one_char == '\t') || (one_char == '\n') || (one_char == '\r'));
	ungetc(one_char, f);
}

static void skipLine(FILE *f)
{
	int one_char;
	do
	{
		one_char = fgetc(f);
	} while (one_char != '\n');
}

static void print16(uint8_t *buffer)
{
	int i;
	for (i = 0; i < 16; i++)
	{
		printf("%02x", (int)buffer[i]);
	}
}

static void scanTestVectors(char *filename)
{
	FILE *test_vector_file;
	int test_number;
	int is_encrypt;
	int i;
	int j;
	int value;
	int seen_count;
	int test_failed;
	char buffer[16];
	uint8_t key[16];
	uint8_t plaintext[16];
	uint8_t ciphertext[16];
	uint8_t compare_text[16];
	uint8_t expanded_key[EXPANDED_KEY_SIZE];

	test_vector_file = fopen(filename, "r");
	if (test_vector_file == NULL)
	{
		printf("Could not open %s, please get it \
(\"AES Known Answer Test (KAT) Vectors\") \
from http://csrc.nist.gov/groups/STM/cavp/#01", filename);
		exit(1);
	}

	test_number = 1;
	for (i = 0; i < 9; i++)
	{
		skipLine(test_vector_file);
	}
	is_encrypt = 1;
	while (!feof(test_vector_file))
	{
		// Check for [DECRYPT]
		skipWhiteSpace(test_vector_file);
		seen_count = 0;
		while (!seen_count)
		{
			fgets(buffer, 6, test_vector_file);
			skipLine(test_vector_file);
			skipWhiteSpace(test_vector_file);
			if (!strcmp(buffer, "[DECR"))
			{
				is_encrypt = 0;
			}
			else if (!strcmp(buffer, "COUNT"))
			{
				seen_count = 1;
			}
			else
			{
				printf("Expected \"COUNT\" or \"[DECR\"\n");
				exit(1);
			}
		}

		// Get key
		fgets(buffer, 7, test_vector_file);
		if (strcmp(buffer, "KEY = "))
		{
			printf("Parse error; expected \"KEY = \"\n");
			exit(1);
		}
		for (i = 0; i < 16; i++)
		{
			fscanf(test_vector_file, "%02x", &value);
			key[i] = (uint8_t)value;
		}
		skipWhiteSpace(test_vector_file);
		// Get plaintext/ciphertext
		// For encryption tests, the order is: plaintext, then ciphertext.
		// For decryption tests, the order is: ciphertext, then plaintext.
		for (j = 0; j < 2; j++)
		{
			if (((is_encrypt) && (j == 0))
				|| ((!is_encrypt) && (j != 0)))
			{
				fgets(buffer, 13, test_vector_file);
				if (strcmp(buffer, "PLAINTEXT = "))
				{
					printf("Parse error; expected \"PLAINTEXT = \"\n");
					exit(1);
				}
				for (i = 0; i < 16; i++)
				{
					fscanf(test_vector_file, "%02x", &value);
					plaintext[i] = (uint8_t)value;
				}
			}
			else
			{
				fgets(buffer, 14, test_vector_file);
				if (strcmp(buffer, "CIPHERTEXT = "))
				{
					printf("Parse error; expected \"CIPHERTEXT = \"\n");
					exit(1);
				}
				for (i = 0; i < 16; i++)
				{
					fscanf(test_vector_file, "%02x", &value);
					ciphertext[i] = (uint8_t)value;
				}
			}
			skipWhiteSpace(test_vector_file);
		} // end for (j = 0; j < 2; j++)
		// Do encryption/decryption and compare
		aesExpandKey(expanded_key, key);
		test_failed = 0;
		if (is_encrypt)
		{
			aesEncrypt(compare_text, plaintext, expanded_key);
			if (memcmp(compare_text, ciphertext, 16))
			{
				test_failed = 1;
			}
		}
		else
		{
			aesDecrypt(compare_text, ciphertext, expanded_key);
			if (memcmp(compare_text, plaintext, 16))
			{
				test_failed = 1;
			}
		}
		if (!test_failed)
		{
			succeeded++;
		}
		else
		{
			printf("Test %d failed\n", test_number);
			printf("Key: ");
			print16(key);
			printf("\nPlaintext: ");
			print16(plaintext);
			printf("\nCiphertext: ");
			print16(ciphertext);
			printf("\n");
			failed++;
		}
		test_number++;
	}
	fclose(test_vector_file);
}

int main(void)
{
	succeeded = 0;
	failed = 0;
	scanTestVectors("ECBVarTxt128.rsp");
	scanTestVectors("ECBVarKey128.rsp");
	scanTestVectors("ECBKeySbox128.rsp");
	scanTestVectors("ECBGFSbox128.rsp");
	printf("Tests which succeeded: %d\n", succeeded);
	printf("Tests which failed: %d\n", failed);
	exit(0);
}

#endif // #ifdef TEST

