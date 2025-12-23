#include "crow.h"
#include "common/db_conn.hpp"
#include "common/rabbitmq.hpp"
#include "common/dto.hpp"
#include "repository.hpp"
#include <iostream>
#include <thread>
#include <chrono>

const std::string DB_CONN_STR = "postgresql://user:password@postgres:5432/gozon_db";
const std::string RABBIT_HOST = "rabbitmq";

const std::string QUEUE_INCOMING = "orders_queue";
const std::string QUEUE_OUTGOING = "payment_results_queue";

struct CORSHandler {
    struct context {};

    void before_handle(crow::request& req, crow::response& res, context& ctx) {}

    void after_handle(crow::request& req, crow::response& res, context& ctx) {
        res.add_header("Access-Control-Allow-Origin", "*");
        res.add_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.add_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    }
};

void run_payment_consumer(PaymentRepository& repo, common::RabbitMQ& rabbit) {
    std::cout << "[Consumer] Starting..." << std::endl;
    rabbit.connect();
    rabbit.start_consume();

    while (true) {
        try {
            auto msg_opt = rabbit.consume_message();
            if (msg_opt.has_value()) {
                std::string payload = msg_opt.value();
                std::cout << "[Consumer] Received: " << payload << std::endl;

                auto json = nlohmann::json::parse(payload);
                common::OrderCreatedEvent event = json.get<common::OrderCreatedEvent>();

                repo.process_payment(event.order_id, event.user_id, event.amount);
            }
        } catch (const std::exception& e) {
            std::cerr << "[Consumer] Error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void run_payment_outbox(PaymentRepository& repo, common::RabbitMQ& rabbit) {
    std::cout << "[Outbox] Starting..." << std::endl;
    rabbit.connect();

    while (true) {
        try {
            auto payload_opt = repo.process_next_outbox_event();
            if (payload_opt.has_value()) {
                std::string payload = payload_opt.value();
                std::cout << "[Outbox] Sending result: " << payload << std::endl;

                rabbit.publish(payload);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        } catch (const std::exception& e) {
            std::cerr << "[Outbox] Error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
}

int main() {
    common::Database db(DB_CONN_STR);
    db.wait_for_connection();

    PaymentRepository repo(db);

    common::RabbitMQ rabbit_consumer(RABBIT_HOST, QUEUE_INCOMING);
    common::RabbitMQ rabbit_producer(RABBIT_HOST, QUEUE_OUTGOING);

    std::thread t1(run_payment_consumer, std::ref(repo), std::ref(rabbit_consumer));
    std::thread t2(run_payment_outbox, std::ref(repo), std::ref(rabbit_producer));
    t1.detach();
    t2.detach();

    crow::App<CORSHandler> app;

    CROW_ROUTE(app, "/account").methods(crow::HTTPMethod::POST)([&repo](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json || !json.has("user_id")) return crow::response(400);
        if (repo.create_account(json["user_id"].i())) return crow::response(201, "Account created");
        return crow::response(500);
    });

    CROW_ROUTE(app, "/account/topup").methods(crow::HTTPMethod::POST)([&repo](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json || !json.has("user_id") || !json.has("amount")) return crow::response(400);
        if (repo.top_up(json["user_id"].i(), json["amount"].i())) return crow::response(200, "Balance updated");
        return crow::response(404, "User not found");
    });

    CROW_ROUTE(app, "/account/balance")([&repo](const crow::request& req) {
        char* uid = req.url_params.get("user_id");
        if (!uid) return crow::response(400);
        auto bal = repo.get_balance(std::stoi(uid));
        if (bal) {
            crow::json::wvalue x;
			x["balance"] = *bal;
			x["user_id"] = std::stoi(uid);
            return crow::response(x);
        }
        return crow::response(404);
    });

    std::cout << "Starting Payment Service on port 8080..." << std::endl;
    app.port(8080).multithreaded().run();
}