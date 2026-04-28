#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/time.h>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "spdlog/spdlog.h"

// for unknown tasks, ex: request

namespace HG
{

// task base class 
class TaskInterface {
public:
    virtual ~TaskInterface() = default;
    virtual void execute() = 0;
    virtual void cancel() = 0;
    virtual bool beCanceled() const = 0;
    virtual std::string getId() const = 0;
};

template <typename ResultType>
class MyTask : public TaskInterface {
public:
    MyTask(const std::string& sTaskId, std::function<ResultType()> func) 
        : m_sTaskId(sTaskId), m_func(func) {}
    virtual ~MyTask() {}

    virtual void execute() {
        if (m_bCancelFlag.load()) return;
        try {
            m_tResult.set_value(m_func());
        } catch (...) {
            m_tResult.set_exception(std::current_exception());
        }
    }

    virtual void cancel() {
        m_bCancelFlag.store(true);
    }

    virtual bool beCanceled() const {
        return m_bCancelFlag.load();
    }

    virtual std::string getId() const {
        return m_sTaskId;
    }

    std::future<ResultType> getFuture() {
        return m_tResult.get_future();
    }

private:
    std::string m_sTaskId;
    std::atomic<bool> m_bCancelFlag = false;
    std::promise<ResultType> m_tResult;
    std::function<ResultType()> m_func; 
};

class TaskPool {
public:
    using UpdateProgressCallback = std::function<void(const std::string&, int, const std::string&)>;

    explicit TaskPool(int nNumThreads = 1) : m_nNumThreads(nNumThreads), m_bStop(false) {
        if (m_nNumThreads <= 0) m_nNumThreads = std::thread::hardware_concurrency();

        m_vWorkers.reserve(m_nNumThreads);
        for (size_t i = 0; i < m_nNumThreads; ++i) {
            m_vWorkers.emplace_back(&TaskPool::workerFunc, this);
        }
    }

    ~TaskPool() {
        stop();
    }

    template<class func_t, class... args_t>
    auto addTask(const std::string& sTaskId, func_t&& func, args_t&&... args) 
        -> std::future<typename std::result_of<func_t(args_t...)>::type> {

            using result_type = typename std::result_of<func_t(args_t...)>::type;
            // create ReconTask
            auto task = std::make_shared<MyTask<result_type>>( 
                sTaskId, 
                [this, sTaskId, fun = std::forward<func_t>(func), 
                args_tuple = std::make_tuple(std::forward<args_t>(args)...)]() 
                mutable -> result_type {

                    updateTaskProgress(sTaskId, 1, "sMessageRunning");
                    try {
                        auto result = std::apply(fun, args_tuple);
                        return result;
                    } catch (const std::exception& e) {
                        spdlog::get("Message")->critical("addTask apply failed: {} ", e.what());
                        updateTaskProgress(sTaskId, -1, e.what());
                    } catch (...) {
                        spdlog::get("Message")->critical("addTask apply failed.");
                        updateTaskProgress(sTaskId, 1, "sMessageFailed unknown error!");
                    } 
            });

        auto future = task->getFuture();
            
        // add to queue
        {
            std::unique_lock<std::mutex> lock(m_mutexQuTasks);
            m_mapTasks.insert({sTaskId, task});

            if (m_bStop) {
                // std::promise<int> p;
                // p.set_value(0);
                // return p.get_future();
                return std::future<result_type>();
            }

            m_quTasks.emplace([this, task, sTaskId](){
                task->execute();
                {
                    std::unique_lock<std::mutex> lock(m_mutexMapTasks);
                    m_mapTasks.erase(sTaskId);
                }
            });
        }
        
        m_cvStop.notify_all();
        return future;
    }

    // cancel the whole task
    bool cancelTask(const std::string& sTaskId) {
        bool bCanceled = false;
        {
            std::unique_lock<std::mutex> lock(m_mutexMapTasks);
            auto itr = m_mapTasks.find(sTaskId);
            if (itr != m_mapTasks.end()) {
                itr->second->cancel();

                bCanceled = true;

                m_mapTasks.erase(itr);
            }
        }

        return bCanceled;
    }

    bool cancelAllTasks() {
        std::unique_lock<std::mutex> lock(m_mutexMapTasks);
        for (auto itr = m_mapTasks.begin(); itr != m_mapTasks.end(); ) {
            itr->second->cancel();
            itr = m_mapTasks.erase(itr);
        }
        return true;
    }

    void updateTaskProgress(const std::string& sTaskId, int nProgress, const std::string& sMessage = "") {
        // do something ...
    }

    void stop() {
        {
            m_bStop = true;
        }
        m_cvStop.notify_all();
        for (std::thread& worker : m_vWorkers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

private:
    void workerFunc() {
        while (true) {
            bool bHasTask = false;
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(m_mutexQuTasks);
                m_cvStop.wait(lock, [this] { return !m_quTasks.empty() || m_bStop; });

                if (m_bStop) {
                    while (!m_quTasks.empty()) {
                        // task = std::move(m_quTasks.front());
                        // auto sTaskId = task->getId();
                        m_quTasks.pop();
                    }
                    break;
                }
                if (!m_quTasks.empty()) {
                    task = std::move(m_quTasks.front());
                    m_quTasks.pop();
                    bHasTask = true;
                }
            }
            if (bHasTask) {
                try {
                    task();
                } catch (const std::exception& e) {
                    spdlog::get("Message")->critical("Task execution failed: {}", e.what());
                } catch (...) {
                    spdlog::get("Message")->critical("Task execution failed.");
                }
            }
        }
    }

private:
    int m_nNumThreads = 1;
    std::vector<std::thread> m_vWorkers;
    std::queue<std::function<void()> > m_quTasks;
    // save task info 
    std::unordered_map<std::string, std::shared_ptr<TaskInterface > > m_mapTasks;
    // mutex
    std::mutex m_mutexQuTasks;
    std::mutex m_mutexMapTasks;
    // stop for update server
    std::condition_variable m_cvStop;
    std::atomic<bool> m_bStop = false;
};

} // namespace HG
    