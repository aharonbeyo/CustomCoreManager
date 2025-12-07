#ifndef Command_H
#define Command_H

#include <string>
#include <vector>   
/**
 * @brief Represents a command received by the system to manage a process.
 */
struct Command {
    std::string id;
    std::string action; // "start", "pause", "resume", "terminate", "status"
    std::string programPath; // e.g., "/bin/bash"
    std::vector<std::string> args;
    std::string processId; // ID of the tracked process (if action != start)
};

#endif // Command_H