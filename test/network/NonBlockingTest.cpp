//
// Created by ilya on 11/14/17.
//

#include <gtest/gtest.h>

#include <afina/execute/Command.h>
#include "../../src/protocol/Parser.h"

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

TEST(ParseCase, ParseComm) {
    Afina::Protocol::Parser parser = Afina::Protocol::Parser();
    char *str_recv, *str_args;

    char *str_first = "set foo 0 0";
    char *str_second = " 6\r\nfooval\r\nadd bar 10 -1 6\r\nbar";
    char *str_third = "val\r\n";

    size_t len_recv, parsed = 0, all_parsed;
    uint32_t len_args = 0, parsed_args = 0;

    std::unique_ptr<Afina::Execute::Command> p_command;

    bool is_create_command = false;

    int i = 0;
    while(i != 3) {
        if (i == 0)
            str_recv = str_first;

        if (i == 1)
            str_recv = str_second;

        if (i == 2)
            str_recv = str_third;

        len_recv = strlen(str_recv);
        if (!len_recv)
            continue;

        all_parsed = 0;
        while (all_parsed != len_recv) {

            if (is_create_command) {
                //(*p_command).Execute()
                delete[] str_args;
                is_create_command = false;
            }

            if (str_recv[all_parsed] == '\n' || str_recv[all_parsed] == '\r') {
                all_parsed++;
                continue;
            }

            if (len_args) {
                parsed = parsed_args;

                if (ParseArgs(str_recv + all_parsed, len_args - parsed_args, str_args + parsed_args, parsed_args)) {
                    all_parsed += parsed_args - parsed;
                    len_args = 0, parsed_args = 0;
                    is_create_command = true;
                    continue;
                } else
                    break;
            }

            parsed = 0;
            if (parser.Parse(str_recv + all_parsed, len_recv - all_parsed, parsed)) {
                all_parsed += parsed;

                p_command = parser.Build(len_args);
                parser.Reset();

                if (len_args) {
                    str_args = new char[len_args];
                    parsed_args = 0;
                } else {
                    is_create_command = true;
                }
            } else
                break;
        }

        i++;
    }
}

