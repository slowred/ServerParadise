#include "server.h"
#include "logger.h"
#include <iostream>
#include <nlohmann/json.hpp>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>
#include <mutex>

using json = nlohmann::json;

// Function to get the current time in string format for logs
std::string get_current_time() {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    #ifdef _WIN32
    // Safe version for Windows
    struct tm timeinfo;
    localtime_s(&timeinfo, &now_c);
    ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
    #else
    // For other platforms
    ss << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S");
    #endif
    return ss.str();
}

Session::Session(boost::asio::ip::tcp::socket socket, Database& db)
    : socket_(std::move(socket)), db_(db) {
}

void Session::start() {
    read_request();
}

void Session::read_request() {
    auto self(shared_from_this());
    
    // Возвращаемся к чтению до одинарного перевода строки для первой части запроса
    boost::asio::async_read_until(
        socket_,
        request_buffer_,
        '\n',
        [this, self](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                std::string command;
                std::istream is(&request_buffer_);
                std::getline(is, command);
                
                log_message("Received command: " + command, "DEBUG");
                
                // Проверяем, требует ли команда дополнительных данных
                if (command == "GET_MOD_BY_ID") {
                    // Если команда требует данных, читаем следующую строку
                    boost::asio::async_read_until(
                        socket_,
                        request_buffer_,
                        '\n',
                        [this, self, command](boost::system::error_code ec2, std::size_t length2) {
                            if (!ec2) {
                                std::string data;
                                std::istream is2(&request_buffer_);
                                std::getline(is2, data);
                                
                                log_message("Received data for command: '" + data + "'", "DEBUG");
                                handle_command(command, data);
                            } else {
                                // Обработка ошибок при чтении данных
                                log_message("Error reading command data: " + ec2.message(), "ERROR");
                                handle_command(command, ""); // Пустые данные
                            }
                        });
                } else {
                    // Если команда не требует данных, обрабатываем ее сразу
                    handle_command(command, "");
                }
            } else {
                // Если клиент отключился или произошла ошибка
                if (ec == boost::asio::error::eof || 
                    ec == boost::asio::error::connection_reset) {
                    log_message("Client disconnected: " + 
                        socket_.remote_endpoint().address().to_string() + ":" +
                        std::to_string(socket_.remote_endpoint().port()), "INFO");
                } else {
                    log_message("Error reading from client: " + ec.message(), "ERROR");
                }
                
                // Закрываем сокет правильно
                boost::system::error_code ignored_ec;
                socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
                socket_.close(ignored_ec);
            }
        });
}

void Session::send_response(const std::string& response) {
    auto self(shared_from_this());
    
    log_message("Sending response: '" + response + "'", "DEBUG");
    
    std::string full_response = response + "\n";
    
    boost::asio::async_write(
        socket_,
        boost::asio::buffer(full_response),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                log_message("Response sent successfully", "DEBUG");
                // Очищаем буфер перед чтением следующего запроса
                request_buffer_.consume(request_buffer_.size());
                // Продолжаем чтение следующего запроса
                read_request();
            } else {
                log_message("Error sending response: " + ec.message(), "ERROR");
                // Закрываем сокет только при ошибке
                boost::system::error_code ignored_ec;
                socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
                socket_.close(ignored_ec);
            }
        });
}

void Session::handle_command(const std::string& command, const std::string& data) {
    log_message("Received command: " + command, "INFO");
    
    if (command == "PING") {
        send_response("PONG");
    } else if (command == "GET_ALL_MODS") {
        handle_get_all_mods();
    } else if (command == "GET_MOD_BY_ID") {
        handle_get_mod_by_id(data);
    } else {
        log_message("Unknown command received: " + command, "WARNING");
        send_response("ERROR: Unknown command");
    }
}

void Session::handle_get_all_mods() {
    try {
        log_message("Начинаем обработку запроса GET_ALL_MODS", "DEBUG");
        auto mods = db_.getAllMods();
        
        log_message("Получено " + std::to_string(mods.size()) + " модов из базы данных", "DEBUG");
        
        nlohmann::json json_response = nlohmann::json::array();
        for (const auto& mod : mods) {
            json_response.push_back({
                {"id", mod.id},
                {"name", mod.name},
                {"description", mod.description},
                {"link", mod.link},
                {"media", mod.media_links},
                {"category", mod.category}
            });
        }
        
        log_message("JSON ответ сформирован, размер: " + std::to_string(json_response.dump().size()) + " байт", "DEBUG");
        send_response(json_response.dump());
    } catch (const std::exception& e) {
        log_message("Error in handle_get_all_mods: " + std::string(e.what()), "ERROR");
        send_response("[]");
    }
}

void Session::handle_get_mod_by_id(const std::string& data) {
    try {
        log_message("Начинаем обработку запроса GET_MOD_BY_ID, полученные данные: '" + data + "'", "DEBUG");
        
        // Убираем только символы перевода строки и пробелы по краям
        std::string clean_data = data;
        // Убираем начальные пробелы и символы новой строки
        clean_data.erase(0, clean_data.find_first_not_of(" \n\r\t"));
        // Убираем конечные пробелы и символы новой строки
        clean_data.erase(clean_data.find_last_not_of(" \n\r\t") + 1);
        
        log_message("Очищенные данные: '" + clean_data + "'", "DEBUG");
        
        if (clean_data.empty()) {
            log_message("Получены пустые данные для ID мода", "WARNING");
            send_response("ERROR: Empty mod ID");
            return;
        }
        
        // Получаем ID мода из данных запроса
        int mod_id = std::stoi(clean_data);
        log_message("Запрошен мод с ID: " + std::to_string(mod_id), "DEBUG");
        
        // Получаем данные мода из базы данных
        auto mod = db_.getModById(mod_id);
        
        if (!mod) {
            log_message("Мод с ID " + std::to_string(mod_id) + " не найден", "WARNING");
            send_response("ERROR: Mod not found");
            return;
        }
        
        // Формируем JSON-ответ
        nlohmann::json json_response = {
            {"id", mod->id},
            {"name", mod->name},
            {"description", mod->description},
            {"link", mod->link},
            {"media", mod->media_links},
            {"category", mod->category}
        };
        
        log_message("Отправляем данные мода с ID " + std::to_string(mod_id), "INFO");
        send_response(json_response.dump());
    }
    catch (const std::exception& e) {
        log_message("Ошибка при обработке GET_MOD_BY_ID: " + std::string(e.what()), "ERROR");
        send_response("ERROR: " + std::string(e.what()));
    }
}

Server::Server(boost::asio::io_context& io_context, short port, Database& db)
    : io_context_(io_context)
    , acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
    , db_(db) {
    log_message("Server started on port " + std::to_string(port), "INFO");
    do_accept();
}

void Server::do_accept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            if (!ec) {
                try {
                    std::string client_info = socket.remote_endpoint().address().to_string() + 
                                            ":" + std::to_string(socket.remote_endpoint().port());
                    log_message("New client connected: " + client_info, "INFO");
                    
                    auto session = std::make_shared<Session>(std::move(socket), db_);
                    session->start();
                } catch (const std::exception& e) {
                    log_message("Error accepting connection: " + std::string(e.what()), "ERROR");
                }
            } else {
                log_message("Accept error: " + ec.message(), "ERROR");
            }
            
            // Продолжаем принимать новые соединения
            do_accept();
        });
} 