#include <stdio.h>
#include <cassert>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <vector>

/*
* This code is a rehash of the 04-server.cpp code, but using Event Loops. 
* A connection-oriented request-response protocol can be used for any number of request-response 
* pairs, and the client can hold the connection as long as it wants. So there is a need to handle 
* multiple connections simultaneously, because while the server is waiting on one client, it cannot 
* do anything with the other clients.
* 
* modern server apps use event loops to handle concurrent IO without creating new threads. What are 
* the drawbacks of thread-based IO?
*    1. Memory usage: Many threads means many stacks. Stacks are used for local variables and 
*                     function calls, memory usage per thread is hard to control.
*    2. Overhead: Stateless clients like PHP apps will create many short-lived connections, adding 
*                 overhead to both latency and CPU usage.
* 
* In an event loop. Each loop iteration waits for any readiness events, then reacts to events without
* blocking, so that all sockets are processed without delay. 
*/

static void die(const char *msg) {
    perror(msg);
    exit(1);
}

static void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
}

/*
* With an event loop, an application task can span multiple loop iterations, 
* so the state must be explicitly stored somewhere. Here is our per-connection 
state:
*/
struct Conn {
    int fd = -1;
    // application's intention, for the event loop
    bool want_read = false;
    bool want_write = false;
    bool want_close = false;

    std::vector<uint8_t> incoming; // data to be parsed by the application
    std::vector<uint8_t> outgoing; // data to be parsed by the application
    
};

static int32_t read_full(int fd, char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = read(fd, buf, n); 
        if (rv <= 0) {
            return -1;
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv; // size_t—the standard unsigned integer type to represent sizes and array indices in bytes
        buf += rv;
    }
    return 0;
}

static int32_t write_all(int fd, const char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = write(fd, buf, n);
        if (rv <= 0) {
            return -1;  // error
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

const size_t k_max_msg = 4096;

static int32_t one_request(int connfd) {
    // 4 bytes header
    char rbuf[4 + k_max_msg];
    errno = 0;
    int32_t err = read_full(connfd, rbuf, 4);
    if (err) {
        msg(errno == 0 ? "EOF" : "read() error"); // (condition ? value_if_true : value_if_false)
        return err;
    }

    uint32_t len = 0;
    memcpy(&len, rbuf, 4); // short for memory copy; function used to rapidly copy a specific number of bytes from a source memory location to a destination memory location
    if (len > k_max_msg) {
        msg("too long");
        return -1;
    }

    // request body
    err = read_full(connfd, &rbuf[4], len);
    if (err) {
        msg("read() error");
        return err;
    }

    // do stuff
    printf("client says: %.*s\n", len, &rbuf[4]);

    // reply using same protocol
    const char reply[] = "world";
    char wbuf[4 + sizeof(reply)];
    len = (uint32_t)strlen(reply);
    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4], reply, len);
    return write_all(connfd, wbuf, len + 4);
}

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);        // port
    addr.sin_addr.s_addr = htonl(0);    // wildcard IP 0.0.0.0
    int rv = bind(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if (rv) { die("bind()"); }

    // listen
    rv = listen(fd, SOMAXCONN);
    if (rv) { die("listen()"); }
    while (true) {
        // accept
        struct sockaddr_in client_addr = {};
        socklen_t addrlen = sizeof(client_addr);
        int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
        if (connfd < 0) {
            continue;   // error
        }
        // only serve one client at once
        while (true) {
            int32_t err = one_request(connfd);
            if (err) {
                break;
            }
        }
        close(connfd);
    }
    return 0;
}