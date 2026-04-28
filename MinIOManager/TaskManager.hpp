#pragma once

#include <atomic>
#include <condition_variable>
#include <thread>
#include <vector>

// for fix count tasks, ex: download/upload files in folder

template<typename Task>
class TaskManager {
public:
    using TaskFunc = std::function<bool(const Task&, const std::atomic<bool>&)>;

    explicit TaskManager(size_t nNumTheeads, TaskFunc funTaskFunc) 
        : m_nNumThreads(nNumTheeads), m_bStop(false), m_nRemainTasks(0), m_funTaskFunc(std::move(funTaskFunc)) {}

    TaskManager(const TaskManager&) = delete;
    TaskManager& operator = (const TaskManager&) = delete;

    ~TaskManager() {
        stop();
    }

    void addTask(const Task& stTask) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_quTasks.push(stTask);
        ++m_nRemainTasks;
    }

    void addTasks(const std::vector<Task>& vTasks) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto &stTask : vTasks) {
            m_quTasks.push(stTask);
            ++m_nRemainTasks;
        }
    }

    void start() {
        for (size_t i = 0; i < m_nNumThreads; ++i) {
            m_vWorkers.emplace_back(&TaskManager::workerFunc, this);
        }
    }

    void waitForComplete() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cvDone.wait(lock, [this] {
            return (m_nRemainTasks == 0 && m_quTasks.empty());
        });
    }

    void stop() {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_bStop = true;
        }
        m_cvStop.notify_all();
        m_cvDone.notify_all();

        for (auto &th : m_vWorkers) {
            if (th.joinable()) th.join();
        }
        m_vWorkers.clear();
    }

private:
    void workerFunc() {
        while (true) {
            Task stTask;
            bool bHasTask = false;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cvStop.wait(lock, [this] { return !m_quTasks.empty() || m_bStop; });
            
                if (m_bStop) {
                    while (!m_quTasks.empty()) m_quTasks.pop();
                    break;
                }
                if (!m_quTasks.empty()) {
                    stTask = m_quTasks.front();
                    m_quTasks.pop();
                    bHasTask = true;
                }
            }

            if (bHasTask) {
                bool bSuccess = m_funTaskFunc(stTask, m_bStop);

                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    --m_nRemainTasks;
                    if (m_nRemainTasks == 0 && m_quTasks.empty()) {
                        m_cvDone.notify_all();
                        break;
                    }
                }
            }
        }
    }

private:
    size_t m_nNumThreads;
    std::atomic<size_t> m_nRemainTasks;
    mutable std::mutex m_mutex;
    std::vector<std::thread> m_vWorkers;
    std::queue<Task> m_quTasks;
    std::condition_variable m_cvDone;
    std::condition_variable m_cvStop;
    std::atomic<bool> m_bStop;
    TaskFunc m_funTaskFunc;
};
