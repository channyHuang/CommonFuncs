#include "HGThreadPool.hpp"

int poolThreadFunc(std::string sId, unsigned int nId, HG::ThreadPool::ProgressCallback update_progress) {
    // do something...
    std::this_thread::sleep_for(std::chrono::milliseconds{1});

    update_progress(1, "update progress " + sId + " " + std::to_string(nId));

    for (size_t i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds{60});
        update_progress((i + 1) * 10, "doing... count " + std::to_string(i) + " times");
    }

    return 0;

}

int main() {
    HG::ThreadPool pool(-1);

    for (size_t i = 0; i < 10; ++i) {
        std::string sId = std::to_string(i);

        pool.AddTrackableTask(sId, poolThreadFunc, sId, i);
    }

    int count = 10;
    while (count) {
        for (size_t i = 0; i < 10; ++i) {
            std::string sId = std::to_string(i);
            auto progress = pool.GetTaskProgress(sId);
            printf("%d : %d %s\n", i, progress.percentage, progress.status.c_str());
        }
        std::this_thread::sleep_for(std::chrono::seconds{60});
        count--;
    }

    return 0;
}