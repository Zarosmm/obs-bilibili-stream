#pragma once
#include <cstddef>
#include <cstdint>

namespace Crypto {
struct MD5Context {
	uint32_t state[4];
	uint64_t count[2];
	unsigned char buffer[64];
};

void md5Init(MD5Context* context);
void md5Update(MD5Context* context, const unsigned char* input, size_t inputLen);
void md5Final(unsigned char digest[16], MD5Context* context);
} // namespace Crypto
