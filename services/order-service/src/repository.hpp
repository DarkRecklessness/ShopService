#pragma once

#include <pqxx/pqxx>
#include <nlohmann/json.hpp>
#include "common/db_conn.hpp"
#include "common/dto.hpp"

class OrderRepository {
public:
    explicit OrderRepository(common::Database& db)
		: db_(db) {}

	// if return value == -1 => error
    int create_order(int user_id, int amount, const std::string& description) {
        try {
            auto conn = db_.get_connection();
            if (!conn) return -1;

            pqxx::work w(*conn);
            pqxx::result r_order = w.exec_params(
                "INSERT INTO orders (user_id, amount, description, status) "
                "VALUES ($1, $2, $3, 'NEW') RETURNING id",
                user_id, amount, description
            );

            int order_id = r_order[0][0].as<int>();
            common::OrderCreatedEvent event;
            event.order_id = order_id;
            event.user_id = user_id;
            event.amount = amount;

            nlohmann::json payload_json = event;

            w.exec_params(
                "INSERT INTO order_outbox (event_type, payload) VALUES ('ORDER_CREATED', $1)",
                payload_json.dump()
            );

            w.commit();

            return order_id;
        } catch (const std::exception& e) {
            std::cerr << "[OrderRepo] Error creating order: " << e.what() << std::endl;
            return -1;
        }
    }

    std::optional<std::string> process_next_outbox_event() {
        try {
            auto conn = db_.get_connection();
            if (!conn) return std::nullopt;

            pqxx::work w(*conn);

			// find raw record, lock, update status
            pqxx::result r = w.exec(
                "UPDATE order_outbox "
                "SET processed = TRUE "
                "WHERE id = ("
                "   SELECT id FROM order_outbox "
                "   WHERE processed = FALSE "
                "   ORDER BY id ASC "
                "   LIMIT 1 "
                "   FOR UPDATE SKIP LOCKED"
                ") "
                "RETURNING payload"
            );

            w.commit();

            if (r.empty()) {
                return std::nullopt;
            }

            return r[0][0].as<std::string>();
        } catch (const std::exception& e) {
            std::cerr << "[Outbox] Error processing event: " << e.what() << std::endl;
            return std::nullopt;
        }
    }

    nlohmann::json get_order(int order_id) {
        try {
            auto conn = db_.get_connection();
            if (!conn) return nullptr;

            pqxx::work w(*conn);
            pqxx::result r = w.exec_params(
                "SELECT id, user_id, amount, description, status FROM orders WHERE id = $1",
                order_id
            );

            if (r.empty()) return nullptr;

            nlohmann::json j;
            j["id"] = r[0]["id"].as<int>();
            j["user_id"] = r[0]["user_id"].as<int>();
            j["amount"] = r[0]["amount"].as<int>();
            j["description"] = r[0]["description"].as<std::string>();
            j["status"] = r[0]["status"].as<std::string>();
            return j;
        } catch (std::exception& e) {
            return nullptr;
        }
    }

    nlohmann::json get_orders_by_user(int user_id) {
         try {
            auto conn = db_.get_connection();
            if (!conn) return nlohmann::json::array();

            pqxx::work w(*conn);
            pqxx::result r = w.exec_params(
                "SELECT id, amount, status, description FROM orders WHERE user_id = $1",
                user_id
            );

            nlohmann::json orders = nlohmann::json::array();
            for (const auto& row : r) {
                nlohmann::json j;
                j["id"] = row["id"].as<int>();
                j["amount"] = row["amount"].as<int>();
                j["status"] = row["status"].as<std::string>();
                j["description"] = row["description"].as<std::string>();
                orders.push_back(j);
            }
            return orders;
        } catch (std::exception& e) {
             std::cerr << "[OrderRepo] Error getting list: " << e.what() << std::endl;
             return nlohmann::json::array();
        }
    }

	void update_order_status(int order_id, const std::string& status) {
        try {
            auto conn = db_.get_connection();
            if (!conn) return;

            pqxx::work w(*conn);

            pqxx::result r = w.exec_params(
                "UPDATE orders SET status = $1 WHERE id = $2",
                status, order_id
            );

            w.commit();

            if (r.affected_rows() > 0) {
                std::cout << "[OrderRepo] Order " << order_id << " updated to " << status << std::endl;
            } else {
                std::cerr << "[OrderRepo] Order " << order_id << " not found for update!" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "[OrderRepo] Error updating status: " << e.what() << std::endl;
        }
    }

private:
    common::Database& db_;
};