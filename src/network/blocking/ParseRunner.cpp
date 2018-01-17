//
// Created by ilya on 1/15/18.
//

#include <iostream>
#include "ParseRunner.h"

namespace Afina {
namespace Network {
namespace Blocking {

bool ParseArgs(char* str_recv, uint32_t s_args, char *str_args, uint32_t &parsed);


//// Implementation
////////////////////////////////////////////////////////////////////////////////

bool ParseRunner::IsEmpty() { return is_empty; }

void ParseRunner::Load(char *recv, ssize_t len_recv) {
    this->recv = recv;
    this->len_recv = len_recv;

    all_parsed = 0, is_empty = false;
}

std::string ParseRunner::Run() {
    bool is_create_command = false;
    std::string out;

    while (all_parsed != len_recv || is_create_command) {

        //// execute command
        ////////////////////////////////////////////////////////////////
        if (is_create_command) {
            if (args) {
                (*p_command).Execute(*pStorage, std::string(args), out);
                delete[] args;
                args = nullptr;
            }
            else
                (*p_command).Execute(*pStorage, std::string(), out);

            len_args = 0;

            out += "\r\n";
            return out;
        }

        //// drop \n, \r
        ////////////////////////////////////////////////////////////////
        if (recv[all_parsed] == '\n' || recv[all_parsed] == '\r') {
            all_parsed++;
            continue;
        }

        //// parse arguments
        ////////////////////////////////////////////////////////////////
        if (len_args) {
            // need for few package of arguments
            prev_parsed_args = parsed_args;

            if (ParseArgs(recv + all_parsed, len_args - parsed_args, args + parsed_args, parsed_args)) {
                all_parsed += parsed_args - prev_parsed_args;
                len_args = 0, parsed_args = 0;
                is_create_command = true;
                continue;
            } else
                break;
        }

        //// parse command
        ////////////////////////////////////////////////////////////////
        size_t parsed = 0;
        if (parser.Parse(recv + all_parsed, len_recv - all_parsed, parsed)) {
            all_parsed += parsed;

            p_command = parser.Build(len_args);
            parser.Reset();

            if (len_args) {
                args = new char[len_args + 1];
                args[len_args] = '\0';
                parsed_args = 0;
            } else {
                is_create_command = true;
            }
        } else {
            all_parsed += parsed;
            break;
        }
        ////////////////////////////////////////////////////////////////
    }

    is_empty = true;
    return "";
}

bool ParseArgs(char* str_recv, uint32_t s_args, char *str_args, uint32_t &parsed) {
    size_t size_recv = strlen(str_recv);

    if (size_recv < s_args) {
        memcpy(str_args, str_recv, size_recv);
        parsed += size_recv;
        return false;
    }
    else {
        memcpy(str_args, str_recv, s_args);
        parsed += s_args;
        return true;
    }
}

}
}
}