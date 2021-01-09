#ifndef CP_MESSAGING_HPP
#define CP_MESSAGING_HPP


#include <string>
#include <cstddef>
#include <zmqpp/zmqpp.hpp>
#include <msgpack.hpp>
#include <utility>


#include "auth.hpp"


enum class RequestType {
    SendMessage,
    Update,
    SignIn,
    SignUp,
    CreateChat,
    UpdateChats,
    GetAllMessagesFromChat,
    InviteUserToChat,
    ClientError,
    ServerError
};

struct MessageData {
    int32_t time{};
    std::string name{};
    std::string buffer{};
    bool flag{};
    std::vector<std::string> vector{};


    MessageData() = default;

    MessageData(time_t time, std::string username, std::string data) : time(time), name(std::move(username)),
                                                                       buffer(std::move(data)) {}

    MessageData(std::string username, std::string buffer) : name(std::move(username)),
                                                            buffer(std::move(buffer)) {}

    MSGPACK_DEFINE (time, name, buffer, flag, vector)
};


struct Request {
    RequestType requestType{};
    AuthenticationStatus authenticationStatus{};
    MessageData message{};

    Request() = default;

    explicit Request(RequestType requestType) : requestType(requestType) {}

    Request(RequestType requestType, MessageData message) : requestType(requestType), message(std::move(message)) {}

    MSGPACK_DEFINE (requestType, authenticationStatus, message);
};


auto sendRequest(zmqpp::socket &socket, const Request &request) -> bool;

auto receiveRequest(zmqpp::socket &socket, Request &request) -> bool;


MSGPACK_ADD_ENUM(RequestType)
MSGPACK_ADD_ENUM(AuthenticationStatus)


#endif //CP_MESSAGING_HPP
