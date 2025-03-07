#include <iostream>
#include <boost/asio.hpp>
#include <thread>
#include <vector>
#include "server.h"
#include "database.h"
#include "logger.h"
#include <boost/asio/signal_set.hpp>
#include <fstream>
#include <nlohmann/json.hpp>

// Используем библиотеку nlohmann/json
using json = nlohmann::json;

// Функция для вывода заголовка программы
void print_banner() {
    std::cout << "=================================================" << std::endl;
    std::cout << "      Paradise Mod Server - версия 1.0.0          " << std::endl;
    std::cout << "    Сервер для обработки запросов модификаций     " << std::endl;
    std::cout << "=================================================" << std::endl;
}

int main(int argc, char* argv[]) {
    try {
        // Загружаем конфигурацию из файла
        std::ifstream config_file("config.json");
        if (!config_file.is_open()) {
            log_message("Не удалось открыть файл конфигурации. Используем значения по умолчанию.", "WARNING");
            config_file.open("config.example.json");
            if (!config_file.is_open()) {
                log_message("Не удалось открыть пример файла конфигурации. Выход.", "ERROR");
                return 1;
            }
        }
        
        json config;
        config_file >> config;
        
        // Получаем параметры из конфигурации
        const int port = config["server"]["port"];
        const int thread_count = config["server"]["thread_count"];
        std::string db_host = config["database"]["host"];
        std::string db_user = config["database"]["user"];
        std::string db_password = config["database"]["password"];
        std::string db_name = config["database"]["dbname"];

        std::cout << "=================================================" << std::endl;
        std::cout << "      Paradise Mod Server - версия 1.0.0" << std::endl;
        std::cout << "    Сервер для обработки запросов модификаций" << std::endl;
        std::cout << "=================================================" << std::endl;
        
        std::cout << "Параметры запуска:" << std::endl;
        std::cout << "- Порт: " << port << std::endl;
        std::cout << "- Количество рабочих потоков: " << thread_count << std::endl;
        std::cout << "- MySQL соединение: " << db_host << ", БД: " << db_name << std::endl;

        boost::asio::io_context io_context;

        // Создаем work guard как shared_ptr, чтобы он жил все время работы программы
        auto work_guard = std::make_shared<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
            boost::asio::make_work_guard(io_context));

        // Обработка сигналов завершения (Ctrl+C)
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait(
            [&io_context, work_guard](const boost::system::error_code& ec, int signal_number) {
                log_message("Получен сигнал завершения, останавливаем сервер...", "INFO");
                // Сбрасываем work guard только при получении сигнала завершения
                work_guard->reset();
                io_context.stop();
            });

        std::cout << "Подключение к базе данных..." << std::endl;
        Database db(db_host, db_user, db_password, db_name);
        if (!db.connectToDatabase()) {
            log_message("Failed to connect to database. Exiting...", "ERROR");
            return 1;
        }
        std::cout << "√ Успешное подключение к базе данных" << std::endl;

        std::cout << "Запуск сервера на порту " << port << "..." << std::endl;
        Server server(io_context, port, db);
        
        std::cout << "√ Сервер запущен и готов принимать соединения" << std::endl;
        std::cout << "=================================================" << std::endl;

        // Запускаем рабочие потоки
        std::vector<std::thread> threads;
        for (int i = 0; i < thread_count - 1; ++i) {
            threads.emplace_back([&io_context]() {
                try {
                    io_context.run();
                } catch (const std::exception& e) {
                    log_message("Thread error: " + std::string(e.what()), "ERROR");
                }
            });
        }

        // Основной поток также выполняет io_context
        try {
            io_context.run();
        } catch (const std::exception& e) {
            log_message("Main thread error: " + std::string(e.what()), "ERROR");
        }

        // Ждем завершения всех потоков
        for (auto& thread : threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }

        log_message("Сервер успешно остановлен", "INFO");
        return 0;
    }
    catch (std::exception& e) {
        log_message("Exception: " + std::string(e.what()), "ERROR");
        return 1;
    }
} 