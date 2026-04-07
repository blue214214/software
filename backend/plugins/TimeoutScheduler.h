#pragma once
#include <drogon/plugins/Plugin.h>

// Drogon plugin: runs every 60s to expire PENDING reservations (15min)
// and PICKED_UP reservations (60min dining timeout)
class TimeoutScheduler : public drogon::Plugin<TimeoutScheduler> {
public:
    void initAndStart(const Json::Value& config) override;
    void shutdown() override {}
private:
    void runExpiryCheck();
};
