#ifndef RAFT_TEST_FRAMEWORK_H
#define RAFT_TEST_FRAMEWORK_H

#include <unordered_map>
#include <vector>
#include <string>
#include <utility>

class RAFTTestFramework {
public:
    RAFTTestFramework();
    ~RAFTTestFramework();
private:
    std::vector<std::pair<std::string, std::string>> addresses;
    std::unordered_map<std::string, std::string> config;
};

#endif // RAFT_TEST_FRAMEWORK_H
