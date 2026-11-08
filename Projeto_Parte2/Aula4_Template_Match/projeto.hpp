// projeto.hpp
#include "raspberry.hpp"
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>

bool testaBytes(BYTE* buf, BYTE b, int n) {
    // Testa se n bytes da memoria buf possuem valor b
    bool igual = true;
    for (unsigned i=0; i<n; i++)
        if (buf[i]!=b) { igual = false; break; }
    return igual;
}

class SERVER {
    const string PORT="3490"; 
    const int BACKLOG=1; 
    int sockfd, new_fd; 
    struct addrinfo hints, *servinfo, *p;

    static void *get_in_addr(struct sockaddr *sa) {
        if (sa->sa_family == AF_INET) {
            return &(((struct sockaddr_in*)sa)->sin_addr);
        }
        return &(((struct sockaddr_in6*)sa)->sin6_addr);
    }

public:
    SERVER() {
        int rv, yes=1;
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        if ((rv = getaddrinfo(NULL, PORT.c_str(), &hints, &servinfo)) != 0) {
            erro("getaddrinfo: falha");
        }

        for(p = servinfo; p != NULL; p = p->ai_next) {
            if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) continue;
            if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) erro("setsockopt");
            if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
                close(sockfd);
                continue;
            }
            break;
        }
        freeaddrinfo(servinfo);
        if (p == NULL) erro("server: failed to bind");
        if (listen(sockfd, BACKLOG) == -1) erro("listen");
    }

    ~SERVER() {
        close(new_fd);
        close(sockfd);
        printf("server: fechei conexao.\n");
    }

    void waitConnection() {
        struct sockaddr_storage their_addr;
        socklen_t sin_size = sizeof their_addr;
        char s[INET6_ADDRSTRLEN];

        printf("server: Esperando conexao...\n");
        while(1) {
            new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
            if (new_fd == -1) {
                perror("accept");
                continue;
            } else {
                break;
            }
        }
        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        printf("server: recebi conexao de %s\n", s);
    }

    void sendBytes(int nBytesToSend, BYTE *buf) {
        int sent = 0;
        while (sent < nBytesToSend) {
            int bytes = send(new_fd, buf + sent, nBytesToSend - sent, 0);
            if (bytes == -1) erro("send");
            sent += bytes;
        }
    }

    void receiveBytes(int nBytesToReceive, BYTE *buf) {
        int received = 0;
        while (received < nBytesToReceive) {
            int bytes = recv(new_fd, buf + received, nBytesToReceive - received, 0);
            if (bytes == -1) erro("recv");
            if (bytes == 0) erro("Conexão fechada prematuramente.");
            received += bytes;
        }
    }
};

class CLIENT {
    const string PORT="3490";
    int sockfd;
    struct addrinfo hints, *servinfo, *p;

    static void *get_in_addr(struct sockaddr *sa) {
        if (sa->sa_family == AF_INET) {
            return &(((struct sockaddr_in*)sa)->sin_addr);
        }
        return &(((struct sockaddr_in6*)sa)->sin6_addr);
    }

public:
    CLIENT(string endereco) {
        int rv;
        char s[INET6_ADDRSTRLEN];

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        if ((rv = getaddrinfo(endereco.c_str(), PORT.c_str(), &hints, &servinfo)) != 0) {
            erro("getaddrinfo: falha");
        }

        for(p = servinfo; p != NULL; p = p->ai_next) {
            if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) continue;
            if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
                close(sockfd);
                continue;
            }
            break;
        }
        if (p == NULL) erro("client: failed to connect");

        inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
        printf("client: conectando a %s\n", s);
        freeaddrinfo(servinfo);
    }

    ~CLIENT() {
        close(sockfd);
    }

    void sendBytes(int nBytesToSend, BYTE *buf) {
        int sent = 0;
        while (sent < nBytesToSend) {
            int bytes = send(sockfd, buf + sent, nBytesToSend - sent, 0);
            if (bytes == -1) erro("send");
            sent += bytes;
        }
    }

    void receiveBytes(int nBytesToReceive, BYTE *buf) {
        int received = 0;
        while (received < nBytesToReceive) {
            int bytes = recv(sockfd, buf + received, nBytesToReceive - received, 0);
            if (bytes == -1) erro("recv");
            if (bytes == 0) erro("Conexão fechada prematuramente.");
            received += bytes;
        }
    }
};
