// fase3p.cpp
#include "raspberry.hpp"
#include <opencv2/opencv.hpp>
#include <vector>
#include <omp.h> // Biblioteca do OpenMP

using namespace std;
using namespace cv;

struct EscalaModelo {
    Mat_<FLT> T_pre;
    Mat_<FLT> T_raw;
    float escala_val;
    int cols, rows;
};

// Funcao auxiliar para achar picos
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
    if (argc != 4) erro("Uso: fase3p <video_in> <template_png> <video_out>\n");

    Mat_<COR> temp_COR = imread(argv[2], 1);
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

    VideoCapture vi(argv[1]);
    if (!vi.isOpened()) erro("Erro: Abertura de video_in");
    int nc = vi.get(CAP_PROP_FRAME_WIDTH);
    int nl = vi.get(CAP_PROP_FRAME_HEIGHT);
    VideoWriter vo(argv[3], VideoWriter::fourcc('X','V','I','D'), 20, Size(nc, nl));

    Mat_<COR> frame_COR;
    Mat_<FLT> frame_FLT;
    
    double t_inicio = timeSinceEpoch();
    int frame_count = 0;

    // Pré-alocar vetores para armazenar as 20 imagens resultantes (10 CC e 10 NCC)
    vector<Mat_<FLT>> R_cc(10);
    vector<Mat_<FLT>> R_ncc(10);

    while (true) {
        vi >> frame_COR;
        if (frame_COR.empty()) break;
        converte(frame_COR, frame_FLT);
        
        // PASSO 1 (PARALELO): Processamento pesado dividido por todos os nucleos
        #pragma omp parallel for
        for (int s = 0; s < 10; s++) {
            R_cc[s] = matchTemplateSame(frame_FLT, modelos[s].T_pre, TM_CCORR, 0.0);
            R_ncc[s] = matchTemplateSame(frame_FLT, modelos[s].T_raw, TM_CCOEFF_NORMED, 0.0);
        }
        
        // PASSO 2 (SEQUENCIAL): Analisar rapidamente os resultados obtidos
        Point melhor_pt(0,0);
        float melhor_ncc = 0;
        float assoc_cc = 0;
        int melhor_escala = -1;
        vector<Point> best_candidates;
        
        for (int s = 0; s < 10; s++) {
            vector<Point> picos = acharPicos(R_cc[s], 2, 10);
            for(Point p : picos) {
                best_candidates.push_back(p);
                float val_ncc = R_ncc[s](p.y, p.x);
                if (val_ncc > melhor_ncc) {
                    melhor_ncc = val_ncc;
                    assoc_cc = R_cc[s](p.y, p.x);
                    melhor_pt = p;
                    melhor_escala = s;
                }
            }
        }
        
        for (Point p : best_candidates) {
            rectangle(frame_COR, Point(p.x - 5, p.y - 5), Point(p.x + 5, p.y + 5), Scalar(255, 255, 0), 1);
        }

        if (melhor_escala != -1 && melhor_ncc > 0.55) {
            int bw = modelos[melhor_escala].cols / 2;
            int bh = modelos[melhor_escala].rows / 2;
            rectangle(frame_COR, Point(melhor_pt.x - bw, melhor_pt.y - bh), 
                                 Point(melhor_pt.x + bw, melhor_pt.y + bh), Scalar(0, 255, 255), 2);
            
            char txt[50];
            sprintf(txt, "Sc:%d", melhor_escala);
            putText(frame_COR, txt, Point(melhor_pt.x + bw + 2, melhor_pt.y - 10), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(0, 255, 255), 1);
            sprintf(txt, "CC:%.2f", assoc_cc);
            putText(frame_COR, txt, Point(melhor_pt.x + bw + 2, melhor_pt.y), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(0, 255, 255), 1);
            sprintf(txt, "NC:%.2f", melhor_ncc);
            putText(frame_COR, txt, Point(melhor_pt.x + bw + 2, melhor_pt.y + 10), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(0, 255, 255), 1);
        }
        
        vo << frame_COR;
        frame_count++;
    }

    double tempo_total = timeSinceEpoch() - t_inicio;
    cout << "Processamento Paralelo Finalizado." << endl;
    cout << "Total Frames: " << frame_count << endl;
    cout << "FPS Paralelo: " << (frame_count / tempo_total) << endl;

    return 0;
}
