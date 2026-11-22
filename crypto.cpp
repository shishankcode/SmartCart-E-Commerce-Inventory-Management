#include "crypto.h"

#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

const std::uint32_t K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
    0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
    0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

inline std::uint32_t rotateRight(std::uint32_t value, int bits) {
    return (value >> bits) | (value << (32 - bits));
}

}  // namespace

std::string sha256(const std::string& data) {
    std::uint32_t h[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                          0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};

    std::string message = data;
    std::uint64_t bitLength = static_cast<std::uint64_t>(data.size()) * 8u;
    message += static_cast<char>(0x80);
    while (message.size() % 64 != 56) {
        message += static_cast<char>(0x00);
    }
    for (int i = 7; i >= 0; --i) {
        message += static_cast<char>((bitLength >> (i * 8)) & 0xFFu);
    }

    for (std::size_t offset = 0; offset < message.size(); offset += 64) {
        std::uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            const unsigned char* p =
                reinterpret_cast<const unsigned char*>(message.data() + offset + i * 4);
            w[i] = (static_cast<std::uint32_t>(p[0]) << 24) |
                   (static_cast<std::uint32_t>(p[1]) << 16) |
                   (static_cast<std::uint32_t>(p[2]) << 8) |
                   static_cast<std::uint32_t>(p[3]);
        }
        for (int i = 16; i < 64; ++i) {
            std::uint32_t s0 = rotateRight(w[i - 15], 7) ^ rotateRight(w[i - 15], 18) ^
                               (w[i - 15] >> 3);
            std::uint32_t s1 = rotateRight(w[i - 2], 17) ^ rotateRight(w[i - 2], 19) ^
                               (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        std::uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];

        for (int i = 0; i < 64; ++i) {
            std::uint32_t s1 = rotateRight(e, 6) ^ rotateRight(e, 11) ^ rotateRight(e, 25);
            std::uint32_t ch = (e & f) ^ (~e & g);
            std::uint32_t temp1 = hh + s1 + ch + K[i] + w[i];
            std::uint32_t s0 = rotateRight(a, 2) ^ rotateRight(a, 13) ^ rotateRight(a, 22);
            std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            std::uint32_t temp2 = s0 + maj;
            hh = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
        h[5] += f;
        h[6] += g;
        h[7] += hh;
    }

    std::string digest(32, '\0');
    for (int i = 0; i < 8; ++i) {
        digest[i * 4 + 0] = static_cast<char>((h[i] >> 24) & 0xFFu);
        digest[i * 4 + 1] = static_cast<char>((h[i] >> 16) & 0xFFu);
        digest[i * 4 + 2] = static_cast<char>((h[i] >> 8) & 0xFFu);
        digest[i * 4 + 3] = static_cast<char>(h[i] & 0xFFu);
    }
    return digest;
}

std::string hmacSha256(const std::string& key, const std::string& data) {
    const std::size_t blockSize = 64;
    std::string blockKey = key.size() > blockSize ? sha256(key) : key;
    blockKey.resize(blockSize, '\0');

    std::string innerPad(blockSize, '\0');
    std::string outerPad(blockSize, '\0');
    for (std::size_t i = 0; i < blockSize; ++i) {
        innerPad[i] = static_cast<char>(blockKey[i] ^ 0x36);
        outerPad[i] = static_cast<char>(blockKey[i] ^ 0x5c);
    }
    return sha256(outerPad + sha256(innerPad + data));
}

std::string pbkdf2Sha256(const std::string& password,
                         const std::string& salt,
                         int iterations,
                         int keyLen) {
    if (iterations < 1 || keyLen < 1) {
        throw std::invalid_argument("pbkdf2: invalid parameters");
    }
    std::string output;
    std::uint32_t blockIndex = 1;
    while (static_cast<int>(output.size()) < keyLen) {
        std::string blockSalt = salt;
        blockSalt += static_cast<char>((blockIndex >> 24) & 0xFFu);
        blockSalt += static_cast<char>((blockIndex >> 16) & 0xFFu);
        blockSalt += static_cast<char>((blockIndex >> 8) & 0xFFu);
        blockSalt += static_cast<char>(blockIndex & 0xFFu);

        std::string u = hmacSha256(password, blockSalt);
        std::string t = u;
        for (int round = 1; round < iterations; ++round) {
            u = hmacSha256(password, u);
            for (std::size_t i = 0; i < t.size(); ++i) {
                t[i] = static_cast<char>(t[i] ^ u[i]);
            }
        }
        output += t;
        ++blockIndex;
    }
    output.resize(static_cast<std::size_t>(keyLen));
    return output;
}

std::string hexEncode(const std::string& bytes) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (unsigned char c : bytes) {
        out += digits[c >> 4];
        out += digits[c & 0x0F];
    }
    return out;
}

std::string hexDecode(const std::string& hex) {
    auto valueOf = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    if (hex.size() % 2 != 0) {
        throw std::invalid_argument("hexDecode: odd length");
    }
    std::string out;
    out.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        int high = valueOf(hex[i]);
        int low = valueOf(hex[i + 1]);
        if (high < 0 || low < 0) {
            throw std::invalid_argument("hexDecode: invalid character");
        }
        out += static_cast<char>((high << 4) | low);
    }
    return out;
}

std::string randomHex(std::size_t bytes) {
    std::random_device device;
    std::mt19937_64 generator((static_cast<std::uint64_t>(device()) << 32) ^ device());
    std::string raw;
    raw.reserve(bytes);
    for (std::size_t i = 0; i < bytes; ++i) {
        raw += static_cast<char>(generator() & 0xFFu);
    }
    return hexEncode(raw);
}

bool constantTimeEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) {
        return false;
    }
    unsigned char diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    }
    return diff == 0;
}
