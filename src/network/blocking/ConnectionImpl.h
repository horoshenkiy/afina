//
// Created by ilya on 1/14/18.
//

#ifndef AFINA_CONNECTIONIMPL_H
#define AFINA_CONNECTIONIMPL_H

#include <afina/network/Connection.h>
#include <netdb.h>
#include "ServerImpl.h"

#define MAX_SIZE_RECV 1024

namespace Afina {
namespace Network {
namespace Blocking {

class ConnectionImpl : public Connection {

public:

    ConnectionImpl() = default;

    ConnectionImpl(std::shared_ptr<Afina::Storage> ps) : pStorage(ps) {}

    void Start(Server *server, int client_socket) override;

    void Stop() override;

private:

    //// Run connection
    //////////////////////////////////////////////////////////////////////////////////

    static void* RunConnectionProxy(void *p);

    void RunConnection(int client_socket);

    //// parse and execute command
    //////////////////////////////////////////////////////////////////////////////////

    void ParseAndExecuteCommand(int client_socket);

    bool ParseArgs(char* str_recv, uint32_t s_args, char *str_args, uint32_t &parsed);

    //////////////////////////////////////////////////////////////////////////////////

    std::atomic<bool> running;

    std::shared_ptr<Afina::Storage> pStorage;

    struct ConnArgs {
        ConnectionImpl *connection;
        ServerImpl *server;
        int socket;
    };

    ConnArgs args;

};

}
}
}

#endif //AFINA_CONNECTIONIMPL_H
