#include <boost/program_options.hpp>
#include <cppkafka/cppkafka.h>
#include <nlohmann/json.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <iostream>
#include <signal.h>

#include "utils/utils.hpp"

namespace po = boost::program_options;

static bool running = true;

auto main(int argc, char** argv) -> int {

	std::string config_path{};

	po::options_description desc("Allowed options");
	desc.add_options()("help", "Display usage information")(
		"config_path", po::value<std::string>(&config_path)->required(),
		"Path to the config file");

	po::variables_map vm;
	try {
		po::store(po::parse_command_line(argc, argv, desc), vm);

		if (vm.count("help")) {
			std::cout << desc << std::endl;
			return 0;
		} else if (!vm.count("config_path")) {
			std::cerr << "Not enough arguments provided. Usage: " << desc
					  << std::endl;
			return 1;
		}

		po::notify(vm);
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	const auto json_config =
		KafkaClient::ConfigLoader::load_config(config_path);

	// Create a logger with console and file sinks
	auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	auto basic_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
		json_config.logfile);
	std::vector<spdlog::sink_ptr> sinks{console_sink, basic_sink};
	auto logger =
		std::make_shared<spdlog::logger>("main", sinks.begin(), sinks.end());

	// Future cleanup
	signal(SIGINT, [](int) { running = false; });

	auto config = cppkafka::Configuration({
		{"metadata.broker.list", json_config.kafka_broker},
		{"group.id", json_config.group_id},
		{"enable.auto.commit", false},
	});

	// Create a Kafka consumer instance
	auto consumer = cppkafka::Consumer(config);

	// Subscribe to the topic
	consumer.subscribe({json_config.kafka_topic});

	// Print the assigned partitions on assignment
	consumer.set_assignment_callback(
		[&logger](const cppkafka::TopicPartitionList& partitions) {
			std::stringstream ss("Got assigned: ");
			ss << partitions;

			logger->info(ss.str());
		});

	// Print the revoked partitions on revocation
	consumer.set_revocation_callback(
		[&logger](const cppkafka::TopicPartitionList& partitions) {
			std::stringstream ss("Got revoked: ");
			ss << partitions;

			logger->info(ss.str());
		});

	// Poll for messages
	while (running) {
		cppkafka::Message msg =
			consumer.poll(std::chrono::milliseconds{json_config.poll_delay});

		// If we managed to get a message
		if (msg) {
			if (msg.get_error()) {
				// Ignore EOF notifications from rdkafka
				if (!msg.is_eof()) {
					logger->critical("Received error notification: {}",
									 msg.get_error().to_string());
				}
			} else {
				try {
					std::cout << msg.get_payload() << std::endl;

				} catch (const std::exception& e) {
					// Catch and handle the exception
					logger->critical("An exception occurred: {}", e.what());
				}
			}

			// Now commit the message
			consumer.commit(msg);
			// and write to the log
			logger->flush();
		}
	}

	return 0;
}