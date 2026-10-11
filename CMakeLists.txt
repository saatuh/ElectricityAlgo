cmake_minimum_required(VERSION 3.20)

project(ElectricityAlgo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CURL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_executable(ElectricityAlgo
    src/main.cpp
    src/HttpClient.cpp
    src/ElexonClient.cpp
    src/Strategy.cpp
)

target_include_directories(ElectricityAlgo
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(ElectricityAlgo
    PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
)

if(MSVC)
    target_compile_definitions(ElectricityAlgo
        PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN
    )

    target_compile_options(ElectricityAlgo
        PRIVATE /W4 /permissive-
    )
else()
    target_compile_options(ElectricityAlgo
        PRIVATE -Wall -Wextra -Wpedantic
    )
endif()