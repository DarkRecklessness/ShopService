add_executable(order_service_exe src/main.cpp)
target_include_directories(order_service_exe PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(order_service_exe PRIVATE
        Crow::Crow
        nlohmann_json::nlohmann_json
        pqxx
        SimpleAmqpClient
)