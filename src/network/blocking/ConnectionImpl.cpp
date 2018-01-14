//
// Created by ilya on 1/14/18.
//

#include "ConnectionImpl.h"
#include <unistd.h>
#include <protocol/Parser.h>
#include <cstring>
#include <afina/execute/Command.h>

namespace Afina {
namespace Network {
namespace Blocking {

//// Start and stop
//////////////////////////////////////////////////////////////////////////////////

void ConnectionImpl::Start(Server *server, int client_socket) {
    running.store(true);

    pthread_t connection;
    args = {this, (ServerImpl*)server, client_socket};

    if (pthread_create(&connection, NULL, this->RunConnectionProxy, &args) < 0) {
        throw std::runtime_error("Could not create server thread");
    }
}

void ConnectionImpl::Stop() {
    running.store(false);
    shutdown(args.socket, SHUT_RDWR);
}

//// Run connection
//////////////////////////////////////////////////////////////////////////////////

void* ConnectionImpl::RunConnectionProxy(void *p) {
    ConnArgs *args = reinterpret_cast<ConnArgs*>(p);

    args->server->AddConnection(args->connection);

    try {
        args->connection->RunConnection(args->socket);
    } catch (std::runtime_error &ex) {
        shutdown(args->socket, SHUT_RDWR);
        close(args->socket);
        std::cerr << "Server fails: " << ex.what() << std::endl;
    }

    args->server->EraseConnection(args->connection);
    delete args->connection;

    return 0;
}

void ConnectionImpl::RunConnection(int client_socket) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    // TODO: All connection work is here
    try {
        ParseAndExecuteCommand(client_socket);
    } catch (std::exception &ex) {
        std::string out = std::string("SERVER_ERROR ") + ex.what() + "\r\n";
        send(client_socket, out.c_str(), out.length(), 0);
    }

    close(client_socket);
    printf("Close connection!\n");
}

//// Parse and execute command
//////////////////////////////////////////////////////////////////////////////////

void ConnectionImpl::ParseAndExecuteCommand(int client_socket) {
    Protocol::Parser parser = Protocol::Parser();
    char *str_recv = new char[MAX_SIZE_RECV], *str_args = nullptr;

    size_t len_recv, parsed = 0, all_parsed;
    uint32_t len_args = 0, parsed_args = 0;

    std::unique_ptr<Execute::Command> p_command;
    bool is_create_command = false;
    std::string out;

    while(true) {
        if (recv(client_socket, str_recv, MAX_SIZE_RECV, 0) <= 0)
            break;

        len_recv = strlen(str_recv);
        if (!len_recv)
            continue;

        // TODO: refactoring to switch
        all_parsed = 0;
        while (all_parsed != len_recv || is_create_command) {

            //// execute command
            ////////////////////////////////////////////////////////////////
            if (is_create_command) {
                printf("execute command");

                if (str_args) {
                    (*p_command).Execute(*pStorage, std::string(str_args), out);
                    delete[] str_args;
                    str_args = nullptr;
                }
                else
                    (*p_command).Execute(*pStorage, std::string(), out);

                out += "\r\n";
                if (send(client_socket, out.c_str(), out.length(), 0) < out.length())
                    return;

                if (!running.load())
                    return;

                is_create_command = false;
                continue;
            }

            //// drop \n, \r
            ////////////////////////////////////////////////////////////////
            if (str_recv[all_parsed] == '\n' || str_recv[all_parsed] == '\r') {
                all_parsed++;
                continue;
            }

            //// parse arguments
            ////////////////////////////////////////////////////////////////
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

            //// parse command
            ////////////////////////////////////////////////////////////////
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
            ////////////////////////////////////////////////////////////////
        }
    }

    delete[] str_recv;
}

bool ConnectionImpl::ParseArgs(char* str_recv, uint32_t s_args, char *str_args, uint32_t &parsed) {
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

//////////////////////////////////////////////////////////////////////////////////

}
}
}