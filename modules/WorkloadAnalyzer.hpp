#pragma once

#include <vector>
#include <map>
#include <string>
#include <cmath>
#include "Models.hpp"

// -----------------------------------------------------------------------------
// WorkloadAnalyzer Class
// Demonstrates: STL Map Aggregation, Capacity Calculations, Warning Logic
// -----------------------------------------------------------------------------
class WorkloadAnalyzer {
public:
    static nlohmann::json analyze(const std::vector<Task>& tasks, double dailyCapacityHours) {
        std::map<std::string, double> dailyHours;
        std::map<std::string, int> dailyTaskCount;
        double totalPendingHours = 0.0;
        int overdueTasksCount = 0;
        double overdueHours = 0.0;

        std::string today = DateUtils::todayDate();
        std::vector<std::string> overloadedDays;
        std::vector<std::string> multiDeadlineDays;

        for (const auto& task : tasks) {
            if (task.getStatus() == Task::Status::Completed) {
                continue;
            }

            totalPendingHours += task.getEstimatedHours();

            if (task.isOverdue()) {
                overdueTasksCount++;
                overdueHours += task.getEstimatedHours();
            }

            std::string dateKey = task.getDeadline().empty() ? "Unscheduled" : task.getDeadline().substr(0, 10);
            dailyHours[dateKey] += task.getEstimatedHours();
            dailyTaskCount[dateKey]++;
        }

        nlohmann::json dailyBreakdown = nlohmann::json::array();
        for (const auto& [date, hours] : dailyHours) {
            if (date == "Unscheduled") continue;
            bool isOverloaded = (hours > dailyCapacityHours);
            if (isOverloaded) {
                overloadedDays.push_back(date);
            }
            if (dailyTaskCount[date] >= 3) {
                multiDeadlineDays.push_back(date);
            }
            dailyBreakdown.push_back({
                {"date", date},
                {"hours", std::round(hours * 10.0) / 10.0},
                {"taskCount", dailyTaskCount[date]},
                {"isOverloaded", isOverloaded},
                {"capacity", dailyCapacityHours}
            });
        }

        return nlohmann::json{
            {"dailyCapacityHours", dailyCapacityHours},
            {"totalPendingHours", std::round(totalPendingHours * 10.0) / 10.0},
            {"overdueTasksCount", overdueTasksCount},
            {"overdueHours", std::round(overdueHours * 10.0) / 10.0},
            {"overloadedDaysCount", overloadedDays.size()},
            {"overloadedDays", overloadedDays},
            {"multiDeadlineDays", multiDeadlineDays},
            {"dailyBreakdown", dailyBreakdown}
        };
    }
};
