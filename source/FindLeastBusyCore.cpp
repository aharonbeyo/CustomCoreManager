#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <iomanip>

int find_least_busy_core();
// Structure to hold the total and idle jiffies (time slices) for a single core
struct CoreStats {
    long long total = 0;
    long long idle = 0;
    // We only need total and idle for the usage calculation
};

// Global constant for the time delay between samples in milliseconds
const int SAMPLE_DELAY_MS = 200; 

// Function Prototypes
bool read_cpu_stats(std::map<int, CoreStats>& stats_map);
int find_least_busy_core();


/**
 * @brief Reads the total and idle jiffies for all 'cpuN' entries from /proc/stat.
 * @param stats_map Output map to store CoreStats (Core ID -> CoreStats).
 * @return true on success, false on failure.
 */
bool read_cpu_stats(std::map<int, CoreStats>& stats_map) 
{
    stats_map.clear();
    std::ifstream stat_file("/proc/stat");

    if (!stat_file.is_open()) 
    {
        std::cerr << "Error: Could not open /proc/stat" << std::endl;
        return false;
    }

    std::string line;
    int core_id = 0;
    
    // Skip the first "cpu" line which aggregates all cores
    std::getline(stat_file, line); 

    // Read lines starting with "cpu" followed by a digit (cpu0, cpu1, etc.)
    while (std::getline(stat_file, line) && line.substr(0, 3) == "cpu") 
    {
        if (line.length() > 3 && std::isdigit(line[3])) {
            std::stringstream ss(line.substr(4)); // Start parsing after "cpuN "
            
            long long user, nice, system, idle;
            
            // Read the first four critical fields
            if (ss >> user >> nice >> system >> idle) {
                CoreStats stats;
                
                // Total Jiffies = sum of all time spent in all modes
                stats.total = user + nice + system + idle; // Simplified sum of main modes
                stats.idle = idle;
                
                stats_map[core_id] = stats;
                core_id++;
            } else {
                // Handle parsing error for a core line
                std::cerr << "Warning: Failed to parse a cpu line in /proc/stat." << std::endl;
            }
        }
    }

    return !stats_map.empty();
}


/**
 * @brief Determines the least busy logical core based on usage percentage.
 * @return The ID of the least busy core, or -1 on error.
 */
int find_least_busy_core() {
    std::map<int, CoreStats> stats1, stats2;
    
    // --- 1. First Sample (T1) ---
    if (!read_cpu_stats(stats1)) {
        return -1;
    }

    // --- 2. Wait ---
    // Sleep for the defined interval (e.g., 200ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(SAMPLE_DELAY_MS));

    // --- 3. Second Sample (T2) ---
    if (!read_cpu_stats(stats2)) {
        return -1;
    }
    
    int least_busy_core = -1;
    double min_usage = 100.0; // Initialize with max possible usage

    // --- 4. Calculate Usage and Compare ---
    for (const auto& pair1 : stats1) {
        int core_id = pair1.first;
        const CoreStats& s1 = pair1.second;

        // Check if the core exists in the second sample
        if (stats2.count(core_id)) {
            const CoreStats& s2 = stats2.at(core_id);
            
            // Calculate differences (Delta)
            long long delta_total = s2.total - s1.total;
            long long delta_idle = s2.idle - s1.idle;
            
            if (delta_total > 0) {
                // Usage = (Delta_Total - Delta_Idle) / Delta_Total
                double usage_percent = 100.0 * (delta_total - delta_idle) / delta_total;
                
                // Debugging output (Optional)
                std::cout << std::fixed << std::setprecision(2) 
                          << "Core " << core_id << " Usage: " 
                          << usage_percent << "%" << std::endl;

                // Find the core with the minimum usage
                if (usage_percent < min_usage) {
                    min_usage = usage_percent;
                    least_busy_core = core_id;
                }
            }
        }
    }
    
    return least_busy_core;
}