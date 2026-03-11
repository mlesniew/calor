#pragma once

#include <PicoMQTT.h>
#include <PicoUtils.h>

class MQTTServer : public PicoMQTT::Server {
public:
    const PicoUtils::Stopwatch & get_last_message_stopwatch() const {
        return last_message;
    }

protected:
    PicoUtils::Stopwatch last_message;

    void on_message(const char * topic, PicoMQTT::IncomingPacket & packet) {
        last_message.reset();
        PicoMQTT::Server::on_message(topic, packet);
    }
};