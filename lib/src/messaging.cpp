#include "../messaging.hpp"


auto sendMessage(zmqpp::socket &socket, const Message &message) -> bool {
    zmqpp::message zmqMessage;
    msgpack::sbuffer package;

    msgpack::pack(&package, message);
    zmqMessage.add_raw(package.data(), package.size());

    // TODO: NO BLOCK
    return socket.send(zmqMessage);
}


auto receiveMessage(zmqpp::socket &socket, Message &message) -> bool {
    zmqpp::message zmqMessage;
    // TODO: NO BLOCK
    if (!socket.receive(zmqMessage)) {
        return false;
    }

    msgpack::unpacked unpackedPackage;
    msgpack::unpack(unpackedPackage, static_cast<const char *>(zmqMessage.raw_data()), zmqMessage.size(0));
    unpackedPackage.get().convert(message);

    return true;
}
