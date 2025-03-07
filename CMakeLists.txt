cmake_minimum_required(VERSION 3.10)
project(ModServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Добавляем определение _WIN32_WINNT для Windows
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)
endif()

# Устанавливаем политики для Boost
if(POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif()
if(POLICY CMP0144)
  cmake_policy(SET CMP0144 NEW)
endif()

# Явно устанавливаем пути к Boost
set(BOOST_ROOT "D:/boost")
set(Boost_INCLUDE_DIR "D:/boost")
set(Boost_LIBRARY_DIRS "D:/boost/stage/lib")
set(Boost_USE_STATIC_LIBS ON)
set(Boost_NO_SYSTEM_PATHS ON)

# Находим Boost
find_package(Boost REQUIRED COMPONENTS system)

# Выводим информацию о найденном Boost для отладки
message(STATUS "Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
message(STATUS "Boost_LIBRARY_DIRS: ${Boost_LIBRARY_DIRS}")
message(STATUS "Boost_LIBRARIES: ${Boost_LIBRARIES}")

# Для MySQL используем ручную настройку
# Убедитесь, что пути соответствуют вашей установке MySQL
set(MYSQL_INCLUDE_DIR "C:/Program Files/MySQL/MySQL Server 8.0/include")
set(MYSQL_LIBRARY "C:/Program Files/MySQL/MySQL Server 8.0/lib/libmysql.lib")

# Проверка наличия необходимых заголовочных файлов
if(NOT EXISTS "${MYSQL_INCLUDE_DIR}/mysql.h")
    message(FATAL_ERROR "MySQL header file not found at ${MYSQL_INCLUDE_DIR}/mysql.h")
endif()

# Скачиваем nlohmann_json, если его нет
set(JSON_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(NOT EXISTS "${JSON_INCLUDE_DIR}/nlohmann/json.hpp")
    file(MAKE_DIRECTORY "${JSON_INCLUDE_DIR}/nlohmann")
    message(STATUS "Downloading nlohmann/json.hpp...")
    file(DOWNLOAD 
        "https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp"
        "${JSON_INCLUDE_DIR}/nlohmann/json.hpp"
        SHOW_PROGRESS
    )
    if(NOT EXISTS "${JSON_INCLUDE_DIR}/nlohmann/json.hpp")
        message(FATAL_ERROR "Failed to download json.hpp")
    else()
        message(STATUS "Successfully downloaded json.hpp")
    endif()
endif()

# Проверяем наличие заголовочных файлов Boost для отладки
if(NOT EXISTS "${Boost_INCLUDE_DIRS}/boost/asio.hpp")
    message(FATAL_ERROR "Boost.Asio header not found at ${Boost_INCLUDE_DIRS}/boost/asio.hpp")
endif()

# Исходные файлы
set(SOURCES
    src/main.cpp
    src/server.cpp
    src/database.cpp
    src/logger.cpp 
)

# Заголовочные файлы
set(HEADERS
    src/server.h
    src/database.h
    include/mod_data.h
)

add_executable(ModServer ${SOURCES} ${HEADERS})

target_include_directories(ModServer PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS}
    ${MYSQL_INCLUDE_DIR}
)

target_link_directories(ModServer PRIVATE
    ${Boost_LIBRARY_DIRS}
)

target_link_libraries(ModServer PRIVATE 
    ${Boost_LIBRARIES}
    ${MYSQL_LIBRARY}
)

# Если MySQL DLL находится не в системном пути, копируем его в выходную директорию
if(WIN32)
    add_custom_command(TARGET ModServer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/Program Files/MySQL/MySQL Server 8.0/lib/libmysql.dll"
        $<TARGET_FILE_DIR:ModServer>
    )
endif() 