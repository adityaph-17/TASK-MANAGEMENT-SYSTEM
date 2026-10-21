#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <nlohmann/json.hpp>

// -----------------------------------------------------------------------------
// Exception Hierarchy
// Demonstrates: Inheritance, Polymorphic what(), Custom Domain Exceptions
// -----------------------------------------------------------------------------
class TaskFlowException : public std::exception {
protected:
    std::string message_;
public:
    explicit TaskFlowException(std::string msg) : message_(std::move(msg)) {}
    const char* what() const noexcept override {
        return message_.c_str();
    }
};

class ValidationException : public TaskFlowException {
public:
    explicit ValidationException(const std::string& msg)
        : TaskFlowException("Validation Error: " + msg) {}
};

class NotFoundException : public TaskFlowException {
public:
    explicit NotFoundException(const std::string& msg)
        : TaskFlowException("Not Found: " + msg) {}
};

class StorageException : public TaskFlowException {
public:
    explicit StorageException(const std::string& msg)
        : TaskFlowException("Storage Error: " + msg) {}
};

// -----------------------------------------------------------------------------
// Template Helper Function
// Demonstrates: C++ Generic Programming / Templates
// -----------------------------------------------------------------------------
template<typename T>
bool inRange(T val, T minVal, T maxVal) {
    return (val >= minVal && val <= maxVal);
}

// -----------------------------------------------------------------------------
// Date Utilities Class
// Demonstrates: Static Methods, Function Overloading
// -----------------------------------------------------------------------------
class DateUtils {
public:
    // Returns current date and time formatted as ISO 8601 (YYYY-MM-DDTHH:MM:SS)
    static std::string nowIso() {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        std::ostringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
        return ss.str();
    }

    // Returns today's date formatted as YYYY-MM-DD
    static std::string todayDate() {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        std::ostringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%d");
        return ss.str();
    }

    // Function Overloading: Parse date string to time_t
    static std::time_t parseDate(const std::string& dateStr) {
        if (dateStr.empty()) return 0;
        std::tm tm{};
        std::istringstream ss(dateStr);
        if (dateStr.find('T') != std::string::npos) {
            ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
        } else {
            ss >> std::get_time(&tm, "%Y-%m-%d");
            tm.tm_hour = 23;
            tm.tm_min = 59;
            tm.tm_sec = 59;
        }
        if (ss.fail()) return 0;
        return std::mktime(&tm);
    }

    // Difference in days between date1 and date2 (date2 - date1)
    static double daysBetween(const std::string& date1, const std::string& date2) {
        std::time_t t1 = parseDate(date1);
        std::time_t t2 = parseDate(date2);
        if (t1 == 0 || t2 == 0) return 0.0;
        return std::difftime(t2, t1) / (60.0 * 60.0 * 24.0);
    }

    // Checks if date is before current time
    static bool isPast(const std::string& dateStr) {
        if (dateStr.empty()) return false;
        std::time_t deadline = parseDate(dateStr);
        if (deadline == 0) return false;
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        return deadline < now;
    }

    // Checks if two dates share the same calendar day (YYYY-MM-DD)
    static bool isSameDay(const std::string& date1, const std::string& date2) {
        if (date1.length() < 10 || date2.length() < 10) return false;
        return date1.substr(0, 10) == date2.substr(0, 10);
    }

    // Adds N days to a date string and returns formatted YYYY-MM-DD
    static std::string addDays(const std::string& dateStr, int days) {
        std::time_t t = parseDate(dateStr);
        if (t == 0) {
            t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        }
        t += static_cast<std::time_t>(days * 86400);
        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        std::ostringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%d");
        return ss.str();
    }
};

// -----------------------------------------------------------------------------
// Base Class: Entity
// Demonstrates: Abstraction, Base Class Inheritance, Virtual Destructor,
// Operator Overloading (operator==)
// -----------------------------------------------------------------------------
class Entity {
protected:
    std::string id_;
    std::string createdAt_;
    std::string updatedAt_;

public:
    Entity() : id_(""), createdAt_(DateUtils::nowIso()), updatedAt_(createdAt_) {}

