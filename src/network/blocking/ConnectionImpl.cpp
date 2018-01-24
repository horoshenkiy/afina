//
// Created by ilya on 1/14/18.
//

#include "ConnectionImpl.h"
#include "ParseRunner.h"
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
    ReceiveAndExecute(client_socket);

    shutdown(client_socket, SHUT_RDWR);
    close(client_socket);
    printf("Close connection!\n");
}

//// Parse and execute command
//////////////////////////////////////////////////////////////////////////////////

void ConnectionImpl::ReceiveAndExecute(int client_socket) {
    char *str_recv = new char[MAX_SIZE_RECV];
    ssize_t len_recv = 0;

    ParseRunner executor = ParseRunner(pStorage);

    std::string out;
    while (true) {
        if ((len_recv = recv(client_socket, str_recv, MAX_SIZE_RECV, 0)) <= 0)
            break;

        str_recv[len_recv] = '\0';

        executor.Load(str_recv, len_recv);
        while (!executor.IsDone()) {
            try {
                out = executor.Run();
            } catch (std::exception &ex) {
                delete[] str_recv;
                out = std::string("SERVER_ERROR ") + ex.what() + "\r\n";
                send(client_socket, out.c_str(), out.length(), 0);
                return;
            }

            if (out == "")
                continue;

            if (send(client_socket, out.c_str(), out.length(), 0) < out.length()) {
                delete[] str_recv;
                throw std::runtime_error("Error send message");
            }

            if (!running.load()) {
                delete[] str_recv;
                return;
            }
        }

    }

    delete[] str_recv;
}

}
}
}