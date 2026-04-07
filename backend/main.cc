#include <drogon/drogon.h>
#include <cstdlib>
#include <string>

static std::string env(const char* key, const char* fallback) {
    const char* v = std::getenv(key);
    return v ? std::string(v) : std::string(fallback);
}

int main() {
    // Override config with environment variables when present
    auto& app = drogon::app();
    app.loadConfigFile("config.json");

    // DB
    // Drogon reads db_clients from config.json; we patch via custom_config at runtime
    // and rely on the env-substituted config.json written by the entrypoint script.
    // For Railway, we use an entrypoint that writes config.json from env vars.

    app.run();
    return 0;
}
