#include <crow.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <nlohmann/json.hpp>

#include "modules/Models.hpp"
#include "modules/Storage.hpp"
#include "modules/ActivityLog.hpp"
#include "modules/TaskManager.hpp"
#include "modules/ProjectManager.hpp"
#include "modules/SmartPlanner.hpp"
#include "modules/WorkloadAnalyzer.hpp"
#include "modules/Rescheduler.hpp"

// Utility helper to read static files from disk
std::string readStaticFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Utility to create a JSON response with status code
crow::response makeJsonResponse(int statusCode, const nlohmann::json& payload) {
    crow::response res;
    res.code = statusCode;
    res.set_header("Content-Type", "application/json");
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
    res.body = payload.dump();
    return res;
}

// Error response helper
crow::response makeErrorResponse(int statusCode, const std::string& message) {
    return makeJsonResponse(statusCode, {
        {"success", false},
        {"error", message}
    });
}

int main(int argc, char* argv[]) {
    int port = 8080;
    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
        } catch (...) {
            std::cerr << "Invalid port argument, defaulting to 8080" << std::endl;
        }
    }

    std::cout << "==================================================" << std::endl;
    std::cout << " TaskFlow – Personal Task Management System (C++17) " << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "[Init] Loading Storage from data/tasks.json..." << std::endl;

    // Composition and dependency setup
    auto storage = std::make_unique<JsonStorage>("data/tasks.json");
    storage->init();

    ActivityLog activityLog(storage.get());
    TaskManager taskManager(storage.get(), &activityLog);
    ProjectManager projectManager(storage.get(), &activityLog, &taskManager);
    Settings settings = storage->loadSettings();

    crow::SimpleApp app;

    // -------------------------------------------------------------------------
    // Static Frontend Assets
    // -------------------------------------------------------------------------
    CROW_ROUTE(app, "/")
    ([]() {
        std::string html = readStaticFile("index.html");
        if (html.empty()) {
            return crow::response(404, "index.html not found.");
        }
        crow::response res(200, html);
        res.set_header("Content-Type", "text/html; charset=utf-8");
        return res;
    });

    CROW_ROUTE(app, "/style.css")
    ([]() {
        std::string css = readStaticFile("style.css");
        crow::response res(200, css);
        res.set_header("Content-Type", "text/css; charset=utf-8");
        return res;
    });

    // -------------------------------------------------------------------------
    // CORS Preflight Options
    // -------------------------------------------------------------------------
    CROW_ROUTE(app, "/api/<path>")
    .methods("OPTIONS"_method)
    ([](const crow::request&, const std::string&) {
        crow::response res(204);
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        return res;
    });

    // -------------------------------------------------------------------------
    // Tasks REST API
    // -------------------------------------------------------------------------

    // GET /api/tasks (supports query filtering and sorting)
    CROW_ROUTE(app, "/api/tasks")
    .methods("GET"_method)
    ([&taskManager](const crow::request& req) {
        try {
            TaskFilter filter;
            if (req.url_params.get("search")) filter.searchQuery = req.url_params.get("search");
            if (req.url_params.get("status")) filter.status = req.url_params.get("status");
            if (req.url_params.get("priority")) filter.priority = req.url_params.get("priority");
            if (req.url_params.get("category")) filter.category = req.url_params.get("category");
            if (req.url_params.get("projectId")) filter.projectId = req.url_params.get("projectId");
            if (req.url_params.get("sortBy")) filter.sortBy = req.url_params.get("sortBy");
            if (req.url_params.get("order")) {
                std::string o = req.url_params.get("order");
                filter.sortAscending = (o != "desc" && o != "DESC");
            }

            auto tasks = taskManager.getTasks(filter);
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& t : tasks) {
                arr.push_back(t.toJson());
            }
            return makeJsonResponse(200, arr);
        } catch (const std::exception& e) {
            return makeErrorResponse(500, e.what());
        }
    });

    // GET /api/tasks/<id>
    CROW_ROUTE(app, "/api/tasks/<string>")
    .methods("GET"_method)
    ([&taskManager](const std::string& id) {
        try {
            Task t = taskManager.getTask(id);
            return makeJsonResponse(200, t.toJson());
        } catch (const NotFoundException& e) {
            return makeErrorResponse(404, e.what());
        } catch (const std::exception& e) {
            return makeErrorResponse(500, e.what());
        }
    });

    // POST /api/tasks
    CROW_ROUTE(app, "/api/tasks")
    .methods("POST"_method)
    ([&taskManager](const crow::request& req) {
        try {
            auto body = nlohmann::json::parse(req.body);
            std::string title = body.value("title", "");
            if (title.empty()) {
                return makeErrorResponse(400, "Task title cannot be empty.");
            }

            std::string priorityStr = body.value("priority", "Medium");
            if (!Task::isValidPriority(priorityStr)) {
                return makeErrorResponse(400, "Invalid priority value.");
            }

            std::string statusStr = body.value("status", "Pending");
            if (!Task::isValidStatus(statusStr)) {
                return makeErrorResponse(400, "Invalid status value.");
            }

            Task task = Task::fromJson(body);
            Task created = taskManager.createTask(task);
            return makeJsonResponse(201, created.toJson());
        } catch (const ValidationException& e) {
            return makeErrorResponse(400, e.what());
        } catch (const nlohmann::json::parse_error& e) {
            return makeErrorResponse(400, std::string("Malformed JSON body: ") + e.what());
        } catch (const std::exception& e) {
            return makeErrorResponse(500, e.what());
        }
    });

    // PUT /api/tasks/<id>
    CROW_ROUTE(app, "/api/tasks/<string>")
    .methods("PUT"_method)
    ([&taskManager](const crow::request& req, const std::string& id) {
        try {
            auto body = nlohmann::json::parse(req.body);
            body["id"] = id;

            std::string title = body.value("title", "");
            if (title.empty()) {
                return makeErrorResponse(400, "Task title cannot be empty.");
            }

            Task task = Task::fromJson(body);
            Task updated = taskManager.updateTask(id, task);
            return makeJsonResponse(200, updated.toJson());
        } catch (const NotFoundException& e) {
            return makeErrorResponse(404, e.what());
        } catch (const ValidationException& e) {
            return makeErrorResponse(400, e.what());
        } catch (const std::exception& e) {
            return makeErrorResponse(500, e.what());
        }
    });

    // PATCH /api/tasks/<id>/complete
    CROW_ROUTE(app, "/api/tasks/<string>/complete")
    .methods("PATCH"_method)
    ([&taskManager](const std::string& id) {
        try {
            Task current = taskManager.getTask(id);
            Task::Status newStatus = (current.getStatus() == Task::Status::Completed)
                                     ? Task::Status::Pending
                                     : Task::Status::Completed;
            Task updated = taskManager.updateStatus(id, newStatus);
            return makeJsonResponse(200, updated.toJson());
        } catch (const NotFoundException& e) {
            return makeErrorResponse(404, e.what());
        } catch (const std::exception& e) {
            return makeErrorResponse(500, e.what());
        }
    });

    // PATCH /api/tasks/<id>/status
    CROW_ROUTE(app, "/api/tasks/<string>/status")
    .methods("PATCH"_method)
    ([&taskManager](const crow::request& req, const std::string& id) {
        try {
            auto body = nlohmann::json::parse(req.body);
            std::string s = body.value("status", "Pending");
            if (!Task::isValidStatus(s)) {
                return makeErrorResponse(400, "Invalid status string.");
            }
            Task updated = taskManager.updateStatus(id, Task::stringToStatus(s));
            return makeJsonResponse(200, updated.toJson());
        } catch (const NotFoundException& e) {
            return makeErrorResponse(404, e.what());
        } catch (const std::exception& e) {
            return makeErrorResponse(500, e.what());
        }
    });

    // DELETE /api/tasks/<id>
    CROW_ROUTE(app, "/api/tasks/<string>")
    .methods("DELETE"_method)
    ([&taskManager](const std::string& id) {
        try {
            taskManager.deleteTask(id);
            return makeJsonResponse(200, {
                {"success", true},
                {"message", "Task deleted successfully."},
                {"id", id}
            });
        } catch (const NotFoundException& e) {
            return makeErrorResponse(404, e.what());
        } catch (const std::exception& e) {
            return makeErrorResponse(500, e.what());
        }
    });

    // -------------------------------------------------------------------------
    // Projects REST API
    // -------------------------------------------------------------------------

    // GET /api/projects
    CROW_ROUTE(app, "/api/projects")
    .methods("GET"_method)
    ([&projectManager]() {
        try {
            return makeJsonResponse(200, projectManager.getProjectsWithStats());
        } catch (const std::exception& e) {
            return makeErrorResponse(500, e.what());
        }
    });

    // GET /api/projects/<id>
    CROW_ROUTE(app, "/api/projects/<string>")
    .methods("GET"_method)
    ([&projectManager, &taskManager](const std::string& id) {
        try {
            Project p = projectManager.getProject(id);
            TaskFilter filter;
            filter.projectId = id;
            auto pTasks = taskManager.getTasks(filter);

            nlohmann::json res = p.toJson();
            nlohmann::json tArr = nlohmann::json::array();
            for (const auto& t : pTasks) {
                tArr.push_back(t.toJson());
            }
            res["tasks"] = tArr;
            return makeJsonResponse(200, res);
        } catch (const NotFoundException& e) {
            return makeErrorResponse(404, e.what());
        } catch (const std::exception& e) {
            return makeErrorResponse(500, e.what());
        }
    });

    // POST /api/projects
    CROW_ROUTE(app, "/api/projects")
    .methods("POST"_method)
    ([&projectManager](const crow::request& req) {
        try {
            auto body = nlohmann::json::parse(req.body);
            std::string title = body.value("title", "");
            if (title.empty()) {
                return makeErrorResponse(400, "Project title cannot be empty.");
            }
            Project p = Project::fromJson(body);
            Project created = projectManager.createProject(p);
            return makeJsonResponse(201, created.toJson());
        } catch (const ValidationException& e) {
            return makeErrorResponse(400, e.what());
        } catch (const std::exception& e) {
            return makeErrorResponse(500, e.what());
        }
    });

    // PUT /api/projects/<id>
    CROW_ROUTE(app, "/api/projects/<string>")
    .methods("PUT"_method)
    ([&projectManager](const crow::request& req, const std::string& id) {
        try {
            auto body = nlohmann::json::parse(req.body);
            body["id"] = id;
            Project p = Project::fromJson(body);
            Project updated = projectManager.updateProject(id, p);
            return makeJsonResponse(200, updated.toJson());
        } catch (const NotFoundException& e) {
            return makeErrorResponse(404, e.what());
        } catch (const ValidationException& e) {
            return makeErrorResponse(400, e.what());
        } catch (const std::exception& e) {
            return makeErrorResponse(500, e.what());
        }
    });

    // DELETE /api/projects/<id>
    CROW_ROUTE(app, "/api/projects/<string>")
    .methods("DELETE"_method)
    ([&projectManager](const std::string& id) {
        try {
            projectManager.deleteProject(id);
            return makeJsonResponse(200, {
                {"success", true},
                {"message", "Project deleted successfully. Associated tasks are now independent."},
                {"id", id}
            });
        } catch (const NotFoundException& e) {
            return makeErrorResponse(404, e.what());
        } catch (const std::exception& e) {
            return makeErrorResponse(500, e.what());
        }
    });

    // -------------------------------------------------------------------------
    // Dashboard, Planning, Workload & Calendar Endpoints
    // -------------------------------------------------------------------------

    // GET /api/dashboard
    CROW_ROUTE(app, "/api/dashboard")
    .methods("GET"_method)
    ([&taskManager, &projectManager, &activityLog, &settings]() {
        try {
            auto tasks = taskManager.getTasks();
            int total = static_cast<int>(tasks.size());
            int pending = 0;
            int inProgress = 0;
            int completed = 0;
            int overdue = 0;
            int dueToday = 0;
            double pendingHours = 0.0;

            std::vector<nlohmann::json> highPriorityTasks;
            std::vector<nlohmann::json> upcomingDeadlines;

            for (const auto& t : tasks) {
                if (t.getStatus() == Task::Status::Pending) {
                    pending++;
                    pendingHours += t.getEstimatedHours();
                } else if (t.getStatus() == Task::Status::InProgress) {
                    inProgress++;
                    pendingHours += t.getEstimatedHours();
                } else if (t.getStatus() == Task::Status::Completed) {
                    completed++;
                }

                if (t.isOverdue()) overdue++;
                if (t.isDueToday()) dueToday++;

                if (t.getStatus() != Task::Status::Completed &&
                    (t.getPriority() == Task::Priority::High || t.getPriority() == Task::Priority::Critical)) {
                    highPriorityTasks.push_back(t.toJson());
                }

                if (t.getStatus() != Task::Status::Completed && !t.getDeadline().empty()) {
                    double diff = DateUtils::daysBetween(DateUtils::todayDate(), t.getDeadline());
                    if (diff >= 0 && diff <= 7.0) {
                        upcomingDeadlines.push_back(t.toJson());
                    }
                }
            }

            std::sort(upcomingDeadlines.begin(), upcomingDeadlines.end(), [](const nlohmann::json& a, const nlohmann::json& b) {
                return a["deadline"] < b["deadline"];
            });
            if (upcomingDeadlines.size() > 5) upcomingDeadlines.resize(5);

            double completionPct = (total > 0) ? (static_cast<double>(completed) / total * 100.0) : 0.0;
            auto acts = activityLog.getRecent(10);
            nlohmann::json actArr = nlohmann::json::array();
            for (const auto& a : acts) actArr.push_back(a.toJson());

            auto nextRecommendation = SmartPlanner::recommendNextTask(tasks);

            return makeJsonResponse(200, {
                {"totalTasks", total},
                {"pendingTasks", pending},
                {"inProgressTasks", inProgress},
                {"completedTasks", completed},
                {"overdueTasks", overdue},
                {"dueTodayTasks", dueToday},
                {"completionPercentage", std::round(completionPct * 10.0) / 10.0},
                {"totalProjects", static_cast<int>(projectManager.getProjects().size())},
                {"pendingHours", std::round(pendingHours * 10.0) / 10.0},
                {"dailyCapacityHours", settings.getDailyCapacityHours()},
                {"highPriorityTasks", highPriorityTasks},
                {"upcomingDeadlines", upcomingDeadlines},
                {"recentActivity", actArr},
                {"recommendation", nextRecommendation.toJson()}
            });
        } catch (const std::exception& e) {
            return makeErrorResponse(500, e.what());
        }
    });

    // GET /api/planner
    CROW_ROUTE(app, "/api/planner")
    .methods("GET"_method)
    ([&taskManager]() {
        try {
            auto tasks = taskManager.getTasks();
            auto rec = SmartPlanner::recommendNextTask(tasks);
            return makeJsonResponse(200, rec.toJson());
        } catch (const std::exception& e) {
            return makeErrorResponse(500, e.what());
        }
    });

    // GET /api/workload
    CROW_ROUTE(app, "/api/workload")
    .methods("GET"_method)
    ([&taskManager, &settings]() {
        try {
            auto tasks = taskManager.getTasks();
            return makeJsonResponse(200, WorkloadAnalyzer::analyze(tasks, settings.getDailyCapacityHours()));
        } catch (const std::exception& e) {
            return makeErrorResponse(500, e.what());
        }
    });

    // GET /api/calendar
    CROW_ROUTE(app, "/api/calendar")
    .methods("GET"_method)
    ([&taskManager]() {
        try {
            auto tasks = taskManager.getTasks();
            std::map<std::string, std::vector<nlohmann::json>> calendarMap;
            for (const auto& t : tasks) {
                if (!t.getDeadline().empty()) {
                    std::string d = t.getDeadline().substr(0, 10);
                    calendarMap[d].push_back(t.toJson());
                }
            }
            nlohmann::json res = nlohmann::json::object();
            for (const auto& [date, list] : calendarMap) {
                res[date] = list;
            }
            return makeJsonResponse(200, res);
        } catch (const std::exception& e) {
            return makeErrorResponse(500, e.what());
        }
    });

    // GET /api/reports
    CROW_ROUTE(app, "/api/reports")
    .methods("GET"_method)
    ([&taskManager, &projectManager, &settings]() {
        try {
            auto tasks = taskManager.getTasks();
            int total = static_cast<int>(tasks.size());
            int pending = 0, inProgress = 0, completed = 0, overdue = 0;
            int low = 0, med = 0, high = 0, critical = 0;
            double pendingHours = 0.0, completedHours = 0.0;

            for (const auto& t : tasks) {
                if (t.getStatus() == Task::Status::Pending) { pending++; pendingHours += t.getEstimatedHours(); }
                else if (t.getStatus() == Task::Status::InProgress) { inProgress++; pendingHours += t.getEstimatedHours(); }
                else if (t.getStatus() == Task::Status::Completed) { completed++; completedHours += t.getEstimatedHours(); }

                if (t.isOverdue()) overdue++;

                switch (t.getPriority()) {
                    case Task::Priority::Low: low++; break;
                    case Task::Priority::Medium: med++; break;
                    case Task::Priority::High: high++; break;
                    case Task::Priority::Critical: critical++; break;
                }
            }

            return makeJsonResponse(200, {
                {"totalTasks", total},
                {"statusDistribution", {{"pending", pending}, {"inProgress", inProgress}, {"completed", completed}}},
                {"priorityDistribution", {{"low", low}, {"medium", med}, {"high", high}, {"critical", critical}}},
                {"overdueCount", overdue},
                {"pendingHours", std::round(pendingHours * 10.0) / 10.0},
                {"completedHours", std::round(completedHours * 10.0) / 10.0},
                {"projects", projectManager.getProjectsWithStats()},
                {"dailyCapacityHours", settings.getDailyCapacityHours()}
            });
        } catch (const std::exception& e) {
            return makeErrorResponse(500, e.what());
        }
    });

    // GET /api/activity
    CROW_ROUTE(app, "/api/activity")
    .methods("GET"_method)
    ([&activityLog]() {
        try {
            auto acts = activityLog.getRecent(50);
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& a : acts) arr.push_back(a.toJson());
            return makeJsonResponse(200, arr);
        } catch (const std::exception& e) {
            return makeErrorResponse(500, e.what());
        }
    });

    // GET /api/notifications
    CROW_ROUTE(app, "/api/notifications")
    .methods("GET"_method)
    ([&taskManager, &settings]() {
        try {
            auto tasks = taskManager.getTasks();
            std::vector<nlohmann::json> alerts;

            int overdueCount = 0;
            for (const auto& t : tasks) {
                if (t.isOverdue()) overdueCount++;
            }
            if (overdueCount > 0) {
                alerts.push_back({
                    {"type", "danger"},
                    {"title", "Overdue Tasks"},
                    {"message", "You have " + std::to_string(overdueCount) + " overdue task(s). Review suggestions in Smart Planner."}
                });
            }

            auto workload = WorkloadAnalyzer::analyze(tasks, settings.getDailyCapacityHours());
            int overloadedDays = workload["overloadedDaysCount"];
            if (overloadedDays > 0) {
                alerts.push_back({
                    {"type", "warning"},
                    {"title", "Capacity Warning"},
                    {"message", "Workload exceeds daily capacity limit (" +
                                std::to_string(settings.getDailyCapacityHours()).substr(0, 3) + "h) on " +
                                std::to_string(overloadedDays) + " upcoming day(s)."}
                });
            }

            return makeJsonResponse(200, alerts);
        } catch (const std::exception& e) {
            return makeErrorResponse(500, e.what());
        }
    });

    // -------------------------------------------------------------------------
    // Rescheduling REST API
    // -------------------------------------------------------------------------

    // GET /api/reschedule-suggestions
    CROW_ROUTE(app, "/api/reschedule-suggestions")
    .methods("GET"_method)
    ([&taskManager, &settings]() {
        try {
            auto tasks = taskManager.getTasks();
            auto suggestions = Rescheduler::generateSuggestions(tasks, settings.getDailyCapacityHours());
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& s : suggestions) arr.push_back(s.toJson());
            return makeJsonResponse(200, arr);
        } catch (const std::exception& e) {
            return makeErrorResponse(500, e.what());
        }
    });

    // POST /api/reschedule-suggestions/<id>/accept
    CROW_ROUTE(app, "/api/reschedule-suggestions/<string>/accept")
    .methods("POST"_method)
    ([&taskManager](const crow::request& req, const std::string& id) {
        try {
            auto body = nlohmann::json::parse(req.body);
            std::string newDeadline = body.value("suggestedDeadline", "");
            if (newDeadline.empty()) {
                newDeadline = body.value("newDeadline", "");
            }
            if (newDeadline.empty()) {
                return makeErrorResponse(400, "suggestedDeadline or newDeadline required.");
            }
            Rescheduler::applySuggestion(taskManager, id, newDeadline);
            return makeJsonResponse(200, {
                {"success", true},
                {"message", "Deadline successfully rescheduled with user approval."},
                {"taskId", id},
                {"newDeadline", newDeadline}
            });
        } catch (const NotFoundException& e) {
            return makeErrorResponse(404, e.what());
        } catch (const std::exception& e) {
            return makeErrorResponse(500, e.what());
        }
    });

    // -------------------------------------------------------------------------
    // Settings REST API
    // -------------------------------------------------------------------------

    // GET /api/settings
    CROW_ROUTE(app, "/api/settings")
    .methods("GET"_method)
    ([&settings]() {
        return makeJsonResponse(200, settings.toJson());
    });

    // PUT /api/settings
    CROW_ROUTE(app, "/api/settings")
    .methods("PUT"_method)
    ([&settings, &storage, &activityLog](const crow::request& req) {
        try {
            auto body = nlohmann::json::parse(req.body);
            double cap = body.value("dailyCapacityHours", 6.0);
            settings.setDailyCapacityHours(cap);
            storage->saveSettings(settings);
            activityLog.log("SETTINGS_UPDATED", "System", "settings",
                "Daily capacity updated to " + std::to_string(cap).substr(0, 4) + "h");
            return makeJsonResponse(200, settings.toJson());
        } catch (const ValidationException& e) {
            return makeErrorResponse(400, e.what());
        } catch (const std::exception& e) {
            return makeErrorResponse(500, e.what());
        }
    });

    std::cout << "[Server] TaskFlow running on http://localhost:" << port << std::endl;
    std::cout << "[Server] Serving index.html and style.css" << std::endl;

    app.port(port).multithreaded().run();
    return 0;
}
