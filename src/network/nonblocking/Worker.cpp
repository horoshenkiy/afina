#include "Worker.h"

#include <iostream>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <protocol/Parser.h>

#include <afina/execute/Command.h>
#include <cstring>

#include "Utils.h"

#define MAXEVENTS 64

namespace Afina {
namespace Network {
namespace NonBlocking {

// See Worker.h
Worker::Worker(std::shared_ptr<Afina::Storage> ps) {
    pStorage = ps;
}

// See Worker.h
Worker::~Worker() {}

// See Worker.h
void Worker::Start(int server_socket) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    bool is_start = false;
    if(!running.compare_exchange_strong(is_start, true))
        return;

    this->server_socket = server_socket;
    if (pthread_create(&thread, NULL, OnRunProxy, this) < 0) {
        throw std::runtime_error("Could not create worker thread");
    }
}

// See Worker.h
void Worker::Stop() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    running.store(false);

}

// See Worker.h
void Worker::Join() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    pthread_join(thread, 0);
}

void* Worker::OnRunProxy(void *p) {
    Worker *worker = reinterpret_cast<Worker *>(p);
    try {
        worker->OnRun();
    } catch (std::runtime_error &ex) {
        std::cerr << "Server fails: " << ex.what() << std::endl;
    }
    return 0;
}

// See Worker.h
void Worker::OnRun() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    int info;

    struct epoll_event event;
    struct epoll_event *events;

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create");
        throw std::runtime_error("Could not create epoll");
    }

    event.data.fd = server_socket;
    event.events = EPOLLIN | EPOLLET | EPOLLEXCLUSIVE;
    info = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event);
    if (info == -1) {
        perror("epoll_ctl");
        throw std::runtime_error("Epoll control error!");
    }

    // Create buffer for events
    events = (epoll_event*)calloc(MAXEVENTS, sizeof(event));

    // Main looper
    std::string out;
    while (running.load()) {
        int n = epoll_wait(epoll_fd, events, MAXEVENTS, -1);

        if (!running.load()) {
            close(epoll_fd);
            return;
        }

        Connection *temp = new Connection();
        delete temp;

        for (int i = 0; i < n; i++) {

            if ((events[i].events & EPOLLERR)  || (events[i].events & EPOLLHUP) || (!events[i].events & EPOLLIN)) {
                if (events[i].events & EPOLLHUP)
                    printf("Close connection!\n");
                else
                    fprintf(stderr, "Error with file descriptor\n");

                // delete struct for connection
                if (connections.find(events[i].data.fd) != connections.end()) {
                    delete connections[events[i].data.fd];
                    connections.erase(events[i].data.fd);
                }

                close(events[i].data.fd);
                continue;
            }
            else if (server_socket == events[i].data.fd) {
                while (true) {
                    struct sockaddr in_addr;
                    socklen_t in_len;
                    int in_fd;

                    in_len = sizeof(in_addr);
                    in_fd = accept(server_socket, &in_addr, &in_len);

                    if (in_fd == -1) {
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                            break;
                        } else {
                            perror("accept");
                            break;
                        }
                    }

                    make_socket_non_blocking(in_fd);

                    event.data.fd = in_fd;
                    event.events = EPOLLIN | EPOLLET;
                    info = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, in_fd, &event);
                    if (info == -1) {
                        perror("epoll_ctl");
                        throw std::runtime_error("Epoll control error!");
                    }

                    // create struct for connection
                    struct Connection *connection = new Connection(in_fd, pStorage);
                    connections.insert(std::pair<int, Connection*>(in_fd, connection));
                }
            } else {
                bool done = false, receive = true;

                // get Parse Runner
                if (connections.find(events[i].data.fd) == connections.end())
                    throw std::runtime_error("Not struct for connection!");

                Connection *connection = connections[events[i].data.fd];
                Afina::Network::Blocking::ParseRunner *parseRunner = connection->parseRunner;

                // begin receive
                while (receive) {
                    ssize_t count = recv(events[i].data.fd,
                                         connection->buf + connection->curr_begin,
                                         MAXSIZERECV - connection->curr_begin, 0);

                    if (count == -1) {
                        if (errno != EAGAIN) {
                            perror("read");
                            done = true;
                        }
                        break;
                    } else if (count == 0) {
                        done = true;
                        break;
                    }

                    // begin parsing
                    count += connection->curr_begin;
                    parseRunner->Load(connection->buf, count);

                    while (!parseRunner->IsDone()) {
                        try {
                            out = parseRunner->Run();
                        } catch (std::exception &ex) {
                            parseRunner->Reset();

                            out = std::string("SERVER_ERROR ") + ex.what() + "\r\n";
                            connection->TrySend(out);

                            done = true, receive = false;
                            break;
                        }

                        if (out == "")
                            continue;

                        if (!connection->TrySend(out)) {
                            done = true, receive = false;
                            break;
                        }

                        if (!running.load()) {
                            done = true, receive = false;
                            break;
                        }
                    }

                    ssize_t parsed = parseRunner->GetParsed();
                    if (parsed < count) {
                        memcpy(connection->buf, connection->buf + parsed, count - parsed);
                        connection->curr_begin = count - parsed;
                    }
                }

                if (done) {
                    printf("Close connection!\n");

                    // send last messages
                    while (!connection->qMessages.empty() && connection->AllSend());

                    // delete struct for connection
                    if (connections.find(events[i].data.fd) != connections.end()) {
                        delete connections[events[i].data.fd];
                        connections.erase(events[i].data.fd);
                    }

                    close(events[i].data.fd);
                }
            }
        }
    }
}

// Implementation for connection
Worker::Connection::Connection(int client_socket, std::shared_ptr<Afina::Storage> storage) {
    this->client_socket = client_socket;
    parseRunner = new Afina::Network::Blocking::ParseRunner(storage);
}

Worker::Connection::~Connection() {
    delete parseRunner;
}

bool Worker::Connection::AllSend() {

    while (!qMessages.empty()) {
        std::string &out = qMessages.front();

        ssize_t n = send(client_socket, out.c_str(), out.length(), 0);
        if (n == -1) {
            if (errno != ENOBUFS)
                return false;

            out = out.substr(n);
            return true;

        } else if (n < out.length()) {
            out = out.substr(n);
            return true;

        } else {
            qMessages.pop();
        }
    }

    return true;
}

bool Worker::Connection::TrySend(std::string &strMessage) {
    qMessages.push(strMessage);
    return AllSend();
}


} // namespace NonBlocking
} // namespace Network
} // namespace Afina
