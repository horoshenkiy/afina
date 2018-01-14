//
// Created by ilya on 1/14/18.
//

#ifndef AFINA_CONNECTION_H
#define AFINA_CONNECTION_H

namespace Afina {
namespace Network {

class Server;

class Connection {

public:

    virtual void Start(Server *server, int client_socket) = 0;

    virtual void Stop() = 0;

};

}
}
#endif //AFINA_CONNECTION_H
