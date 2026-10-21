#pragma once

#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <filesystem>
#include <iostream>
#include "Models.hpp"

// -----------------------------------------------------------------------------
// Abstract Base Class: IStorage
// Demonstrates: Abstraction, Pure Virtual Functions, Runtime Polymorphic Interface
// -----------------------------------------------------------------------------
class IStorage {
public:
    virtual ~IStorage() = default;

    virtual void init() = 0;
    virtual std::vector<Task> loadTasks() = 0;
    virtual void saveTasks(const std::vector<Task>& tasks) = 0;

    virtual std::vector<Project> loadProjects() = 0;
    virtual void saveProjects(const std::vector<Project>& projects) = 0;

    virtual std::vector<ActivityEvent> loadActivities() = 0;
    virtual void saveActivities(const std::vector<ActivityEvent>& activities) = 0;

    virtual Settings loadSettings() = 0;
    virtual void saveSettings(const Settings& settings) = 0;
};

// -----------------------------------------------------------------------------
// Concrete Class: JsonStorage
// Demonstrates: Inheritance (implements IStorage), Function Overriding (override),
// File Handling, Atomic Writes, RAII, Exception Handling
// -----------------------------------------------------------------------------
class JsonStorage : public IStorage {
private:
    std::string dataFilePath_;
    std::string dataDir_;

    // Reads full file content into string
    std::string readFile(const std::string& path) const {
        if (!std::filesystem::exists(path)) {
            return "";
        }
        std::ifstream in(path, std::ios::in);
        if (!in.is_open()) {
            throw StorageException("Failed to open file for reading: " + path);
        }
        return std::string((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
    }

    // Atomic write to prevent file corruption
    void atomicWrite(const std::string& targetPath, const std::string& content) {
        std::string tempPath = targetPath + ".tmp";
        try {
            std::ofstream out(tempPath, std::ios::out | std::ios::trunc);
            if (!out.is_open()) {
                throw StorageException("Failed to open temp file for write: " + tempPath);
            }
            out << content;
            out.flush();
            out.close();

            std::error_code ec;
            std::filesystem::rename(tempPath, targetPath, ec);
            if (ec) {
                std::filesystem::copy_file(tempPath, targetPath,
                    std::filesystem::copy_options::overwrite_existing, ec);
                std::filesystem::remove(tempPath, ec);
            }
        } catch (const std::exception& e) {
            throw StorageException(std::string("Error persisting to ") + targetPath + ": " + e.what());
        }
    }

    // Loads complete root JSON document
    nlohmann::json loadRootJson() {
        std::string raw = readFile(dataFilePath_);
        if (raw.empty()) {
            return nlohmann::json{
                {"tasks", nlohmann::json::array()},
                {"projects", nlohmann::json::array()},
                {"activities", nlohmann::json::array()},
                {"settings", {{"dailyCapacityHours", 6.0}}}
            };
        }
        try {
            return nlohmann::json::parse(raw);
        } catch (const std::exception& e) {
            std::cerr << "[Storage Warning] Could not parse JSON, keeping existing structure: " << e.what() << std::endl;
            return nlohmann::json{
                {"tasks", nlohmann::json::array()},
                {"projects", nlohmann::json::array()},
                {"activities", nlohmann::json::array()},
                {"settings", {{"dailyCapacityHours", 6.0}}}
            };
        }
    }

public:
    explicit JsonStorage(std::string dataFilePath = "data/tasks.json")
        : dataFilePath_(std::move(dataFilePath)) {
        dataDir_ = std::filesystem::path(dataFilePath_).parent_path().string();
        if (dataDir_.empty()) dataDir_ = "data";
    }

    ~JsonStorage() override = default;

    void init() override {
        try {
            if (!std::filesystem::exists(dataDir_)) {
                std::filesystem::create_directories(dataDir_);
            }
            // If data/tasks.json doesn't exist, create it with empty structure
            if (!std::filesystem::exists(dataFilePath_)) {
                nlohmann::json root = {
                    {"tasks", nlohmann::json::array()},
                    {"projects", nlohmann::json::array()},
                    {"activities", nlohmann::json::array()},
                    {"settings", {{"dailyCapacityHours", 6.0}}}
                };
                atomicWrite(dataFilePath_, root.dump(2));
            }
        } catch (const std::exception& e) {
            throw StorageException(std::string("Failed to initialize storage: ") + e.what());
        }
    }

    std::vector<Task> loadTasks() override {
        std::vector<Task> tasks;
        auto root = loadRootJson();
        if (root.contains("tasks") && root["tasks"].is_array()) {
            for (const auto& item : root["tasks"]) {
                try {
                    tasks.push_back(Task::fromJson(item));
                } catch (...) {}
            }
        }
        return tasks;
    }

    void saveTasks(const std::vector<Task>& tasks) override {
        auto root = loadRootJson();
        nlohmann::json jTasks = nlohmann::json::array();
        for (const auto& t : tasks) {
            jTasks.push_back(t.toJson());
        }
        root["tasks"] = jTasks;
        atomicWrite(dataFilePath_, root.dump(2));
    }

    std::vector<Project> loadProjects() override {
        std::vector<Project> projects;
        auto root = loadRootJson();
        if (root.contains("projects") && root["projects"].is_array()) {
            for (const auto& item : root["projects"]) {
                try {
                    projects.push_back(Project::fromJson(item));
                } catch (...) {}
            }
        }
        return projects;
    }

    void saveProjects(const std::vector<Project>& projects) override {
        auto root = loadRootJson();
        nlohmann::json jProjects = nlohmann::json::array();
        for (const auto& p : projects) {
            jProjects.push_back(p.toJson());
        }
        root["projects"] = jProjects;
        atomicWrite(dataFilePath_, root.dump(2));
    }

    std::vector<ActivityEvent> loadActivities() override {
        std::vector<ActivityEvent> activities;
        auto root = loadRootJson();
        if (root.contains("activities") && root["activities"].is_array()) {
            for (const auto& item : root["activities"]) {
                try {
                    activities.push_back(ActivityEvent::fromJson(item));
                } catch (...) {}
            }
        }
        return activities;
    }

    void saveActivities(const std::vector<ActivityEvent>& activities) override {
        auto root = loadRootJson();
        nlohmann::json jActs = nlohmann::json::array();
        size_t start = (activities.size() > 500) ? (activities.size() - 500) : 0;
        for (size_t i = start; i < activities.size(); ++i) {
            jActs.push_back(activities[i].toJson());
        }
        root["activities"] = jActs;
        atomicWrite(dataFilePath_, root.dump(2));
    }

    Settings loadSettings() override {
        auto root = loadRootJson();
        if (root.contains("settings")) {
            try {
                return Settings::fromJson(root["settings"]);
            } catch (...) {}
        }
        return Settings(6.0);
    }

    void saveSettings(const Settings& settings) override {
        auto root = loadRootJson();
        root["settings"] = settings.toJson();
        atomicWrite(dataFilePath_, root.dump(2));
    }
};
