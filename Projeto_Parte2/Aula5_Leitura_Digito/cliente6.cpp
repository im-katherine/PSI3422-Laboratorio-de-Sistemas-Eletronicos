/* cliente6.cpp - Reconhecimento de digito + Maquina de Estados
   
   RESUMO DA MAQUINA DE ESTADOS:
   0 = procura/segue placa --> placa grande (escala<=1) --> 1  
   1 = para --> espera inercia por 1s --> 2
   2 = leitura do digito 
       --> não vê placa--> 0
       --> digito 0/1 --> 4 
       --> outros digitos --> 3
   3 = manobra de acordo com o digito --> execucao --> 5
   4 = encerra o programa
   5 = para --> espera inercia por 0.5s -> 0
*/

#include "projeto.hpp"  // Inclui raspberry.hpp + classes CLIENT/SERVER
#include <opencv2/opencv.hpp>
#include <vector>
#include <omp.h>
#include <algorithm>

using namespace std;
using namespace cv;

// Variavel personalizada - Estrutura da placa p/ modelos de Template Matching
struct EscalaModelo {
    Mat_<FLT> T_pre;    // guarda imagem pre-processada do molde (float p/ precisao e negativos, evita overflow)
    Mat_<FLT> T_raw;    // molde "puro"
    float escala_val;   // fator de reducao
    int cols, rows;     // largura e altura desse molde
};

