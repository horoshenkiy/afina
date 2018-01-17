#ifndef AFINA_NETWORK_NONBLOCKING_WORKER_H
#define AFINA_NETWORK_NONBLOCKING_WORKER_H

#include <atomic>
#include <memory>
#include <pthread.h>
#include <afina/Storage.h>
#include <network/blocking/ParseRunner.h>
#include <iostream>

namespace Afina {

// Forward declaration, see afina/Storage.h
class Storage;

namespace Network {
namespace NonBlocking {

/**
 * # Thread running epoll
 * On Start spaws background thread that is doing epoll on the given server
 * socket and process incoming connections and its data
 */

class Worker {
public:

    Worker() = default;
    Worker(std::shared_ptr<Afina::Storage> ps);

    Worker(const Worker &) = delete;
    Worker(Worker &&) = delete;

    ~Worker();

    /**
     * Spaws new background thread that is doing epoll on the given server
     * socket. Once connection accepted it must be registered and being processed
     * on this thread
     */
    void Start(int server_socket);

    /**
     * Signal background thread to stop. After that signal thread must stop to
     * accept new connections and must stop read new commands from existing. Once
     * all readed commands are executed and results are send back to client, thread
     * must stop
     */
    void Stop();

    /**
     * Blocks calling thread until background one for this worker is actually
     * been destoryed
     */
    void Join();

protected:
    /**
     * Method executing by background thread
     */
    void OnRun();

    static void* OnRunProxy(void *args);

private:
    ///////////////////////////////////////////////////////////////////////////

    pthread_t thread;

    std::atomic<bool> running;

    std::shared_ptr<Afina::Storage> pStorage;

    Afina::Network::Blocking::ParseRunner parseRunner;

    int server_socket, epoll_fd;


};

} // namespace NonBlocking
} // namespace Network
} // namespace Afina
#endif // AFINA_NETWORK_NONBLOCKING_WORKER_H
