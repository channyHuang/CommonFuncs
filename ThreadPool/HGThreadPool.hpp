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

#include "RedisManager.h"

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
        std::atomic<bool> cancelled{false};
        
        // 删除默认的拷贝和移动构造函数/赋值运算符
        TaskProgress() = default;
        TaskProgress(const TaskProgress& other) 
            : percentage(other.percentage),
              status(other.status),
              start_time(other.start_time),
              last_update(other.last_update),
              sTimestamp(other.sTimestamp),
              cancelled(other.cancelled.load()) {}
        
        TaskProgress& operator=(const TaskProgress& other) {
            if (this != &other) {
                percentage = other.percentage;
                status = other.status;
                start_time = other.start_time;
                last_update = other.last_update;
                sTimestamp = other.sTimestamp;
                cancelled.store(other.cancelled.load());
            }
            return *this;
        }
    };

    using ProgressCallback = std::function<void(int, const std::string&)>;

    // 可取消的任务包装器基类
    class CancellableTask {
    public:
        virtual ~CancellableTask() = default;
        virtual void execute() = 0;
        virtual void cancel() = 0;
        virtual bool is_cancelled() const = 0;
        virtual std::string get_id() const = 0;
    };

    // 模板化的可取消任务
    template<typename ResultType>
    class CancellableTaskImpl : public CancellableTask {
    public:
        CancellableTaskImpl(
            const std::string& task_id,
            std::function<ResultType()> func,
            std::shared_ptr<std::atomic<bool>> cancel_flag
        ) : task_id_(task_id), func_(std::move(func)), cancel_flag_(cancel_flag) {}

        void execute() override {
            if (cancel_flag_ && cancel_flag_->load()) {
                throw std::runtime_error("Task cancelled before execution");
            }
            
            try {
                result_.set_value(func_());
            } catch (...) {
                result_.set_exception(std::current_exception());
            }
        }

        void cancel() override {
            if (cancel_flag_) {
                cancel_flag_->store(true);
            }
        }

        bool is_cancelled() const override {
            return cancel_flag_ && cancel_flag_->load();
        }

        std::string get_id() const override {
            return task_id_;
        }

        std::future<ResultType> get_future() {
            return result_.get_future();
        }

    private:
        std::string task_id_;
        std::function<ResultType()> func_;
        std::shared_ptr<std::atomic<bool>> cancel_flag_;
        std::promise<ResultType> result_;
    };

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

    // 添加可取消任务（基础版本）
    template <class func_t, class... args_t>
    auto AddCancellableTask(const std::string& task_id, func_t&& f, args_t&&... args)
        -> std::pair<std::future<typename std::result_of<func_t(args_t...)>::type>, 
                     std::function<bool()>> {
        
        using result_type = typename std::result_of<func_t(args_t...)>::type;
        
        auto cancel_flag = RegisterTaskWithCancel(task_id);
        
        auto task = std::make_shared<CancellableTaskImpl<result_type>>(
            task_id,
            [this, task_id, func = std::forward<func_t>(f), 
             args_tuple = std::make_tuple(std::forward<args_t>(args)...),
             cancel_flag]() mutable -> result_type {
                
                // 检查取消状态
                if (cancel_flag && cancel_flag->load()) {
                    throw std::runtime_error("Task cancelled before execution");
                }
                
                UpdateTaskProgress(task_id, 10, "Starting execution");
                
                try {
                    auto result = std::apply(func, args_tuple);
                    
                    // 再次检查取消状态
                    if (cancel_flag && cancel_flag->load()) {
                        throw std::runtime_error("Task cancelled during execution");
                    }
                    
                    UpdateTaskProgress(task_id, 100, "Completed successfully");
                    return result;
                } catch (const std::exception& e) {
                    UpdateTaskProgress(task_id, -1, std::string("Failed: ") + e.what());
                    throw;
                } catch (...) {
                    UpdateTaskProgress(task_id, -1, "Failed with unknown error");
                    throw;
                }
            },
            cancel_flag
        );
        
        auto future = task->get_future();
        auto cancel_func = [this, task_id]() -> bool {
            return CancelTask(task_id);
        };
        
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (stopped_) {
                throw std::runtime_error("AddTask on stopped ThreadPool");
            }
            
            // 存储任务用于可能的取消
            {
                std::lock_guard<std::mutex> progress_lock(progress_mutex_);
                cancellable_tasks_[task_id] = task;
            }
            
            // 包装执行函数，在执行后清理
            tasks_.emplace([this, task, task_id]() {
                task->execute();
                
                // 清理
                {
                    std::lock_guard<std::mutex> lock(progress_mutex_);
                    cancellable_tasks_.erase(task_id);
                }
            });
        }
        
        task_condition_.notify_one();
        return {std::move(future), cancel_func};
    }

    // 添加可追踪且可取消的任务
    template <class func_t, class... args_t>
    auto AddTrackableCancellableTask(const std::string& task_id, func_t&& f, args_t&&... args)
        -> std::pair<std::future<typename std::result_of<func_t(args_t..., ProgressCallback)>::type>,
                     std::function<bool()>> {
        
        using result_type = typename std::result_of<func_t(args_t..., ProgressCallback)>::type;
        
        auto cancel_flag = RegisterTaskWithCancel(task_id);
        
        auto task = std::make_shared<CancellableTaskImpl<result_type>>(
            task_id,
            [this, task_id, func = std::forward<func_t>(f), 
             args_tuple = std::make_tuple(std::forward<args_t>(args)...),
             cancel_flag]() mutable -> result_type {
                
                // 检查取消状态
                if (cancel_flag && cancel_flag->load()) {
                    throw std::runtime_error("Task cancelled before execution");
                }
                
                auto progress_updater = [this, task_id, cancel_flag](int progress_val, const std::string& status = "") {
                    // 每次更新前检查取消状态
                    if (cancel_flag && cancel_flag->load()) {
                        throw std::runtime_error("Task cancelled during progress update");
                    }
                    UpdateTaskProgress(task_id, progress_val, status);
                };
                
                auto extended_args_tuple = std::tuple_cat(
                    std::move(args_tuple), 
                    std::make_tuple(progress_updater)
                );
                
                UpdateTaskProgress(task_id, 1, "Preparing execution");
                
                try {
                    // 在应用函数前检查取消状态
                    if (cancel_flag && cancel_flag->load()) {
                        throw std::runtime_error("Task cancelled before function execution");
                    }
                    
                    auto result = std::apply(func, extended_args_tuple);
                    
                    // 完成后检查取消状态
                    if (cancel_flag && cancel_flag->load()) {
                        throw std::runtime_error("Task cancelled after function execution");
                    }
                    
                    UpdateTaskProgress(task_id, 100, "Completed successfully");
                    return result;
                } catch (const std::exception& e) {
                    UpdateTaskProgress(task_id, -1, std::string("Failed: ") + e.what());
                    throw;
                } catch (...) {
                    UpdateTaskProgress(task_id, -1, "Failed with unknown error");
                    throw;
                }
            },
            cancel_flag
        );
        
        auto future = task->get_future();
        auto cancel_func = [this, task_id]() -> bool {
            return CancelTask(task_id);
        };
        
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (stopped_) {
                throw std::runtime_error("AddTask on stopped ThreadPool");
            }
            
            // 存储任务
            {
                std::lock_guard<std::mutex> progress_lock(progress_mutex_);
                cancellable_tasks_[task_id] = task;
            }
            
            tasks_.emplace([this, task, task_id]() {
                task->execute();
                
                // 清理
                {
                    std::lock_guard<std::mutex> lock(progress_mutex_);
                    cancellable_tasks_.erase(task_id);
                }
            });
        }
        
        task_condition_.notify_one();
        return {std::move(future), cancel_func};
    }

    // 取消特定任务
    bool CancelTask(const std::string& task_id) {
        std::lock_guard<std::mutex> lock(progress_mutex_);
        
        bool cancelled = false;
        
        // 更新进度状态
        auto it = task_progress_.find(task_id);
        if (it != task_progress_.end()) {
            it->second.cancelled.store(true);
            it->second.status = "Cancelled";
            it->second.last_update = std::chrono::steady_clock::now();
            char buffer[100];
            getTimestamp(buffer, 100);
            it->second.sTimestamp = std::string(buffer);
            cancelled = true;
        }
        
        // 尝试取消可取消任务
        auto task_it = cancellable_tasks_.find(task_id);
        if (task_it != cancellable_tasks_.end()) {
            task_it->second->cancel();
            cancelled = true;
        }
        
        return cancelled;
    }

    // 批量取消任务
    size_t CancelTasks(const std::vector<std::string>& task_ids) {
        size_t cancelled_count = 0;
        for (const auto& task_id : task_ids) {
            if (CancelTask(task_id)) {
                ++cancelled_count;
            }
        }
        return cancelled_count;
    }

    // 取消所有任务
    size_t CancelAllTasks() {
        std::lock_guard<std::mutex> lock(progress_mutex_);
        
        size_t cancelled_count = 0;
        
        // 取消所有任务
        for (auto& pair : task_progress_) {
            pair.second.cancelled.store(true);
            pair.second.status = "Cancelled";
            pair.second.last_update = std::chrono::steady_clock::now();
            char buffer[100];
            getTimestamp(buffer, 100);
            pair.second.sTimestamp = std::string(buffer);
            cancelled_count++;
        }
        
        // 取消所有可取消任务
        for (auto& pair : cancellable_tasks_) {
            pair.second->cancel();
        }
        
        return cancelled_count;
    }

    // 检查任务是否被取消
    bool IsTaskCancelled(const std::string& task_id) const {
        std::lock_guard<std::mutex> lock(progress_mutex_);
        auto it = task_progress_.find(task_id);
        if (it != task_progress_.end()) {
            return it->second.cancelled.load();
        }
        return false;
    }

    // 保留原有方法，添加取消支持
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
        TaskProgress progress;
        progress.percentage = 0;
        progress.status = "Registered";
        progress.start_time = now;
        progress.last_update = now;
        progress.cancelled.store(false);
        
        char buffer[100];
        getTimestamp(buffer, 100);
        progress.sTimestamp = std::string(buffer);
        
        task_progress_[task_id] = progress;
    }

    // 为可取消任务注册（返回取消标志的共享指针）
    std::shared_ptr<std::atomic<bool>> RegisterTaskWithCancel(const std::string& task_id) {
        std::unique_lock<std::mutex> lock(progress_mutex_);
        auto now = std::chrono::steady_clock::now();
        TaskProgress progress;
        progress.percentage = 0;
        progress.status = "Registered";
        progress.start_time = now;
        progress.last_update = now;
        progress.cancelled.store(false);
        
        char buffer[100];
        getTimestamp(buffer, 100);
        progress.sTimestamp = std::string(buffer);
        
        task_progress_[task_id] = progress;
        
        // 返回取消标志的共享指针
        return std::shared_ptr<std::atomic<bool>>(&task_progress_[task_id].cancelled, 
                                                  [](std::atomic<bool>*) {
            // 空删除器，原子变量由 TaskProgress 管理
        });
    }

    void UpdateTaskProgress(const std::string& task_id, int progress, const std::string& status = "") {
        std::lock_guard<std::mutex> lock(progress_mutex_);
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

            // update to redis
            HG::RedisManager::getInstance()->set(task_id.c_str(), std::to_string(progress).c_str());
        }
    }

    TaskProgress GetTaskProgress(const std::string& task_id) const {
        std::lock_guard<std::mutex> lock(progress_mutex_);
        auto it = task_progress_.find(task_id);
        if (it != task_progress_.end()) {
            return it->second;
        }
        return TaskProgress{};
    }

    std::unordered_map<std::string, TaskProgress> GetAllProgress() const {
        std::lock_guard<std::mutex> lock(progress_mutex_);
        return task_progress_;
    }

    void RemoveTaskProgress(const std::string& task_id) {
        std::lock_guard<std::mutex> lock(progress_mutex_);
        task_progress_.erase(task_id);
        cancellable_tasks_.erase(task_id);
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

            try {
                task();
            } catch (const std::exception& e) {
                // 记录异常但不中断线程池运行
                std::cerr << "Task execution failed: " << e.what() << std::endl;
            }

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
    std::unordered_map<std::string, std::shared_ptr<CancellableTask>> cancellable_tasks_;
};

} // namespace Haige
    