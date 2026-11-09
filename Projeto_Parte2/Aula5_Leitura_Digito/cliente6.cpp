// cliente6.cpp
#include "projeto.hpp"
#include <opencv2/opencv.hpp>
#include <vector>
#include <omp.h>
#include <algorithm>

using namespace std;
using namespace cv;

// Estrutura para os modelos de Template Matching
struct EscalaModelo {
    Mat_<FLT> T_pre;
    Mat_<FLT> T_raw;
    float escala_val;
    int cols, rows;
};

// Função para encontrar os picos de deteção
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
    if (argc < 3) erro("Uso: cliente6 <IP_Raspberry> <videosaida.avi> [t/c]\n");

    // Ligar à Raspberry Pi
    CLIENT client(argv[1]);

    // 1. CARREGAR MNIST E TREINAR IA
    cout << "A carregar modelo MNIST..." << endl;
    MNIST mnist(14, true, true);
    mnist.le("./"); 
    flann::Index flann_index(mnist.ax, flann::KDTreeIndexParams(4));

    // 2. PREPARAR 10 ESCALAS DA PLACA
    Mat_<COR> temp_COR = imread("quadrado.png", 1);
    if (temp_COR.total() == 0) erro("Erro: quadrado.png nao encontrado!");
    
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

    // 3. CONFIGURAR GRAVAÇÃO DE VÍDEO E MÁQUINA DE ESTADOS
    // A Raspberry Pi envia vídeo a 320x240
    VideoWriter vo(argv[2], VideoWriter::fourcc('X','V','I','D'), 20, Size(320, 240));
    bool mostrar_tela = (argc >= 4 && argv[3][0] == 't');

    vector<Mat_<FLT>> R_cc(10);
    vector<Mat_<FLT>> R_ncc(10);

    int estado = 0;
    double t_estado = timeSinceEpoch();
    int digito_memoria = -1;
    
    // 4. LOOP PRINCIPAL
    while (true) {
        // --- RECEBER IMAGEM ---
        int tam;
        client.receiveBytes(4, (BYTE*)&tam);
        if (tam <= 0) break; 
        
        vector<BYTE> buffer(tam);
        client.receiveBytes(tam, buffer.data());
        Mat_<COR> frame_COR = imdecode(buffer, 1);
        if (frame_COR.empty()) break;

        Mat_<FLT> frame_FLT;
        converte(frame_COR, frame_FLT);

        Point melhor_pt(0,0);
        float melhor_ncc = 0;
        int melhor_escala = -1;

        // Otimização: Só procurar a placa ativamente nos Estados 0 e 2
        if (estado == 0 || estado == 2) {
            #pragma omp parallel for
            for (int s = 0; s < 10; s++) {
                R_cc[s] = matchTemplateSame(frame_FLT, modelos[s].T_pre, TM_CCORR, 0.0);
                R_ncc[s] = matchTemplateSame(frame_FLT, modelos[s].T_raw, TM_CCOEFF_NORMED, 0.0);
            }
            
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
        }

        BYTE v_esq = 0, v_dir = 0;
        double decorrido = timeSinceEpoch() - t_estado;

        // --- LÓGICA DA MÁQUINA DE ESTADOS FINITA ---
        switch(estado) {
            case 0: // Procurar e Seguir
                if (melhor_escala != -1 && melhor_ncc > 0.50) { // Limiar mais tolerante (0.50)
                    if (melhor_escala <= 1) { 
                        estado = 1;
                        t_estado = timeSinceEpoch();
                    } else {
                        // Controlo Proporcional (Centralizar a placa)
                        int erro_x = melhor_pt.x - 160; 
                        int base = 130; 
                        float K = 0.5; 
                        int calc_esq = base + (K * erro_x);
                        int calc_dir = base - (K * erro_x);
                        v_esq = (calc_esq > 255) ? 255 : (calc_esq < 0 ? 0 : calc_esq);
                        v_dir = (calc_dir > 255) ? 255 : (calc_dir < 0 ? 0 : calc_dir);
                    }
                }
                break;

            case 1: // Travar (Esperar inércia)
                v_esq = 0; v_dir = 0;
                if (decorrido > 1.0) estado = 2; // 1 segundo imóvel
                break;

            case 2: // Ler Dígito da Placa
                v_esq = 0; v_dir = 0;
                if (melhor_escala <= 1 && melhor_ncc > 0.50) {
                    int bw = modelos[melhor_escala].cols / 2;
                    int bh = modelos[melhor_escala].rows / 2;
                    
                    // Margem de 12% para apanhar bem o dígito sem cortar
                    int margem_x = bw * 0.12; 
                    int margem_y = bh * 0.12;
                    Rect regiao_digito(melhor_pt.x - bw/2 + margem_x, melhor_pt.y - bh/2 + margem_y, bw - 2*margem_x, bh - 2*margem_y);
                    regiao_digito &= Rect(0, 0, frame_COR.cols, frame_COR.rows);
                    
                    if (regiao_digito.area() > 0) {
                        Mat_<GRY> img_digito;
                        cvtColor(frame_COR(regiao_digito), img_digito, COLOR_BGR2GRAY);
                        threshold(img_digito, img_digito, 0, 255, THRESH_BINARY | THRESH_OTSU);
                        
                        Mat_<GRY> img_bbox = mnist.bbox(img_digito);
                        Mat_<FLT> digito_flt;
                        img_bbox.convertTo(digito_flt, CV_32F, 1.0/255.0, 0.0);
                        digito_flt = digito_flt.reshape(1, 1);
                        
                        // K-NN (k=5) para maior precisão
                        int k = 5;
                        Mat indices(1, k, CV_32S);
                        Mat dists(1, k, CV_32F);
                        flann_index.knnSearch(digito_flt, indices, dists, k, flann::SearchParams(32));
                        
                        vector<int> votos(10, 0);
                        for(int i = 0; i < k; i++) votos[ mnist.AY[indices.at<int>(0, i)] ]++;
                        digito_memoria = distance(votos.begin(), max_element(votos.begin(), votos.end()));
                        
                        // Decidir transição de estado consoante o dígito lido
                        if (digito_memoria == 0 || digito_memoria == 1) {
                            estado = 4; // Fim da pista
                        } else {
                            estado = 3; // Executar manobra cega
                            t_estado = timeSinceEpoch();
                        }
                    }
                } else {
                    estado = 0; // Se não conseguir ler, volta a procurar
                }
                break;

            case 3: // Manobras Cegas
                if (digito_memoria == 6 || digito_memoria == 7) { // 90º Esq
                    v_esq = 0; v_dir = 200;
                    if (decorrido > 1.0) estado = 5; 
                } 
                else if (digito_memoria == 8 || digito_memoria == 9) { // 90º Dir
                    v_esq = 200; v_dir = 0;
                    if (decorrido > 1.0) estado = 5; 
                } 
                else if (digito_memoria == 2) { // 180º Esq
                    v_esq = 0; v_dir = 200;
                    if (decorrido > 2.0) estado = 5; 
                } 
                else if (digito_memoria == 3) { // 180º Dir
                    v_esq = 200; v_dir = 0;
                    if (decorrido > 2.0) estado = 5; 
                } 
                else if (digito_memoria == 4 || digito_memoria == 5) { // Seguir em frente
                    v_esq = 180; v_dir = 180;
                    if (decorrido > 1.5) estado = 5;
                }
                if (estado == 5) t_estado = timeSinceEpoch();
                break;

            case 4: // Encerrar
                v_esq = 0; v_dir = 0;
                break;

            case 5: // Recuperar (Esperar inércia final e recomeçar)
                v_esq = 0; v_dir = 0;
                if (decorrido > 0.5) estado = 0; 
                break;
        }

        // --- ENVIAR COMANDOS PELA REDE ---
        BYTE cmd[2] = {v_esq, v_dir};
        client.sendBytes(2, cmd);
        
        if (estado == 4) break; // Termina o programa

        // --- DESENHAR INFORMAÇÃO NA TELA ---
        if (melhor_escala != -1 && melhor_ncc > 0.50) {
            int bw = modelos[melhor_escala].cols / 2;
            int bh = modelos[melhor_escala].rows / 2;
            rectangle(frame_COR, Point(melhor_pt.x - bw, melhor_pt.y - bh), 
                                 Point(melhor_pt.x + bw, melhor_pt.y + bh), Scalar(0, 255, 255), 2);
        }

        if (mostrar_tela) {
            char txt_status[100];
            sprintf(txt_status, "Estado: %d | Digito Lido: %d", estado, digito_memoria);
            putText(frame_COR, txt_status, Point(10, 30), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0, 0, 255), 2);
            
            char txt_debug[100];
            sprintf(txt_debug, "Confianca (NCC): %.2f | Escala: %d", melhor_ncc, melhor_escala);
            putText(frame_COR, txt_debug, Point(10, 60), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255, 255, 0), 2);
            
            imshow("Visao do Carrinho", frame_COR);
        }
        
        // --- GRAVAR FRAME NO VÍDEO ---
        vo << frame_COR;
        
        // Premir ESC para sair à força
        if (waitKey(1) == 27) break;
    }

    return 0;
}
