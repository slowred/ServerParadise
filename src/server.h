#pragma once

#include <boost/asio.hpp>
#include <string>
#include <memory>
#include <functional>
#include <array>
#include "database.h"
#include "logger.h"

// Класс, представляющий сессию клиента
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(boost::asio::ip::tcp::socket socket, Database& db);
    
    void start();
    
private:
    void read_request();
    void send_response(const std::string& response);
    
    void process_data(const std::string& data);
    void handle_command(const std::string& command, const std::string& data);
    
    // Обработчики команд
    void handle_get_all_mods();
    void handle_get_mod_by_id(const std::string& data);
    
    boost::asio::ip::tcp::socket socket_;
    boost::asio::streambuf request_buffer_;
    std::string incomplete_data_;
    Database& db_; // Ссылка на базу данных
};

// Класс, представляющий сервер
class Server {
public:
    Server(boost::asio::io_context& io_context, short port, Database& db);
    
private:
    void do_accept();
    
    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    Database& db_;
};

// Не объявляем log_message здесь, так как она уже определена в logger.h 