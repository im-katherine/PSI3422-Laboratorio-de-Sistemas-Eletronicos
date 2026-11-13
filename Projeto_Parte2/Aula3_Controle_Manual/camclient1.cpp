#include "../projeto.hpp"
#include <chrono>

int main(int argc, char *argv[]) {
    if (argc != 2) erro("camclient1 servidorIpAddr");

    CLIENT client(argv[1]);

    Mat_<COR> img;
    char comando = 'c';
    using Relogio = std::chrono::steady_clock;
    auto inicio = Relogio::now();
    int quadros = 0;
    double fps = 0.0;

    namedWindow("Camera Raspberry", WINDOW_NORMAL);

    while (comando == 'c') {
        client.receiveImg(img);

        if (img.empty()) erro("Imagem vazia recebida");

        quadros++;
        auto agora = Relogio::now();
        double segundos = std::chrono::duration<double>(agora - inicio).count();

        if (segundos >= 1.0) {
            fps = quadros / segundos;
            quadros = 0;
            inicio = agora;
            cout << "FPS: " << fps << endl;
        }

        putText(img, "FPS: " + to_string((int)fps),
                Point(10, 30), FONT_HERSHEY_SIMPLEX,
                0.8, Scalar(0, 255, 0), 2);

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
