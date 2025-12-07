#include <iostream>
#include <unistd.h>     // For fork(), execv()
#include <sched.h>      // For sched_setaffinity(), CPU_SET, etc.
#include <sys/wait.h>   // For wait()
#include <sys/types.h>  // For pid_t
#include <errno.h>
#include <cstring>      // For strerror()

void execute_on_core(int core_id, const char* path, const char* const args[]);



void execute_on_core(int core_id, const char* path, const char* const args[]) 
{
    std::cout << "Attempting to execute program (" << path 
              << ") on least busy core: " << core_id << std::endl;

    // 1. Create a new process
    pid_t pid = fork();

    if (pid == -1) {
        // --- Fork Error ---
        std::cerr << "Error: fork failed: " << strerror(errno) << std::endl;
        return;
    } 
    
    else if (pid == 0) {
        // --- CHILD PROCESS EXECUTION BLOCK ---
        
        // 2. Define the CPU set (the core affinity mask)
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core_id, &cpuset); // Set the bit corresponding to the target core

        // 3. Apply the CPU affinity to the current (child) process
        if (sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) == -1) {
            std::cerr << "Child Error: Failed to set affinity to core " 
                      << core_id << ": " << strerror(errno) << std::endl;
            // It's critical to exit cleanly if execv fails or if you want to abort
            // (Note: We still attempt execv even if affinity fails, as it's not fatal)
        } else {
            std::cout << "Child Process (PID " << getpid() << "): Affinity set to core " 
                      << core_id << std::endl;
        }

        // 4. Replace the child process image with the new program
        // The new process will inherit the affinity mask set above.
        if (execv(path, (char* const*)args) == -1) {
            std::cerr << "Child Error: execv failed for " << path 
                      << ": " << strerror(errno) << std::endl;
            // Terminate the child process if execv fails
            _exit(1); 
        }
        // execv does not return on success; the code below this line is unreachable.
    } 
    
    else {
        // --- PARENT PROCESS BLOCK ---
        int status;
        std::cout << "Parent (PID " << getpid() << "): Launched child with PID " << pid 
                  << ". Waiting for program completion..." << std::endl;

        // 5. Wait for the child process to finish
        waitpid(pid, &status, 0); 
        
        if (WIFEXITED(status)) {
            std::cout << "Parent: Program finished with exit status " << WEXITSTATUS(status) << std::endl;
        } else {
            std::cout << "Parent: Program terminated abnormally." << std::endl;
        }
    }
}

// int main() {
// Assuming you have found this value using the previous functions
// int least_busy_core = 3; // Example: Core 3 was found to be the least busy

// // Target program and arguments
// const char* program_path = "/bin/sleep"; 
// const char* program_args[] = {"sleep", "10", (char*)NULL}; // sleep for 10 seconds
//     // NOTE: In a real application, 'least_busy_core' would be the result 
//     // of your previously written find_least_busy_core() function.
    
//     execute_on_core(least_busy_core, program_path, program_args);

//     return 0;
// }