#include <iostream>
#include <chrono>
#include <zmq_addon.hpp>
#include <random> //used for RX message example
#include <iostream>
#include "json.hpp"

static zmq::context_t ctx;

int main()
{
    // Initialize all sockets
    zmq::socket_t pusher(ctx, zmq::socket_type::push);
    zmq::socket_t receiver(ctx, zmq::socket_type::pull);
    // Connect sockets to open TCP ports
    pusher.bind("tcp://127.0.0.1:5557");
    receiver.bind("tcp://127.0.0.1:5558");
    zmq::pollitem_t items[] = {
        {static_cast<void *>(receiver), 0, ZMQ_POLLIN, 0}};
    size_t iter = 0;
    time_t lastMsg = time(NULL);
    for (;;)
    {
        iter++;
        // --------------------RECEIVE-------------------
        zmq::poll(items, 1, 10);
        if (items[0].revents & ZMQ_POLLIN)
        {
            zmq::message_t topic;
            zmq::recv_result_t result = receiver.recv(topic); // mention why recv_result_t
            std::cout << "\nReceiving Message Type: " << topic.to_string_view();
            

            if (topic.more())
            {
                zmq::message_t data;
                zmq::recv_result_t result = receiver.recv(data);
                auto jsonData = nlohmann::json::parse(data.to_string_view());
                std::cout << jsonData.dump(4);
                if (jsonData.contains("DUID"))
                {
                    std::cout << "\nDUID:" << jsonData.at("DUID") << std::endl;
                }
                else
                {
                    std::cout << "\nInvalid IPC packet\n";
                }
            }

            std::string dataStr = "DATA";
            std::string hbtStr = "HEARTBEAT";
            if (topic.to_string()[0,8] == hbtStr[0,8]) //have to compare char arrays due to unprintable characters
            {
                std::cout << "\nHEARTBEAT received\n";
            }
            else if (topic.to_string()[0,3] == dataStr[0,3]) {
                std::cout << "\nDATA received" << std::endl;
                std::cout <<"\nDATA TRANSMITTED";
            }
            else
            {
                std::cout << "\nTopic Read Failure\n";
            }
        }

        // --------------------SEND-------------------
        // EXAMPLE CAPTIVE PORTAL MESSAGE (THIS WOULD BE REPLACED BY USER INPUT)
        // This block will send a CPM at random intervals to simuate user input
        // FOR DEMONSTRATION PURPOSES ONLY
        std::random_device dev;
        std::mt19937 rng(dev());
        std::uniform_int_distribution<std::mt19937::result_type> dist(7, 14);
        if ((time(NULL) - lastMsg) > dist(rng))
        {
            if (iter > 1000)
            {
                iter = 1;
            }
            zmq::message_t topic("DATA");
            nlohmann::ordered_json jmsg;
            jmsg["DUID"] = "DUCK" + std::to_string(iter);
            jmsg["TOPIC"] = "cpm";
            jmsg["DATA"] = "Incoming Transmission.";
            std::cout << "\nSENDING:\n" << topic.to_string_view()
                      << jmsg.dump(4) << std::endl;
            zmq::message_t data(jmsg.dump());
            pusher.send(topic, zmq::send_flags::sndmore);
            pusher.send(data, zmq::send_flags::none);
            lastMsg = time(NULL);
        }

        // If the dashboard hasn't sent a message in the last 10 seconds it will send a ping to update the other programs that it is still connected
        if ((time(NULL) - lastMsg) > 10)
        {
            zmq::message_t topic("HEARTBEAT");
            pusher.send(topic, zmq::send_flags::none);
            std::cout << "\nSENT: " << "HEARTBEAT";
            lastMsg = time(NULL);
        }
        // std::this_thread::sleep_for(400ms);
    }
}
