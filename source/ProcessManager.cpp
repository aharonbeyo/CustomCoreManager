#include "ProcessManager.h"
#include <iostream>
#include <sstream>
#include <chrono>

// OS-specific headers for implementation details
#include <unistd.h>      // For fork() and execv()
#include <sys/wait.h>    // For waitpid()
#include <cstring>       // For strdup(), strerror(), strsignal()
#include <algorithm>     // For std::vector manipulation
#include <errno.h>       // For errno

#include "MessageQueue.h"
#include "Config.h"
#include "MqMessage.h"

ProcessManager::ProcessManager(MessageQueue* mq) : queue(mq) 
    {}

// Helper to convert std::vector<std::string> to char* const* required by execv
char** ProcessManager::createArgv(const std::string& path, const std::vector<std::string>& args) 
{
    // execv expects the program path itself as the first argument (argv[0])
    std::vector<std::string> fullArgs = {path};
    fullArgs.insert(fullArgs.end(), args.begin(), args.end());
    
    // Allocate space for char pointers (one more for the nullptr terminator)
    char** argv = new char*[fullArgs.size() + 1];
    for (size_t i = 0; i < fullArgs.size(); ++i) {
        // strdup is used to allocate new C-style strings in the heap
        argv[i] = strdup(fullArgs[i].c_str());
    }
    argv[fullArgs.size()] = nullptr; // Null-terminated array
    return argv;
}

void ProcessManager::freeArgv(char** argv) {
    if (!argv) return;
    // Free the individual strings allocated by strdup
    for (int i = 0; argv[i] != nullptr; ++i) {
        free(argv[i]);
    }
    // Free the array of pointers
    delete[] argv;
}

void ProcessManager::startProgram(const Command& cmd) {
    if (cmd.programPath.empty()) 
    {
        std::cerr << "[ERROR] 'programPath' missing for START command ID " << cmd.id << std::endl;
        return;
    }

    std::lock_guard<std::mutex> lock(trackerMutex);
    if (runningProcesses.count(cmd.id)) 
    {
        std::cout << "[INFO] Process ID " << cmd.id << " is already running." << std::endl;
        return;
    }

    char** argv = createArgv(cmd.programPath, cmd.args);
    pid_t pid = fork();

    if (pid == -1) 
    {
        std::cerr << "[ERROR] Failed to fork process for ID " << cmd.id << std::endl;
        freeArgv(argv);
        return;
    } 
    else if (pid == 0) 
    {
         std::cout << "[DEBUG] In child process before execv for ID " << cmd.id << std::endl;
        // Child process: Execute the new program
        if (execv(cmd.programPath.c_str(), argv) == -1)
        {
            perror("execv failed"); 
            freeArgv(argv);
            _exit(EXIT_FAILURE); // Use _exit to skip cleanup, especially flush buffers
        }
    } 
    else 
    {
        // Parent process: Track the new process
        freeArgv(argv); // Free arguments in the parent process memory

        TrackedProcess newProc;
        newProc.pid = pid;
        newProc.status = "running";
        newProc.path = cmd.programPath;
        newProc.startTime = std::chrono::time_point_cast<std::chrono::seconds>(
            std::chrono::system_clock::now()).time_since_epoch().count();

        runningProcesses[cmd.id] = newProc;

        std::cout << "[SUCCESS] Started program '" << cmd.programPath << "'.\n";
        std::cout << "          -> Assigned ID: " << cmd.id << ", OS PID: " << pid << std::endl;
    }
}

void ProcessManager::controlProcess(const std::string& processId, int signalVal, const std::string& newStatus) {
    std::lock_guard<std::mutex> lock(trackerMutex);

    if (!runningProcesses.count(processId)) {
        std::cerr << "[ERROR] Process ID " << processId << " not found in tracker." << std::endl;
        return;
    }

    TrackedProcess& proc = runningProcesses.at(processId);
    pid_t pid = proc.pid;

    // Fast check if process has already finished (WNOHANG ensures non-blocking check)
    int status;
    if (waitpid(pid, &status, WNOHANG) != 0 && (WIFEXITED(status) || WIFSIGNALED(status))) {
        std::cout << "[INFO] Process " << processId << " (PID " << pid << ") already exited. Updating status." << std::endl;
        proc.status = "finished";
        return;
    }
    
    // Send the signal
    if (kill(pid, signalVal) == -1) {
        std::cerr << "[ERROR] Failed to send signal (" << strsignal(signalVal) 
                  << ") to PID " << pid << ": " << strerror(errno) << std::endl;
    } 
    else 
    {
        proc.status = newStatus;
        std::cout << "[SUCCESS] Sent " << strsignal(signalVal) << " to process ID " 
                  << processId << " (PID " << pid << ").\n";
        std::cout << "          -> New Status: " << newStatus << std::endl;
        
        if (newStatus == "terminated") 
        {
            // Use a blocking wait to ensure the zombie is reaped immediately after termination signal
            waitpid(pid, nullptr, 0); 
            runningProcesses.erase(processId);
            std::cout << "[SUCCESS] Process ID " << processId << " reaped and removed from tracker." << std::endl;
        }
    }
}

