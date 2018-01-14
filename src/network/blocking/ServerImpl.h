#ifndef AFINA_NETWORK_BLOCKING_SERVER_H
#define AFINA_NETWORK_BLOCKING_SERVER_H

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <pthread.h>

#include <afina/network/Server.h>
#include <unordered_map>
#include <assert.h>

namespace Afina {
namespace Network {
namespace Blocking {

/**
 * # Network resource manager implementation
 * Server that is spawning a separate thread for each connection
 */
class ServerImpl : public Server {
public:
    ServerImpl(std::shared_ptr<Afina::Storage> ps);
    ~ServerImpl();

    // See Server.h
    void Start(uint32_t port, uint16_t workers) override;

    // See Server.h
    void Stop() override;

    // See Server.h
    void Join() override;

protected:
    /**
     * Method is running in the connection acceptor thread
     */
    void RunAcceptor();

    /**
     * Methos is running for each connection
     */
    void RunConnection(int client_socket);

private:

    static void *RunAcceptorProxy(void *p);

    static void *RunConnectionProxy(void *p);

    //// struct for arguments of connection
    ///////////////////////////////////////////////////////////////////////////////

    struct ConnectionArgs {
        ServerImpl *server;
        int socket;
    };

    //// Methods for miltithreading work woth connections
    ///////////////////////////////////////////////////////////////////////////////

    void AddConnection(pthread_t pthread, int client_socket) {
        std::unique_lock<std::mutex> __lock(connections_mutex);
        connections.insert(std::pair<pthread_t, int>(pthread, client_socket));
        __lock.unlock();
    }

    int SizeConnections() {
        std::unique_lock<std::mutex> __lock(connections_mutex);
        int size = connections.size();
        __lock.unlock();

        return size;
    }

    void EraseConnection(pthread_t pthread) {
        std::unique_lock<std::mutex> __lock(connections_mutex);

        auto pos = connections.find(pthread);
        assert(pos != connections.end());
        connections.erase(pos);

        __lock.unlock();
    }

    //// Parsing and execute command
    ///////////////////////////////////////////////////////////////////////////////

    bool ParseArgs(char* str_recv, uint32_t s_args, char *str_args, uint32_t &parsed);

    void ParseAndExecuteCommand(int client_socket);

    ///////////////////////////////////////////////////////////////////////////////

    // Atomic flag to notify threads when it is time to stop. Note that
    // flag must be atomic in order to safely publisj changes cross thread
    // bounds
    std::atomic<bool> running;

    // Thread that is accepting new connections
    pthread_t accept_thread;

    // Maximum number of client allowed to exists concurrently
    // on server, permits access only from inside of accept_thread.
    // Read-only
    uint16_t max_workers;

    // Port to listen for new connections, permits access only from
    // inside of accept_thread
    // Read-only
    uint32_t listen_port;

    // Mutex used to access connections list
    std::mutex connections_mutex;

    // Conditional variable used to notify waiters about empty
    // connections list
    std::condition_variable connections_cv;

    // Threads that are processing connection data, permits
    // access only from inside of accept_thread
    std::unordered_map<pthread_t, int> connections;

    int server_socket;
};

} // namespace Blocking
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_BLOCKING_SERVER_H
