#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <mutex>

/**
 * @brief Logging utility for the AI Simulation Game
 * 
 * Provides separate log files for:
 * - Character actions (warriors, medics, suppliers, commanders)
 * - Control/system events (keyboard input, game state changes)
 * 
 * Log files are cleared at the start of each simulation run.
 */
class Logger {
public:
    enum class LogType {
        CHARACTER,  // Character actions and decisions
        CONTROL     // Control inputs and system events
    };

    /**
     * @brief Initialize the logger and create log files
     * @param logDirectory Directory where log files will be created
     */
    static void initialize(const std::string& logDirectory = "logs");

    /**
     * @brief Log a message to the specified log file
     * @param type Which log file to write to
     * @param message The message to log
     */
    static void log(LogType type, const std::string& message);

    /**
     * @brief Close log files (called on shutdown)
     */
    static void shutdown();

    /**
     * @brief Check if logger is initialized
     */
    static bool isInitialized() { return initialized; }

private:
    static std::ofstream characterLog;
    static std::ofstream controlLog;
    static std::mutex logMutex;
    static bool initialized;
    static std::string logDir;
};

#endif // LOGGER_H
