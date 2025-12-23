#pragma once

#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <string>
#include <iostream>
#include <thread>
#include <optional>

namespace common {

class RabbitMQ {
public:
	RabbitMQ(const std::string& host, const std::string& queue_name)
    	: host_(host), queue_name_(queue_name) {}

	void connect() {
    	while (true) {
        	try {
            	std::string uri = "amqp://user:password@" + host_ + ":5672";
            	AmqpClient::Channel::OpenOpts opts = AmqpClient::Channel::OpenOpts::FromUri(uri);

            	channel_ = AmqpClient::Channel::Open(opts);

            	if (channel_) {
                	channel_->DeclareQueue(queue_name_, false, true, false, false);
                	std::cout << "[RabbitMQ] Connected to queue: " << queue_name_ << std::endl;
                	break;
   		    	}
    	    } catch (const std::exception& e) {
    	        std::cerr << "[RabbitMQ] Connection failed: " << e.what() << ". Retrying..." << std::endl;
    	        std::this_thread::sleep_for(std::chrono::seconds(3));
    	    }
    	}
	}

	void publish(const std::string& message) {
	    if (!channel_) return;
	    try {
	        auto msg = AmqpClient::BasicMessage::Create(message);
	        channel_->BasicPublish("", queue_name_, msg);
	    } catch (const std::exception& e) {
	        std::cerr << "[RabbitMQ] Publish error: " << e.what() << std::endl;
	    }
	}

	void start_consume() {
    	if (!channel_) return;
    	try {
        	consumer_tag_ = channel_->BasicConsume(queue_name_, "", true, true, false, 1);
    	} catch (const std::exception& e) {
        	std::cerr << "[RabbitMQ] Start consume error: " << e.what() << std::endl;
   		}
	}

	std::optional<std::string> consume_message(int timeout_ms = 100) {
    	if (!channel_ || consumer_tag_.empty()) return std::nullopt;

   	 	try {
    	    AmqpClient::Envelope::ptr_t envelope;
    	    if (channel_->BasicConsumeMessage(consumer_tag_, envelope, timeout_ms)) {
   	         	return envelope->Message()->Body();
    	    }
    	} catch (const std::exception& e) {
     	   	std::cerr << "[RabbitMQ] Consume error: " << e.what() << std::endl;
    	}
    	return std::nullopt;
	}

	AmqpClient::Channel::ptr_t get_channel() {
    	return channel_;
	}

private:
	std::string host_;
	std::string queue_name_;
	AmqpClient::Channel::ptr_t channel_;
	std::string consumer_tag_;
};

} // namespace common