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


constexpr int32_t sendTimeout = 10 * 1000;
constexpr int32_t receiveTimeout = 10 * 1000;


class Server {
    Database db{};

    zmqpp::context context{};
    zmqpp::socket pullSocket{context, zmqpp::socket_type::pull};

    std::set<User> users{db.getAllUsers()};
    std::deque<std::thread> threads;

    auto findUser(const std::string &username);

    auto connectionMonitor() -> void;

    auto attachClient(zmqpp::socket &clientSocket, const std::string &clientEndPoint) -> User;

    // TODO: should be noexcept
    auto clientMonitor(const std::string &clientEndPoint) -> void;

public:
    static auto get() -> Server &;

    auto configurePullSocketEndPoint(const std::string &endPoint) -> void;

    auto run() -> void;
};


auto Server::findUser(const std::string &username) {
    return std::find_if(users.begin(), users.end(), [&username](auto user) -> bool {
        return user.username == username;
    });
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



auto Server::attachClient(zmqpp::socket &clientSocket, const std::string &clientEndPoint) -> User {
    clientSocket.set(zmqpp::socket_option::send_timeout, sendTimeout);
    clientSocket.set(zmqpp::socket_option::receive_timeout, receiveTimeout);

    clientSocket.connect(clientEndPoint);

    User user;
    Message authRequest;
    receiveMessage(clientSocket, authRequest);

    user.username = authRequest.message.name;

    AuthenticationStatus status;
    if (authRequest.messageType == MessageType::SignIn) {
        if (findUser(authRequest.message.name) == users.end()) {
            status = AuthenticationStatus::NotExists;
        } else {
            status = db.authenticateUser(authRequest.message.name, authRequest.message.buffer);
            user.id = db.getUserId(user.username);
        }
    } else if (authRequest.messageType == MessageType::SignUp) {
        if (findUser(authRequest.message.name) != users.end()) {
            status = AuthenticationStatus::Exists;
        } else {
            db.createUser(authRequest.message.name, authRequest.message.buffer);
            user.id = db.getUserId(user.username);
            if (user.id == -1) {
                // TODO: handle
            }
            status = AuthenticationStatus::Success;
            users.insert(user);
        }
    } else {
        sendMessage(clientSocket, Message(MessageType::ClientError));
        throw std::runtime_error("invalid massage type");
    }


    Message authResponse;
    authResponse.authenticationStatus = status;
    sendMessage(clientSocket, authResponse);

    if (status != AuthenticationStatus::Success) {
        throw std::runtime_error("auth error");
    }

    return user;
}


auto Server::clientMonitor(const std::string &clientEndPoint) -> void {
    std::cout << "new clientMonitor started, monitoring " << clientEndPoint << " port" << std::endl;

    try {
        zmqpp::socket clientSocket(context, zmqpp::socket_type::reply);

        User user = attachClient(clientSocket, clientEndPoint);

        while (true) {
            Message request;
            receiveMessage(clientSocket, request);
            switch (request.messageType) {
                case MessageType::CreateMessage: {
                    db.createMessage(request.message.name, user.id, time(nullptr), request.message.buffer);
                    break;
                }
                case MessageType::Update: {
                    break;
                }
                case MessageType::UpdateChats: {
                    std::cout << "update chats received" << std::endl;
                    auto it = findUser(request.message.name);
                    if (it == users.end()) {
                        request.messageType = MessageType::ClientError;
                        break;
                    }
                    request.message.vector = db.getChatsByTime(it->id, request.message.time);
                    request.message.time = time(nullptr);
                    break;
                }
                case MessageType::CreateChat: {
                    std::vector<int32_t> userIds;
                    userIds.reserve(request.message.vector.size());

                    for (const auto &username: request.message.vector) {
                        auto it = findUser(username);
                        if (it != users.end()) {
                            userIds.push_back(it->id);
                        } else {
                            request.messageType = MessageType::ClientError;
                            break;
                        }
                    }

                    db.createChat(request.message.buffer, user.id, userIds);
                    break;
                }
                case MessageType::GetAllMessagesFromChat: {
                    request.message.vector = db.getAllMessagesFromChat(request.message.name, user.id);
                    break;
                }
                case MessageType::InviteUserToChat: {
                    auto it = findUser(request.message.buffer);
                    if (it == users.end()) {
                        request.messageType = MessageType::ClientError;
                        break;
                    }

                    db.inviteUserToChat(request.message.name, user.id, it->id, request.message.flag);
                    break;
                }
                default:
                    break;
            }

            std::cout << "sending request back" << std::endl;
            sendMessage(clientSocket, request);
        }
    } catch (zmqpp::exception &exception) {
        std::cerr << "caught zmq exception: " << exception.what() << std::endl;
    } catch (std::runtime_error &exception) {
        std::cerr << exception.what() << std::endl;
    }

    std::cout << "client monitor exiting" << std::endl;
}


auto Server::get() -> Server & {
    static Server instance;
    return instance;
}


auto Server::configurePullSocketEndPoint(const std::string &endPoint) -> void {
    pullSocket.bind(endPoint);
}


auto Server::run() -> void {
    std::thread pullerThread(&Server::connectionMonitor, &Server::get());
    pullerThread.join();

    for (auto &thread: threads) {
        thread.join();
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