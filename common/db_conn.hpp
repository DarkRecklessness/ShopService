#pragma once

#include <pqxx/pqxx>
#include <string>
#include <memory>
#include <iostream>
#include <thread>
#include <chrono>

namespace common {

    class Database {
    public:
        explicit Database(const std::string& conn_str)
            : connection_string_(conn_str) {}

        std::shared_ptr<pqxx::connection> get_connection() {
            try {
                auto C = std::make_shared<pqxx::connection>(connection_string_);
                if (C->is_open()) {
                    return C;
                }
            } catch (const std::exception& e) {
                std::cerr << "[DB Error] Connection failed: " << e.what() << std::endl;
            }
            return nullptr;
        }

        void wait_for_connection() {
            while (true) {
                try {
                    pqxx::connection C(connection_string_);
                    if (C.is_open()) {
                        std::cout << "[DB Info] Successfully connected to database." << std::endl;
                        break;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[DB Warning] Waiting for DB... (" << e.what() << ")" << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                }
            }
        }

    private:
        std::string connection_string_;
    };

} // namespace common