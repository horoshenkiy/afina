//
// Created by ilya on 1/15/18.
//

#include <iostream>
#include "ParseRunner.h"

namespace Afina {
namespace Network {
namespace Blocking {

void ParseRunner::Reset() {
    parser.Reset();

    if (len_args)
        delete[] args;

    len_args = 0;
}

bool ParseRunner::IsDone() { return is_done; }

ssize_t ParseRunner::GetParsed() { return all_parsed; }

void ParseRunner::Load(char *recv, ssize_t len_recv) {
    this->recv = recv;
    this->len_recv = len_recv;

    all_parsed = 0, is_done = false;
}

std::string ParseRunner::Run() {

    std::string out;
    size_t parsed;
    bool is_exit = false;

    while (!is_exit) {

        // drop \n, \r
        if (all_parsed != len_recv && (recv[all_parsed] == '\n' || recv[all_parsed] == '\r')) {
            all_parsed++;
            continue;
        }

        // parse and execute
        switch (state) {
            // parse command
            case State::sParseComm:
                parsed = 0;
                if (parser.Parse(recv + all_parsed, len_recv - all_parsed, parsed)) {
                    p_command = parser.Build(len_args);
                    parser.Reset();

                    if (len_args) {
                        args = new char[len_args];
                        state = State::sParseArgs;
                    } else
                        state = State::sComm;

                } else
                    is_exit = true;

                all_parsed += parsed;
                break;

            // parse arguments
            case State::sParseArgs:
                prev_parsed_args = parsed_args;

                if (ParseArgs(recv + all_parsed, len_args - parsed_args, args + parsed_args, parsed_args)) {
                    all_parsed += parsed_args - prev_parsed_args;
                    parsed_args = 0;
                    state = State::sComm;
                } else
                    is_exit = true;

                break;

            // execute command
            case State::sComm:
                std::string strArgs = (len_args) ? std::string(args, len_args) : std::string();
                Reset();

                p_command.get()->Execute(*pStorage, strArgs, out);
                state = State::sParseComm;

                return out + "\r\n";
        }
    }

    is_done = true;
    return "";
}

inline bool ParseRunner::ParseArgs(char* str_recv, uint32_t s_args, char *str_args, uint32_t &parsed) {
    ssize_t size_recv = (len_recv - all_parsed < s_args) ? len_recv - all_parsed : s_args;

    memcpy(str_args, str_recv, size_recv);
    parsed += size_recv;

    return size_recv == s_args;
}

}
}
}