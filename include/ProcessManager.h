#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include <string>
#include <vector>
#include <map>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

// Unix/Linux Specific Headers for Process Control Data Types
// Required for pid_t and signal constants
#include <sys/types.h>
#include <signal.h> 
#include "MessageQueue.h"
#include "TrackedProcess.h"
#include "Command.h"

// --- Configuration ---
// Signals for controlling processes
// These are defined in the header as they are constant integral types
const int SIG_PAUSE = SIGSTOP;
const int SIG_RESUME = SIGCONT;
const int SIG_TERMINATE = SIGTERM;


/**
 * @brief Manages the lifecycle of external processes using fork, exec, and signals.
 * It uses worker threads to process commands and monitor child process status.
 */
class ProcessManager {
public:
    MessageQueue* queue;
    std::map<std::string, TrackedProcess> runningProcesses;
    std::mutex trackerMutex;
    std::thread commandProcessorThread;
    std::thread monitorThread;
    bool running = false;
    
    /**
     * @brief Helper to convert std::vector<std::string> to char* const* for execv.
     */
    char** createArgv(const std::string& path, const std::vector<std::string>& args);

    /**
     * @brief Helper to free memory allocated by createArgv.
     */
    void freeArgv(char** argv);

    /**
     * @brief Logic to start an external program via fork and execv.
     */
    void startProgram(const Command& cmd);

    /**
     * @brief Sends a specified signal to a tracked process and updates its status.
     */
    void controlProcess(const std::string& processId, int signalVal, const std::string& newStatus);
    
    /**
     * @brief Prints the status of all or a specific tracked process.
     */
    void printStatus(const std::string& commandId = "");

    /**
     * @brief The main loop for processing commands from the queue (runs in its own thread).
     */
    void processCommands();
    
    /**
     * @brief The loop for monitoring exited children (runs in its own thread).
     */
    void monitorProcesses();

    /**
     * @brief Gracefully terminates all remaining tracked processes on shutdown.
     */
    void cleanupProcesses();

public:
    /**
     * @brief Constructs a new ProcessManager.
     * @param mq Reference to the thread-safe message queue.
     */
    ProcessManager(MessageQueue* mq);
    
    /**
     * @brief Starts the command processor and monitor worker threads.
     */
    void start();

    /**
     * @brief Initiates a graceful shutdown of the manager and its threads.
     */
    void stop();
};

#endif // PROCESS_MANAGER_H