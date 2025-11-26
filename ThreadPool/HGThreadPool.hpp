#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <unordered_map>
#include <atomic>
#include <chrono>
#include <iostream>
#include <sstream>
#include <type_traits>

namespace HG
{

class ThreadPool {
public:
    static const int kMaxNumThreads = -1;

    struct TaskProgress {
        int percentage = 0;
        std::string status;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point last_update;
        std::string sTimestamp;
    };

    using ProgressCallback = std::function<void(int, const std::string&)>;

    explicit ThreadPool(int num_threads = kMaxNumThreads) 
        : stopped_(false), num_active_workers_(0) {
        
        if (num_threads == kMaxNumThreads) {
            num_threads = std::thread::hardware_concurrency();
        }
        if (num_threads <= 0) {
            num_threads = 1;
        }

        workers_.reserve(num_threads);
        for (int i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this, i] { WorkerFunc(i); });
        }
    }

    ~ThreadPool() {
        Stop();
    }

    size_t NumThreads() const {
        return workers_.size();
    }

    // 原始 AddTask 方法（保持兼容性）
    template <class func_t, class... args_t>
    auto AddTask(func_t&& f, args_t&&... args)
        -> std::future<typename std::result_of<func_t(args_t...)>::type> {
        
        using result_type = typename std::result_of<func_t(args_t...)>::type;
        
        auto task = std::make_shared<std::packaged_task<result_type()>>(
            [func = std::forward<func_t>(f), 
             args_tuple = std::make_tuple(std::forward<args_t>(args)...)]() mutable {
                return std::apply(func, args_tuple);
            }
        );
        
        std::future<result_type> result = task->get_future();
        
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (stopped_) {
                throw std::runtime_error("AddTask on stopped ThreadPool");
            }
            tasks_.emplace([task]() { (*task)(); });
        }
        
        task_condition_.notify_one();
        return result;
    }

    template <class func_t, class... args_t>
    auto AddTaskWithProgress(const std::string& task_id, func_t&& f, args_t&&... args)
        -> std::future<typename std::result_of<func_t(args_t...)>::type> {
        
        using result_type = typename std::result_of<func_t(args_t...)>::type;
        
        RegisterTask(task_id);
        
        auto task = std::make_shared<std::packaged_task<result_type()>>(
            [this, task_id, func = std::forward<func_t>(f), 
             args_tuple = std::make_tuple(std::forward<args_t>(args)...)]() mutable {
                
                UpdateTaskProgress(task_id, 10, "Starting execution");
                
                try {
                    auto result = std::apply(func, args_tuple);
                    UpdateTaskProgress(task_id, 100, "Completed successfully");
                    return result;
                } catch (const std::exception& e) {
                    UpdateTaskProgress(task_id, -1, std::string("Failed: ") + e.what());
                    throw;
                } catch (...) {
                    UpdateTaskProgress(task_id, -1, "Failed with unknown error");
                    throw;
                }
            }
        );
        
        std::future<result_type> result = task->get_future();
        
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (stopped_) {
                throw std::runtime_error("AddTask on stopped ThreadPool");
            }
            tasks_.emplace([task]() { (*task)(); });
        }
        
        task_condition_.notify_one();
        return result;
    }

    template <class func_t, class... args_t>
    auto AddTrackableTask(const std::string& task_id, func_t&& f, args_t&&... args)
        -> std::future<typename std::result_of<func_t(args_t..., ProgressCallback)>::type> {
        
        using result_type = typename std::result_of<func_t(args_t..., ProgressCallback)>::type;
        
        RegisterTask(task_id);
        
        auto task = std::make_shared<std::packaged_task<result_type()>>(
            [this, task_id, func = std::forward<func_t>(f), 
             args_tuple = std::make_tuple(std::forward<args_t>(args)...)]() mutable {
                
                auto progress_updater = [this, task_id](int progress, const std::string& status = "") {
                    UpdateTaskProgress(task_id, progress, status);
                };
                
                auto extended_args_tuple = std::tuple_cat(
                    std::move(args_tuple), 
                    std::make_tuple(progress_updater)
                );
                
                UpdateTaskProgress(task_id, 1, "Preparing execution");
                
                try {
                    auto result = std::apply(func, extended_args_tuple);
                    UpdateTaskProgress(task_id, 100, "Completed successfully");
                    return result;
                } catch (const std::exception& e) {
                    UpdateTaskProgress(task_id, -1, std::string("Failed: ") + e.what());
                    throw;
                } catch (...) {
                    UpdateTaskProgress(task_id, -1, "Failed with unknown error");
                    throw;
                }
            }
        );
        
        std::future<result_type> result = task->get_future();
        
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (stopped_) {
                throw std::runtime_error("AddTask on stopped ThreadPool");
            }
            tasks_.emplace([task]() { (*task)(); });
        }
        
        task_condition_.notify_one();
        return result;
    }

    void RegisterTask(const std::string& task_id) {
        std::unique_lock<std::mutex> lock(progress_mutex_);
        auto now = std::chrono::steady_clock::now();
        task_progress_[task_id] = TaskProgress{0, "Registered", now, now};
    }

    void UpdateTaskProgress(const std::string& task_id, int progress, const std::string& status = "") {
        std::unique_lock<std::mutex> lock(progress_mutex_);
        auto it = task_progress_.find(task_id);
        if (it != task_progress_.end()) {
            it->second.percentage = progress;
            if (!status.empty()) {
                it->second.status = status;
            }
            it->second.last_update = std::chrono::steady_clock::now();

            char buffer[100];
            getTimestamp(buffer, 100);
            it->second.sTimestamp = std::string(buffer);
        }
    }

    TaskProgress GetTaskProgress(const std::string& task_id) const {
        std::unique_lock<std::mutex> lock(progress_mutex_);
        auto it = task_progress_.find(task_id);
        if (it != task_progress_.end()) {
            return it->second;
        }
        return TaskProgress{-1, "Task not found", {}, {}};
    }

    std::unordered_map<std::string, TaskProgress> GetAllProgress() const {
        std::unique_lock<std::mutex> lock(progress_mutex_);
        return task_progress_;
    }

    void RemoveTaskProgress(const std::string& task_id) {
        std::unique_lock<std::mutex> lock(progress_mutex_);
        task_progress_.erase(task_id);
    }

    void Stop() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            stopped_ = true;
        }
        task_condition_.notify_all();
        for (std::thread& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    void Wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        finished_condition_.wait(lock, [this] {
            return tasks_.empty() && num_active_workers_ == 0;
        });
    }

    std::thread::id GetThreadId() const {
        return std::this_thread::get_id();
    }

    int GetThreadIndex() {
        std::unique_lock<std::mutex> lock(mutex_);
        auto it = thread_id_to_index_.find(std::this_thread::get_id());
        if (it != thread_id_to_index_.end()) {
            return it->second;
        }
        return -1;
    }

private:
    void WorkerFunc(int index) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            thread_id_to_index_[std::this_thread::get_id()] = index;
        }

        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                task_condition_.wait(lock, [this] {
                    return stopped_ || !tasks_.empty();
                });

                if (stopped_ && tasks_.empty()) {
                    return;
                }

                task = std::move(tasks_.front());
                tasks_.pop();
                ++num_active_workers_;
            }

            task();

            {
                std::unique_lock<std::mutex> lock(mutex_);
                --num_active_workers_;
                if (tasks_.empty() && num_active_workers_ == 0) {
                    finished_condition_.notify_all();
                }
            }
        }
    }

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

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;

    std::mutex mutex_;
    std::condition_variable task_condition_;
    std::condition_variable finished_condition_;

    bool stopped_;
    int num_active_workers_;
    std::unordered_map<std::thread::id, int> thread_id_to_index_;

    mutable std::mutex progress_mutex_;
    std::unordered_map<std::string, TaskProgress> task_progress_;
};

} // namespace Haige
    