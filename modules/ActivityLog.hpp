#pragma once

#include <vector>
#include <string>
#include <mutex>
#include "Models.hpp"
#include "Storage.hpp"

// -----------------------------------------------------------------------------
// ActivityLog Class
// Demonstrates: Encapsulation, Aggregation, Thread Safety
// -----------------------------------------------------------------------------
class ActivityLog {
private:
    std::vector<ActivityEvent> events_;
    IStorage* storage_{nullptr};
    mutable std::mutex mutex_;

public:
    ActivityLog() = default;

    explicit ActivityLog(IStorage* storage) : storage_(storage) {
        if (storage_) {
            events_ = storage_->loadActivities();
        }
    }

    void setStorage(IStorage* storage) {
        std::lock_guard<std::mutex> lock(mutex_);
        storage_ = storage;
        if (storage_) {
            events_ = storage_->loadActivities();
        }
    }

    void log(const std::string& eventType, const std::string& entityType,
             const std::string& entityId, const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto ev = ActivityEvent::create(eventType, entityType, entityId, message);
        events_.push_back(ev);
        if (storage_) {
            storage_->saveActivities(events_);
        }
    }

    std::vector<ActivityEvent> getRecent(size_t limit = 50) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<ActivityEvent> result;
        if (events_.empty()) return result;

        size_t start = (events_.size() > limit) ? (events_.size() - limit) : 0;
        for (size_t i = events_.size(); i > start; --i) {
            result.push_back(events_[i - 1]);
        }
        return result;
    }
};
