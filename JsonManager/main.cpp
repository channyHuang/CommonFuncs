#include "JsonManager.h"

int main() {
    std::string sBody = "";
    bool bSuc = JsonManager::getInstance()->parseRequest(sBody.c_str());
    return 0;
}