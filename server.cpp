#include <set>
#include <deque>
#include <string>
#include <thread>
#include <utility>
#include <iostream>
#include <algorithm>
#include <zmqpp/zmqpp.hpp>


#include "lib/user.hpp"
#include "lib/database.hpp"
#include "lib/messaging.hpp"
#include "lib/networking.hpp"


class Server {
    Database db{};

    zmqpp::context context{};
    zmqpp::socket pullSocket{context, zmqpp::socket_type::pull};

    std::set<User> users{db.getAllUsers()};
    std::deque<std::thread> threads;

    auto connectionMonitor() -> void;

    auto clientMonitor(const std::string &clientEndPoint) -> void;

    auto findUser(const std::string &username) {
        return std::find_if(users.begin(), users.end(), [&username](auto user) -> bool {
            return user.username == username;
        });
    }

public:
    static auto get() -> Server &;

    auto configurePullSocketEndPoint(const std::string &endPoint) -> void;

    auto run() -> void;
};


auto Server::get() -> Server & {
    static Server instance;
    return instance;
}


auto Server::configurePullSocketEndPoint(const std::string &endPoint) -> void {
    pullSocket.bind(endPoint);
}


auto Server::connectionMonitor() -> void {
    std::cout << "connectionMonitor started" << std::endl;
    try {
        while (pullSocket) {
            zmqpp::message message;
            pullSocket.receive(message);

            std::string s;
            message >> s;

            std::thread connectionMonitorThread(&Server::clientMonitor, &Server::get(), s);
            threads.push_back(std::move(connectionMonitorThread));
        }
    } catch (zmqpp::exception &exception) {
        std::cout << "connectionMonitor caught zmqpp exception: " << exception.what() << std::endl;
    } catch (...) {
        std::cout << "connectionMonitor caught undefined exception" << std::endl;
    }
    std::cout << "connectionMonitor exiting, new connections won't be maintained" << std::endl;
}


auto Server::run() -> void {
    std::thread pullerThread(&Server::connectionMonitor, &Server::get());
    pullerThread.join();

    for (auto &thread: threads) {
        thread.join();
    }
}


auto Server::clientMonitor(const std::string &clientEndPoint) -> void {
    std::cout << "new clientMonitor started, monitoring " << clientEndPoint << "port" << std::endl;
    std::string clientUsername;
    int32_t clientId;


    zmqpp::socket clientSocket(context, zmqpp::socket_type::reply);

    // options
    clientSocket.set(zmqpp::socket_option::connect_timeout, 2 * 1000);
    clientSocket.set(zmqpp::socket_option::receive_timeout, 100 * 1000);
    clientSocket.set(zmqpp::socket_option::send_timeout, 100 * 1000);

    try {
        clientSocket.connect(clientEndPoint);
    } catch (zmqpp::zmq_internal_exception &exception) {
        std::cout << exception.what() << std::endl;
        return;
    }
    // authentication
    zmqpp::message authentication;
    zmqpp::message authResult;
    clientSocket.receive(authentication);
    auto authRequest = *static_cast<Request *>(const_cast<void *>(authentication.raw_data()));

    clientUsername = authRequest.message.name;

    AuthenticationStatus status;
    if (authRequest.requestType == RequestType::SignIn) {
        if (findUser(authRequest.message.name) == users.end()) {
            status = AuthenticationStatus::NotExists;
        } else {
            status = db.authenticateUser(authRequest.message.name, authRequest.message.buffer);
            clientId = db.getUserId(clientUsername);
        }
    } else if (authRequest.requestType == RequestType::SignUp) {
        if (findUser(authRequest.message.name) != users.end()) {
            status = AuthenticationStatus::Exists;
        } else {
            db.createUser(authRequest.message.name, authRequest.message.buffer);
            clientId = db.getUserId(clientUsername);
            if (clientId == -1) {
                // TODO: handle
            }
            status = AuthenticationStatus::Success;
            users.insert(User(clientId, clientUsername));
        }
    } else {
        return;
    }


    authRequest.authenticationStatus = status;
    authResult.add_raw(&authRequest, sizeof authRequest);

    clientSocket.send(authResult);
    if (status != AuthenticationStatus::Success) {
        return;
    }


    while (true) {
        Request request;
        receiveRequest(clientSocket, request);
        switch (request.requestType) {
            case RequestType::SendMessage: {
                db.createMessage(request.message.name, clientId, time(nullptr), request.message.buffer);
                break;
            }
            case RequestType::Update: {
                break;
            }
            case RequestType::UpdateChats: {
                std::cout << "update chats received" << std::endl;
                auto it = findUser(request.message.name);
                if (it == users.end()) {
                    request.requestType = RequestType::Error;
                    break;
                }
                request.message.vector = db.getChatsByTime(it->id, request.message.time);
                request.message.time = time(nullptr);
                break;
            }
            case RequestType::CreateChat: {
                std::vector<int32_t> userIds;
                userIds.reserve(request.message.vector.size());

                for (const auto &username: request.message.vector) {
                    auto it = findUser(username);
                    if (it != users.end()) {
                        userIds.push_back(it->id);
                    } else {
                        request.requestType = RequestType::Error;
                        break;
                    }
                }

                db.createChat(request.message.buffer, clientId, userIds);
                break;
            }
            case RequestType::GetAllMessagesFromChat: {
                request.message.vector = db.getAllMessagesFromChat(request.message.name, clientId);
                break;
            }
            case RequestType::InviteUserToChat: {
                auto it = findUser(request.message.buffer);
                if (it == users.end()) {
                    request.requestType = RequestType::Error;
                    break;
                }

                db.inviteUserToChat(request.message.name, clientId, it->id, request.message.flag);
                break;
            }
            default:
                break;
        }

        std::cout << "sending request back" << std::endl;
        if (!sendRequest(clientSocket, request)) {
            break;
        }
    }

}


auto main() -> int {
    try {
        Server::get().configurePullSocketEndPoint("tcp://" + getIP() + ":4506");
        Server::get().run();
    } catch (std::runtime_error &err) {
        std::cout << err.what() << std::endl;
        exit(1);
    }
    return 0;
}