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
#include <csi_hcl_asio/http_client.h>
#include <csi_kafka/highlevel_consumer.h>
#include <csi_kafka/consumer_coordinator.h>

#define CONSUMER_GROUP "kafka-influx2influx"

// we inline small parts of kspp to make life easy...


static std::string default_kafka_broker() {
  if (const char* env_p = std::getenv("KAFKA_BROKER"))
    return std::string(env_p);
  return "localhost";
}

static std::string default_influxdb() {
  if (const char* env_p = std::getenv("INFLUXDB_ADDRESS"))
    return std::string(env_p);
  return "localhost:8086";
}


namespace kspp {
  inline int64_t milliseconds_since_epoch() {
    return std::chrono::duration_cast<std::chrono::milliseconds>
      (std::chrono::system_clock::now().time_since_epoch()).count();
  }

  template<class K, class V>
  struct krecord
  {
    krecord() : event_time(-1), offset(-1) {}
    krecord(const K& k) : event_time(milliseconds_since_epoch()), offset(-1), key(k) {}
    krecord(const K& k, const V& v) : event_time(milliseconds_since_epoch()), offset(-1), key(k), value(std::make_shared<V>(v)) {}
    krecord(const K& k, std::shared_ptr<V> v) : event_time(milliseconds_since_epoch()), offset(-1), key(k), value(v) {}
    krecord(const K& k, std::shared_ptr<V> v, int64_t ts) : event_time(ts), offset(-1), key(k), value(v) {}

    K                  key;
    std::shared_ptr<V> value;
    int64_t            event_time;
    int64_t            offset;
  };
};

class influx_batch_handler
{
  public:
  influx_batch_handler(std::string base_url, size_t batch_size)
    : _work(new boost::asio::io_service::work(_ios))
    , _thread(boost::bind(&boost::asio::io_service::run, &_ios))
    , _http_handler(_ios)
    , _base_url(base_url)
    , _batch_size(batch_size)
    , _next_time_to_send(kspp::milliseconds_since_epoch() + 1000) {}

  virtual ~influx_batch_handler() {
    //_source->remove_sink()
  }

  virtual std::string name() const {
    return "influx_batch_handler";
  }

  virtual bool eof() const {
    return (_messages.size() == 0);
  }

  virtual bool process_one() {
    int64_t now = kspp::milliseconds_since_epoch();
    if (_messages.size() >= _batch_size || now > _next_time_to_send) {
      _next_time_to_send = now + 1000;
      send();
      return true;
    }
    return false;
  }

  virtual void close() {}

  virtual int produce(std::shared_ptr<kspp::krecord<std::string, std::string>> r) {
    std::string s = r->key + " value=" + *(r->value) + " " + std::to_string(r->event_time) + "\n";
    _messages.push_back(s);
    return 0;
  }

  virtual size_t queue_len() {
    return _messages.size();
  }

  bool send() {
    while (_messages.size()) {
      auto request = csi::http::create_http_request(csi::http::POST, _base_url, {}, std::chrono::milliseconds(2000));

      size_t items_to_send = std::min<size_t>(_messages.size(), _batch_size);
      std::vector<std::string>::const_iterator cursor = _messages.begin();
      for (size_t i = 0; i != items_to_send; ++i, ++cursor)
        request->append(_messages[i]);

      size_t max_no_of_retries = 60; // ~10min
      size_t no_of_retries = 0;

      while (true) // until we managed to send
      {
        auto result = _http_handler.perform(request, false);
        if (result->ok()) {
          BOOST_LOG_TRIVIAL(debug) << "influx insert ok, items: " << items_to_send << ", time: " << result->milliseconds() << " ms";
          //metrics_counter += items_to_send;
          _messages.erase(_messages.begin(), _messages.begin() + items_to_send);
          //time to commit kafka cursor? every sec?
          //commit cursors....
          //boost::this_thread::sleep(boost::posix_time::milliseconds(200));
          break; // this is good
        }

        if (!result->transport_result()) {
          BOOST_LOG_TRIVIAL(warning) << "transport failed, " << " retry_count: " << no_of_retries;
          boost::this_thread::sleep(boost::posix_time::milliseconds(10000));
          no_of_retries++;

        } else {
          BOOST_LOG_TRIVIAL(error) << "http post: " << result->uri() << " result: " << result->http_result() << ", retry_count: " << no_of_retries;
          boost::this_thread::sleep(boost::posix_time::milliseconds(10000));
          no_of_retries++;
        }

        //max no of retries and die??
        if (no_of_retries > max_no_of_retries) {
          BOOST_LOG_TRIVIAL(error) << "no_of_retries > max_no_of_retries - exiting....";
          boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
          return 1;
        }
      }
    }
    return true;
  }

  private:
  boost::asio::io_service                      _ios;
  std::auto_ptr<boost::asio::io_service::work> _work;
  boost::thread                                _thread;
  csi::http::client                            _http_handler;
  std::string                                  _base_url;
  std::vector<std::string>                     _messages;
  size_t                                       _batch_size;
  int64_t                                      _next_time_to_send;
};


