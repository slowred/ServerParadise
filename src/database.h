#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <vector>
#include <optional>
#include <mysql.h>
#include <chrono>
#include <mutex>
#include "logger.h"

struct ModData {
    int id;
    std::string name;
    std::string description;
    std::string link;
    std::vector<std::string> media_links;
    std::string category;
};

class Database {
public:
    Database(const std::string& host, const std::string& user, 
             const std::string& password, const std::string& db_name);
    ~Database();

    bool connectToDatabase();
    bool reconnect();
    bool ping();
    std::vector<ModData> getAllMods();
    std::optional<ModData> getModById(int mod_id);

private:
    void processMySQLResult(MYSQL_RES* result, std::vector<ModData>& mods);
    bool checkConnection();
    
    MYSQL* mysql;
    std::string host;
    std::string user;
    std::string password;
    std::string db_name;
    std::chrono::steady_clock::time_point last_ping;
    static constexpr auto PING_INTERVAL = std::chrono::seconds(60);
    
    // Заменяем обычный мьютекс на рекурсивный мьютекс
    std::recursive_mutex db_mutex;
};

#endif // DATABASE_H 