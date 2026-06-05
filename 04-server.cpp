#include <stdio.h>
#include <cassert>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>

static void die(const char *msg) {
    perror(msg);
    exit(1);
}

static void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
}

static void do_something(int connfd) {
    char rbuf[64] = {};//  creates a fixed-size memory array used to temporarily hold incoming data
    ssize_t n = read(connfd, rbuf, sizeof(rbuf)-1);
    if (n<0) {
        msg("read() error");
        return;
    }
    printf("client says: %s\n", rbuf);
    char wbuf[] = "world";
    write(connfd, wbuf, strlen(wbuf));
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
            /*
            * one_request function will read 1 request and write 1 reponse. 
            * the problem is, how many bytes to raed? that is the function of
            * an *application protocol*. A ttypical protocol has 2 levels
            * of structures: 
            *   1. high level byte structure to split the byte stream into messages
            *   2. the structure within a message (aka deserialization)
            */
            if (err) {
                break;
            }
        }
        close(connfd);
    }
}

/*Essentially, when communicating messages with many bytes, 
* you need to keep calling. this is because read and write 
* are not guaranteed to process all 4 bytes at once. The 
* problem is The problem is how the read() function interacts 
* with a network connection (specifically TCP). TCP is a 
* stream protocol. It doesn't know what a "message" is; it 
* just sees a continuous pipe of bytes. When data travels 
* across the internet, it gets chopped up into packets.
* 
* Imagine you call read(fd, &n, 4);. You are asking the operating system for 4 bytes.
* But what if, at that exact millisecond, only 2 bytes have arrived through the network 
* cable, and the other 2 bytes are still halfway across the country? The read() function 
* won't wait for the rest. It will happily put those 2 bytes into n, return the number 2 
* (to tell you it only read 2 bytes), and finish. */
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
int errno;

static int32_t one_request(int connfd) {
    // 4 bytes header
    char rbuf[4 + k_max_msg];
    errno = 0;
    int32_t err = read_full(connfd, rbuf, 4);
    if (err) {
        msg(errno == 0 ? "EOF" : "read() error") // (condition ? value_if_true : value_if_false)
    }
    uint32_t len = 0;
    memcpy(&len, rbuf, 4); // short for memory copy; function used to rapidly copy a specific number of bytes from a source memory location to a destination memory location
    if (len > k_max_msg) {
        msg("too long");
        return -1;
    }
    // NOW we read the body... wtfff
    err = read_full(connfd, &rbuf[4], len);
    if (err) {
        msg("read() error");
        return err;
    }
    // do stuff
    printf("client says: %s\n", len, &rbuf[4]);
    // reply using same protocol
    const char reply[] = "world";
    char wbuf[4+sizeof(reply)];
    len = (uint32_t)strlen(reply);
    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4], reply, len);
    return write_all(connfd, wbuf, len + 4);
}