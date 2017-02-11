#include <fstream>
#include <assert.h>
#include <boost/make_shared.hpp>
#include <iostream>
#include <iomanip>
#include <string>
#include <chrono>
#include <assert.h>
#include <thread>
#include <chrono>
#include <sstream>
#include <array>
#include <cstdlib>
#include <stdexcept>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/bind.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/program_options.hpp>
#include <boost/endian/arithmetic.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/chrono/thread_clock.hpp>
#include <boost/tokenizer.hpp>
#include <boost/timer/timer.hpp>
#include <csi_kafka/highlevel_producer.h>

static std::string default_kafka_broker() {
  if (const char* env_p = std::getenv("KAFKA_BROKER"))
    return std::string(env_p);
  return "localhost";
}

inline int64_t milliseconds_since_epoch() {
  return std::chrono::duration_cast<std::chrono::milliseconds>
    (std::chrono::system_clock::now().time_since_epoch()).count();
}

int main(int argc, char** argv) {
  boost::log::trivial::severity_level log_level;
  boost::log::add_console_log(std::cout, boost::log::keywords::auto_flush = true);
  boost::program_options::options_description desc("options");
  desc.add_options()
    ("help", "produce help message")
    ("topic", boost::program_options::value<std::string>()->default_value("kspp_metrics"), "topic")
    ("broker", boost::program_options::value<std::string>()->default_value(default_kafka_broker()), "broker")
    ("log_level", boost::program_options::value<boost::log::trivial::severity_level>(&log_level)->default_value(boost::log::trivial::info), "log level to output");
  ;

  boost::program_options::variables_map vm;
  boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
  boost::program_options::notify(vm);

  boost::log::core::get()->set_filter(boost::log::trivial::severity >= log_level);
  BOOST_LOG_TRIVIAL(info) << "loglevel " << log_level;

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return 0;
  }

  std::string topic;
  if (vm.count("topic")) {
    topic = vm["topic"].as<std::string>();
  } else {
    std::cout << "--topic must be specified" << std::endl;
    return 0;
  }

  std::string broker;
  if (vm.count("broker")) {
    broker = vm["broker"].as<std::string>();
  } else {
    std::cout << "--broker must be specified" << std::endl;
    return 0;
  }


  BOOST_LOG_TRIVIAL(info) << "kafka broker(s)   : " << broker;
  BOOST_LOG_TRIVIAL(info) << "topic             : " << topic;

  boost::chrono::system_clock::time_point last = boost::chrono::system_clock::now();
  boost::chrono::milliseconds sixty_seconds(60 * 1000);

  boost::asio::io_service ios;
  std::auto_ptr<boost::asio::io_service::work> work(new boost::asio::io_service::work(ios));
  boost::thread bt(boost::bind(&boost::asio::io_service::run, &ios));


  csi::kafka::highlevel_producer producer(ios, topic, -1, 1000, 100000);
  
  while (producer.connect(broker, 1000)) {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    BOOST_LOG_TRIVIAL(info) << "retrying to connect";
  }

  size_t metrics_counter = 0;
  int64_t next_batch = milliseconds_since_epoch();

  std::vector<std::string> metrics_type = {"lag", "in_count", "errors", "out_count"};
  std::vector<std::string> topologys = {"topology1", "topology2"};
  std::vector<std::string> depths = {"0", "1"};
  std::vector<std::string> key_types = {"void", "uuid"};
  std::vector<std::string> value_types = {"string", "client_metrics", "requestlog", "syslog"};
  std::vector<std::string> processors = {"flat_map", "filter","count", "join", "delay", "rate_limiter", "repartition"};
  std::vector<std::string> partitions = {"0", "1", "2", "3", "4", "5", "6", "7"};

  int64_t value = 0;
  while (true) {
    if (milliseconds_since_epoch() > next_batch) {
      std::vector<std::shared_ptr<csi::kafka::basic_message>> v;
      for (auto metric : metrics_type)
        for (auto topology : topologys)
          for (auto depth : depths)
            for (auto key_type : key_types)
              for (auto value_type : value_types)
                for (auto processor : processors)
                  for (auto partition : partitions) {
                    std::string key = metric + ",app_id=kspp-metrics-test-gen,depth=" + depth + ",key_type=" + key_type + ",partition=" + partition + ",processor_type=" + processor + ",topology=" + topology + ",value_type=" + value_type;
                    std::string val = std::to_string(value);
                    value++;
                    v.push_back(std::make_shared<csi::kafka::basic_message>(key, val, milliseconds_since_epoch()));
                  }
      auto res = producer.send_sync(v);
      next_batch = milliseconds_since_epoch() + 10000;
    }
  
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }



  return 0;
}

