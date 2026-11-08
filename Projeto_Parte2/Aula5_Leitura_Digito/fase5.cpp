// fase5.cpp
#include "raspberry.hpp"
#include <opencv2/opencv.hpp>
#include <vector>
#include <omp.h>
#include <algorithm> // Necessario para std::max_element

using namespace std;
using namespace cv;

// Estrutura para guardar os modelos redimensionados
struct EscalaModelo {
    Mat_<FLT> T_pre;
    Mat_<FLT> T_raw;
    float escala_val;
    int cols, rows;
};

// Função auxiliar para encontrar picos no resultado do Template Matching
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
    // 1. VERIFICAÇÃO DE ARGUMENTOS
    if (argc != 4) erro("Uso: fase5 <video_in> <template_png> <video_out>\n");

    // 2. CONFIGURAÇÃO DA INTELIGÊNCIA ARTIFICIAL (MNIST + FLANN)
    cout << "A carregar base de dados MNIST e a treinar o modelo..." << endl;
    MNIST mnist(14, true, true);
    mnist.le("./"); 
    
    flann::Index flann_index(mnist.ax, flann::KDTreeIndexParams(4));
    cout << "Treino do modelo concluido!" << endl;

    // 3. PREPARAÇÃO DO MODELO DA PLACA (10 ESCALAS)
    Mat_<COR> temp_COR = imread(argv[2], 1);
    if (temp_COR.total() == 0) erro("Erro na leitura da imagem da placa (quadrado.png)");
    
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

    // 4. PREPARAÇÃO DO VÍDEO
    VideoCapture vi(argv[1]);
    if (!vi.isOpened()) erro("Erro ao abrir o video de entrada.");
    
    int nc = vi.get(CAP_PROP_FRAME_WIDTH);
    int nl = vi.get(CAP_PROP_FRAME_HEIGHT);
    VideoWriter vo(argv[3], VideoWriter::fourcc('X','V','I','D'), 20, Size(nc, nl));

    Mat_<COR> frame_COR;
    Mat_<FLT> frame_FLT;
    
    vector<Mat_<FLT>> R_cc(10);
    vector<Mat_<FLT>> R_ncc(10);

    double t_inicio = timeSinceEpoch();
    int frame_count = 0;

    // 5. LOOP PRINCIPAL DE PROCESSAMENTO DO VÍDEO
    while (true) {
        vi >> frame_COR;
        if (frame_COR.empty()) break;
        converte(frame_COR, frame_FLT);
        
        // --- PROCESSAMENTO PARALELO DE BUSCA ---
        #pragma omp parallel for
        for (int s = 0; s < 10; s++) {
            R_cc[s] = matchTemplateSame(frame_FLT, modelos[s].T_pre, TM_CCORR, 0.0);
            R_ncc[s] = matchTemplateSame(frame_FLT, modelos[s].T_raw, TM_CCOEFF_NORMED, 0.0);
        }
        
        // --- ANÁLISE SEQUENCIAL PARA ACHAR A MELHOR PLACA ---
        Point melhor_pt(0,0);
        float melhor_ncc = 0;
        int melhor_escala = -1;
        
        for (int s = 0; s < 10; s++) {
            vector<Point> picos = acharPicos(R_cc[s], 2, 10);
            for(Point p : picos) {
                float val_ncc = R_ncc[s](p.y, p.x);
                if (val_ncc > melhor_ncc) {
                    melhor_ncc = val_ncc;
                    melhor_pt = p;
                    melhor_escala = s;
                }
            }
        }
        
        // --- RECONHECIMENTO DE DÍGITO E DESENHO NA TELA ---
        if (melhor_escala != -1 && melhor_ncc > 0.55) {
            int bw = modelos[melhor_escala].cols / 2;
            int bh = modelos[melhor_escala].rows / 2;
            
            // Desenha a caixa principal em redor da placa (amarelo)
            rectangle(frame_COR, Point(melhor_pt.x - bw, melhor_pt.y - bh), 
                                 Point(melhor_pt.x + bw, melhor_pt.y + bh), Scalar(0, 255, 255), 2);
            
            // Só tentamos ler o dígito se a placa estiver próxima (Escalas 0 ou 1)
            if (melhor_escala <= 1) {
                
                // Recorte otimizado com margem de 25% para evitar a moldura preta da placa
                int margem_x = bw * 0.12; 
                int margem_y = bh * 0.12;
                Rect regiao_digito(melhor_pt.x - bw/2 + margem_x, melhor_pt.y - bh/2 + margem_y, bw - 2*margem_x, bh - 2*margem_y);
                
                regiao_digito &= Rect(0, 0, frame_COR.cols, frame_COR.rows); // Limita às margens do frame
                
                if (regiao_digito.area() > 0) {
                    // A: Recortar e converter para cinzento
                    Mat_<GRY> img_digito;
                    cvtColor(frame_COR(regiao_digito), img_digito, COLOR_BGR2GRAY);
                    
                    // Guardar cópia crua para o painel de depuração inferior
                    Mat img_digito_original = img_digito.clone();
                    
                    // B: Binarização Otsu
                    threshold(img_digito, img_digito, 0, 255, THRESH_BINARY | THRESH_OTSU);
                    
                    // C: Bounding box e redimensionamento MNIST
                    Mat_<GRY> img_bbox = mnist.bbox(img_digito);
                    
                    // D: Conversão e predição (k=5)
                    Mat_<FLT> digito_flt;
                    img_bbox.convertTo(digito_flt, CV_32F, 1.0/255.0, 0.0);
                    digito_flt = digito_flt.reshape(1, 1);
                    
                    int k = 5;
                    Mat indices(1, k, CV_32S);
                    Mat dists(1, k, CV_32F);
                    flann_index.knnSearch(digito_flt, indices, dists, k, flann::SearchParams(32));
                    
                    vector<int> votos(10, 0);
                    for(int i = 0; i < k; i++) {
                        votos[ mnist.AY[indices.at<int>(0, i)] ]++;
                    }
                    int predicao = distance(votos.begin(), max_element(votos.begin(), votos.end()));
                    
                    // E: Textos e Desenhos Visuais na Tela Principal
                    char txt[10];
                    sprintf(txt, "%d", predicao);
                    putText(frame_COR, txt, Point(melhor_pt.x - 30, melhor_pt.y + 15), 
                            FONT_HERSHEY_SIMPLEX, 1.5, Scalar(0, 0, 255), 4);
                            
                    // Retângulo vermelho marcando exatamente a área extraída
                    rectangle(frame_COR, regiao_digito, Scalar(0, 0, 255), 1);
                    
                    // --- PAINÉIS DE DEPURAÇÃO (PICTURE-IN-PICTURE) ---
                    
                    // 1. Painel Superior Direito (A imagem 14x14 processada)
                    Mat vis_bin;
                    cvtColor(img_bbox, vis_bin, COLOR_GRAY2BGR); 
                    resize(vis_bin, vis_bin, Size(100, 100), 0, 0, INTER_NEAREST);
                    
                    Rect roi_top(frame_COR.cols - 100, 0, 100, 100);
                    // Garante que o painel não sai do ecrã
                    if (roi_top.x >= 0 && roi_top.y >= 0) {
                        vis_bin.copyTo(frame_COR(roi_top));
                    }

                    // 2. Painel Inferior Direito (A imagem cinzenta original recortada)
                    Mat vis_crop;
                    cvtColor(img_digito_original, vis_crop, COLOR_GRAY2BGR);
                    resize(vis_crop, vis_crop, Size(100, 100));
                    
                    char txt_pip[10];
                    sprintf(txt_pip, "%d", predicao);
                    putText(vis_crop, txt_pip, Point(10, 80), FONT_HERSHEY_SIMPLEX, 2.0, Scalar(0, 0, 255), 3);
                    
                    Rect roi_bottom(frame_COR.cols - 100, frame_COR.rows - 100, 100, 100);
                    if (roi_bottom.x >= 0 && roi_bottom.y >= 0) {
                        vis_crop.copyTo(frame_COR(roi_bottom));
                    }
                }
            }
        }
        
        vo << frame_COR;
        frame_count++;
    }

    double tempo_total = timeSinceEpoch() - t_inicio;
    cout << "--- PROCESSAMENTO CONCLUIDO ---" << endl;
    cout << "Total Frames: " << frame_count << endl;
    cout << "FPS: " << (frame_count / tempo_total) << endl;

    return 0;
}
