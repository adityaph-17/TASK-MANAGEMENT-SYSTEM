#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <algorithm>
#include <random>
#include "Models.hpp"
#include "Storage.hpp"
#include "ActivityLog.hpp"

struct TaskFilter {
    std::string searchQuery; // Matches title or description or tags
    std::string status;      // "Pending", "In Progress", "Completed", or empty/All
    std::string priority;    // "Low", "Medium", "High", "Critical", or empty/All
    std::string category;    // Category name or empty/All
    std::string projectId;   // Project ID or empty/All
    std::string sortBy;      // "deadline", "priority", "created", "title"
    bool sortAscending{true};
};

// -----------------------------------------------------------------------------
// TaskManager Class
// Demonstrates: Encapsulation, Aggregation (tasks_), Function Overloading,
// Exception Handling, Thread Safety
// -----------------------------------------------------------------------------
class TaskManager {
private:
    std::vector<Task> tasks_;
    IStorage* storage_{nullptr};
    ActivityLog* activityLog_{nullptr};
    mutable std::mutex mutex_;

    std::string generateId() const {
        static std::mt19937_64 rng(std::random_device{}());
        static std::uniform_int_distribution<uint64_t> dist(100000, 999999);
        return "tsk_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(dist(rng));
    }

    size_t findIndex(const std::string& id) const {
        for (size_t i = 0; i < tasks_.size(); ++i) {
            if (tasks_[i].getId() == id) return i;
        }
        throw NotFoundException("Task with ID '" + id + "' does not exist.");
    }

public:
    TaskManager() = default;

    TaskManager(IStorage* storage, ActivityLog* activityLog)
        : storage_(storage), activityLog_(activityLog) {
        if (storage_) {
            tasks_ = storage_->loadTasks();
        }
    }

    void setStorage(IStorage* storage, ActivityLog* activityLog) {
        std::lock_guard<std::mutex> lock(mutex_);
        storage_ = storage;
        activityLog_ = activityLog;
        if (storage_) {
            tasks_ = storage_->loadTasks();
        }
    }

