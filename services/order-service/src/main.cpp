#include "crow.h"
#include "common/db_conn.hpp"
#include "common/rabbitmq.hpp"
#include "common/dto.hpp"
#include "repository.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

const std::string DB_CONN_STR = "postgresql://user:password@postgres:5432/gozon_db";
const std::string RABBIT_HOST = "rabbitmq";

const std::string QUEUE_OUTGOING = "orders_queue";
const std::string QUEUE_INCOMING = "payment_results_queue";

struct CORSHandler {
    struct context {};

    void before_handle(crow::request& req, crow::response& res, context& ctx) {}

    void after_handle(crow::request& req, crow::response& res, context& ctx) {
        res.add_header("Access-Control-Allow-Origin", "*");
        res.add_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.add_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    }
};

void run_outbox_processor(OrderRepository& repo, common::RabbitMQ& rabbit) {
    std::cout << "[Outbox] Worker started..." << std::endl;
    rabbit.connect();

    while (true) {
        try {
            auto payload_opt = repo.process_next_outbox_event();

            if (payload_opt.has_value()) {
                std::string payload = payload_opt.value();
                std::cout << "[Outbox] Processing event: " << payload << std::endl;
                rabbit.publish(payload);
                std::cout << "[Outbox] Event published to RabbitMQ!" << std::endl;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        } catch (const std::exception& e) {
            std::cerr << "[Outbox] Error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
}

void run_result_consumer(OrderRepository& repo, common::RabbitMQ& rabbit) {
    std::cout << "[ResultConsumer] Starting..." << std::endl;
    rabbit.connect();
    rabbit.start_consume();

    while (true) {
        try {
            auto msg_opt = rabbit.consume_message();
            if (msg_opt.has_value()) {
                std::string payload = msg_opt.value();
                std::cout << "[ResultConsumer] Received: " << payload << std::endl;

                auto json = nlohmann::json::parse(payload);
                int order_id = json["order_id"].get<int>();
                std::string status = json["status"].get<std::string>();

                repo.update_order_status(order_id, status);
            }
        } catch (const std::exception& e) {
            std::cerr << "[ResultConsumer] Error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

int main() {
    common::Database db(DB_CONN_STR);
    db.wait_for_connection();

    OrderRepository repo(db);

    common::RabbitMQ rabbit_out(RABBIT_HOST, QUEUE_OUTGOING);
    common::RabbitMQ rabbit_in(RABBIT_HOST, QUEUE_INCOMING);

    std::thread t1(run_outbox_processor, std::ref(repo), std::ref(rabbit_out));
    std::thread t2(run_result_consumer, std::ref(repo), std::ref(rabbit_in));
    t1.detach();
    t2.detach();

    crow::App<CORSHandler> app;

    CROW_ROUTE(app, "/orders").methods(crow::HTTPMethod::POST)([&repo](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json || !json.has("user_id") || !json.has("amount")) {
            return crow::response(400, "Missing fields");
        }
        int user_id = json["user_id"].i();
        int amount = json["amount"].i();
        std::string desc = json.has("description") ? (std::string)json["description"].s() : std::string("");

        int order_id = repo.create_order(user_id, amount, desc);
        if (order_id != -1) {
            crow::json::wvalue resp;
            resp["order_id"] = order_id;
            resp["status"] = "NEW";
            return crow::response(201, resp);
        }
        return crow::response(500, "Failed to create order");
    });

    CROW_ROUTE(app, "/orders/<int>")([&repo](int order_id) {
        auto order = repo.get_order(order_id);
        if (order != nullptr) return crow::response(order.dump());
        return crow::response(404, "Order not found");
    });

    CROW_ROUTE(app, "/orders/user/<int>")([&repo](int user_id) {
        auto orders = repo.get_orders_by_user(user_id);
        return crow::response(orders.dump());
    });

    std::cout << "Starting Order Service on port 8080..." << std::endl;
    app.port(8080).multithreaded().run();
}