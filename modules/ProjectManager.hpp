#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <random>
#include "Models.hpp"
#include "Storage.hpp"
#include "ActivityLog.hpp"
#include "TaskManager.hpp"

// -----------------------------------------------------------------------------
// ProjectManager Class
// Demonstrates: Encapsulation, Aggregation, Separation of Concerns
// -----------------------------------------------------------------------------
class ProjectManager {
private:
    std::vector<Project> projects_;
    IStorage* storage_{nullptr};
    ActivityLog* activityLog_{nullptr};
    TaskManager* taskManager_{nullptr};
    mutable std::mutex mutex_;

    std::string generateId() const {
        static std::mt19937_64 rng(std::random_device{}());
        static std::uniform_int_distribution<uint64_t> dist(100000, 999999);
        return "prj_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(dist(rng));
    }

    size_t findIndex(const std::string& id) const {
        for (size_t i = 0; i < projects_.size(); ++i) {
            if (projects_[i].getId() == id) return i;
        }
        throw NotFoundException("Project with ID '" + id + "' does not exist.");
    }

public:
    ProjectManager() = default;

    ProjectManager(IStorage* storage, ActivityLog* activityLog, TaskManager* taskManager)
        : storage_(storage), activityLog_(activityLog), taskManager_(taskManager) {
        if (storage_) {
            projects_ = storage_->loadProjects();
        }
    }

    void setDependencies(IStorage* storage, ActivityLog* activityLog, TaskManager* taskManager) {
        std::lock_guard<std::mutex> lock(mutex_);
        storage_ = storage;
        activityLog_ = activityLog;
        taskManager_ = taskManager;
        if (storage_) {
            projects_ = storage_->loadProjects();
        }
    }

    std::vector<Project> getProjects() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return projects_;
    }

    Project getProject(const std::string& id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t idx = findIndex(id);
        return projects_[idx];
    }

    Project createProject(Project project) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string newId = generateId();
        project.setId(newId);
        project.validate();

        projects_.push_back(project);
        if (storage_) storage_->saveProjects(projects_);
        if (activityLog_) {
            activityLog_->log("PROJECT_CREATED", "Project", newId, "Created project: " + project.getTitle());
        }
        return project;
    }

    Project updateProject(const std::string& id, const Project& updatedData) {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t idx = findIndex(id);
        updatedData.validate();

        Project& existing = projects_[idx];
        existing.setTitle(updatedData.getTitle());
        existing.setDescription(updatedData.getDescription());
        existing.setCategory(updatedData.getCategory());
        existing.setDeadline(updatedData.getDeadline());
        existing.setColorHex(updatedData.getColorHex());

        if (storage_) storage_->saveProjects(projects_);
        if (activityLog_) {
            activityLog_->log("PROJECT_UPDATED", "Project", id, "Updated project: " + existing.getTitle());
        }
        return existing;
    }

    // Safe Deletion: Deleting project unassigns tasks rather than deleting them
    void deleteProject(const std::string& id) {
        std::string title;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            size_t idx = findIndex(id);
            title = projects_[idx].getTitle();
            projects_.erase(projects_.begin() + idx);
            if (storage_) storage_->saveProjects(projects_);
            if (activityLog_) {
                activityLog_->log("PROJECT_DELETED", "Project", id, "Deleted project: " + title);
            }
        }
        // Unassign project association from tasks
        if (taskManager_) {
            taskManager_->unassignProject(id);
        }
    }

    // Returns projects with calculated completion progress
    nlohmann::json getProjectsWithStats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        nlohmann::json result = nlohmann::json::array();

        std::vector<Task> allTasks;
        if (taskManager_) {
            allTasks = taskManager_->getTasks();
        }

        for (const auto& prj : projects_) {
            int total = 0;
            int pending = 0;
            int inProgress = 0;
            int completed = 0;
            double totalHours = 0.0;
            double completedHours = 0.0;

            for (const auto& t : allTasks) {
                if (t.getProjectId() == prj.getId()) {
                    total++;
                    totalHours += t.getEstimatedHours();
                    switch (t.getStatus()) {
                        case Task::Status::Pending: pending++; break;
                        case Task::Status::InProgress: inProgress++; break;
                        case Task::Status::Completed:
                            completed++;
                            completedHours += t.getEstimatedHours();
                            break;
                    }
                }
            }

            double progress = (total > 0) ? (static_cast<double>(completed) / total * 100.0) : 0.0;
            auto item = prj.toJson();
            item["totalTasks"] = total;
            item["pendingTasks"] = pending;
            item["inProgressTasks"] = inProgress;
            item["completedTasks"] = completed;
            item["progressPercentage"] = std::round(progress * 10.0) / 10.0;
            item["totalHours"] = std::round(totalHours * 10.0) / 10.0;
            item["completedHours"] = std::round(completedHours * 10.0) / 10.0;
            result.push_back(item);
        }
        return result;
    }
};
