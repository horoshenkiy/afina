//
// Created by ilya on 1/15/18.
//

#ifndef AFINA_EXECUTOR_H
#define AFINA_EXECUTOR_H

#include <stdio.h>
#include <protocol/Parser.h>
#include <cstring>

#include <afina/execute/Command.h>


namespace Afina {
namespace Network {
namespace Blocking {

class ParseRunner {

public:

    // constructors
    ParseRunner() = default;
    ParseRunner(std::shared_ptr<Afina::Storage> ps) : pStorage(ps) {}

    ParseRunner(const ParseRunner &other) = default;
    ParseRunner(ParseRunner &&other) = default;

    ParseRunner& operator=(const ParseRunner& other) = default;

    // API
    void Load(char *recv, ssize_t len_recv);

    std::string Run();

    void Reset();

    // Check status
    bool IsDone();

    ssize_t GetParsed();

private:

    // Help methods
    bool ParseArgs(char* str_recv, uint32_t s_args, char *str_args, uint32_t &parsed);

    //// storage
    std::shared_ptr<Afina::Storage> pStorage;

    //// receive data
    char *recv;
    ssize_t len_recv;

    //// state of parsing
    ///////////////////////////////////////////////////////////
    bool is_done = true;

    ssize_t all_parsed;

    // command
    Protocol::Parser parser = Protocol::Parser();
    std::shared_ptr<Execute::Command> p_command;

    // arguments
    char *args = nullptr;
    uint32_t len_args = 0, prev_parsed_args, parsed_args;

    // State of ParseRunner
    enum State : int {sParseComm, sParseArgs, sComm};
    State state = State::sParseComm;

};

}
}
}

#endif //AFINA_EXECUTOR_H
