#include "database.h"
#include "logger.h"
#include <sstream>
#include <iostream>
#include <chrono>
#include <mutex>

Database::Database(const std::string& host, const std::string& user, 
                   const std::string& password, const std::string& db_name)
    : host(host), user(user), password(password), db_name(db_name), mysql(nullptr) {
}

Database::~Database() {
    if (mysql) {
        mysql_close(mysql);
    }
}

bool Database::connectToDatabase() {
    // Защищаем доступ к БД мьютексом
    std::lock_guard<std::recursive_mutex> lock(db_mutex);
    
    mysql = mysql_init(nullptr);
    if (!mysql) {
        log_message("Error initializing MySQL", "ERROR");
        return false;
    }

    // Устанавливаем таймаут подключения
    int timeout = 5;
    mysql_options(mysql, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

    // Включаем автоматическое переподключение
    bool reconnect = 1;
    mysql_options(mysql, MYSQL_OPT_RECONNECT, &reconnect);

    // Устанавливаем таймаут чтения
    int read_timeout = 30;
    mysql_options(mysql, MYSQL_OPT_READ_TIMEOUT, &read_timeout);

    // Устанавливаем таймаут записи
    int write_timeout = 30;
    mysql_options(mysql, MYSQL_OPT_WRITE_TIMEOUT, &write_timeout);

    if (!mysql_real_connect(mysql, host.c_str(), user.c_str(), 
                          password.c_str(), db_name.c_str(), 
                          0, nullptr, 0)) {
        log_message("Failed to connect to database: " + 
                   std::string(mysql_error(mysql)), "ERROR");
        return false;
    }

    last_ping = std::chrono::steady_clock::now();
    log_message("Successfully connected to database", "INFO");
    return true;
}

bool Database::reconnect() {
    // Защищаем доступ к базе данных мьютексом
    std::lock_guard<std::recursive_mutex> lock(db_mutex);
    
    if (mysql) {
        mysql_close(mysql);
        mysql = nullptr;
    }
    return connectToDatabase();
}

bool Database::ping() {
    // Защищаем доступ к базе данных мьютексом
    std::lock_guard<std::recursive_mutex> lock(db_mutex);
    
    if (!mysql) return false;
    return mysql_ping(mysql) == 0;
}

bool Database::checkConnection() {
    // Защищаем доступ к базе данных мьютексом
    std::lock_guard<std::recursive_mutex> lock(db_mutex);
    
    auto now = std::chrono::steady_clock::now();
    if (now - last_ping > PING_INTERVAL) {
        if (!ping()) {
            log_message("Lost connection to MySQL (ping failed), attempting to reconnect...", "WARNING");
            return reconnect();
        }
        last_ping = now;
    }
    return true;
}

std::vector<ModData> Database::getAllMods() {
    std::vector<ModData> mods;
    
    // Защищаем доступ к базе данных мьютексом
    std::lock_guard<std::recursive_mutex> lock(db_mutex);
    
    if (!checkConnection()) {
        log_message("Database connection check failed", "ERROR");
        return mods;
    }

    const char* query = "SELECT id, name, description, link, category FROM mods";
    
    if (mysql_query(mysql, query)) {
        std::string error = mysql_error(mysql);
        log_message("Error executing query: " + error, "ERROR");
        
        if (error.find("Lost connection") != std::string::npos) {
            log_message("Attempting to reconnect to database...", "INFO");
            if (reconnect() && !mysql_query(mysql, query)) {
                MYSQL_RES* result = mysql_store_result(mysql);
                if (result) {
                    processMySQLResult(result, mods);
                    mysql_free_result(result);
                    return mods;
                }
            }
        }
        return mods;
    }

    MYSQL_RES* result = mysql_store_result(mysql);
    if (result) {
        processMySQLResult(result, mods);
        mysql_free_result(result);
    }
    
    return mods;
}

void Database::processMySQLResult(MYSQL_RES* result, std::vector<ModData>& mods) {
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        ModData mod;
        mod.id = row[0] ? std::stoi(row[0]) : 0;
        mod.name = row[1] ? row[1] : "";
        mod.description = row[2] ? row[2] : "";
        mod.link = row[3] ? row[3] : "";
        mod.category = row[4] ? row[4] : "Общее";

        std::string media_query = "SELECT media_link FROM mod_media WHERE mod_id = " + std::to_string(mod.id);
        if (!mysql_query(mysql, media_query.c_str())) {
            MYSQL_RES* media_result = mysql_store_result(mysql);
            if (media_result) {
                MYSQL_ROW media_row;
                while ((media_row = mysql_fetch_row(media_result))) {
                    if (media_row[0]) {
                        mod.media_links.push_back(media_row[0]);
                    }
                }
                mysql_free_result(media_result);
            }
        }
        
        mods.push_back(mod);
    }
}

std::optional<ModData> Database::getModById(int mod_id) {
    // Защищаем доступ к базе данных мьютексом
    std::lock_guard<std::recursive_mutex> lock(db_mutex);
    
    if (!mysql) {
        log_message("Not connected to database", "ERROR");
        return std::nullopt;
    }
    
    std::string query = "SELECT id, name, description, link FROM mods WHERE id = " + std::to_string(mod_id);
    if (mysql_query(mysql, query.c_str())) {
        log_message("Error executing query: " + std::string(mysql_error(mysql)), "ERROR");
        return std::nullopt;
    }
    
    MYSQL_RES* result = mysql_store_result(mysql);
    if (!result) {
        log_message("Error getting results: " + std::string(mysql_error(mysql)), "ERROR");
        return std::nullopt;
    }
    
    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row) {
        mysql_free_result(result);
        return std::nullopt;
    }
    
    ModData mod;
    mod.id = std::stoi(row[0]);
    mod.name = row[1] ? row[1] : "";
    mod.description = row[2] ? row[2] : "";
    mod.link = row[3] ? row[3] : "";
    
    // Get the media links
    query = "SELECT media_link FROM mod_media WHERE mod_id = " + std::to_string(mod.id);
    if (!mysql_query(mysql, query.c_str())) {
        MYSQL_RES* media_result = mysql_store_result(mysql);
        if (media_result) {
            MYSQL_ROW media_row;
            while ((media_row = mysql_fetch_row(media_result))) {
                if (media_row[0]) {
                    mod.media_links.push_back(media_row[0]);
                }
            }
            mysql_free_result(media_result);
        }
    }
    
    mysql_free_result(result);
    return mod;
} 