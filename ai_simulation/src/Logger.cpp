#include "Logger.h"
#include <iostream>
#include <filesystem>
#include <ctime>
#include <iomanip>
#include <sstream>

// Initialize static members
std::ofstream Logger::characterLog;
std::ofstream Logger::controlLog;
std::mutex Logger::logMutex;
bool Logger::initialized = false;
std::string Logger::logDir;

void Logger::initialize(const std::string& logDirectory) {
    std::lock_guard<std::mutex> lock(logMutex);
    
    if (initialized) {
        shutdown();
    }
    
    logDir = logDirectory;
    
    // Create logs directory if it doesn't exist
    try {
        std::filesystem::create_directories(logDirectory);
    } catch (const std::exception& e) {
        std::cerr << "Failed to create log directory: " << e.what() << std::endl;
        return;
    }
    
    // Get current timestamp for log file names
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream timestamp;
    timestamp << std::put_time(&tm, "%Y%m%d_%H%M%S");
    
    // Open log files (overwrite existing)
    std::string characterLogPath = logDirectory + "/character_log.txt";
    std::string controlLogPath = logDirectory + "/control_log.txt";
    
    characterLog.open(characterLogPath, std::ios::out | std::ios::trunc);
    controlLog.open(controlLogPath, std::ios::out | std::ios::trunc);
    
    if (!characterLog.is_open() || !controlLog.is_open()) {
        std::cerr << "Failed to open log files" << std::endl;
        return;
    }
    
    // Write headers
    characterLog << "=================================================\n";
    characterLog << "  AI Simulation - Character Actions Log\n";
    characterLog << "  Started: " << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "\n";
    characterLog << "=================================================\n\n";
    characterLog.flush();
    
    controlLog << "=================================================\n";
    controlLog << "  AI Simulation - Control & System Log\n";
    controlLog << "  Started: " << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "\n";
    controlLog << "=================================================\n\n";
    controlLog.flush();
    
    initialized = true;
    
    std::cout << "Logger initialized. Logs saved to:\n";
    std::cout << "  - " << characterLogPath << "\n";
    std::cout << "  - " << controlLogPath << "\n";
}

void Logger::log(LogType type, const std::string& message) {
    if (!initialized) {
        // Fallback to console if logger not initialized
        std::cout << message << std::endl;
        return;
    }
    
    std::lock_guard<std::mutex> lock(logMutex);
    
    std::ofstream& targetLog = (type == LogType::CHARACTER) ? characterLog : controlLog;
    
    if (targetLog.is_open()) {
        targetLog << message << std::endl;
        targetLog.flush();
    }
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(logMutex);
    
    if (characterLog.is_open()) {
        characterLog << "\n=================================================\n";
        characterLog << "  Log ended\n";
        characterLog << "=================================================\n";
        characterLog.close();
    }
    
    if (controlLog.is_open()) {
        controlLog << "\n=================================================\n";
        controlLog << "  Log ended\n";
        controlLog << "=================================================\n";
        controlLog.close();
    }
    
    initialized = false;
    
    std::cout << "Logger shut down.\n";
}
