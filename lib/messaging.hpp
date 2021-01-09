#ifndef CP_MESSAGING_HPP
#define CP_MESSAGING_HPP


#include <string>
#include <cstddef>
#include <zmqpp/zmqpp.hpp>
#include <msgpack.hpp>
#include <utility>


#include "auth.hpp"


struct Message {
    int32_t time{};
    std::string name{};
    std::string buffer{};
    bool flag{};
    std::vector<std::string> vector{};


    Message() = default;

    Message(time_t time, std::string username, std::string data) : time(time), name(std::move(username)),
                                                                   buffer(std::move(data)) {}

    Message(std::string username, std::string buffer) : name(std::move(username)),
                                                        buffer(std::move(buffer)) {}

    MSGPACK_DEFINE (time, name, buffer, flag, vector)
};


enum class RequestType {
    SendMessage,
    Update,
    SignIn,
    SignUp,
    CreateChat,
    UpdateChats,
    GetAllMessagesFromChat,
    InviteUserToChat,
    Error
};


struct Request {
    RequestType requestType{};
    AuthenticationStatus authenticationStatus{};
    Message message{};

    Request() = default;

    explicit Request(RequestType requestType) : requestType(requestType) {}

    Request(RequestType requestType, Message message) : requestType(requestType), message(std::move(message)) {}

    MSGPACK_DEFINE (requestType, authenticationStatus, message);
};


auto sendRequest(zmqpp::socket &socket, const Request &request) -> bool;

auto receiveRequest(zmqpp::socket &socket, Request &request) -> bool;


MSGPACK_ADD_ENUM(RequestType)
MSGPACK_ADD_ENUM(AuthenticationStatus)


#endif //CP_MESSAGING_HPP
