#include "../projeto.hpp"

int main(int argc, char *argv[]) {
    if (argc != 2) erro("camclient1 servidorIpAddr");

    CLIENT client(argv[1]);

    Mat_<COR> img;
    char comando = 'c';

    namedWindow("Camera Raspberry", WINDOW_NORMAL);

    while (comando == 'c') {
        client.receiveImgComp(img);

        if (img.empty()) erro("Imagem vazia recebida");

        imshow("Camera Raspberry", img);
        int tecla = waitKey(1);

        if (tecla == 27) {
            comando = 'q';
        } else {
            comando = 'c';
        }

        client.sendBytes(1, (BYTE*)&comando);
    }

    destroyAllWindows();
    return 0;
}
