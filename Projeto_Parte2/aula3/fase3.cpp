#include <opencv2/opencv.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace cv;
using namespace std;

struct Candidato {
    int x, y, escala, w, h;
    float cc, ncc;
};

static bool melhor(const Candidato& a, const Candidato& b) {
    return a.cc > b.cc;
}

// Correlação normalizada considerando apenas os pixels válidos.
static float nccMascarado(const Mat& imagem, const Mat& modelo,
                          const Mat& mascara, int x, int y) {
    double somaI = 0, somaT = 0;
    double somaII = 0, somaTT = 0, somaIT = 0;
    int n = 0;

    for (int j = 0; j < modelo.rows; ++j) {
        const float* t = modelo.ptr<float>(j);
        const uchar* m = mascara.ptr<uchar>(j);
        const float* p = imagem.ptr<float>(y + j) + x;

        for (int i = 0; i < modelo.cols; ++i) {
            if (m[i] == 0) continue;
            double a = p[i], b = t[i];
            somaI += a;
            somaT += b;
            somaII += a * a;
            somaTT += b * b;
            somaIT += a * b;
            ++n;
        }
    }

    if (n < 2) return -1.0f;

    double cov = somaIT - somaI * somaT / n;
    double vi = somaII - somaI * somaI / n;
    double vt = somaTT - somaT * somaT / n;

    if (vi <= 1e-12 || vt <= 1e-12) return -1.0f;
    return (float)(cov / sqrt(vi * vt));
}

int main(int argc, char** argv) {
    if (argc != 4) {
        cerr << "Uso: " << argv[0]
             << " video.avi quadrado.png saida.avi\n";
        return 1;
    }

    VideoCapture video(argv[1]);
    if (!video.isOpened()) {
        cerr << "Erro: nao foi possivel abrir o video "
             << argv[1] << '\n';
        return 1;
    }

    Mat original = imread(argv[2], IMREAD_COLOR);
    if (original.empty()) {
        cerr << "Erro: nao foi possivel abrir o modelo "
             << argv[2] << '\n';
        return 1;
    }

    // O pixel BGR exatamente branco (255,255,255) e "don't care".
    Mat mascaraOriginal(original.rows, original.cols, CV_8U);
    for (int y = 0; y < original.rows; ++y) {
        for (int x = 0; x < original.cols; ++x) {
            Vec3b p = original.at<Vec3b>(y, x);
            mascaraOriginal.at<uchar>(y, x) =
                (p[0] == 255 && p[1] == 255 && p[2] == 255) ? 0 : 255;
        }
    }

    Mat modeloOriginal;
    cvtColor(original, modeloOriginal, COLOR_BGR2GRAY);
    modeloOriginal.convertTo(modeloOriginal, CV_32F, 1.0 / 255.0);

    // Dez escalas geometricas entre 69x69 e 19x19 pixels.
    vector<Mat> modelos, mascaras;
    for (int s = 0; s < 10; ++s) {
        double fator = pow(19.0 / 69.0, s / 9.0);
        int lado = cvRound(69.0 * fator);

        Mat t, m;
        resize(modeloOriginal, t, Size(lado, lado), 0, 0, INTER_NEAREST);
        resize(mascaraOriginal, m, Size(lado, lado), 0, 0, INTER_NEAREST);

        modelos.push_back(t);
        mascaras.push_back(m);
    }

    double fpsEntrada = video.get(CAP_PROP_FPS);
    if (fpsEntrada <= 0) fpsEntrada = 25.0;

    VideoWriter saida(argv[3],
                      VideoWriter::fourcc('X', 'V', 'I', 'D'),
                      fpsEntrada, Size(320, 240));

    if (!saida.isOpened()) {
        cerr << "Erro: nao foi possivel criar o video de saida\n";
        return 1;
    }

    int quadros = 0;
    auto inicio = chrono::steady_clock::now();
    Mat frame;

    while (video.read(frame)) {
        if (frame.empty()) break;

        if (frame.cols != 320 || frame.rows != 240) {
            resize(frame, frame, Size(320, 240));
        }

        Mat cinza, imagem;
        cvtColor(frame, cinza, COLOR_BGR2GRAY);
        cinza.convertTo(imagem, CV_32F, 1.0 / 255.0);

        vector<Candidato> candidatos;

        // CC: busca candidatos em todas as escalas.
        for (int s = 0; s < (int)modelos.size(); ++s) {
            const Mat& t = modelos[s];
            const Mat& m = mascaras[s];

            if (t.cols > imagem.cols || t.rows > imagem.rows) continue;

            Mat correlacao;
            matchTemplate(imagem, t, correlacao,
                          TM_CCORR_NORMED, m);

            // Extrai picos locais; depois aplica separacao espacial.
            Mat dilatada;
            dilate(correlacao, dilatada,
                   getStructuringElement(MORPH_RECT, Size(3, 3)));

            for (int y = 0; y < correlacao.rows; ++y) {
                const float* r = correlacao.ptr<float>(y);
                const float* d = dilatada.ptr<float>(y);

                for (int x = 0; x < correlacao.cols; ++x) {
                    if (r[x] < 0.10f || r[x] < d[x] - 1e-6f)
                        continue;

                    candidatos.push_back({
                        x, y, s, t.cols, t.rows, r[x], -1.0f
                    });
                }
            }
        }

        sort(candidatos.begin(), candidatos.end(), melhor);

        // Mantem os 20 maiores picos separados por pelo menos 10 px.
        vector<Candidato> selecionados;
        for (const auto& c : candidatos) {
            bool perto = false;
            for (const auto& q : selecionados) {
                int dx = c.x - q.x;
                int dy = c.y - q.y;
                if (dx * dx + dy * dy < 100) {
                    perto = true;
                    break;
                }
            }
            if (!perto) selecionados.push_back(c);
            if (selecionados.size() == 20) break;
        }

        // Valida os candidatos com NCC mascarado.
        int indiceMelhor = -1;
        float maiorNcc = -1.0f;

        for (int i = 0; i < (int)selecionados.size(); ++i) {
            auto& c = selecionados[i];
            c.ncc = nccMascarado(imagem, modelos[c.escala],
                                  mascaras[c.escala], c.x, c.y);

            if (c.ncc > maiorNcc) {
                maiorNcc = c.ncc;
                indiceMelhor = i;
            }
        }


        // Desenha os 20 candidatos em azul.
        for (const auto& c : selecionados) {
            rectangle(frame, Rect(c.x, c.y, c.w, c.h),
                      Scalar(255, 255, 0), 1);
        }

        // Destaca a deteccao aceita em amarelo.
        if (indiceMelhor >= 0 && maiorNcc > 0.55f) {
            const auto& c = selecionados[indiceMelhor];
            rectangle(frame, Rect(c.x, c.y, c.w, c.h),
                      Scalar(0, 255, 255), 2);

            putText(frame,
                    format("escala %d CC %.2f NCC %.2f",
                           c.escala, c.cc, c.ncc),
                    Point(5, 18), FONT_HERSHEY_SIMPLEX,
                    0.45, Scalar(0, 255, 255), 1);
        }

        saida.write(frame);
        ++quadros;
    }

    auto fim = chrono::steady_clock::now();
    double segundos =
        chrono::duration<double>(fim - inicio).count();

    cout << "Quadros processados: " << quadros << '\n';
    cout << "Tempo de processamento: " << segundos << " s\n";
    cout << "FPS de processamento: "
         << (segundos > 0 ? quadros / segundos : 0) << '\n';
    cout << "Video gerado: " << argv[3] << '\n';

    return 0;
}