void ProcessManager::printStatus(const std::string& commandId) {
    std::lock_guard<std::mutex> lock(trackerMutex);
    std::cout << "\n" << std::string(50, '-') << std::endl;

    if (runningProcesses.empty() && commandId.empty()) {
        std::cout << "No processes currently being tracked." << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        return;
    }

    std::cout << "--- Process Status Report (Total: " << runningProcesses.size() << ") ---" << std::endl;
    
    std::vector<std::string> keys;
    if (commandId.empty()) {
        for (const auto& pair : runningProcesses) keys.push_back(pair.first);
    } else {
        if (runningProcesses.count(commandId)) keys.push_back(commandId);
        else {
            std::cout << "Process ID " << commandId << " not found." << std::endl;
            std::cout << std::string(50, '-') << std::endl;
            return;
        }
    }
    
    for (const auto& c_id : keys) {
        const TrackedProcess& p_info = runningProcesses.at(c_id);
        long long runningTime = std::chrono::time_point_cast<std::chrono::seconds>(
            std::chrono::system_clock::now()).time_since_epoch().count() - p_info.startTime;

        std::cout << "\n[ID: " << c_id << "] (PID: " << p_info.pid << ") - Status: " 
                  << p_info.status << "\n";
        std::cout << "  > Path: " << p_info.path << " | Running for: " << runningTime << "s" << std::endl;
    }

    std::cout << std::string(50, '-') << std::endl;
}

void ProcessManager::processCommands() 
{
    Command cmd;
 //   usleep (2000000); // Sleep for 2 seconds to allow setup

    while (running) 
    {
        std::string raw = queue->receive();
        MQMessage msg = MQMessage::deserialize(raw);
        std::cout << "\n-- Received Message --\n";
        std::cout << "Command: " << msg.command << "\n";    
        // Here you would convert msg to Command struct
        // For demonstration, let's assume msg.command is the action and msg.parameters contains other fields
        cmd.action = msg.command;
        cmd.programPath = msg.parameters.value("ProgramPath", "");
        cmd.args = msg.parameters.value("Args", std::vector<std::string>{});
      //  std::cout << "\n[PROCESSOR] Received command: ID=" << cmd.id << ", Action=" << cmd.action << std::endl;

        if (cmd.action == "StartJob") 
        {
         startProgram(cmd);
        } 
        else if (cmd.action == "pause") {
            controlProcess(cmd.id, SIG_PAUSE, "paused");
        } else if (cmd.action == "resume") {
            controlProcess(cmd.id, SIG_RESUME, "running");
        } else if (cmd.action == "terminate") {
            controlProcess(cmd.id, SIG_TERMINATE, "terminated");
        } else if (cmd.action == "status") {
            printStatus(cmd.id.empty() ? cmd.processId : cmd.id);
        } else {
            std::cerr << "[ERROR] Unknown action '" << cmd.action << "' for ID " << cmd.id << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); 
     }
     
    std::cout << "[WORKER] Command Processor thread stopped." << std::endl;
}

void ProcessManager::monitorProcesses() {
    while (running) {
        int status;
        // WNOHANG: returns immediately if no child has exited
        pid_t pid = waitpid(-1, &status, WNOHANG); 

        if (pid > 0) 
        {
            // A child process has exited
            std::lock_guard<std::mutex> lock(trackerMutex);
            for (auto it = runningProcesses.begin(); it != runningProcesses.end(); ) {
                if (it->second.pid == pid) 
                {
                    std::cout << "\n[MONITOR] Child process ID " << it->first << " (PID " << pid << ") finished.\n";
                    
                    if (WIFEXITED(status)) 
                    {
                        std::cout << "          Exit Code: " << WEXITSTATUS(status) << std::endl;
                    } 
                    else if (WIFSIGNALED(status)) {
                         std::cout << "          Terminated by Signal: " << WTERMSIG(status) << " (" << strsignal(WTERMSIG(status)) << ")" << std::endl;
                    }
                    
                    it = runningProcesses.erase(it); // Remove finished process
                } 
                else 
                {
                    ++it;
                }
            }
        } 
        else if (pid == -1 && errno != ECHILD) 
        {
            // An error occurred that wasn't "no more children"
            std::cerr << "[MONITOR ERROR] waitpid failed: " << strerror(errno) << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << "[MONITOR] Process Monitor thread stopped." << std::endl;
}

void ProcessManager::start() {
    running = true;
    commandProcessorThread = std::thread(&ProcessManager::processCommands, this);
  //  commandProcessorThread.detach();
    monitorThread = std::thread(&ProcessManager::monitorProcesses, this);
    std::cout << "[MANAGER] Process Manager started." << std::endl;
}

void ProcessManager::cleanupProcesses() {
    std::lock_guard<std::mutex> lock(trackerMutex);
    if (runningProcesses.empty()) return;

    std::cout << "\n[CLEANUP] Terminating " << runningProcesses.size() << " remaining processes..." << std::endl;
    
    // Iterate over a copy of keys to avoid iterator invalidation
    std::vector<std::string> ids_to_terminate;
    for (const auto& pair : runningProcesses) {
        ids_to_terminate.push_back(pair.first);
    }

    for (const auto& id : ids_to_terminate) {
        if (runningProcesses.count(id)) {
            pid_t pid = runningProcesses.at(id).pid;
            std::cout << "[CLEANUP] Sending SIGTERM to process ID " << id << " (PID " << pid << ")." << std::endl;
            if (kill(pid, SIG_TERMINATE) == 0) {
                // We'll wait for the process to be reaped by the monitor thread
                // or handle the wait here for immediate cleanup.
                waitpid(pid, nullptr, 0); 
            }
        }
    }
    
    runningProcesses.clear(); // Clear the map after attempting cleanup
}

void ProcessManager::stop() 
{
    running = false;
 //   queue.stop(); // Unblock waiting threads
    
    // Cleanup must happen before threads join, but after 'running' is false
    cleanupProcesses();
        
    // Wait for worker threads to finish
    if (commandProcessorThread.joinable()) {
        commandProcessorThread.join();
    }
    if (monitorThread.joinable()) {
        monitorThread.join();
    }
    
    std::cout << "[MANAGER] Process Manager stopped." << std::endl;
}


