#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace common {

    struct OrderCreatedEvent {
        int order_id;
        int user_id;
        int amount;

        // nlohmann/json serialization
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(OrderCreatedEvent, order_id, user_id, amount)
    };

    struct PaymentResultEvent {
        int order_id;
        std::string status; // "PAID" / "FAILED"

		// nlohmann/json serialization
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(PaymentResultEvent, order_id, status)
    };

}