    explicit Entity(std::string id)
        : id_(std::move(id)), createdAt_(DateUtils::nowIso()), updatedAt_(createdAt_) {}

    Entity(std::string id, std::string createdAt, std::string updatedAt)
        : id_(std::move(id)), createdAt_(std::move(createdAt)), updatedAt_(std::move(updatedAt)) {}

    virtual ~Entity() = default;

    // Getters
    const std::string& getId() const noexcept { return id_; }
    const std::string& getCreatedAt() const noexcept { return createdAt_; }
    const std::string& getUpdatedAt() const noexcept { return updatedAt_; }

    // Setters
    void setId(const std::string& id) { id_ = id; }
    void markUpdated() { updatedAt_ = DateUtils::nowIso(); }
    void setTimestamps(const std::string& created, const std::string& updated) {
        createdAt_ = created;
        updatedAt_ = updated;
    }

    // Operator Overloading: Compare entities by ID
    bool operator==(const Entity& other) const noexcept {
        return id_ == other.id_;
    }

    bool operator!=(const Entity& other) const noexcept {
        return !(*this == other);
    }
};

// -----------------------------------------------------------------------------
// Derived Class: Project
// Demonstrates: Inheritance (Project IS-A Entity), Encapsulation
// -----------------------------------------------------------------------------
class Project : public Entity {
private:
    std::string title_;
    std::string description_;
    std::string category_;
    std::string deadline_;
    std::string colorHex_;

public:
    Project() : Entity(), colorHex_("#245C4A") {}

    Project(std::string id, std::string title, std::string description,
            std::string category, std::string deadline, std::string colorHex = "#245C4A")
        : Entity(std::move(id)),
          title_(std::move(title)),
          description_(std::move(description)),
          category_(std::move(category)),
          deadline_(std::move(deadline)),
          colorHex_(std::move(colorHex)) {
        validate();
    }

    void validate() const {
        if (title_.empty()) {
            throw ValidationException("Project title cannot be empty.");
        }
    }

    // Getters
    const std::string& getTitle() const noexcept { return title_; }
    const std::string& getDescription() const noexcept { return description_; }
    const std::string& getCategory() const noexcept { return category_; }
    const std::string& getDeadline() const noexcept { return deadline_; }
    const std::string& getColorHex() const noexcept { return colorHex_; }

    // Setters with Encapsulation
    void setTitle(const std::string& title) {
        if (title.empty()) throw ValidationException("Project title cannot be empty.");
        title_ = title;
        markUpdated();
    }

    void setDescription(const std::string& desc) {
        description_ = desc;
        markUpdated();
    }

    void setCategory(const std::string& cat) {
        category_ = cat;
        markUpdated();
    }

    void setDeadline(const std::string& dl) {
        deadline_ = dl;
        markUpdated();
    }

    void setColorHex(const std::string& color) {
        colorHex_ = color.empty() ? "#245C4A" : color;
        markUpdated();
    }

    // JSON Serialization
    nlohmann::json toJson() const {
        return nlohmann::json{
            {"id", id_},
            {"title", title_},
            {"description", description_},
            {"category", category_},
            {"deadline", deadline_},
            {"colorHex", colorHex_},
            {"createdAt", createdAt_},
            {"updatedAt", updatedAt_}
        };
    }

    static Project fromJson(const nlohmann::json& j) {
        Project p;
        p.id_ = j.value("id", "");
        p.title_ = j.value("title", "");
        p.description_ = j.value("description", "");
        p.category_ = j.value("category", "General");
        p.deadline_ = j.value("deadline", "");
        p.colorHex_ = j.value("colorHex", "#245C4A");
        p.createdAt_ = j.value("createdAt", DateUtils::nowIso());
        p.updatedAt_ = j.value("updatedAt", p.createdAt_);
        p.validate();
        return p;
    }
};

