#include <time.h>

#include "httplib.h"
#include "json.hpp"

void getTimestamp(char *buffer, size_t size) {
    struct timeval tv;
    struct tm tm_info;
    
    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &tm_info);
    
    snprintf(buffer, size, "%02d%02d%02d%02d%02d%02d%03ld",
            tm_info.tm_year % 100, tm_info.tm_mon + 1, tm_info.tm_mday,
            tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec,
            tv.tv_usec / 1000);
};

std::string dump_headers(const httplib::Headers &headers) {
  std::string s;
  char buf[BUFSIZ];

  for (auto it = headers.begin(); it != headers.end(); ++it) {
    const auto &x = *it;
    snprintf(buf, sizeof(buf), "%s: %s\n", x.first.c_str(), x.second.c_str());
    s += buf;
  }

  return s;
}

void logger(const httplib::Request &req, const httplib::Response &res) {
    std::string s;
    char buf[BUFSIZ];

    s += "================================\n";

    snprintf(buf, sizeof(buf), "%s %s %s", req.method.c_str(),
            req.version.c_str(), req.path.c_str());
    s += buf;

    std::string query;
    for (auto it = req.params.begin(); it != req.params.end(); ++it) {
    const auto &x = *it;
    snprintf(buf, sizeof(buf), "%c%s=%s",
                (it == req.params.begin()) ? '?' : '&', x.first.c_str(),
                x.second.c_str());
    query += buf;
    }
    snprintf(buf, sizeof(buf), "%s\n", query.c_str());
    s += buf;

    // s += dump_headers(req.headers);
    // s += "--------------------------------\n";
    // snprintf(buf, sizeof(buf), "%d %s\n", res.status, res.version.c_str());
    // s += buf;
    // s += dump_headers(res.headers);
    // s += "\n";

    // if (!res.body.empty()) { s += res.body; }
    // s += "\n";

    std::cout << s;
}

int main() {
    httplib::Server server;
    server.set_logger(logger);

    server.Post("/Start", [&](const httplib::Request &req, httplib::Response &res) {
        res.set_content("POST", "text/plain");
    })
    .Get("/Start", [&](const httplib::Request &req, httplib::Response &res) {
        nlohmann::json stResJson, stJsonData;
        stResJson["code"] = 200;

        // read params
        std::string sParamdimension = req.get_param_value("dimension");
        std::string sParamTaskid = req.get_param_value("task_id");

        std::string sParamMinioAddr = req.get_param_value("endpoint");
        if (sParamMinioAddr.empty()) {
            stResJson["code"] = 9001;
            stResJson["message"] = "params error";
        }
        
        if (stResJson["code"] == 200) {
            if (sParamTaskid.length() <= 0) {
                char buffer[100];
                getTimestamp(buffer, 100);
                sParamTaskid = std::string(buffer);
            }
            
            stJsonData["task_id"] = sParamTaskid;
            stResJson["message"] = "start ...";
            
            // add to ThreadPool
            // pool.AddTrackableTask(sParamTaskid, flow, sParamTaskid, sParamMinioAddr);
        }
        
        stResJson["data"] = stJsonData;
        res.set_content(stResJson.dump(), "appliation/json");
    })
    .Get("/Check", [&](const httplib::Request & req, httplib::Response &res) {
        nlohmann::json stResJson, stJsonData;
        stResJson["code"] = 200;

        // read params
        if (!req.has_param("task_id")) {
            stResJson["code"] = 9001;
            stResJson["message"] = "params error";
        } else {
            std::string sParamTaskid = req.get_param_value("task_id");

            // get percentage from threadpool
            // auto progress = pool.GetTaskProgress(sParamTaskid);
            
            stJsonData["task_id"] = sParamTaskid;
            stJsonData["status"] = 2;
            // stJsonData["percentage"] = progress.percentage;
            // stJsonData["message"] = progress.status;

            stResJson["code"] = res.status;
            stResJson["message"] = "success";
            stResJson["data"] = stJsonData;
            // stResJson["timestamp"] = progress.sTimestamp;
        }
        res.set_content(stResJson.dump(), "appliation/json");
    });


    server.set_error_handler([](const httplib::Request & /*req*/, httplib::Response &res) {
        const char *fmt = "<p>Error Status: <span style='color:red;'>%d</span></p>";
        char buf[BUFSIZ];
        snprintf(buf, sizeof(buf), fmt, res.status);
        res.set_content(buf, "text/html");
    });

    server.listen("0.0.0.0", 8080); 

    return 0;
}