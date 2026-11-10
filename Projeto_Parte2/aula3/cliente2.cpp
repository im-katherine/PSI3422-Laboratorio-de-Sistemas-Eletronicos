#include "../projeto.hpp"

int comandoMouse = 0;
int mapaTeclas[9] = {7, 8, 9, 4, 5, 6, 1, 2, 3};

void cliqueMouse(int evento, int x, int y, int, void*) {
    if (evento != EVENT_LBUTTONDOWN) return;

    comandoMouse = 0;
    for (int i = 0; i < 9; i++) {
        int bx = 345 + (i % 3) * 90;
        int by = 55 + (i / 3) * 55;

        if (x >= bx && x <= bx + 55 &&
            y >= by && y <= by + 35) {
            comandoMouse = mapaTeclas[i];
            return;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2)
        erro("Uso: cliente2 IP_RASPBERRY [video.avi] [t/c]");

    CLIENT client(argv[1]);

    char comando = '0';
    client.sendBytes(1, (BYTE*)&comando);

    bool gravar = argc >= 3;
    char modo = (argc >= 4) ? argv[3][0] : 't';

    if (modo != 't' && modo != 'c')
        erro("Modo invalido: use t ou c");

    VideoWriter video;

    if (gravar) {
        int codec = VideoWriter::fourcc('M', 'J', 'P', 'G');
        Size tamanho = (modo == 'c') ? Size(320, 240) : Size(640, 240);

        video.open(argv[2], codec, 10.0, tamanho, true);
        if (!video.isOpened())
            erro("Erro ao abrir arquivo de video");
    }

    namedWindow("Cliente");
    setMouseCallback("Cliente", cliqueMouse);

    while (true) {
        Mat_<COR> camera;
        client.receiveImgComp(camera);

        if (camera.empty()) break;

        Mat_<COR> tela(240, 640, COR(0, 0, 0));
        camera.copyTo(tela(Rect(0, 0, 320, 240)));

        rectangle(tela, Point(320, 0), Point(639, 239),
                  Scalar(60, 60, 60), -1);

        putText(tela, "TECLADO VIRTUAL", Point(345, 30),
                FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255, 255, 255), 1);

        for (int i = 0; i < 9; i++) {
            int x = 345 + (i % 3) * 90;
            int y = 55 + (i / 3) * 55;

            rectangle(tela, Point(x, y), Point(x + 55, y + 35),
                      Scalar(200, 200, 200), 1);

            putText(tela, to_string(mapaTeclas[i]), Point(x + 20, y + 24),
                    FONT_HERSHEY_SIMPLEX, 0.7, Scalar(255, 255, 255), 1);
        }

        imshow("Cliente", tela);

        if (gravar)
            video.write(modo == 'c' ? camera : tela);

        int tecla = waitKey(30);

        if (tecla == 27) {
            comando = 's';
            client.sendBytes(1, (BYTE*)&comando);
            break;
        }

        comando = char('0' + comandoMouse);
        client.sendBytes(1, (BYTE*)&comando);
    }

    video.release();
    destroyAllWindows();
    return 0;
}
