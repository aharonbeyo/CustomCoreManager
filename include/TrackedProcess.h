#ifndef TrackedProcess_H    
#define TrackedProcess_H

#include <string>
/**
 * @brief Stores runtime information about a tracked external process.
 */
struct TrackedProcess {
    pid_t pid = 0;
    std::string status = "initialized"; // "running", "paused", "finished", "terminated"
    std::string path;
    long long startTime = 0; // Epoch time in seconds
};

#endif // TrackedProcess_H