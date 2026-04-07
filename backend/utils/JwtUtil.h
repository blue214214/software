#pragma once
#include <string>
#include <chrono>
#include <stdexcept>
#include <jwt-cpp/jwt.h>

namespace utils {

class JwtUtil {
public:
    explicit JwtUtil(const std::string& secret,
                     int accessExpSec  = 1800,
                     int refreshExpSec = 604800)
        : secret_(secret), accessExpSec_(accessExpSec), refreshExpSec_(refreshExpSec) {}

    // Generate access token (type=access)
    std::string generateAccessToken(long long userId,
                                    const std::string& studentId,
                                    const std::string& role) const {
        auto now = std::chrono::system_clock::now();
        return jwt::create()
            .set_issuer("bjtu-canteen")
            .set_subject(studentId)
            .set_id(generateJti())
            .set_issued_at(now)
            .set_expires_at(now + std::chrono::seconds(accessExpSec_))
            .set_payload_claim("userId", jwt::claim(std::to_string(userId)))
            .set_payload_claim("role",   jwt::claim(role))
            .set_payload_claim("type",   jwt::claim(std::string("access")))
            .sign(jwt::algorithm::hs256{secret_});
    }

    // Generate refresh token (type=refresh)
    std::string generateRefreshToken(long long userId,
                                     const std::string& studentId) const {
        auto now = std::chrono::system_clock::now();
        return jwt::create()
            .set_issuer("bjtu-canteen")
            .set_subject(studentId)
            .set_id(generateJti())
            .set_issued_at(now)
            .set_expires_at(now + std::chrono::seconds(refreshExpSec_))
            .set_payload_claim("userId", jwt::claim(std::to_string(userId)))
            .set_payload_claim("type",   jwt::claim(std::string("refresh")))
            .sign(jwt::algorithm::hs256{secret_});
    }

    // Parse and verify token; throws std::runtime_error on failure
    jwt::decoded_jwt<jwt::traits::kazuho_picojson> parse(const std::string& token) const {
        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{secret_})
            .with_issuer("bjtu-canteen");
        auto decoded = jwt::decode(token);
        verifier.verify(decoded);
        return decoded;
    }

    std::string getJti(const std::string& token) const {
        return jwt::decode(token).get_id();
    }

    std::string getType(const std::string& token) const {
        return jwt::decode(token).get_payload_claim("type").as_string();
    }

    long long getUserId(const std::string& token) const {
        return std::stoll(jwt::decode(token).get_payload_claim("userId").as_string());
    }

    std::string getRole(const std::string& token) const {
        return jwt::decode(token).get_payload_claim("role").as_string();
    }

    // Remaining TTL in seconds (may be negative if expired)
    long long getRemainingSeconds(const std::string& token) const {
        auto decoded = jwt::decode(token);
        auto exp = decoded.get_expires_at();
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(exp - now).count();
    }

private:
    std::string secret_;
    int accessExpSec_;
    int refreshExpSec_;

    static std::string generateJti() {
        return drogon::utils::getUuid();
    }
};

} // namespace utils
