#pragma once

#include <vector>
#include <string>
#include <map>
#include <sstream>
#include <iomanip>
#include "Models.hpp"
#include "TaskManager.hpp"

struct RescheduleSuggestion {
    std::string taskId;
    std::string taskTitle;
    std::string currentDeadline;
    std::string suggestedDeadline;
    double estimatedHours{0.0};
    double projectedDayWorkload{0.0};
    std::string reason;

    nlohmann::json toJson() const {
        return nlohmann::json{
            {"id", taskId},
            {"taskId", taskId},
            {"taskTitle", taskTitle},
            {"currentDeadline", currentDeadline},
            {"suggestedDeadline", suggestedDeadline},
            {"estimatedHours", estimatedHours},
            {"projectedDayWorkload", projectedDayWorkload},
            {"reason", reason}
        };
    }
};

// -----------------------------------------------------------------------------
// Rescheduler Class
// Demonstrates: Algorithm Encapsulation, Safe Mutable Operation Delegations
// -----------------------------------------------------------------------------
class Rescheduler {
public:
    static std::vector<RescheduleSuggestion> generateSuggestions(
        const std::vector<Task>& tasks, double dailyCapacityHours) {

        std::vector<RescheduleSuggestion> suggestions;
        std::string today = DateUtils::todayDate();

        // Calculate future load per day
        std::map<std::string, double> futureLoad;
        for (const auto& task : tasks) {
            if (task.getStatus() != Task::Status::Completed && !task.getDeadline().empty()) {
                std::string d = task.getDeadline().substr(0, 10);
                if (DateUtils::daysBetween(today, d) >= 0) {
                    futureLoad[d] += task.getEstimatedHours();
                }
            }
        }

        // Suggest new dates for overdue tasks
        for (const auto& task : tasks) {
            if (task.getStatus() != Task::Status::Completed && task.isOverdue()) {
                std::string targetDate = "";
                double projectedLoad = 0.0;

                // Look ahead 14 days for the first day with sufficient capacity
                for (int dayOffset = 1; dayOffset <= 14; ++dayOffset) {
                    std::string candidate = DateUtils::addDays(today, dayOffset);
                    double currentDayLoad = futureLoad[candidate];

                    if (currentDayLoad + task.getEstimatedHours() <= dailyCapacityHours) {
                        targetDate = candidate;
                        projectedLoad = currentDayLoad + task.getEstimatedHours();
                        futureLoad[candidate] += task.getEstimatedHours(); // reserve in simulation
                        break;
                    }
                }

                // If all days are near/at capacity, pick the lowest loaded day
                if (targetDate.empty()) {
                    std::string minDay = DateUtils::addDays(today, 1);
                    double minLoad = futureLoad[minDay];
                    for (int dayOffset = 2; dayOffset <= 14; ++dayOffset) {
                        std::string candidate = DateUtils::addDays(today, dayOffset);
                        if (futureLoad[candidate] < minLoad) {
                            minLoad = futureLoad[candidate];
                            minDay = candidate;
                        }
                    }
                    targetDate = minDay;
                    projectedLoad = minLoad + task.getEstimatedHours();
                    futureLoad[targetDate] += task.getEstimatedHours();
                }

                std::ostringstream reason;
                reason << "Proposed " << targetDate << " because projected workload will be "
                       << std::fixed << std::setprecision(1) << projectedLoad
                       << "h within your " << dailyCapacityHours << "h capacity.";

                RescheduleSuggestion s;
                s.taskId = task.getId();
                s.taskTitle = task.getTitle();
                s.currentDeadline = task.getDeadline();
                s.suggestedDeadline = targetDate;
                s.estimatedHours = task.getEstimatedHours();
                s.projectedDayWorkload = projectedLoad;
                s.reason = reason.str();

                suggestions.push_back(s);
            }
        }

        return suggestions;
    }

    // Applies suggestion ONLY when explicitly accepted by user
    static bool applySuggestion(TaskManager& taskManager, const std::string& taskId,
                                const std::string& newDeadline) {
        taskManager.updateDeadline(taskId, newDeadline);
        return true;
    }
};
