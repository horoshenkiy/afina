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

    ParseRunner() = default;

    ParseRunner(std::shared_ptr<Afina::Storage> ps) : pStorage(ps) {}

    void Load(char *recv, ssize_t len_recv);

    std::string Run();

    bool IsEmpty();

    void Reset() {
        parser.Reset();
    }

private:

    //// storage
    std::shared_ptr<Afina::Storage> pStorage;

    //// receive data
    char *recv;
    ssize_t len_recv;

    //// state of parsing
    ///////////////////////////////////////////////////////////
    bool is_empty = true;

    ssize_t all_parsed;

    // command
    Protocol::Parser parser = Protocol::Parser();
    std::unique_ptr<Execute::Command> p_command;

    // arguments
    char *args = nullptr;
    uint32_t len_args = 0, prev_parsed_args, parsed_args;

};

}
}
}

#endif //AFINA_EXECUTOR_H
