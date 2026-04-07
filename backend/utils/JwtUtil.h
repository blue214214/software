#pragma once
#include <string>
#include <chrono>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <vector>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/rand.h>
#include <json/json.h>

// Minimal JWT implementation using OpenSSL HMAC-SHA256
// Avoids jwt-cpp traits compatibility issues
namespace utils {

class JwtUtil {
public:
    explicit JwtUtil(const std::string& secret,
                     int accessExpSec  = 1800,
                     int refreshExpSec = 604800)
        : secret_(secret), accessExpSec_(accessExpSec), refreshExpSec_(refreshExpSec) {}

    std::string generateAccessToken(long long userId,
                                    const std::string& studentId,
                                    const std::string& role) const {
        return makeToken(userId, studentId, role, "access", accessExpSec_);
    }

    std::string generateRefreshToken(long long userId,
                                     const std::string& studentId) const {
        return makeToken(userId, studentId, "", "refresh", refreshExpSec_);
    }

    // Returns parsed payload; throws std::runtime_error on failure
    Json::Value parse(const std::string& token) const {
        auto parts = split(token, '.');
        if (parts.size() != 3) throw std::runtime_error("invalid token format");

        // Verify signature
        std::string sigInput = parts[0] + "." + parts[1];
        std::string expectedSig = hmacSha256Base64Url(sigInput, secret_);
        if (expectedSig != parts[2]) throw std::runtime_error("invalid signature");

        // Decode payload
        std::string payloadJson = base64UrlDecode(parts[1]);
        Json::Value payload;
        Json::Reader reader;
        if (!reader.parse(payloadJson, payload)) throw std::runtime_error("invalid payload");

        // Check expiry
        long long now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        if (payload["exp"].asInt64() < now)
            throw std::runtime_error("token expired");

        return payload;
    }

    // Parse without verifying expiry (for blacklist operations on logout)
    Json::Value parseUnverified(const std::string& token) const {
        auto parts = split(token, '.');
        if (parts.size() != 3) throw std::runtime_error("invalid token format");
        std::string payloadJson = base64UrlDecode(parts[1]);
        Json::Value payload;
        Json::Reader reader;
        if (!reader.parse(payloadJson, payload)) throw std::runtime_error("invalid payload");
        return payload;
    }

    long long getRemainingSeconds(const std::string& token) const {
        auto payload = parseUnverified(token);
        long long exp = payload["exp"].asInt64();
        long long now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return exp - now;
    }

private:
    std::string secret_;
    int accessExpSec_;
    int refreshExpSec_;

    std::string makeToken(long long userId, const std::string& sub,
                          const std::string& role, const std::string& type,
                          int expSec) const {
        long long now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        // Header
        std::string header = base64UrlEncode(R"({"alg":"HS256","typ":"JWT"})");

        // Payload
        Json::Value p;
        p["iss"] = "bjtu-canteen";
        p["sub"] = sub;
        p["jti"] = generateJti();
        p["iat"] = (Json::Int64)now;
        p["exp"] = (Json::Int64)(now + expSec);
        p["userId"] = std::to_string(userId);
        p["type"] = type;
        if (!role.empty()) p["role"] = role;

        Json::StreamWriterBuilder swb;
        swb["indentation"] = "";
        std::string payloadJson = Json::writeString(swb, p);
        // Remove trailing newline if any
        while (!payloadJson.empty() && (payloadJson.back() == '\n' || payloadJson.back() == '\r'))
            payloadJson.pop_back();
        std::string payloadStr = base64UrlEncode(payloadJson);

        std::string sigInput = header + "." + payloadStr;
        std::string sig = hmacSha256Base64Url(sigInput, secret_);

        return sigInput + "." + sig;
    }

    static std::string hmacSha256Base64Url(const std::string& data, const std::string& key) {
        unsigned char digest[EVP_MAX_MD_SIZE];
        unsigned int digestLen = 0;
        HMAC(EVP_sha256(),
             key.c_str(), (int)key.size(),
             (const unsigned char*)data.c_str(), data.size(),
             digest, &digestLen);
        return base64UrlEncode(std::string((char*)digest, digestLen));
    }

    static std::string base64UrlEncode(const std::string& input) {
        BIO* b64 = BIO_new(BIO_f_base64());
        BIO* mem = BIO_new(BIO_s_mem());
        b64 = BIO_push(b64, mem);
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
        BIO_write(b64, input.data(), (int)input.size());
        BIO_flush(b64);
        BUF_MEM* bptr;
        BIO_get_mem_ptr(b64, &bptr);
        std::string result(bptr->data, bptr->length);
        BIO_free_all(b64);
        // URL-safe: replace + with -, / with _, remove =
        for (char& c : result) {
            if (c == '+') c = '-';
            else if (c == '/') c = '_';
        }
        while (!result.empty() && result.back() == '=') result.pop_back();
        return result;
    }

    static std::string base64UrlDecode(std::string input) {
        // Restore padding and standard chars
        for (char& c : input) {
            if (c == '-') c = '+';
            else if (c == '_') c = '/';
        }
        while (input.size() % 4 != 0) input += '=';

        BIO* b64 = BIO_new(BIO_f_base64());
        BIO* mem = BIO_new_mem_buf(input.data(), (int)input.size());
        b64 = BIO_push(b64, mem);
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
        std::string result(input.size(), '\0');
        int len = BIO_read(b64, &result[0], (int)input.size());
        BIO_free_all(b64);
        result.resize(len > 0 ? len : 0);
        return result;
    }

    static std::vector<std::string> split(const std::string& s, char delim) {
        std::vector<std::string> parts;
        std::stringstream ss(s);
        std::string part;
        while (std::getline(ss, part, delim)) parts.push_back(part);
        return parts;
    }

    static std::string generateJti() {
        unsigned char buf[16];
        RAND_bytes(buf, sizeof(buf));
        std::ostringstream oss;
        for (int i = 0; i < 16; ++i)
            oss << std::hex << std::setw(2) << std::setfill('0') << (int)buf[i];
        return oss.str();
    }
};

} // namespace utils
