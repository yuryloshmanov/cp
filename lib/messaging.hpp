#ifndef CP_MESSAGING_HPP
#define CP_MESSAGING_HPP


#include <string>
#include <cstddef>
#include <zmqpp/zmqpp.hpp>
#include <msgpack.hpp>
#include <utility>


#include "auth.hpp"


enum class MessageType {
    CreateMessage,
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


struct Message {
    MessageType messageType{};
    AuthenticationStatus authenticationStatus{};
    MessageData message{};

    Message() = default;

    explicit Message(MessageType messageType) : messageType(messageType) {}

    Message(MessageType messageType, MessageData message) : messageType(messageType), message(std::move(message)) {}

    MSGPACK_DEFINE (messageType, authenticationStatus, message);
};


auto sendMessage(zmqpp::socket &socket, const Message &message) -> bool;

auto receiveMessage(zmqpp::socket &socket, Message &message) -> bool;


MSGPACK_ADD_ENUM(MessageType)
MSGPACK_ADD_ENUM(AuthenticationStatus)


#endif //CP_MESSAGING_HPP
