//https://github.com/influxdb/influxdb/pull/3125

//servers.localhost.cpu.loadavg.10
//Template:.host.resource.measurement*
//Output : measurement = loadavg.10 tags = host = localhost resource = cpu


#include <fstream>
#include <assert.h>
#include <boost/make_shared.hpp>
#include <iostream>
#include <iomanip>
#include <string>
#include <assert.h>
#include <thread>
#include <chrono>
#include <sstream>
#include <array>
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
#include <csi_http/server/http_server.h>
#include <csi_http/csi_http.h>
#include <csi_http/client/http_client.h>
#include <csi_kafka/highlevel_consumer.h>
#include <csi_kafka/internal/utility.h>



//measurement[, tag_key1 = tag_value1...] field_key = field_value[, field_key2 = field_value2][timestamp]
//
//For example :
//
//measurement, tkey1 = tval1, tkey2 = tval2 fkey = fval, fkey2 = fval2 1234567890000000000

//https://influxdb.com/docs/v0.9/write_protocols/write_syntax.html


size_t _remaining_records = 0;

static boost::uuids::string_generator uuid_from_string;


typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

struct tag
{
    enum tag_t { TAG, MEASUREMENT, EMPTY };

    tag(std::string s) : wildcard(false), type(EMPTY)
    {
        if (s == "measurement")
        {
            type = MEASUREMENT;
        }
        else if (s == "measurement*")
        {
            type = MEASUREMENT;
            wildcard = true;
        }
        else if (s.size())
        {
            type = TAG;
            name = s;
        }
    }
    
    std::string name;
    tag_t       type;
    bool        wildcard;
};

struct tag2
{
    std::string name;
    int         index;
};

std::vector<tag> parse_template(const std::string& s)
{
    std::vector<tag> v;
    boost::char_separator<char> sep(".", 0, boost::keep_empty_tokens);
    {
        tokenizer tok(s, sep);
        for (tokenizer::iterator beg = tok.begin(); beg != tok.end(); ++beg)
        {
            v.emplace_back(*beg);
        }
    }
    return v;
}

std::vector<tag2> ordered_tags(const std::vector<tag>& v)
{
    std::vector<std::string> s;
    for (std::vector<tag>::const_iterator i = v.begin(); i != v.end(); ++i)
    if (i->type == tag::TAG)
        s.push_back(i->name);
    std::sort(s.begin(), s.end());

    std::vector<tag2> result;
    for (std::vector<std::string>::const_iterator i = s.begin(); i != s.end(); ++i)
    {
        int index = 0;
        for (std::vector<tag>::const_iterator j = v.begin(); j != v.end(); ++j, ++index)
        {
            if (*i == j->name)
            {
                tag2 x;
                x.index = index;
                x.name = *i;
                result.push_back(x);
                continue;
            }
        }
    }
    return result;
}

int measurement_index(const std::vector<tag>& v)
{
    int index = 0;
    for (std::vector<tag>::const_iterator i = v.begin(); i != v.end(); ++i, ++index)
    {
        if (i->type == tag::MEASUREMENT)
            return index;
    }
    return -1;
}

std::vector<std::string> parse_metric(const std::string& s)
{
    std::vector<std::string> v;
    boost::char_separator<char> sep(".", 0, boost::keep_empty_tokens);
    {
        tokenizer tok(s, sep);
        for (tokenizer::iterator beg = tok.begin(); beg != tok.end(); ++beg)
        {
            v.emplace_back(*beg);
        }
    }
    return v;
}

//this will parse a line in 3 parts metric_name value timestamp
std::vector<std::string> parse_graphite(const std::string& s)
{
    std::vector<std::string> v;
    boost::char_separator<char> sep(" ", 0, boost::keep_empty_tokens);
    {
        tokenizer tok(s, sep);
        for (tokenizer::iterator beg = tok.begin(); beg != tok.end(); ++beg)
        {
            v.emplace_back(*beg);
        }
    }
    return v;
}

std::string build_message(const std::vector<tag2>& tags, int message_index, bool wildcard, const std::string s)
{
    std::string message;
    auto parts = parse_graphite(s);
    if (parts.size() != 3)
    {
        std::string what = std::string("parse error - bad graphite format: ") + s;
        throw std::invalid_argument(what.c_str());
    }

    std::vector<std::string> tokens = parse_metric(parts[0]);
    if (tokens.size() < message_index)
    {
        std::string what = std::string("parse error - to few tags: ") + s;
        throw std::invalid_argument(what.c_str());
    }

    //measurement name
    if (wildcard)
    {
        for (std::vector<std::string>::const_iterator i = tokens.begin() + message_index; i != tokens.end(); ++i)
        {
            message += *i;
            if (i != tokens.end() - 1)
            {
                message += ".";
            }
        }
    }
    else
    {
        message += tokens[message_index];
    }

    for (std::vector<tag2>::const_iterator i = tags.begin(); i != tags.end(); ++i)
    {
        message += "," + i->name + "=" + tokens[i->index];
    }

    //add value and time (in sec) and linefeed
    message += " value=" + parts[1] + " " + parts[2] +"\n";
    return message;
}