// Encontrar os picos de deteccao no mapa do OpenCV
vector<Point> acharPicos(const Mat_<FLT>& mapa, int limite, int dist) {
    vector<Point> picos; // vetor de coordenadas
    Mat_<FLT> work_map = mapa.clone(); // para poder apagar os picos da copia ao encontrar 
    
    // Loop ate acabar ou atingir o limite de picos
    for(int i = 0; i < limite; i++) {
        double maxVal; Point maxLoc;  // pontuacao e coordenadas do pico 
        minMaxLoc(work_map, NULL, &maxVal, NULL, &maxLoc); // varre a matriz encontrando maximo (nao quero minimo -> null)
        if (maxVal <= 0.0) break; // se nao tem mais picos, interrompe o loop
        picos.push_back(maxLoc);

        // Apaga o pico ja encontrado (cria um quadrado preto em volta)
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
    // Verifica se o comando no terminal esta completo
    if (argc < 3) erro("Uso: cliente6 <IP_Raspberry> <videosaida.avi> [t/c]\n");

    // Instancia o cliente e tenta conectar na raspberry
    CLIENT client(argv[1]);

    // 1. CARREGAR MNIST E TREINAR IA
    cout << "Carregando modelo MNIST..." << endl;
    MNIST mnist(14, true, true);
    mnist.le("./");  // le os arquivos de treinamento
    flann::Index flann_index(mnist.ax, flann::KDTreeIndexParams(4)); // organiza os exemplos em uma arvore de busca rapida

    // 2. PREPARA 10 ESCALAS DA PLACA
    // Converte a matriz colorida da placa em um float
    Mat_<COR> temp_COR = imread("quadrado.png", 1); 
    if (temp_COR.total() == 0) erro("Erro: quadrado.png nao encontrado!"); 
    Mat_<FLT> T_original;
    converte(temp_COR, T_original);
    
    vector<EscalaModelo> modelos(10); //prepara um vetor p/ guardar as 10 escalas
    float fator = 0.1721;  
    float decrecimo = pow(0.0473 / 0.1721, 1.0/9.0); 
    
    // Redimensiona a imagem, remove cinza medio (rejeicao DC) e salva a versao otimizada
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

    // 3. CONFIGURAR GRAVACAO DE VIDEO E MAQUINA DE ESTADOS
    // Abre o arquivo de video (320x240)
    VideoWriter vo(argv[2], VideoWriter::fourcc('X','V','I','D'), 20, Size(320, 240));
    bool mostrar_tela = (argc >= 4 && argv[3][0] == 't'); //se tem t no comando, abre a janela grafica

    vector<Mat_<FLT>> R_cc(10);   // Cross-Correlation - armazena os mapas da busca preliminar
    vector<Mat_<FLT>> R_ncc(10);  // Normalized Cross-Correlation - mapas da busca refinada

    // Prepara as memorias da FSM (estado atual, cronometro, ultimo digito encontrado)
    int estado = 0; 
    double t_estado = timeSinceEpoch();
    int digito_memoria = -1;
    
    // 4. LOOP PRINCIPAL
    while (true) {
        // Recebe tamanho da imagem
        int tam;
        client.receiveBytes(4, (BYTE*)&tam);
        if (tam <= 0) break; // se vier zero, a conexao caiu --> quebra o loop
        
        vector<BYTE> buffer(tam);                   // cria espaco na memoria para a imagem
        client.receiveBytes(tam, buffer.data());    // recebe a imagem do servidor
        Mat_<COR> frame_COR = imdecode(buffer, 1);  // descompacta os bytes recebidos
        if (frame_COR.empty()) break;

        Mat_<FLT> frame_FLT;
        converte(frame_COR, frame_FLT); // pega a foto colorida (0 a 255) da rede e converte para float

        Point melhor_pt(0,0);    // posicao da placa de maior clareza
        float melhor_ncc = 0;    // melhor nivel de confianca (NCC) 
        int melhor_escala = -1;  // guarda o tamanho com maior nota (escala de 0 a 9)

        // Otimizacao: procura a placa ativamente apenas nos estados 0 e 2
        if (estado == 0 || estado == 2) {
            #pragma omp parallel for  // executa o codigo paralelamente nos nucleos do processador
            for (int s = 0; s < 10; s++) {
                R_cc[s] = matchTemplateSame(frame_FLT, modelos[s].T_pre, TM_CCORR, 0.0); // compara com o modelo pre processado e guarda o mapa de calor em Rcc
                R_ncc[s] = matchTemplateSame(frame_FLT, modelos[s].T_raw, TM_CCOEFF_NORMED, 0.0); // busca refinada, compara o frame com o modelu cru e gera um segundo mapa
            }
            
            for (int s = 0; s < 10; s++) { // segundo loop, sem paralelismo, para evitar condicao de corrida
                vector<Point> picos = acharPicos(R_cc[s], 2, 10); // devolve os dois melhores picos do Rcc
                for(Point p : picos) { // analisa os 2 picos
                    if (R_ncc[s](p.y, p.x) > melhor_ncc) {  // verificacao cruzada com o Rncc. se for o melhor, atualiza as variaveis
                        melhor_ncc = R_ncc[s](p.y, p.x);
                        melhor_pt = p;
                        melhor_escala = s;
                    }
                }
            }
        }

        BYTE v_esq = 0, v_dir = 0;
        double decorrido = timeSinceEpoch() - t_estado;

        // LOGICA DA MAQUINA DE ESTADOS FINITA
        switch(estado) {
            case 0: // procurar e seguir
                if (melhor_escala != -1 && melhor_ncc > 0.50) { // limiar de tolerancia = 0.5
                    if (melhor_escala <= 1) { // verifica se a placa tem um tamanho bom
                        estado = 1;
                        t_estado = timeSinceEpoch(); // zera o cronometro
                    } else { // se nao tiver, se aproxima da placa
                        // mini sistema de controle para ajustar rota
                        int erro_x = melhor_pt.x - 160; // ve o erro de rota (quao longe do centro = 160)
                        int base = 130; // velocidade
                        float K = 0.5;  // constante proporcional

                        // ajusta a velocidade de acordo com o erro
                        int calc_esq = base + (K * erro_x);
                        int calc_dir = base - (K * erro_x);

                        // trava de seguranca: mantem na escala 0 a 255
                        v_esq = (calc_esq > 255) ? 255 : (calc_esq < 0 ? 0 : calc_esq);
                        v_dir = (calc_dir > 255) ? 255 : (calc_dir < 0 ? 0 : calc_dir);
                    }
                }
                break;

            case 1: // travar (esperar inercia)
                v_esq = 0; v_dir = 0;
                if (decorrido > 1.0) estado = 2; // 1 segundo parado
                break;

            case 2: // ler digito da placa
                v_esq = 0; v_dir = 0; // para os motores enquanto a foto eh processada

                if (melhor_escala <= 1 && melhor_ncc > 0.50) { // confirma escala e qualidade antes de ler
                    // Encontra o centro aproximado da placa
                    int bw = modelos[melhor_escala].cols / 2;
                    int bh = modelos[melhor_escala].rows / 2;
                    
                    // Margem de seguranca - Capturar o digito sem cortar nem aparecer a borda preta
                    int margem_x = bw * 0.12; 
                    int margem_y = bh * 0.12;
                    Rect regiao_digito(melhor_pt.x - bw/2 + margem_x, melhor_pt.y - bh/2 + margem_y, bw - 2*margem_x, bh - 2*margem_y); // retangulo para recorte
                    regiao_digito &= Rect(0, 0, frame_COR.cols, frame_COR.rows); // se tentar cortar fora da camera, ajusta
                    
                    if (regiao_digito.area() > 0) {
                        // Converte o miolo da placa para tons de cinza (MNIST nao entende cores) 
                        Mat_<GRY> img_digito;
                        cvtColor(frame_COR(regiao_digito), img_digito, COLOR_BGR2GRAY);
                        threshold(img_digito, img_digito, 0, 255, THRESH_BINARY | THRESH_OTSU); // limiar de otsu --> encontra o melhor tom de cinza parasepara a caneta do papel
                        
                        Mat_<GRY> img_bbox = mnist.bbox(img_digito);  // funcao do cekeikon para forcar imagem em 14x14
                        Mat_<FLT> digito_flt;

                        // Converte imagem 14x14 p/ uma unica linha de 196 numeros decimais
                        img_bbox.convertTo(digito_flt, CV_32F, 1.0/255.0, 0.0); 
                        digito_flt = digito_flt.reshape(1, 1); 
                        
                        // KNN (k=5)
                        int k = 5;
                        Mat indices(1, k, CV_32S);
                        Mat dists(1, k, CV_32F);
                        flann_index.knnSearch(digito_flt, indices, dists, k, flann::SearchParams(32));
                        
                        // Olha as 5 imagens mais parecidas e decide qual o digito por "votacao" 
                        vector<int> votos(10, 0);
                        for(int i = 0; i < k; i++) votos[ mnist.AY[indices.at<int>(0, i)] ]++;
                        digito_memoria = distance(votos.begin(), max_element(votos.begin(), votos.end()));
                        
                        // Decidir transicao de estado de acordo com o digito vencedor
                        if (digito_memoria == 0 || digito_memoria == 1) {
                            estado = 4; // fim
                        } else {
                            estado = 3; // executar manobra
                            t_estado = timeSinceEpoch();
                        }
                    }
                } else {
                    estado = 0; // se nao conseguir ler, volta a procurar
                }
                break;

            case 3: // manobras 
                if (digito_memoria == 6 || digito_memoria == 7) { // 90 esq
                    v_esq = 0; v_dir = 200;
                    if (decorrido > 1.0) estado = 5; 
                } 
                else if (digito_memoria == 8 || digito_memoria == 9) { // 90 dir
                    v_esq = 200; v_dir = 0;
                    if (decorrido > 1.0) estado = 5; 
                } 
                else if (digito_memoria == 2) { // 180 esq
                    v_esq = 0; v_dir = 200;
                    if (decorrido > 2.0) estado = 5; 
                } 
                else if (digito_memoria == 3) { // 180 dir
                    v_esq = 200; v_dir = 0;
                    if (decorrido > 2.0) estado = 5; 
                } 
                else if (digito_memoria == 4 || digito_memoria == 5) { // seguir em frente
                    v_esq = 180; v_dir = 180;
                    if (decorrido > 1.5) estado = 5;
                }
                if (estado == 5) t_estado = timeSinceEpoch(); // ao trocar de estado, reseta o cronometro
                break;

            case 4: // encerrar programa
                v_esq = 0; v_dir = 0;
                break;

            case 5: // recuperacao (esperar inercia e recomecar)
                v_esq = 0; v_dir = 0;
                if (decorrido > 0.5) estado = 0; 
                break;
        }

        // ENVIAR COMANDOS PELA REDE
        BYTE cmd[2] = {v_esq, v_dir};
        client.sendBytes(2, cmd);
        
        if (estado == 4) break; // Termina o programa

        // DESENHAR INFORMACAO NA TELA
        if (melhor_escala != -1 && melhor_ncc > 0.50) {
            int bw = modelos[melhor_escala].cols / 2;
            int bh = modelos[melhor_escala].rows / 2;
            rectangle(frame_COR, Point(melhor_pt.x - bw, melhor_pt.y - bh), 
                                 Point(melhor_pt.x + bw, melhor_pt.y + bh), Scalar(0, 255, 255), 2);
        }

        // se estiver com janela grafica, carimba texto de debug no topo
        if (mostrar_tela) {
            char txt_status[100];
            sprintf(txt_status, "Estado: %d | Digito Lido: %d", estado, digito_memoria);
            putText(frame_COR, txt_status, Point(10, 30), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0, 0, 255), 2);
            
            char txt_debug[100];
            sprintf(txt_debug, "Confianca (NCC): %.2f | Escala: %d", melhor_ncc, melhor_escala);
            putText(frame_COR, txt_debug, Point(10, 60), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255, 255, 0), 2);
            
            imshow("Visao do Carrinho", frame_COR);
        }
        
        // GRAVAR FRAME NO VIDEO
        vo << frame_COR;
        
        // ESC --> Sai a forca
        if (waitKey(1) == 27) break;
    }

    return 0;
}
