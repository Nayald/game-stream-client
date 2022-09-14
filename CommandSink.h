#ifndef REMOTE_CLIENT_COMMANDSINK_H
#define REMOTE_CLIENT_COMMANDSINK_H

class CommandSink {
public:
    CommandSink() = default;
    virtual ~CommandSink() = default;

    virtual void handle(const std::string &msg) = 0;
    virtual void handle(const char *msg, size_t size) = 0;
};

#endif //REMOTE_CLIENT_COMMANDSINK_H
