#pragma once

#include <cstddef>
#include <cstdint>

namespace Pokerogue3DS {

// Streaming SHA-256 for content and save integrity. A digest detects corruption;
// it does not authenticate the publisher of a downloaded package.
class IntegritySha256 {
public:
    IntegritySha256() { reset(); }

    void reset() {
        const uint32_t initial[8] = {
            0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
            0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
        };
        for (unsigned i = 0; i < 8; ++i) m_state[i] = initial[i];
        m_bytes = 0;
        m_bufferSize = 0;
    }

    void update(const void* bytes, size_t size) {
        const auto* source = static_cast<const uint8_t*>(bytes);
        m_bytes += size;
        while (size) {
            m_buffer[m_bufferSize++] = *source++;
            --size;
            if (m_bufferSize == sizeof(m_buffer)) {
                transform(m_buffer);
                m_bufferSize = 0;
            }
        }
    }

    // Finalization uses a copy so callers can inspect the digest and keep writing.
    void finish(uint8_t output[32]) const {
        IntegritySha256 copy = *this;
        const uint64_t bits = m_bytes * 8;
        const uint8_t marker = 0x80;
        copy.update(&marker, 1);
        const uint8_t zero = 0;
        while (copy.m_bufferSize != 56) copy.update(&zero, 1);
        uint8_t length[8];
        for (unsigned i = 0; i < 8; ++i) length[i] = static_cast<uint8_t>(bits >> (56 - i * 8));
        copy.update(length, sizeof(length));
        for (unsigned i = 0; i < 8; ++i) {
            for (unsigned j = 0; j < 4; ++j) {
                output[i * 4 + j] = static_cast<uint8_t>(copy.m_state[i] >> (24 - j * 8));
            }
        }
    }

    static void toHex(const uint8_t digest[32], char output[65]) {
        static constexpr char digits[] = "0123456789abcdef";
        for (unsigned i = 0; i < 32; ++i) {
            output[i * 2] = digits[digest[i] >> 4];
            output[i * 2 + 1] = digits[digest[i] & 15];
        }
        output[64] = '\0';
    }

    static void hashHex(const void* bytes, size_t size, char output[65]) {
        IntegritySha256 hash;
        hash.update(bytes, size);
        uint8_t digest[32];
        hash.finish(digest);
        toHex(digest, output);
    }

private:
    uint32_t m_state[8]{};
    uint8_t m_buffer[64]{};
    size_t m_bufferSize = 0;
    uint64_t m_bytes = 0;

    static uint32_t rotate(uint32_t value, unsigned bits) {
        return (value >> bits) | (value << (32 - bits));
    }

    void transform(const uint8_t block[64]) {
        static constexpr uint32_t constants[64] = {
            0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
            0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
            0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
            0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
            0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
            0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
            0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
            0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
        };
        uint32_t words[64];
        for (unsigned i = 0; i < 16; ++i) {
            words[i] = (static_cast<uint32_t>(block[i * 4]) << 24)
                | (static_cast<uint32_t>(block[i * 4 + 1]) << 16)
                | (static_cast<uint32_t>(block[i * 4 + 2]) << 8) | block[i * 4 + 3];
        }
        for (unsigned i = 16; i < 64; ++i) {
            const uint32_t first = rotate(words[i - 15], 7) ^ rotate(words[i - 15], 18) ^ (words[i - 15] >> 3);
            const uint32_t second = rotate(words[i - 2], 17) ^ rotate(words[i - 2], 19) ^ (words[i - 2] >> 10);
            words[i] = words[i - 16] + first + words[i - 7] + second;
        }
        uint32_t a = m_state[0], b = m_state[1], c = m_state[2], d = m_state[3];
        uint32_t e = m_state[4], f = m_state[5], g = m_state[6], h = m_state[7];
        for (unsigned i = 0; i < 64; ++i) {
            const uint32_t sum1 = rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25);
            const uint32_t choose = (e & f) ^ (~e & g);
            const uint32_t temp1 = h + sum1 + choose + constants[i] + words[i];
            const uint32_t sum0 = rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22);
            const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            h = g; g = f; f = e; e = d + temp1;
            d = c; c = b; b = a; a = temp1 + sum0 + majority;
        }
        m_state[0] += a; m_state[1] += b; m_state[2] += c; m_state[3] += d;
        m_state[4] += e; m_state[5] += f; m_state[6] += g; m_state[7] += h;
    }
};

} // namespace Pokerogue3DS
