#include <mutex>
#include <chrono>
#include <string>
#include <thread>
#include <random>
#include <sstream>
#include <utility>
#include <iostream>
#include <zmqpp/zmqpp.hpp>


#include "lib/messaging.hpp"
#include "lib/networking.hpp"


std::string username;


std::vector<std::string> chats;
zmqpp::context context;
zmqpp::socket clientSocket(context, zmqpp::socket_type::request);
std::mutex mutex;


auto updater() -> void {
    time_t lastChatsUpdateTime{0};

    while (true) {
        auto request = Request(RequestType::UpdateChats, Message(lastChatsUpdateTime, username, ""));
        mutex.lock();
        if (!sendRequest(clientSocket, request)) {
            break;
        }
        if (!receiveRequest(clientSocket, request)) {
            break;
        }
        mutex.unlock();

        for (const auto &chat: request.message.vector) {
            chats.push_back(chat);
        }
        lastChatsUpdateTime = request.message.time;

        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    std::cout << "updater stopped" << std::endl;
}


auto main() -> int {
    std::string password;

    // end points
    const std::string serverEndPoint("tcp://192.168.1.2:4506");
    std::string clientEndPoint("tcp://" + getIP() + ":");


    zmqpp::socket serverSocket(context, zmqpp::socket_type::push);
//    zmqpp::socket clientSocket(context, zmqpp::socket_type::request);

    serverSocket.set(zmqpp::socket_option::send_timeout, 60 * 1000);
    serverSocket.set(zmqpp::socket_option::connect_timeout, 2 * 1000);


    serverSocket.connect(serverEndPoint);


    // if testing on same machine with server
    for (int i = 0; i < 5; i++) {
        try {
            std::random_device randomDevice;
            std::mt19937 randomEngine(randomDevice());
            std::uniform_int_distribution distribution(4000, 9999);

            uint32_t port = distribution(randomEngine);
            clientSocket.bind(clientEndPoint + std::to_string(port));
            clientEndPoint += std::to_string(port);
            break;
        } catch (...) {
            if (i == 4) {
                std::cerr << "can't find appropriate port" << std::endl;
                exit(1);
            }
            continue;
        }
    }

    int command;
    std::cout << "Choose:\n    1.Sign in\n    2.Sign up\nEnter number: ";
    if (!(std::cin >> command)) {
        return 0;
    }
    RequestType requestType;
    if (command == 1) {
        requestType = RequestType::SignIn;
    } else if (command == 2) {
        requestType = RequestType::SignUp;
    } else {
        std::cout << "invalid command" << std::endl;
        return 0;
    }


    std::cout << "username: ";
    std::cin >> username;
    std::cout << "password: ";
    std::cin >> password;


    zmqpp::message connectMessage;
    connectMessage << clientEndPoint;
    if (!serverSocket.send(connectMessage)) {
        std::cerr << "send error" << std::endl;
        return 1;
    }

    if (requestType == RequestType::SignIn) {
        auto request = Request(RequestType::SignIn, Message(username, password));
        zmqpp::message signInMessage;
        zmqpp::message responseMessage;
        signInMessage.add_raw(&request, sizeof request);
        clientSocket.send(signInMessage);
        clientSocket.receive(responseMessage);
        auto response = *static_cast<Request *>(const_cast<void *>(responseMessage.raw_data()));
        if (response.authenticationStatus == AuthenticationStatus::NotExists) {
            std::cout << "user not exists" << std::endl;
            return 0;
        } else if (response.authenticationStatus == AuthenticationStatus::InvalidPassword) {
            std::cout << "invalid password" << std::endl;
            return 0;
        } else if (response.authenticationStatus == AuthenticationStatus::Success) {
            std::cout << "sing in succeeded" << std::endl;
        }
    } else {
        auto request = Request(RequestType::SignUp, Message(username, password));
        zmqpp::message signUpMessage;
        zmqpp::message responseMessage;
        signUpMessage.add_raw(&request, sizeof request);
        clientSocket.send(signUpMessage);
        clientSocket.receive(responseMessage);
        auto response = *static_cast<Request *>(const_cast<void *>(responseMessage.raw_data()));
        if (response.authenticationStatus == AuthenticationStatus::Exists) {
            std::cout << "user exists" << std::endl;
            return 0;
        } else if (response.authenticationStatus == AuthenticationStatus::Success) {
            std::cout << "sing up succeeded" << std::endl;
        }
    }

    std::thread th(updater);
    while (true) {
        std::cout << "Choose:\n    1. Show chats\n    2. Create chat\n    3. Send message\n"
                     "    4. Show messages from chat\n    5. Invite user to chat\n"
                     "Enter num: ";
        std::cin >> command;

        if (command == 1) {
            for (const auto &chat: chats) {
                std::cout << "    " << chat << std::endl;
            }
        } else if (command == 2) {
            std::string chatName;
            std::cout << "Enter chat name: ";
            std::cin.ignore();
            std::getline(std::cin, chatName);

            std::string line;
            std::cout << "Enter usernames: ";
            std::getline(std::cin, line);
            std::stringstream ss(line);

            Message msg;
            msg.name = username;
            msg.buffer = chatName;
            msg.vector.push_back(username);

            for (std::string s; ss >> s;) {
                msg.vector.push_back(s);
            }

            auto req = Request(RequestType::CreateChat, msg);

            mutex.lock();
            sendRequest(clientSocket, req);
            receiveRequest(clientSocket, req);
            mutex.unlock();
            if (req.requestType == RequestType::Error) {
                std::cout << "error" << std::endl;
            }
        } else if (command == 3) {
            std::string chatName;
            std::string data;
            std::cout << "Enter chat name: ";
            std::cin >> chatName;
            std::cout << "Enter message:" << std::endl;
            std::cin.ignore();
            std::getline(std::cin, data);
            Message msg;
            msg.name = chatName;
            msg.buffer = data;
            auto req = Request(RequestType::SendMessage, msg);

            mutex.lock();
            sendRequest(clientSocket, req);
            receiveRequest(clientSocket, req);
            mutex.unlock();
        } else if (command == 4) {
            std::string chatName;
            std::cout << "Enter chat name: ";
            std::cin >> chatName;

            Message msg;
            msg.name = chatName;
            auto req = Request(RequestType::GetAllMessagesFromChat, msg);


            mutex.lock();
            sendRequest(clientSocket, req);
            receiveRequest(clientSocket, req);
            mutex.unlock();

            for (const auto &message: req.message.vector) {
                std::cout << message << std::endl;
            }
        } else if (command == 5) {
            std::string chatName, user;
            std::cout << "Enter chat name: ";
            std::cin >> chatName;
            std::cout << "Enter username: ";
            std::cin >> user;

            std::string value;
            std::cout << "Share history with user? (y/n): ";
            std::cin >> value;

            Message msg;
            msg.name = chatName;
            msg.buffer = user;

            if (value == "y" || value == "Y") {
                msg.flag = true;
            } else if (value == "n" || value == "N") {
                msg.flag = false;
            } else {
                std::cout << "invalid command" << std::endl;
                break;
            }
            auto req = Request(RequestType::InviteUserToChat, msg);


            mutex.lock();
            sendRequest(clientSocket, req);
            receiveRequest(clientSocket, req);
            mutex.unlock();
        } else {
            break;
        }

    }
    return 0;
}
