#pragma once
#include <drogon/plugins/Plugin.h>

// Runs every Sunday at 23:59 to generate weekly reports for all users
class WeeklyReportScheduler : public drogon::Plugin<WeeklyReportScheduler> {
public:
    void initAndStart(const Json::Value& config) override;
    void shutdown() override {}
private:
    void generateReports();
};
