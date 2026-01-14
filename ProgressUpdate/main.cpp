#include "libProgressExport.h"

int main() {
    HG_registRedisAddr("1234567890", "http://127.0.0.1:6379", 3);

    HG_updateProgress("1234567890", 30, "test update");

    return 0;
}