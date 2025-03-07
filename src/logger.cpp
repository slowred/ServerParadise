#include "logger.h"
#include <iostream>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <mutex>

// Мьютекс для синхронизации вывода логов
static std::mutex log_mutex;

void log_message(const std::string& message, const std::string& level) {
    // Захватываем мьютекс на время вывода сообщения
    std::lock_guard<std::mutex> lock(log_mutex);
    
    auto now = std::time(nullptr);
    std::stringstream timestamp;
    timestamp << "[" << now << "]";
    
    // Используем один вызов operator<< для атомарности вывода
    std::cout << timestamp.str() << " [" << level << "] " 
              << message << std::endl;
} 