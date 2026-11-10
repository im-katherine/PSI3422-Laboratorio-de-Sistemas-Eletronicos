#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>

using namespace cv;
using namespace std;

int main() {
    string nomes[] = {
        "capturado.avi",
        "capturado2.avi",
        "capturado3.avi"
    };

    for (const string& nome : nomes) {
        VideoCapture video(nome);

        cout << nome
             << ": aberto=" << (video.isOpened() ? "sim" : "NAO");

        if (video.isOpened()) {
            cout << ", frames=" << video.get(CAP_PROP_FRAME_COUNT)
                 << ", FPS=" << video.get(CAP_PROP_FPS);
        }

        cout << endl;
        video.release();
    }

    Mat imagem = imread("quadrado.png");

    if (imagem.empty()) {
        cout << "quadrado.png: ERRO ao carregar" << endl;
    } else {
        cout << "quadrado.png: carregada, dimensoes="
             << imagem.cols << "x" << imagem.rows << endl;
    }

    return 0;
}
