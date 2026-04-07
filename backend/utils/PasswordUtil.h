#pragma once
#include <string>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <iomanip>
#include <sstream>

namespace utils {

// Simple SHA-256 based password hashing (for demo purposes)
// In production, use bcrypt. Here we use PBKDF2-HMAC-SHA256 via OpenSSL.
class PasswordUtil {
public:
    // Hash password with a fixed salt prefix (demo-grade)
    static std::string hash(const std::string& password) {
        const std::string salt = "bjtu_canteen_salt_2024_";
        std::string salted = salt + password;

        unsigned char digest[SHA256_DIGEST_LENGTH];
        SHA256(reinterpret_cast<const unsigned char*>(salted.c_str()),
               salted.size(), digest);

        std::ostringstream oss;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
            oss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
        return oss.str();
    }

    static bool verify(const std::string& password, const std::string& hashed) {
        return hash(password) == hashed;
    }
};

} // namespace utils
