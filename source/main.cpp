// --- Main Program and Simulation ---
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

#include <iostream>
#include "MessageQueue.h"
#include "Config.h"
#include "MqMessage.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sched.h>
#include <errno.h>  



#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <map>
#include <dirent.h>
#include <algorithm>
#include <cctype>

int find_least_busy_core();
void execute_on_core(int core_id, const char* path, const char* const args[]);

//////////////////////////////////////////////////////////////////////////////////////////////////
// The 39th field in /proc/[pid]/stat is the processor ID
const int CORE_FIELD_INDEX = 39;

// --- 1. Core Retrieval Function (C++ style) ---
// Retrieves the last used core ID for a given PID by parsing /proc/[pid]/stat.
int get_process_core(int pid) {
    // Construct the path to the stat file
    std::string path = "/proc/" + std::to_string(pid) + "/stat";
    std::ifstream stat_file(path);

    if (!stat_file.is_open()) {
        // Process likely terminated or permissions denied
        return -1;
    }

    std::string line;
    if (!std::getline(stat_file, line)) {
        return -1;
    }

    // Use stringstream to easily tokenize the line
    std::stringstream ss(line);
    std::string token;
    int field_count = 0;

    // Iterate through tokens until the 39th field
    while (ss >> token) {
        field_count++;
        if (field_count == CORE_FIELD_INDEX) {
            try {
                // Convert the 39th token (the processor ID) to an integer
                return std::stoi(token);
            } catch (const std::exception& e) {
                // Handle conversion error
                return -1;
            }
        }
    }
    return -1;
}

// --- 2. Main Mapping Function ---
// Iterates /proc, collects PIDs, and maps them to their core IDs.
std::map<int, int> find_all_pids_and_cores() {
    std::map<int, int> pid_core_map;
    
    // Use POSIX functions to open and read the directory (still the standard way)
    DIR *dir = opendir("/proc");
    if (!dir) {
        std::cerr << "Error opening /proc: " << strerror(errno) << std::endl;
        return pid_core_map; // Return empty map
    }

    struct dirent *ent;
    
    while ((ent = readdir(dir)) != NULL) {
        std::string name = ent->d_name;
        
        // Check if the entry name consists only of digits (a PID)
        bool is_pid = std::all_of(name.begin(), name.end(), ::isdigit);

        if (is_pid) {
            try {
                int pid = std::stoi(name);
                int core_id = get_process_core(pid);

                // Only add valid entries to the map
                if (core_id != -1) {
                    pid_core_map[pid] = core_id;
                }
            } catch (const std::exception& e) {
                // Handle conversion error, skip entry
                continue;
            }
        }
    }

    closedir(dir);
    return pid_core_map;
}

// --- 3. Main Function (Testing) ---
// int main() 
// {
//     std::cout << "Scanning /proc filesystem for PID-Core mapping..." << std::endl;
    
//     // Get the map
//     std::map<int, int> pid_core_map = find_all_pids_and_cores();

//     if (pid_core_map.empty()) {
//         std::cout << "No processes found or error occurred during scanning." << std::endl;
//         return 1;
//     }

//     // Print the results
//     std::cout << "\n--- PID to Last Used Core Map (" << pid_core_map.size() << " Processes) ---\n";
//     for (const auto& pair : pid_core_map) {
//         std::cout << "PID: " << pair.first << " -> Core ID: " << pair.second << "\n";
//     }

//     return 0;
// }






////////////////////////////////////////////////


// Function to get the current CPU core number
int get_current_cpu() {
    // This is the preferred way to call sched_getcpu() if <sched.h> is not included
    // or if the standard library wrapper is unavailable.
    // However, including <sched.h> and calling the wrapper is cleaner.
    // For simplicity, let's use the standard C library function call:
    
    // On many Linux systems, sched_getcpu() is available by including <sched.h>
    // If you need the raw syscall, use: syscall(SYS_getcpu, &cpu, &node, NULL);
    
    // We use the function wrapper provided by the C library:
    int cpu = sched_getcpu();
    
    if (cpu == -1) {
        perror("sched_getcpu failed");
    }
    return cpu;
}

int main() 
{
  const int SAMPLE_DELAY_MS = 200;
    MQConfig cfg;
    loadConfig("mq.json", cfg);



  std::cout << "Starting CPU load analysis. Sampling interval: " 
              << SAMPLE_DELAY_MS << "ms." << std::endl;
    
    int least_busy_core = find_least_busy_core();

    std::cout << "------------------------------------------" << std::endl;

    if (least_busy_core != -1) 
    {
        std::cout << "The LEAST busy logical core is: " << least_busy_core << std::endl;
        execute_on_core(least_busy_core, program_path, program_args);
        // --- Next step idea ---
        // You could now integrate this with the fork/execv logic to set
        // the affinity of a new program to this core.
        // E.g.: set_affinity(getpid(), least_busy_core);
    } else {
        std::cerr << "Failed to determine the least busy core." << std::endl;
    }




    std::cout << "Scanning /proc filesystem for PID-Core mapping..." << std::endl;
    
    // Get the map
    std::map<int, int> pid_core_map = find_all_pids_and_cores();

    if (pid_core_map.empty()) {
        std::cout << "No processes found or error occurred during scanning." << std::endl;
        return 1;
    }

    // Print the results
    std::cout << "\n--- PID to Last Used Core Map (" << pid_core_map.size() << " Processes) ---\n";
    for (const auto& pair : pid_core_map) {
        std::cout << "PID: " << pair.first << " -> Core ID: " << pair.second << "\n";
    }






  long num_cores;
  int core_id = get_current_cpu();
    
    if (core_id != -1) 
    {
        printf("The current thread is running on logical core: %d\n", core_id);
    }
    // Use sysconf to get the number of processors that are currently online
    num_cores = sysconf(_SC_NPROCESSORS_ONLN);

    if (num_cores == -1) 
    {
        // Check if the error is an indicator of failure (sysconf returns -1 on error)
        if (errno != 0) {
            perror("Failed to get processor count via sysconf");
            return 1;
        }
        // If sysconf returns -1 but errno is 0, it means the value is indeterminate,
        // but this is rare for _SC_NPROCESSORS_ONLN.
        printf("Could not determine processor count.\n");
        return 1;
    }

    // Output the result
    printf("This system has %ld logical CPU core(s) available.\n", num_cores);

    MQConfig cfg;
    loadConfig("mq.json", cfg);

    MessageQueue mq (cfg, true);  // create & own queue
    ProcessManager pm(&mq);
  //  pm.processCommands();
    pm.start();
   pm.commandProcessorThread.join();
    // while (true) {
    //     std::string raw = mq.receive();
    //     MQMessage msg = MQMessage::deserialize(raw);

    //     std::cout << "\n-- Received Message --\n";
    //     std::cout << "Command: " << msg.command << "\n";
    //     std::cout << "Parameters: " << msg.parameters.dump(4) << "\n";

    //     if (msg.command == "exit")
    //         break;
    // }

//     while (true) {
//         sleep(10000);   
//     }

}