// -----------------------------------------------------------------------------
// Derived Class: Task
// Demonstrates: Inheritance (Task IS-A Entity), Encapsulation,
// Static Conversion Helpers, Operator Overloading (<, <<)
// -----------------------------------------------------------------------------
class Task : public Entity {
public:
    enum class Priority { Low = 1, Medium = 2, High = 3, Critical = 4 };
    enum class Status { Pending, InProgress, Completed };

private:
    std::string title_;
    std::string description_;
    std::string notes_;
    Priority priority_{Priority::Medium};
    Status status_{Status::Pending};
    std::string deadline_;       // YYYY-MM-DD
    std::string startDate_;      // YYYY-MM-DD
    std::string completedAt_;    // ISO timestamp
    double estimatedHours_{1.0}; // in hours, 0.1 to 100
    std::string category_{"General"};
    std::vector<std::string> tags_;
    std::string projectId_;      // Optional project association

public:
    // Constructors (Function Overloading)
    Task() : Entity() {}

    Task(std::string id, std::string title, Priority priority, std::string deadline, double hours)
        : Entity(std::move(id)),
          title_(std::move(title)),
          priority_(priority),
          deadline_(std::move(deadline)),
          estimatedHours_(hours) {
        validate();
    }

    Task(std::string id, std::string title, std::string description, std::string notes,
         Priority priority, Status status, std::string deadline, std::string startDate,
         double hours, std::string category, std::vector<std::string> tags,
         std::string projectId)
        : Entity(std::move(id)),
          title_(std::move(title)),
          description_(std::move(description)),
          notes_(std::move(notes)),
          priority_(priority),
          status_(status),
          deadline_(std::move(deadline)),
          startDate_(std::move(startDate)),
          estimatedHours_(hours),
          category_(std::move(category)),
          tags_(std::move(tags)),
          projectId_(std::move(projectId)) {
        if (status_ == Status::Completed) {
            completedAt_ = DateUtils::nowIso();
        }
        validate();
    }

    void validate() const {
        if (title_.empty()) {
            throw ValidationException("Task title cannot be empty.");
        }
        if (!inRange(estimatedHours_, 0.1, 100.0)) {
            throw ValidationException("Estimated completion time must be between 0.1 and 100 hours.");
        }
    }

    // Static Helpers
    static std::string priorityToString(Priority p) {
        switch (p) {
            case Priority::Low: return "Low";
            case Priority::Medium: return "Medium";
            case Priority::High: return "High";
            case Priority::Critical: return "Critical";
        }
        return "Medium";
    }

    static Priority stringToPriority(const std::string& s) {
        if (s == "Low" || s == "low") return Priority::Low;
        if (s == "High" || s == "high") return Priority::High;
        if (s == "Critical" || s == "critical") return Priority::Critical;
        return Priority::Medium;
    }

    static int priorityToWeight(Priority p) {
        return static_cast<int>(p);
    }

    static std::string statusToString(Status s) {
        switch (s) {
            case Status::Pending: return "Pending";
            case Status::InProgress: return "In Progress";
            case Status::Completed: return "Completed";
        }
        return "Pending";
    }

    static Status stringToStatus(const std::string& s) {
        if (s == "In Progress" || s == "in_progress" || s == "InProgress") return Status::InProgress;
        if (s == "Completed" || s == "completed") return Status::Completed;
        return Status::Pending;
    }

    static bool isValidPriority(const std::string& s) {
        return (s == "Low" || s == "Medium" || s == "High" || s == "Critical" ||
                s == "low" || s == "medium" || s == "high" || s == "critical");
    }

    static bool isValidStatus(const std::string& s) {
        return (s == "Pending" || s == "In Progress" || s == "Completed" ||
                s == "pending" || s == "in_progress" || s == "completed");
    }

    bool isOverdue() const {
        if (status_ == Status::Completed) return false;
        if (deadline_.empty()) return false;
        return DateUtils::isPast(deadline_);
    }

