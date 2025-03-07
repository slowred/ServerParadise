#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

struct ModData {
    int id;
    std::string name;
    std::string description;
    std::string link;
    std::vector<std::string> media_links;

    // Convert to JSON
    nlohmann::json to_json() const {
        return {
            {"id", id},
            {"name", name},
            {"description", description},
            {"link", link},
            {"media_links", media_links}
        };
    }
}; 