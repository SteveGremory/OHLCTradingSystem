#include <boost/program_options.hpp>
#include <cppkafka/cppkafka.h>
#include <nlohmann/json.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <unordered_map>

#include "utils/utils.hpp"

namespace po = boost::program_options;

auto main(int argc, char** argv) -> int {
	std::string data_folder, config_path;

	// Boost argparser
	po::options_description desc(
		"\nProducer: Takes in the path to the data and sends it to the "
		"consumer.\n"
		"Invocation : --data <folderpath> --config_path <filepath> "
		"\nAgruments");
	desc.add_options()("help", "Display usage information")(
		"data", po::value<std::string>(&data_folder)->required(),
		"The folder with the JSON data inside it")(
		"config_path", po::value<std::string>(&config_path)->required(),
		"Path to the JSON configuration file");

	po::variables_map vm{};

	try {
		po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);

		po::notify(vm);

		if (vm.count("help")) {
			// Display usage information and exit
			std::cout << desc << std::endl;
			return 0;
		}

	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	// Create a configuration for the producer
	const auto json_config =
		KafkaClient::ConfigLoader::load_config(config_path);
	cppkafka::Configuration config = {
		{"metadata.broker.list", json_config.kafka_broker},
	};

	// Create a Kafka producer instance
	cppkafka::Producer producer(config);

	// Create a message to send
	cppkafka::MessageBuilder message({json_config.kafka_topic});

	// Create an object to be sent
	nlohmann::json json_obj = {
		{"p1", "v1"},
		{"p2", "v2"},
		{"p3", "v3"},
	};
	std::string json_str = json_obj.dump();

	message.payload(json_str);

	producer.produce(message);

	// Send the message
	producer.flush();

	return 0;
}