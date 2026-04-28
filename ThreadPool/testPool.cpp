#include "TaskPool.hpp"

int poolThreadFunc(std::string sId, unsigned int nId) {
    // do something...
    std::this_thread::sleep_for(std::chrono::milliseconds{1});

    for (size_t i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds{60});
    }

    return 0;
}

int main() {
    HG::TaskPool pool(-1);

    for (size_t i = 0; i < 10; ++i) {
        std::string sId = std::to_string(i);

        pool.addTask(sId, poolThreadFunc, sId, i);
    }

    int count = 10;
    while (count) {
        for (size_t i = 0; i < 10; ++i) {
            std::string sId = std::to_string(i);
        }
        std::this_thread::sleep_for(std::chrono::seconds{60});
        count--;

        if (count == 9) {
            pool.cancelTask(std::to_string(count));
        }
    }

    return 0;
}