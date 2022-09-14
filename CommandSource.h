#ifndef REMOTE_CLIENT_COMMANDSOURCE_H
#define REMOTE_CLIENT_COMMANDSOURCE_H

#include <unordered_set>
#include "CommandSink.h"

class CommandSource {
protected:
    std::unordered_set<CommandSink*> command_sinks;

public:
    CommandSource() = default;
    virtual ~CommandSource() = default;

    void attachInputSink(CommandSink *sink) {
        command_sinks.emplace(sink);
    }

    void detachCommandSink(CommandSink *sink) {
        command_sinks.erase(sink);
    }
};

#endif //REMOTE_CLIENT_COMMANDSOURCE_H
