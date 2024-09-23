#include <iostream>
#include <chrono>
#include <zmq_addon.hpp>
#include "json.hpp"

static zmq::context_t ctx;

int main()
{
    // Initialize all sockets
    zmq::socket_t dashboardIn(ctx, zmq::socket_type::pull);
    zmq::socket_t dashboardOut(ctx, zmq::socket_type::push);
    zmq::socket_t loraIn(ctx, zmq::socket_type::pull);
    zmq::socket_t loraOut(ctx, zmq::socket_type::push);
    // Connect sockets to open TCP ports
    dashboardIn.connect("tcp://127.0.0.1:5555");
    dashboardOut.connect("tcp://127.0.0.1:5556");
    loraIn.connect("tcp://127.0.0.1:5557");
    loraOut.connect("tcp://127.0.0.1:5558");
    // Create polls for each socket. Polls are non blocking so it will allow the code to continue if the queues are empty
    zmq::pollitem_t items[] = {
        // Always poll the lora RX activity
        {static_cast<void *>(loraIn), 0, ZMQ_POLLIN, 0},
        // Poll dashboard if there is no incoming RX
        {static_cast<void *>(dashboardIn), 0, ZMQ_POLLIN, 0}};
    // Initialize Heart Beat timers
    time_t loraHBStart = time(NULL);
    time_t dbHBStart = time(NULL);

    while (1)
    {
        // --------------- RECEIVE ---------------
        // LORA CHIP POLLING
        zmq::poll(&items[0], 2, 1);
        if (items[0].revents & ZMQ_POLLIN)
        {
            // Messages are broken up into 2 parts, topic and data. This allows the IPC protocol to route packets accordingly without modifying the packet itself
            zmq::message_t topicIn;
            zmq::recv_result_t result = loraIn.recv(topicIn); // recv_result_t is based on trivial_optional<> and outputs a bool. Allows for a graceful queue read failure
            std::cout << "\nRecieving Message Type: " << topicIn.to_string_view();
            // Set up strings to compare against topics
            // Forced to compare char arrays due to unprintable characters appearing during zmq transmission
            std::string dataStr = "DATA";
            std::string hbtStr = "HEARTBEAT";

            zmq::message_t data;
            if (topicIn.more())
            {
                zmq::recv_result_t result = loraIn.recv(data);
                auto jsonData = nlohmann::ordered_json::parse(data.to_string_view());
                std::cout << jsonData.dump(4);
                if (jsonData.contains("DUID"))
                {
                    std::cout << "\nDUID:" << jsonData.at("DUID") << std::endl;
                }
                else
                {
                    std::cout << "\nInvalid LORA packet\n";
                }
            }
            // Decide action based on message topic

            if (topicIn.to_string()[0, 8] == hbtStr[0, 8]) 
            {
                std::cout << "\nTXRX HEARTBEAT received\n";
            }
            else if (topicIn.to_string()[0, 3] == dataStr[0, 3])
            {
                std::cout << "\nDATA received";
                //Redirect data message from the LORA chip to the dashboard
                dashboardOut.send(topicIn, zmq::send_flags::sndmore);
                dashboardOut.send(data, zmq::send_flags::none);
                std::cout << "\nData sent to Dashboard." << std::endl;
                dbHBStart = time(NULL);
            }
            else
            {
                std::cout << "\nTopic Read Failure\n";
            }
        }
        // DASHBOARD POLLING
        if (items[1].revents & ZMQ_POLLIN)
        {
            // Messages are broken up into 2 parts, topic and data. This allows the IPC protocol to route packets accordingly without modifying the packet itself
            zmq::message_t topicIn;
            zmq::recv_result_t result = dashboardIn.recv(topicIn); // mention why recv_result_t
            std::cout << "\nRecieving Message Type: " << topicIn.to_string_view();
            std::string dataStr = "DATA";
            std::string hbtStr = "HEARTBEAT";

            zmq::message_t data;
            if (topicIn.more())
            {
                zmq::recv_result_t result = dashboardIn.recv(data);
                auto jsonData = nlohmann::ordered_json::parse(data.to_string_view());
                std::cout << jsonData.dump(4);
                if (jsonData.contains("DUID"))
                {
                    std::cout << "\nDUID:" << jsonData.at("DUID") << std::endl;
                }
                else
                {
                    std::cout << "\nInvalid Dashboard packet\n";
                }
            }

            if (topicIn.to_string()[0, 8] == hbtStr[0, 7]) // have to compare char arrays due to unprintable characters
            {
                std::cout << "\nDashboard HEARTBEAT received\n";
            }
            else if (topicIn.to_string()[0, 3] == dataStr[0, 3])
            {
                // Redirect message data from the dashboard to LORA chip
                std::cout << "\nDATA received";
                loraOut.send(topicIn, zmq::send_flags::sndmore);
                loraOut.send(data, zmq::send_flags::none);
                std::cout << "\nData sent to LORA chip." << std::endl;
                loraHBStart = time(NULL);
            }
            else
            {
                std::cout << "\nTopic Read Failure\n";
            }
        }
        // --------------------SEND-------------------
        // Heartbeats are used as a ping on the socket to let the connected devices know there is a device connected
        // Pings occur if there has not been a message in the last 10 seconds
        // LORA HEARTBEAT
        if ((time(NULL) - loraHBStart) > 10)
        {
            zmq::message_t topic("HEARTBEAT");
            loraOut.send(topic, zmq::send_flags::none);
            std::cout << "\nHEARTBEAT SENT\n";
            loraHBStart = time(NULL);
        }

        // Dashboard HEARTBEAT
        if ((time(NULL) - dbHBStart) > 10)
        {
            zmq::message_t topic("HEARTBEAT");
            dashboardOut.send(topic, zmq::send_flags::none);
            std::cout << "\nHEARTBEAT SENT\n";
            dbHBStart = time(NULL);
        }

        // --------------------PROGRAM HEALTH CHECK-------------------
        // Warning that a process is no longer responsive (there have been no messages or heartbeats in the last 15 seconds)
        if ((time(NULL) - loraHBStart) > 15)
        {
            std::cout << "The LORA chip is not responsive. Check connection";
        }
        if ((time(NULL) - dbHBStart) > 15)
        {
            std::cout << "The dashboard is not responsive. Check connection";
        }
    }
};