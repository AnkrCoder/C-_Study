#ifndef MASTER_WORKER_H
#define MASTER_WORKER_H

#include "../HTTP_Connection/HTTP_Connection.h"
#include "../Logger/Logger.h"
#include <vector>
#include <atomic>
#include <functional>
#include <sys/types.h>
#include <sys/socket.h>

class ProcessMaster {
public:
    explicit ProcessMaster(int workers);
    void run(int listen_fd);

private:
    void create_workers(int listen_fd);
    void monitor_workers();
    static void signal_handler(int sig);

    const int worker_count;
    std::vector<pid_t> workers;
    static bool master_keep_running;
};

#endif // MASTER_WORKER_H