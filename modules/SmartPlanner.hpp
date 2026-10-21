#pragma once

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <sstream>
#include <cmath>
#include "Models.hpp"

// -----------------------------------------------------------------------------
// Planner Data Structures
// -----------------------------------------------------------------------------
struct PlannerScore {
    double totalScore{0.0};
    double priorityScore{0.0};
    double deadlineScore{0.0};
    double effortScore{0.0};
    double statusScore{0.0};
    std::string rationale;
};

struct PlannerRecommendation {
    bool hasRecommendation{false};
    Task task;
    double score{0.0};
    std::string rationale;
    std::map<std::string, double> factors;

    nlohmann::json toJson() const {
        if (!hasRecommendation) {
            return nlohmann::json{
                {"hasRecommendation", false},
                {"message", "All current tasks are completed! Great job."}
            };
        }
        return nlohmann::json{
            {"hasRecommendation", true},
            {"task", task.toJson()},
            {"score", std::round(score * 10.0) / 10.0},
            {"rationale", rationale},
            {"factors", factors}
        };
    }
};

// -----------------------------------------------------------------------------
// SmartPlanner Class
// Demonstrates: Algorithm Encapsulation, Business Logic Abstraction,
// Deterministic Multi-Factor Scoring
// -----------------------------------------------------------------------------
class SmartPlanner {
public:
    static PlannerRecommendation recommendNextTask(const std::vector<Task>& tasks) {
        PlannerRecommendation rec;
        rec.hasRecommendation = false;

        const Task* bestTask = nullptr;
        double highestScore = -1.0;
        PlannerScore bestBreakdown;

        std::string today = DateUtils::todayDate();

        for (const auto& task : tasks) {
            // Ignore completed tasks
            if (task.getStatus() == Task::Status::Completed) {
                continue;
            }

            PlannerScore current;
            std::ostringstream reason;

            // 1. Priority Weight Component
            switch (task.getPriority()) {
                case Task::Priority::Critical: current.priorityScore = 90.0; break;
                case Task::Priority::High: current.priorityScore = 60.0; break;
                case Task::Priority::Medium: current.priorityScore = 30.0; break;
                case Task::Priority::Low: current.priorityScore = 10.0; break;
            }

            // 2. Deadline & Urgency Component
            if (!task.getDeadline().empty()) {
                double daysRemaining = DateUtils::daysBetween(today, task.getDeadline());
                if (daysRemaining < 0) {
                    // Task is overdue - top priority attention needed
                    double daysOver = std::abs(daysRemaining);
                    current.deadlineScore = 150.0 + std::min(daysOver * 15.0, 60.0);
                    reason << "Overdue by " << static_cast<int>(std::ceil(daysOver)) << " day(s)";
                } else if (daysRemaining < 1.0) {
                    current.deadlineScore = 100.0;
                    reason << "Due today";
                } else if (daysRemaining <= 2.0) {
                    current.deadlineScore = 70.0;
                    reason << "Due within 48 hours (" << static_cast<int>(std::ceil(daysRemaining)) << " days left)";
                } else if (daysRemaining <= 7.0) {
                    current.deadlineScore = 40.0;
                    reason << "Approaching deadline (" << static_cast<int>(std::ceil(daysRemaining)) << " days left)";
                } else {
                    current.deadlineScore = 15.0;
                    reason << "Scheduled for " << task.getDeadline();
                }
            } else {
                current.deadlineScore = 5.0;
                reason << "Open timeframe (no deadline)";
            }

            // 3. Status Momentum Component
            if (task.getStatus() == Task::Status::InProgress) {
                current.statusScore = 25.0;
                reason << "; already in progress";
            }

            // 4. Effort & Velocity Optimization
            // Rewarding short tasks provides completion momentum
            if (task.getEstimatedHours() <= 1.0) {
                current.effortScore = 20.0;
                reason << "; quick win (under 1 hr)";
            } else if (task.getEstimatedHours() <= 2.5) {
                current.effortScore = 12.0;
            } else {
                current.effortScore = 5.0;
            }

            current.totalScore = current.priorityScore + current.deadlineScore +
                                 current.statusScore + current.effortScore;

            if (bestTask == nullptr || current.totalScore > highestScore) {
                highestScore = current.totalScore;
                bestTask = &task;
                current.rationale = reason.str() + " with " +
                                    Task::priorityToString(task.getPriority()) +
                                    " priority (" + std::to_string(task.getEstimatedHours()).substr(0, 3) + " hrs est).";
                bestBreakdown = current;
            }
        }

        if (bestTask != nullptr) {
            rec.hasRecommendation = true;
            rec.task = *bestTask;
            rec.score = highestScore;
            rec.rationale = bestBreakdown.rationale;
            rec.factors["priorityWeight"] = bestBreakdown.priorityScore;
            rec.factors["deadlineUrgency"] = bestBreakdown.deadlineScore;
            rec.factors["statusMomentum"] = bestBreakdown.statusScore;
            rec.factors["effortOptimization"] = bestBreakdown.effortScore;
        }

        return rec;
    }
};
