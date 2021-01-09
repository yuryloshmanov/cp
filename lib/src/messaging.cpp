#include "../messaging.hpp"


auto sendRequest(zmqpp::socket &socket, const Request &request) -> bool {
    zmqpp::message zmqMessage;
    msgpack::sbuffer package;

    msgpack::pack(&package, request);
    zmqMessage.add_raw(package.data(), package.size());

    // TODO: NO BLOCK
    return socket.send(zmqMessage);
}


auto receiveRequest(zmqpp::socket &socket, Request &request) -> bool {
    zmqpp::message message;
    // TODO: NO BLOCK
    if (!socket.receive(message)) {
        return false;
    }

    msgpack::unpacked unpackedPackage;
    msgpack::unpack(unpackedPackage, static_cast<const char *>(message.raw_data()), message.size(0));
    unpackedPackage.get().convert(request);

    return true;
}
