#include "ServerImpl.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>

#include <pthread.h>
#include <signal.h>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#include <afina/Storage.h>
#include <afina/execute/Command.h>
#include <storage/MapBasedGlobalLockImpl.h>
#include "protocol/Parser.h"

#define MAX_SIZE_RECV 1024

namespace Afina {
namespace Network {
namespace Blocking {

void *ServerImpl::RunAcceptorProxy(void *p) {
    ServerImpl *srv = reinterpret_cast<ServerImpl *>(p);
    try {
        srv->RunAcceptor();
    } catch (std::runtime_error &ex) {
        std::cerr << "Server fails: " << ex.what() << std::endl;
    }
    return 0;
}

// See Server.h
ServerImpl::ServerImpl(std::shared_ptr<Afina::Storage> ps) : Server(ps) {}

// See Server.h
ServerImpl::~ServerImpl() {}

// See Server.h
void ServerImpl::Start(uint32_t port, uint16_t n_workers) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    // If a client closes a connection, this will generally produce a SIGPIPE
    // signal that will kill the process. We want to ignore this signal, so send()
    // just returns -1 when this happens.
    sigset_t sig_mask;
    sigemptyset(&sig_mask);
    sigaddset(&sig_mask, SIGPIPE);
    if (pthread_sigmask(SIG_BLOCK, &sig_mask, NULL) != 0) {
        throw std::runtime_error("Unable to mask SIGPIPE");
    }

    // Setup server parameters BEFORE thread created, that will guarantee
    // variable value visibility
    max_workers = n_workers;
    listen_port = port;

    // Create a new thread.
    running.store(true);
    if (pthread_create(&accept_thread, NULL, ServerImpl::RunAcceptorProxy, this) < 0) {
        throw std::runtime_error("Could not create server thread");
    }
}

// See Server.h
void ServerImpl::Stop() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    running.store(false);
    shutdown(server_socket, SHUT_RDWR);
}

// See Server.h
void ServerImpl::Join() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    pthread_join(accept_thread, 0);
}

// See Server.h
void ServerImpl::RunAcceptor() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    // For IPv4 we use struct sockaddr_in:
    // struct sockaddr_in {
    //     short int          sin_family;  // Address family, AF_INET
    //     unsigned short int sin_port;    // Port number
    //     struct in_addr     sin_addr;    // Internet address
    //     unsigned char      sin_zero[8]; // Same size as struct sockaddr
    // };
    //
    // Note we need to convert the port to network order

    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;          // IPv4
    server_addr.sin_port = htons(listen_port); // TCP port number
    server_addr.sin_addr.s_addr = INADDR_ANY;  // Bind to any address

    // Arguments are:
    // - Family: IPv4
    // - Type: Full-duplex stream (reliable)
    // - Protocol: TCP
    server_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == -1) {
        throw std::runtime_error("Failed to open socket");
    }

    // when the server closes the socket,the connection must stay in the TIME_WAIT state to
    // make sure the client received the acknowledgement that the connection has been terminated.
    // During this time, this port is unavailable to other processes, unless we specify this option
    //
    // This option let kernel knows that we are OK that multiple threads/processes are listen on the
    // same port. In a such case kernel will balance input traffic between all listeners (except those who
    // are closed already)
    int opts = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opts, sizeof(opts)) == -1) {
        close(server_socket);
        throw std::runtime_error("Socket setsockopt() failed");
    }

    // Bind the socket to the address. In other words let kernel know data for what address we'd
    // like to see in the socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        close(server_socket);
        throw std::runtime_error("Socket bind() failed");
    }

    // Start listening. The second parameter is the "backlog", or the maximum number of
    // connections that we'll allow to queue up. Note that listen() doesn't block until
    // incoming connections arrive. It just makesthe OS aware that this process is willing
    // to accept connections on this socket (which is bound to a specific IP and port)
    if (listen(server_socket, 5) == -1) {
        close(server_socket);
        throw std::runtime_error("Socket listen() failed");
    }

    int client_socket;
    struct sockaddr_in client_addr;
    socklen_t sinSize = sizeof(struct sockaddr_in);
    while (running.load()) {
        std::cout << "network debug: waiting for connection..." << std::endl;

        // When an incoming connection arrives, accept it. The call to accept() blocks until
        // the incoming connection arrives
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &sinSize)) == -1)
            break;

        // Check size of connections
        if (SizeConnections() == max_workers) {
            close(client_socket);
            continue;
        }

        // Start new thread and process data from/to connection
        pthread_t connection;
        ConnectionArgs args = {this, client_socket};

        if (pthread_create(&connection, NULL, ServerImpl::RunConnectionProxy, &args) < 0) {
            throw std::runtime_error("Could not create server thread");
        }

        AddConnection(connection, client_socket);
    }

    std::unique_lock<std::mutex> __lock(connections_mutex);
    for (auto p : connections)
        shutdown(p.second, SHUT_RDWR);
    __lock.unlock();

    // Cleanup on exit...
    close(server_socket);

    // Wait until for all connections to be complete
    __lock.lock();
    while (!connections.empty()) {
        connections_cv.wait(__lock);
    }
    __lock.unlock();
}

//// run connection
//////////////////////////////////////////////////////////////////////////////////

void* ServerImpl::RunConnectionProxy(void *p) {
    ConnectionArgs *args = reinterpret_cast<ConnectionArgs*>(p);

    try {
        args->server->RunConnection(args->socket);
    } catch (std::runtime_error &ex) {
        shutdown(args->socket, SHUT_RDWR);
        close(args->socket);
        std::cerr << "Server fails: " << ex.what() << std::endl;
    }

    return 0;
}

// See Server.h
void ServerImpl::RunConnection(int client_socket) {
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

    // TODO: create as function
    // Thread is about to stop, remove self from list of connections
    // and it was the very last one, notify main thread
    EraseConnection(pthread_self());

    if (connections.empty()) {
        // We are pretty sure that only ONE thread is waiting for connections
        // queue to be empty - main thread
        connections_cv.notify_one();
        return;
    }

    std::cout << "сдох!!\n";

}

//// parse and execute command
//////////////////////////////////////////////////////////////////////////////////

void ServerImpl::ParseAndExecuteCommand(int client_socket) {
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

                if (send(client_socket, (out + "\r\n").c_str(), out.length(), 0) < out.length())
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

bool ServerImpl::ParseArgs(char* str_recv, uint32_t s_args, char *str_args, uint32_t &parsed) {
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

} // namespace Blocking
} // namespace Network
} // namespace Afina
