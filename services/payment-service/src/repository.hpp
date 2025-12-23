#pragma once

#include <pqxx/pqxx>
#include <memory>
#include <optional>
#include <nlohmann/json.hpp>
#include "common/db_conn.hpp"
#include "common/dto.hpp"

class PaymentRepository {
public:
    explicit PaymentRepository(common::Database& db)
        : db_(db) {}

    bool create_account(int user_id) {
        try {
            auto conn = db_.get_connection();
            if (!conn) return false;
            pqxx::work w(*conn);
            w.exec_params(
                "INSERT INTO accounts (user_id, balance) VALUES ($1, 0) ON CONFLICT (user_id) DO NOTHING",
                user_id
            );
            w.commit();

            return true;
        } catch (...) {
            return false;
        }
    }

    bool top_up(int user_id, int amount) {
        try {
            auto conn = db_.get_connection();
            if (!conn) return false;
            pqxx::work w(*conn);
            pqxx::result r = w.exec_params(
                "UPDATE accounts SET balance = balance + $1 WHERE user_id = $2 RETURNING balance",
                amount,
                user_id
            );
            w.commit();

            return !r.empty();
        } catch (...) {
            return false;
        }
    }

    std::optional<int> get_balance(int user_id) {
        try {
            auto conn = db_.get_connection();
            if (!conn) return std::nullopt;
            pqxx::work w(*conn);
            pqxx::result r = w.exec_params(
                "SELECT balance FROM accounts WHERE user_id = $1",
                user_id
            );

            if (r.empty()) return std::nullopt;
            return r[0][0].as<int>();
        } catch (...) {
            return std::nullopt;
        }
    }

    void process_payment(int order_id, int user_id, int amount) {
        try {
            auto conn = db_.get_connection();
            if (!conn) return;
            pqxx::work w(*conn);
            std::string msg_id = "order_" + std::to_string(order_id);
            pqxx::result check = w.exec_params(
                "INSERT INTO payment_inbox (message_id) VALUES ($1) ON CONFLICT (message_id) DO NOTHING",
                msg_id
            );

            if (check.affected_rows() == 0) {
                w.commit();
                return;
            }

            std::string status = "FAILED";
            pqxx::result r = w.exec_params(
                "UPDATE accounts SET balance = balance - $1 WHERE user_id = $2 AND balance >= $1 RETURNING balance",
                amount,
                user_id
            );

            if (!r.empty()) status = "PAID";
            else std::cout << "[PaymentRepo] Payment failed for user " << user_id << std::endl;

            common::PaymentResultEvent event;
            event.order_id = order_id;
            event.status = status;
            nlohmann::json payload = event;

            w.exec_params(
                "INSERT INTO payment_outbox (event_type, payload) VALUES ($1, $2)",
                status == "PAID" ? "PAYMENT_SUCCESS" : "PAYMENT_FAILED",
                payload.dump()
            );

            w.commit();
            std::cout << "[PaymentRepo] Processed order " << order_id << ": " << status << std::endl;

        } catch (const std::exception& e) {
            std::cerr << "[PaymentRepo] Error processing payment: " << e.what() << std::endl;
        }
    }

    std::optional<std::string> process_next_outbox_event() {
        try {
            auto conn = db_.get_connection();
            if (!conn) return std::nullopt;
            pqxx::work w(*conn);
            pqxx::result r = w.exec(
                "UPDATE payment_outbox "
                "SET processed = TRUE "
                "WHERE id = ("
                "   SELECT id FROM payment_outbox "
                "   WHERE processed = FALSE "
                "   ORDER BY id ASC "
                "   LIMIT 1 "
                "   FOR UPDATE SKIP LOCKED"
                ") "
                "RETURNING payload"
            );

            w.commit();

            if (r.empty()) return std::nullopt;
            return r[0][0].as<std::string>();
        } catch (const std::exception& e) {
            std::cerr << "[PaymentOutbox] Error: " << e.what() << std::endl;
            return std::nullopt;
        }
    }

private:
    common::Database& db_;
};