// --topic collectd.graphite --broker 10.1.47.4 --influxdb 10.1.47.16:8086 --database collectd --template "hostgroup.host...resource.measurement*"




int main(int argc, char** argv)
{
    boost::log::trivial::severity_level log_level;
    boost::log::add_console_log(std::cout, boost::log::keywords::format = ">> %Message%");
    boost::program_options::options_description desc("options");
    desc.add_options()
        ("help", "produce help message")
        ("topic", boost::program_options::value<std::string>(), "topic")
        ("broker", boost::program_options::value<std::string>(), "broker")
        ("template", boost::program_options::value<std::string>(), "template")
        ("influxdb", boost::program_options::value<std::string>()->default_value("localhost:8086"), "influxdb")
		("database", boost::program_options::value<std::string>(), "database")
        ("log_level", boost::program_options::value<boost::log::trivial::severity_level>(&log_level)->default_value(boost::log::trivial::info), "log level to output");
        ;

    boost::program_options::variables_map vm;
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
    boost::program_options::notify(vm);

    boost::log::core::get()->set_filter(boost::log::trivial::severity >= log_level);
    BOOST_LOG_TRIVIAL(info) << "loglevel " << log_level;

    if (vm.count("help"))
    {
        std::cout << desc << std::endl;
        return 0;
    }

    std::string topic;
    if (vm.count("topic"))
    {
        topic = vm["topic"].as<std::string>();
    }
    else
    {
        std::cout << "--topic must be specified" << std::endl;
        return 0;
    }

    int32_t kafka_port = 9092;
    std::vector<csi::kafka::broker_address> kafka_brokers;
    if (vm.count("broker"))
    {
        std::string s = vm["broker"].as<std::string>();
        size_t last_colon = s.find_last_of(':');
        if (last_colon != std::string::npos)
            kafka_port = atoi(s.substr(last_colon + 1).c_str());
        s = s.substr(0, last_colon);

        // now find the brokers...
        size_t last_separator = s.find_last_of(',');
        while (last_separator != std::string::npos)
        {
            std::string host = s.substr(last_separator + 1);
            kafka_brokers.push_back(csi::kafka::broker_address(host, kafka_port));
            s = s.substr(0, last_separator);
            last_separator = s.find_last_of(',');
        }
        kafka_brokers.push_back(csi::kafka::broker_address(s, kafka_port));
    }
    else
    {
        std::cout << "--broker must be specified" << std::endl;
        return 0;
    }

	std::string influxuri;
	if (vm.count("influxdb"))
	{
		influxuri = vm["influxdb"].as<std::string>();
	}
	else
	{
		std::cout << "--influxdb must be specified" << std::endl;
		return 0;
	}

	std::string database;
	if (vm.count("database"))
	{
		database = vm["database"].as<std::string>();
	}
	else
	{
		std::cout << "--database must be specified" << std::endl;
		return 0;
	}

    std::vector<tag> tags;
    if (vm.count("template"))
    {
        std::string templ = vm["template"].as<std::string>();
        tags = parse_template(templ);
    }
    else
    {
        std::cout << "--template must be specified" << std::endl;
        return 0;
    }

	auto ot = ordered_tags(tags);
	int  mi = measurement_index(tags);
	bool wildcard = true;


    std::string kafka_broker_str = "";
    for (std::vector<csi::kafka::broker_address>::const_iterator i = kafka_brokers.begin(); i != kafka_brokers.end(); ++i)
    {
        kafka_broker_str += i->host_name + ":" + std::to_string(i->port);
        if (i != kafka_brokers.end() - 1)
            kafka_broker_str += ", ";
    }

    BOOST_LOG_TRIVIAL(info) << "kafka broker(s): " << kafka_broker_str;
    BOOST_LOG_TRIVIAL(info) << "topic             : " << topic;
    BOOST_LOG_TRIVIAL(info) << "template          : " << vm["template"].as<std::string>();
    BOOST_LOG_TRIVIAL(info) << "measurement index : " << mi;
    BOOST_LOG_TRIVIAL(info) << "influxdb          : " << influxuri;
    BOOST_LOG_TRIVIAL(info) << "database          : " << database;

    boost::asio::io_service ios;
    std::auto_ptr<boost::asio::io_service::work> work(new boost::asio::io_service::work(ios));
    boost::thread bt(boost::bind(&boost::asio::io_service::run, &ios));

    try
    {
        csi::kafka::highlevel_consumer consumer(ios, topic, 1000, 10000);
        consumer.connect(kafka_brokers);
        //std::vector<int64_t> result = consumer.get_offsets();
        consumer.connect_forever(kafka_brokers);

	    consumer.set_offset(csi::kafka::earliest_available_offset);
		//consumer.set_offset(csi::kafka::latest_offsets);

        csi::http_client http_handler(ios);

        size_t metrics_counter = 0;
       
        boost::chrono::system_clock::time_point last = boost::chrono::system_clock::now();
        boost::chrono::milliseconds  sixty_seconds(60 * 1000);

        while (true)
        {
            std::vector<std::string> to_send;

            std::vector<csi::kafka::highlevel_consumer::fetch_response> response = consumer.fetch();
            for (std::vector<csi::kafka::highlevel_consumer::fetch_response>::const_iterator i = response.begin(); i != response.end(); ++i)
            {
                if (i->ec1 || i->ec2 || i->data->error_code)
                    continue;


                for (std::vector<std::shared_ptr<csi::kafka::basic_message>>::const_iterator j = i->data->messages.begin(); j != i->data->messages.end(); ++j)
                {
					if ((*j)->value.is_null())
                        continue;
                    std::string line((const char*)(*j)->value.data(), (*j)->value.size());


					// might be several messages in a line
					boost::char_separator<char> sep("\n\r");
					{
						tokenizer tok(line, sep);
						for (tokenizer::iterator k = tok.begin(); k != tok.end(); ++k)
						{
							try
							{
								auto s = build_message(ot, mi, wildcard, *k);
								if (s.size())
								{
									to_send.push_back(std::move(s));
								}
								else
								{
									assert(false); // should never get here
                                    BOOST_LOG_TRIVIAL(error) << "could not parse: " << *k;
								}
							}
							catch (std::exception& e)
							{
                                BOOST_LOG_TRIVIAL(error) << e.what();
							}
						}
					}
                    //highwater_mark_offset[i->data->partition_id] = i->data->highwater_mark_offset;
                }
			}

			//time to send or more that 1000 msgs
			std::string uri = influxuri + "/write?db=" + database + "&precision=s"; // TBD create it if it does not exist
			while (to_send.size())
			{
				auto request = csi::create_http_request(csi::http::POST, uri, {}, std::chrono::milliseconds(60000));
				avro::StreamWriter writer(request->tx_content());

				size_t items_to_send = std::min<size_t>(to_send.size(), 100);
				std::vector<std::string>::const_iterator cursor = to_send.begin();
				for (size_t i = 0; i != items_to_send; ++i, ++cursor)
					writer.writeBytes((const uint8_t*)cursor->data(), cursor->size());
				writer.flush();

                size_t max_no_of_retries = 60; // ~10min
                size_t no_of_retries = 0;

                while (true)
                {
                    auto result = http_handler.perform(request);
                    if (result->ok())
                    {
                        metrics_counter += items_to_send;
                        to_send.erase(to_send.begin(), to_send.begin() + items_to_send);
                        //time to commit kafka cursor? every sec?
                        //commit cursors....
                        //boost::this_thread::sleep(boost::posix_time::milliseconds(200));
                        break;
                    }

                    if (!result->transport_result())
                    {
                        BOOST_LOG_TRIVIAL(warning) << "transport failed, " << " retry_count: " << no_of_retries;
                    }
                    else
                    {
                        BOOST_LOG_TRIVIAL(error) << "http post: " << result->uri() << " result: " << result->http_result() << " (" << to_string(result->http_result()) << "), " << " retry_count: " << no_of_retries;
                    }

                    boost::this_thread::sleep(boost::posix_time::milliseconds(10000));
                    no_of_retries++;

                    //max no of retries and die??
                    if (no_of_retries > max_no_of_retries)
                    {
                        BOOST_LOG_TRIVIAL(error) << "no_of_retries > max_no_of_retries - exiting....";
                        boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
                        return 1; 
                    }
                }
			}


            boost::chrono::system_clock::time_point now = boost::chrono::system_clock::now();
            boost::chrono::milliseconds elapsed = boost::chrono::duration_cast<boost::chrono::milliseconds>(now - last);
            if (elapsed>sixty_seconds)
            {
                BOOST_LOG_TRIVIAL(info) << "nr of metrics: " << metrics_counter;
                metrics_counter = 0;
                last = now;
            }

			/*
            this is not working because of kafka lib
			auto res4 = client.commit_consumer_offset(CONSUMER_GROUP, 1, CONSUMER_ID, TOPIC_NAME, 0, 22, "nisse", 44);
			if (res4)
			{
				std::cerr << to_string(res4.ec) << std::endl;
				return -1;
			}
			*/
        }
    }
    catch (std::exception& e)
    {
        BOOST_LOG_TRIVIAL(error) << "exception: " << e.what() << " : exiting";
    }
    ios.stop();
    return 0;
}