int main(int argc, char** argv) {
  boost::log::trivial::severity_level log_level;
  //boost::log::add_console_log(std::cerr, boost::log::keywords::format = ">> %Message%", auto_flush=true);
  boost::log::add_console_log(std::cout, boost::log::keywords::auto_flush = true);
  boost::program_options::options_description desc("options");
  desc.add_options()
    ("help", "produce help message")
    ("topic", boost::program_options::value<std::string>()->default_value("kspp_metrics"), "topic")
    ("broker", boost::program_options::value<std::string>()->default_value(default_kafka_broker()), "broker")
    ("consumer_group", boost::program_options::value<std::string>()->default_value(CONSUMER_GROUP), "consumer_group")
    ("influxdb", boost::program_options::value<std::string>()->default_value(default_influxdb()), "influxdb")
    ("database", boost::program_options::value<std::string>()->default_value("kspp_metrics"), "database")
    ("batch_size", boost::program_options::value<int>()->default_value(200), "batch_size")
    ("reset_offset", boost::program_options::value<bool>()->default_value(false), "reset_offset")
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

  std::string consumer_group;
  if (vm.count("consumer_group")) {
    consumer_group = vm["consumer_group"].as<std::string>();
  } else {
    std::cout << "--consumer_group must be specified" << std::endl;
    return 0;
  }

  bool reset_offset = false;
  if (vm.count("reset_offset")) {
    reset_offset = vm["reset_offset"].as<bool>();
  }

  std::string influxuri;
  if (vm.count("influxdb")) {
    influxuri = vm["influxdb"].as<std::string>();
  } else {
    std::cout << "--influxdb must be specified" << std::endl;
    return 0;
  }

  std::string database;
  if (vm.count("database")) {
    database = vm["database"].as<std::string>();
  } else {
    std::cout << "--database must be specified" << std::endl;
    return 0;
  }

  int batch_size;
  if (vm.count("batch_size")) {
    batch_size = vm["batch_size"].as<int>();
  } else {
    std::cout << "--batch_size must be specified" << std::endl;
    return 0;
  }

  BOOST_LOG_TRIVIAL(info) << "kafka broker(s)   : " << broker;
  BOOST_LOG_TRIVIAL(info) << "topic             : " << topic;
  BOOST_LOG_TRIVIAL(info) << "consumer_group    : " << consumer_group;
  BOOST_LOG_TRIVIAL(info) << "reset_offset      : " << reset_offset;
  BOOST_LOG_TRIVIAL(info) << "influxdb          : " << influxuri;
  BOOST_LOG_TRIVIAL(info) << "database          : " << database;
  BOOST_LOG_TRIVIAL(info) << "batch_size        : " << batch_size;

  boost::chrono::system_clock::time_point last = boost::chrono::system_clock::now();
  boost::chrono::milliseconds sixty_seconds(60 * 1000);

  influx_batch_handler batch_handler(influxuri + "/write?db=" + database + "&precision=ms", batch_size);

  boost::asio::io_service ios;
  std::auto_ptr<boost::asio::io_service::work> work(new boost::asio::io_service::work(ios));
  boost::thread bt(boost::bind(&boost::asio::io_service::run, &ios));
  
  csi::kafka::consumer_coordinator coordinator(ios, consumer_group);
  auto coordinator_connect_res = coordinator.connect(broker, 1000);
  if (coordinator_connect_res) {
    std::cerr << coordinator_connect_res.message() << std::endl;
    return -1;
  }

  auto offset_res = coordinator.get_consumer_offset(topic);
  if (offset_res) {
    std::cerr << to_string(offset_res.ec) << std::endl;
    return -1;
  }

  int32_t ec;
  auto offsets = parse(offset_res, topic, ec);
  if (ec) {
    std::cerr << to_string((csi::kafka::error_codes) ec) << std::endl;
    return -1;
  }

  if (reset_offset) {
    for (auto i : offsets)
      i.second = csi::kafka::earliest_available_offset;
    auto res = coordinator.commit_consumer_offset(-1, consumer_group, topic, offsets, "reset_offset_command");
    if (res) {
      std::cerr << to_string(res.ec) << std::endl;
      return -1;
    }
  }

  csi::kafka::highlevel_consumer consumer(ios, topic, 1000, 100000);
  consumer.connect(broker);
  consumer.set_offset(offsets); // start where we left...

  std::map<int32_t, int64_t> cursor;
  int64_t next_commit = kspp::milliseconds_since_epoch() + 60000;

  size_t metrics_counter = 0;

  while (true) {
    auto r = consumer.fetch();
    for (auto i : r) {
      if (i.ec) {
        BOOST_LOG_TRIVIAL(fatal) << "fetch failed - exiting fast";
        return 1; // test with quick death
                  //continue; // or die??
      }
      for (auto j : i.data->topics) {
        for (auto k : j.partitions) {
          if (k->error_code) {
            BOOST_LOG_TRIVIAL(fatal) << "fetch failed, partition: " << k->partition_id << ", exiting fast";
            return 1; // test with quick death
          }
          for (auto m : k->messages) {
            if (m->key.size() == 0 || m->value.data() == NULL || m->value.data() == 0)
              continue;
            std::string key((const char*)m->key.data(), m->key.size());
            auto val = std::make_shared<std::string>((const char*)m->value.data(), m->value.size());
            batch_handler.produce(std::make_shared<kspp::krecord<std::string, std::string>>(key, val, m->timestamp)); // add the offsets here and let the batch handler tell whats committed TBD
            batch_handler.process_one();
            cursor[k->partition_id] = m->offset; // this is probably a bit bad and we should really not write the offsets until we actually have committed them to influx. we will loose up to 200 messages here....
          }
        }
      }
    }

    while (batch_handler.process_one())
    {
    }

    if (kspp::milliseconds_since_epoch()>next_commit) {
      auto res = coordinator.commit_consumer_offset(-1, consumer_group, topic, cursor, "graff");
      next_commit = kspp::milliseconds_since_epoch() + 60000; // once per minute
    }

  }
  return 0;
}