    // Function Overloading: getTasks()
    std::vector<Task> getTasks() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tasks_;
    }

    // Function Overloading: getTasks(filter)
    std::vector<Task> getTasks(const TaskFilter& filter) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Task> result;

        auto toLower = [](std::string s) {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
            return s;
        };

        std::string query = toLower(filter.searchQuery);

        for (const auto& t : tasks_) {
            // Search filter
            if (!query.empty()) {
                bool matchTitle = toLower(t.getTitle()).find(query) != std::string::npos;
                bool matchDesc = toLower(t.getDescription()).find(query) != std::string::npos;
                bool matchTag = false;
                for (const auto& tag : t.getTags()) {
                    if (toLower(tag).find(query) != std::string::npos) {
                        matchTag = true;
                        break;
                    }
                }
                if (!matchTitle && !matchDesc && !matchTag) continue;
            }

            // Status filter
            if (!filter.status.empty() && filter.status != "All") {
                if (Task::statusToString(t.getStatus()) != filter.status) continue;
            }

            // Priority filter
            if (!filter.priority.empty() && filter.priority != "All") {
                if (Task::priorityToString(t.getPriority()) != filter.priority) continue;
            }

            // Category filter
            if (!filter.category.empty() && filter.category != "All") {
                if (t.getCategory() != filter.category) continue;
            }

            // Project filter
            if (!filter.projectId.empty() && filter.projectId != "All") {
                if (t.getProjectId() != filter.projectId) continue;
            }

            result.push_back(t);
        }

        // Sorting
        if (filter.sortBy == "deadline") {
            std::sort(result.begin(), result.end(), [asc = filter.sortAscending](const Task& a, const Task& b) {
                if (a.getDeadline().empty()) return !asc;
                if (b.getDeadline().empty()) return asc;
                return asc ? (a.getDeadline() < b.getDeadline()) : (a.getDeadline() > b.getDeadline());
            });
        } else if (filter.sortBy == "priority") {
            std::sort(result.begin(), result.end(), [asc = filter.sortAscending](const Task& a, const Task& b) {
                int wa = Task::priorityToWeight(a.getPriority());
                int wb = Task::priorityToWeight(b.getPriority());
                return asc ? (wa < wb) : (wa > wb);
            });
        } else if (filter.sortBy == "created") {
            std::sort(result.begin(), result.end(), [asc = filter.sortAscending](const Task& a, const Task& b) {
                return asc ? (a.getCreatedAt() < b.getCreatedAt()) : (a.getCreatedAt() > b.getCreatedAt());
            });
        } else if (filter.sortBy == "title") {
            std::sort(result.begin(), result.end(), [asc = filter.sortAscending](const Task& a, const Task& b) {
                return asc ? (a.getTitle() < b.getTitle()) : (a.getTitle() > b.getTitle());
            });
        }

        return result;
    }

    Task getTask(const std::string& id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t idx = findIndex(id);
        return tasks_[idx];
    }

    Task createTask(Task task) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string newId = generateId();
        task.setId(newId);
        task.validate();

        tasks_.push_back(task);
        if (storage_) storage_->saveTasks(tasks_);
        if (activityLog_) {
            activityLog_->log("TASK_CREATED", "Task", newId, "Created task: " + task.getTitle());
        }
        return task;
    }

    Task updateTask(const std::string& id, const Task& updatedData) {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t idx = findIndex(id);
        updatedData.validate();

        Task& existing = tasks_[idx];
        std::string oldStatus = Task::statusToString(existing.getStatus());
        std::string oldDeadline = existing.getDeadline();

        existing.setTitle(updatedData.getTitle());
        existing.setDescription(updatedData.getDescription());
        existing.setNotes(updatedData.getNotes());
        existing.setPriority(updatedData.getPriority());
        existing.setStatus(updatedData.getStatus());
        existing.setDeadline(updatedData.getDeadline());
        existing.setStartDate(updatedData.getStartDate());
        existing.setEstimatedHours(updatedData.getEstimatedHours());
        existing.setCategory(updatedData.getCategory());
        existing.setTags(updatedData.getTags());
        existing.setProjectId(updatedData.getProjectId());

        if (storage_) storage_->saveTasks(tasks_);

        std::string newStatus = Task::statusToString(existing.getStatus());
        if (activityLog_) {
            if (oldStatus != newStatus) {
                activityLog_->log("STATUS_CHANGED", "Task", id,
                    "Task '" + existing.getTitle() + "' status changed from " + oldStatus + " to " + newStatus);
            } else if (oldDeadline != existing.getDeadline()) {
                activityLog_->log("DEADLINE_CHANGED", "Task", id,
                    "Task '" + existing.getTitle() + "' deadline changed to " + existing.getDeadline());
            } else {
                activityLog_->log("TASK_UPDATED", "Task", id, "Updated task: " + existing.getTitle());
            }
        }
        return existing;
    }

    Task updateStatus(const std::string& id, Task::Status newStatus) {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t idx = findIndex(id);
        Task& t = tasks_[idx];
        t.setStatus(newStatus);

        if (storage_) storage_->saveTasks(tasks_);
        if (activityLog_) {
            std::string act = (newStatus == Task::Status::Completed) ? "TASK_COMPLETED" : "STATUS_CHANGED";
            activityLog_->log(act, "Task", id,
                "Task '" + t.getTitle() + "' set to " + Task::statusToString(newStatus));
        }
        return t;
    }

    void deleteTask(const std::string& id) {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t idx = findIndex(id);
        std::string title = tasks_[idx].getTitle();
        tasks_.erase(tasks_.begin() + idx);

        if (storage_) storage_->saveTasks(tasks_);
        if (activityLog_) {
            activityLog_->log("TASK_DELETED", "Task", id, "Deleted task: " + title);
        }
    }

    // Direct deadline setter (used by Rescheduler)
    void updateDeadline(const std::string& id, const std::string& newDeadline) {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t idx = findIndex(id);
        Task& t = tasks_[idx];
        std::string oldDeadline = t.getDeadline();
        t.setDeadline(newDeadline);

        if (storage_) storage_->saveTasks(tasks_);
        if (activityLog_) {
            activityLog_->log("RESCHEDULED", "Task", id,
                "Rescheduled '" + t.getTitle() + "' from " + oldDeadline + " to " + newDeadline);
        }
    }

    // Helper: Unassign project from tasks when project is deleted
    void unassignProject(const std::string& projectId) {
        std::lock_guard<std::mutex> lock(mutex_);
        bool changed = false;
        for (auto& t : tasks_) {
            if (t.getProjectId() == projectId) {
                t.setProjectId("");
                changed = true;
            }
        }
        if (changed && storage_) {
            storage_->saveTasks(tasks_);
        }
    }
};
