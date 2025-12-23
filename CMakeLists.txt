cmake_minimum_required(VERSION 3.14)
project(GozonShop)

set(CMAKE_CXX_STANDARD 17)

find_package(PostgreSQL REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread)

include(FetchContent)
FetchContent_Declare(
        Crow
        GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
        GIT_TAG v1.0+5
)
FetchContent_MakeAvailable(Crow)

FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)


add_subdirectory(services/order-service)
add_subdirectory(services/payment-service)