    bool isDueToday() const {
        if (status_ == Status::Completed) return false;
        if (deadline_.empty()) return false;
        return DateUtils::isSameDay(deadline_, DateUtils::todayDate());
    }

    // Getters
    const std::string& getTitle() const noexcept { return title_; }
    const std::string& getDescription() const noexcept { return description_; }
    const std::string& getNotes() const noexcept { return notes_; }
    Priority getPriority() const noexcept { return priority_; }
    Status getStatus() const noexcept { return status_; }
    const std::string& getDeadline() const noexcept { return deadline_; }
    const std::string& getStartDate() const noexcept { return startDate_; }
    const std::string& getCompletedAt() const noexcept { return completedAt_; }
    double getEstimatedHours() const noexcept { return estimatedHours_; }
    const std::string& getCategory() const noexcept { return category_; }
    const std::vector<std::string>& getTags() const noexcept { return tags_; }
    const std::string& getProjectId() const noexcept { return projectId_; }

    // Setters with Encapsulation
    void setTitle(const std::string& title) {
        if (title.empty()) throw ValidationException("Task title cannot be empty.");
        title_ = title;
        markUpdated();
    }

    void setDescription(const std::string& desc) {
        description_ = desc;
        markUpdated();
    }

    void setNotes(const std::string& notes) {
        notes_ = notes;
        markUpdated();
    }

    void setPriority(Priority p) {
        priority_ = p;
        markUpdated();
    }

    void setStatus(Status s) {
        if (status_ != Status::Completed && s == Status::Completed) {
            completedAt_ = DateUtils::nowIso();
        } else if (s != Status::Completed) {
            completedAt_.clear();
        }
        status_ = s;
        markUpdated();
    }

    void setDeadline(const std::string& deadline) {
        deadline_ = deadline;
        markUpdated();
    }

    void setStartDate(const std::string& startDate) {
        startDate_ = startDate;
        markUpdated();
    }

    void setEstimatedHours(double hours) {
        if (!inRange(hours, 0.1, 100.0)) {
            throw ValidationException("Estimated completion time must be between 0.1 and 100 hours.");
        }
        estimatedHours_ = hours;
        markUpdated();
    }

    void setCategory(const std::string& cat) {
        category_ = cat.empty() ? "General" : cat;
        markUpdated();
    }

    void setTags(const std::vector<std::string>& tags) {
        tags_ = tags;
        markUpdated();
    }

    void setProjectId(const std::string& pid) {
        projectId_ = pid;
        markUpdated();
    }

    // Operator Overloading: Compare urgency for priority queues and ordering
    bool operator<(const Task& other) const {
        if (priority_ != other.priority_) {
            return static_cast<int>(priority_) > static_cast<int>(other.priority_);
        }
        if (!deadline_.empty() && !other.deadline_.empty()) {
            return deadline_ < other.deadline_;
        }
        return createdAt_ < other.createdAt_;
    }

    // JSON Serialization
    nlohmann::json toJson() const {
        return nlohmann::json{
            {"id", id_},
            {"title", title_},
            {"description", description_},
            {"notes", notes_},
            {"priority", priorityToString(priority_)},
            {"status", statusToString(status_)},
            {"deadline", deadline_},
            {"startDate", startDate_},
            {"completedAt", completedAt_},
            {"estimatedHours", estimatedHours_},
            {"category", category_},
            {"tags", tags_},
            {"projectId", projectId_},
            {"isOverdue", isOverdue()},
            {"isDueToday", isDueToday()},
            {"createdAt", createdAt_},
            {"updatedAt", updatedAt_}
        };
    }

