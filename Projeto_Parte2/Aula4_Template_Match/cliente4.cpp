// cliente4.cpp
#include "projeto.hpp" 
#include <opencv2/opencv.hpp>
#include <vector>
#include <omp.h>

using namespace std;
using namespace cv;

// Definição da estrutura e função que estavam faltando
struct EscalaModelo {
    Mat_<FLT> T_pre;
    Mat_<FLT> T_raw;
    float escala_val;
    int cols, rows;
};

vector<Point> acharPicos(const Mat_<FLT>& mapa, int limite, int dist) {
    vector<Point> picos;
    Mat_<FLT> work_map = mapa.clone();
    
    for(int i = 0; i < limite; i++) {
        double maxVal; Point maxLoc;
        minMaxLoc(work_map, NULL, &maxVal, NULL, &maxLoc);
        if (maxVal <= 0.0) break; 
        
        picos.push_back(maxLoc);
        int startX = max(0, maxLoc.x - dist);
        int startY = max(0, maxLoc.y - dist);
        int endX = min(work_map.cols - 1, maxLoc.x + dist);
        int endY = min(work_map.rows - 1, maxLoc.y + dist);
        for (int y = startY; y <= endY; y++) {
            for (int x = startX; x <= endX; x++) work_map(y, x) = -1.0f; 
        }
    }
    return picos;
}

int main(int argc, char** argv) {
    if (argc < 3) erro("Uso: cliente4 <IP_Raspberry> [videosaida.avi] [t/c]\n");

    CLIENT client(argv[1]);

    Mat_<COR> temp_COR = imread("quadrado.png", 1);
    if (temp_COR.total() == 0) erro("Erro leitura quadrado.png");
    
    Mat_<FLT> T_original;
    converte(temp_COR, T_original);

    vector<EscalaModelo> modelos(10);
    float fator = 0.1721;
    float decrecimo = pow(0.0473 / 0.1721, 1.0/9.0);
    for(int s = 0; s < 10; s++) {
        Mat_<FLT> T_redim;
        resize(T_original, T_redim, Size(), fator, fator, INTER_NEAREST);
        modelos[s].escala_val = fator;
        modelos[s].cols = T_redim.cols;
        modelos[s].rows = T_redim.rows;
        Mat_<FLT> mascara_dc = dcReject(T_redim, 1.0);
        modelos[s].T_pre = somaAbsDois(mascara_dc);
        modelos[s].T_raw = mascara_dc.clone();
        fator *= decrecimo;
    }

    vector<Mat_<FLT>> R_cc(10);
    vector<Mat_<FLT>> R_ncc(10);
    
    bool mostrar_tela = (argc == 4 && argv[3][0] == 't');

    while (true) {
        int tam;
        client.receiveBytes(4, (BYTE*)&tam);
        if (tam <= 0) break; 
        
        vector<BYTE> buffer(tam);
        client.receiveBytes(tam, buffer.data());
        Mat_<COR> frame_COR = imdecode(buffer, 1);
        if (frame_COR.empty()) break;

        Mat_<FLT> frame_FLT;
        converte(frame_COR, frame_FLT);

        #pragma omp parallel for
        for (int s = 0; s < 10; s++) {
            R_cc[s] = matchTemplateSame(frame_FLT, modelos[s].T_pre, TM_CCORR, 0.0);
            R_ncc[s] = matchTemplateSame(frame_FLT, modelos[s].T_raw, TM_CCOEFF_NORMED, 0.0);
        }

        Point melhor_pt(0,0);
        float melhor_ncc = 0;
        int melhor_escala = -1;

        for (int s = 0; s < 10; s++) {
            vector<Point> picos = acharPicos(R_cc[s], 2, 10);
            for(Point p : picos) {
                if (R_ncc[s](p.y, p.x) > melhor_ncc) {
                    melhor_ncc = R_ncc[s](p.y, p.x);
                    melhor_pt = p;
                    melhor_escala = s;
                }
            }
        }

        BYTE v_esq = 0, v_dir = 0;

        if (melhor_escala != -1 && melhor_ncc > 0.55) { 
            if (melhor_escala <= 1) { 
                v_esq = 0; v_dir = 0;
            } else {
                int erro_x = melhor_pt.x - 160; 
                float K = 0.5; 
                int base = 150; 

                int calc_esq = base + (K * erro_x);
                int calc_dir = base - (K * erro_x);

                v_esq = (calc_esq > 255) ? 255 : (calc_esq < 0 ? 0 : calc_esq);
                v_dir = (calc_dir > 255) ? 255 : (calc_dir < 0 ? 0 : calc_dir);
            }
        } else {
            v_esq = 0; v_dir = 0;
        }

        BYTE cmd[2] = {v_esq, v_dir};
        client.sendBytes(2, cmd);

        if (mostrar_tela) {
            if (melhor_ncc > 0.55) {
                int bw = modelos[melhor_escala].cols / 2;
                int bh = modelos[melhor_escala].rows / 2;
                rectangle(frame_COR, Point(melhor_pt.x - bw, melhor_pt.y - bh), 
                                     Point(melhor_pt.x + bw, melhor_pt.y + bh), Scalar(0, 255, 255), 2);
            }
            imshow("Visao do Carrinho", frame_COR);
        }
        
        if (waitKey(1) == 27) break;
    }

    return 0;
}
