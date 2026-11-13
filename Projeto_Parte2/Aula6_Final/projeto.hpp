// projeto.hpp
#include <opencv2/opencv.hpp>
using namespace cv;
typedef Vec3b COR;
#include "raspberry.hpp"
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdint.h>
#include <vector>

bool testaVb(const vector<BYTE> vb, BYTE b) {
    bool igual = true;
    for (unsigned i = 0; i < vb.size(); i++)
        if (vb[i] != b) {
            igual = false;
            break;
        }
    return igual;
}

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
    void sendUint(uint32_t m) {
        uint32_t mnet = htonl(m);
        sendBytes(sizeof(mnet), (BYTE*)&mnet);
    }

    void receiveUint(uint32_t& m) {
        uint32_t mnet;
        receiveBytes(sizeof(mnet), (BYTE*)&mnet);
        m = ntohl(mnet);
    }

    void sendVb(const vector<BYTE>& vb) {
        sendUint((uint32_t)vb.size());

        uint32_t confirmacao;
        receiveUint(confirmacao);
        if (confirmacao != vb.size()) erro("Confirmacao de tamanho incorreta");

        if (!vb.empty())
            sendBytes((int)vb.size(), (BYTE*)vb.data());
    }

    void receiveVb(vector<BYTE>& st) {
        uint32_t tamanho;
        receiveUint(tamanho);

        st.resize(tamanho);
        sendUint(tamanho);

        if (tamanho > 0)
            receiveBytes((int)tamanho, st.data());
    }

    void sendImg(const Mat_<COR>& img) {
        if (!img.isContinuous()) erro("Imagem nao continua");
        sendUint((uint32_t)img.rows);
        sendUint((uint32_t)img.cols);
        sendBytes((int)(3 * img.total()), (BYTE*)img.data);
    }

    void receiveImg(Mat_<COR>& img) {
        uint32_t nl, nc;
        receiveUint(nl);
        receiveUint(nc);
        if (nl == 0 || nc == 0) erro("Dimensoes de imagem invalidas");
        img.create((int)nl, (int)nc);
        receiveBytes((int)(3 * img.total()), (BYTE*)img.data);
    }

    void sendImgComp(const Mat_<COR>& img) {
        vector<BYTE> vb;
        vector<int> param{CV_IMWRITE_JPEG_QUALITY, 80};
        if (!imencode(".jpg", img, vb, param))
            erro("Erro ao compactar imagem");
        sendVb(vb);
    }

    void receiveImgComp(Mat_<COR>& img) {
        vector<BYTE> vb;
        receiveVb(vb);
        Mat decodificada = imdecode(vb, 1);
        if (decodificada.empty()) erro("Erro ao descompactar imagem");
        decodificada.copyTo(img);
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
    void sendUint(uint32_t m) {
        uint32_t mnet = htonl(m);
        sendBytes(sizeof(mnet), (BYTE*)&mnet);
    }

    void receiveUint(uint32_t& m) {
        uint32_t mnet;
        receiveBytes(sizeof(mnet), (BYTE*)&mnet);
        m = ntohl(mnet);
    }

    void sendVb(const vector<BYTE>& vb) {
        sendUint((uint32_t)vb.size());

        uint32_t confirmacao;
        receiveUint(confirmacao);
        if (confirmacao != vb.size()) erro("Confirmacao de tamanho incorreta");

        if (!vb.empty())
            sendBytes((int)vb.size(), (BYTE*)vb.data());
    }

    void receiveVb(vector<BYTE>& st) {
        uint32_t tamanho;
        receiveUint(tamanho);

        st.resize(tamanho);
        sendUint(tamanho);

        if (tamanho > 0)
            receiveBytes((int)tamanho, st.data());
    }

    void sendImg(const Mat_<COR>& img) {
        if (!img.isContinuous()) erro("Imagem nao continua");
        sendUint((uint32_t)img.rows);
        sendUint((uint32_t)img.cols);
        sendBytes((int)(3 * img.total()), (BYTE*)img.data);
    }

    void receiveImg(Mat_<COR>& img) {
        uint32_t nl, nc;
        receiveUint(nl);
        receiveUint(nc);
        if (nl == 0 || nc == 0) erro("Dimensoes de imagem invalidas");
        img.create((int)nl, (int)nc);
        receiveBytes((int)(3 * img.total()), (BYTE*)img.data);
    }

    void sendImgComp(const Mat_<COR>& img) {
        vector<BYTE> vb;
        vector<int> param{CV_IMWRITE_JPEG_QUALITY, 80};
        if (!imencode(".jpg", img, vb, param))
            erro("Erro ao compactar imagem");
        sendVb(vb);
    }

    void receiveImgComp(Mat_<COR>& img) {
        vector<BYTE> vb;
        receiveVb(vb);
        Mat decodificada = imdecode(vb, 1);
        if (decodificada.empty()) erro("Erro ao descompactar imagem");
        decodificada.copyTo(img);
    }



};