    static Task fromJson(const nlohmann::json& j) {
        Task t;
        t.id_ = j.value("id", "");
        t.title_ = j.value("title", "");
        t.description_ = j.value("description", "");
        t.notes_ = j.value("notes", "");
        t.priority_ = stringToPriority(j.value("priority", "Medium"));
        t.status_ = stringToStatus(j.value("status", "Pending"));
        t.deadline_ = j.value("deadline", "");
        t.startDate_ = j.value("startDate", "");
        t.completedAt_ = j.value("completedAt", "");
        t.estimatedHours_ = j.value("estimatedHours", 1.0);
        t.category_ = j.value("category", "General");
        if (j.contains("tags") && j["tags"].is_array()) {
            t.tags_ = j["tags"].get<std::vector<std::string>>();
        }
        t.projectId_ = j.value("projectId", "");
        t.createdAt_ = j.value("createdAt", DateUtils::nowIso());
        t.updatedAt_ = j.value("updatedAt", t.createdAt_);
        t.validate();
        return t;
    }
};

// Operator Overloading: stream output for Task
inline std::ostream& operator<<(std::ostream& os, const Task& t) {
    os << "Task[" << t.getId() << ": " << t.getTitle() << "]";
    return os;
}

// -----------------------------------------------------------------------------
// Activity Log Entry
// Demonstrates: Static Factory Method, Encapsulation
// -----------------------------------------------------------------------------
class ActivityEvent {
private:
    std::string id_;
    std::string timestamp_;
    std::string eventType_; // TASK_CREATED, TASK_UPDATED, TASK_COMPLETED, etc.
    std::string entityType_; // Task, Project, System
    std::string entityId_;
    std::string message_;

public:
    ActivityEvent() : timestamp_(DateUtils::nowIso()) {}

    ActivityEvent(std::string id, std::string timestamp, std::string eventType,
                  std::string entityType, std::string entityId, std::string message)
        : id_(std::move(id)),
          timestamp_(std::move(timestamp)),
          eventType_(std::move(eventType)),
          entityType_(std::move(entityType)),
          entityId_(std::move(entityId)),
          message_(std::move(message)) {}

    static ActivityEvent create(const std::string& eventType, const std::string& entityType,
                                const std::string& entityId, const std::string& message) {
        static uint64_t counter = 0;
        std::string eventId = "act_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(++counter);
        return ActivityEvent(eventId, DateUtils::nowIso(), eventType, entityType, entityId, message);
    }

    const std::string& getId() const noexcept { return id_; }
    const std::string& getTimestamp() const noexcept { return timestamp_; }
    const std::string& getEventType() const noexcept { return eventType_; }
    const std::string& getEntityType() const noexcept { return entityType_; }
    const std::string& getEntityId() const noexcept { return entityId_; }
    const std::string& getMessage() const noexcept { return message_; }

    nlohmann::json toJson() const {
        return nlohmann::json{
            {"id", id_},
            {"timestamp", timestamp_},
            {"eventType", eventType_},
            {"entityType", entityType_},
            {"entityId", entityId_},
            {"message", message_}
        };
    }

    static ActivityEvent fromJson(const nlohmann::json& j) {
        return ActivityEvent(
            j.value("id", ""),
            j.value("timestamp", DateUtils::nowIso()),
            j.value("eventType", "GENERAL"),
            j.value("entityType", "System"),
            j.value("entityId", ""),
            j.value("message", "")
        );
    }
};

// -----------------------------------------------------------------------------
// System Settings
// Demonstrates: Encapsulation
// -----------------------------------------------------------------------------
class Settings {
private:
    double dailyCapacityHours_{6.0};

public:
    Settings() = default;
    explicit Settings(double capacity) : dailyCapacityHours_(capacity) {
        if (dailyCapacityHours_ <= 0.0) dailyCapacityHours_ = 6.0;
    }

    double getDailyCapacityHours() const noexcept { return dailyCapacityHours_; }
    void setDailyCapacityHours(double cap) {
        if (cap <= 0.0 || cap > 24.0) {
            throw ValidationException("Daily capacity must be between 0.5 and 24.0 hours.");
        }
        dailyCapacityHours_ = cap;
    }

    nlohmann::json toJson() const {
        return nlohmann::json{
            {"dailyCapacityHours", dailyCapacityHours_}
        };
    }

    static Settings fromJson(const nlohmann::json& j) {
        return Settings(j.value("dailyCapacityHours", 6.0));
    }
};
