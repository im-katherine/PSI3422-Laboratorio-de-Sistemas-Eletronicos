/* cliente7_final.cpp - Fase 7: Carrinho Autônomo + Controle Manual (FSM)
   
   RESUMO DA MÁQUINA DE ESTADOS:
   0 = procura/segue placa --> placa grande (escala<=1) --> 1  
   1 = para --> espera inércia por 1s --> 2
   2 = leitura do dígito 
       --> não vê placa --> 0
       --> dígito 0/1 --> 4 
       --> outros dígitos --> 3 
   3 = manobra de acordo com o dígito --> execução --> 5
   4 = MODO MANUAL (controlado pelo mouse)
   5 = para --> espera inércia por 0.5s -> 0
*/

#include "projeto.hpp"  // Inclui raspberry.hpp + classes CLIENT/SERVER
#include <opencv2/opencv.hpp>
#include <vector>
#include <omp.h>
#include <algorithm>

using namespace std;
using namespace cv;

// --- VARIÁVEIS GLOBAIS (Interface Manual) ---
int comandoMouse = 5; // 5 = Parado
int mapaTeclas[9] = {7, 8, 9, 4, 5, 6, 1, 2, 3};
bool modo_automatico = false; // Começa no modo manual

// Variável personalizada - Estrutura da placa p/ modelos de Template Matching
struct EscalaModelo {
    Mat_<FLT> T_pre;    // guarda imagem pré-processada do molde (float p/ precisão e negativos, evita overflow)
    Mat_<FLT> T_raw;    // molde "puro"
    float escala_val;   // fator de redução
    int cols, rows;     // largura e altura desse molde
};

/* =================================================================
   ===================   FUNCOES - MODO MANUAL   ===================
   ================================================================= */ 

void cliqueMouse(int evento, int x, int y, int, void*) {
    if (evento == EVENT_LBUTTONUP && !modo_automatico) comandoMouse = 5; // soltou o botao = para
    if (evento != EVENT_LBUTTONDOWN) return;

    for (int i = 0; i < 9; i++) {
        int bx = 345 + (i % 3) * 90;
        int by = 55 + (i / 3) * 55;

        if (x >= bx && x <= bx + 55 && y >= by && y <= by + 35) {
            // Se clicou no botão central ("5" / "M/A"), alterna o modo
            if (i == 4) { 
                modo_automatico = !modo_automatico;
                comandoMouse = 5; // Para o carrinho ao trocar de modo
            } else if (!modo_automatico) {
                // Só aceita comandos de direção se estiver no modo manual
                comandoMouse = mapaTeclas[i];
            }
            return;
        }
    }
}

void desenharTeclado(Mat_<COR>& tela) {
    // Fundo cinza escuro para a área do teclado
    rectangle(tela, Point(320, 0), Point(639, 239), Scalar(60, 60, 60), -1);
    
    // Status do Modo no topo
    string txt_modo = modo_automatico ? "MODO AUTOMATICO (IA)" : "MODO MANUAL";
    Scalar cor_txt = modo_automatico ? Scalar(0, 255, 0) : Scalar(255, 255, 255);
    putText(tela, txt_modo, Point(345, 30), FONT_HERSHEY_SIMPLEX, 0.6, cor_txt, 2);

    // Desenhar os botões
    for (int i = 0; i < 9; i++) {
        int x = 345 + (i % 3) * 90;
        int y = 55 + (i / 3) * 55;
        
        rectangle(tela, Point(x, y), Point(x + 55, y + 35), Scalar(200, 200, 200), 1);
        
        string texto_botao = (i == 4) ? "M/A" : to_string(mapaTeclas[i]);
        Scalar cor_botao = (i == 4) ? Scalar(0, 0, 255) : Scalar(255, 255, 255);
        
        // Ajuste de posição para "M/A" ficar centralizado
        Point pos_txt = (i == 4) ? Point(x + 10, y + 24) : Point(x + 20, y + 24);
        putText(tela, texto_botao, pos_txt, FONT_HERSHEY_SIMPLEX, 0.7, cor_botao, 1);
    }
}

void traduzirComando(int comando, BYTE& v_esq, BYTE& v_dir, BYTE& r_esq, BYTE& r_dir) {
    // Traduz as teclas do controle manual (0 a 9) para velocidades PWM de 0 a 255
    // v_* = velocidade para a frente, r_* = velocidade para trás (ré)
    v_esq = 0; v_dir = 0; r_esq = 0; r_dir = 0;
    switch (comando) {
        case 8: v_esq = 200; v_dir = 200; break; // Frente
        case 4: v_esq = 0; v_dir = 200; break;   // Esquerda
        case 6: v_esq = 200; v_dir = 0; break;   // Direita
        case 7: v_esq = 100; v_dir = 200; break; // Frente-Esquerda
        case 9: v_esq = 200; v_dir = 100; break; // Frente-Direita
        case 2: r_esq = 200; r_dir = 200; break; // Ré
        case 1: r_esq = 100; r_dir = 200; break; // Ré-Esquerda
        case 3: r_esq = 200; r_dir = 100; break; // Ré-Direita
        default: break;                          // Parado
    }
}


/* =================================================================
   =================== FUNCOES - MODO AUTOMATICO ===================
   ================================================================= */ 

// 2. PREPARA 10 ESCALAS DA PLACA
vector<EscalaModelo> prepararModelos(const string& nome_arquivo) {
    // Converte a matriz colorida da placa em um float
    Mat_<COR> temp_COR = imread(nome_arquivo, 1); 
    if (temp_COR.total() == 0) erro("Erro: molde nao encontrado!"); 
    Mat_<FLT> T_original;
    converte(temp_COR, T_original);
    
    vector<EscalaModelo> modelos(10); // prepara um vetor p/ guardar as 10 escalas
    float fator = 0.1721;  
    float decrecimo = pow(0.0473 / 0.1721, 1.0/9.0); 
    
    // Redimensiona a imagem, remove cinza médio (rejeição DC) e salva a versão otimizada
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
    return modelos;
}

// Encontrar os picos de detecção no mapa do OpenCV
vector<Point> acharPicos(const Mat_<FLT>& mapa, int limite, int dist) {
    vector<Point> picos; // vetor de coordenadas
    Mat_<FLT> work_map = mapa.clone(); // para poder apagar os picos da cópia ao encontrar 
    
    // Loop até acabar ou atingir o limite de picos
    for(int i = 0; i < limite; i++) {
        double maxVal; Point maxLoc;  // pontuação e coordenadas do pico 
        minMaxLoc(work_map, NULL, &maxVal, NULL, &maxLoc); // varre a matriz encontrando máximo (não quero mínimo -> null)
        if (maxVal <= 0.0) break; // se não tem mais picos, interrompe o loop
        picos.push_back(maxLoc);

        // Apaga o pico já encontrado (cria um quadrado preto em volta)
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

void procurarPlaca(const Mat_<FLT>& frame_FLT, const vector<EscalaModelo>& modelos, float& melhor_ncc, int& melhor_escala, Point& melhor_pt) {
    vector<Mat_<FLT>> R_cc(10);   // Cross-Correlation - armazena os mapas da busca preliminar
    vector<Mat_<FLT>> R_ncc(10);  // Normalized Cross-Correlation - mapas da busca refinada
    
    melhor_ncc = 0; melhor_escala = -1; melhor_pt = Point(0,0);
    
    #pragma omp parallel for // executa o código paralelamente nos núcleos do processador
    for (int s = 0; s < 10; s++) {
        R_cc[s] = matchTemplateSame(frame_FLT, modelos[s].T_pre, TM_CCORR, 0.0); // compara com o modelo pré processado e guarda o mapa de calor em Rcc
        R_ncc[s] = matchTemplateSame(frame_FLT, modelos[s].T_raw, TM_CCOEFF_NORMED, 0.0); // busca refinada, compara o frame com o modelo cru e gera um segundo mapa
    }
    
    // segundo loop, sem paralelismo, para evitar condição de corrida
    for (int s = 0; s < 10; s++) { 
        vector<Point> picos = acharPicos(R_cc[s], 2, 10); // devolve os dois melhores picos do Rcc
        for(Point p : picos) { // analisa os 2 picos
            if (R_ncc[s](p.y, p.x) > melhor_ncc) {  // verificação cruzada com o Rncc. se for o melhor, atualiza as variáveis
                melhor_ncc = R_ncc[s](p.y, p.x);
                melhor_pt = p;
                melhor_escala = s;
            }
        }
    }
}

int lerDigito(const Mat_<COR>& frame_COR, const EscalaModelo& modelo, Point pt, MNIST& mnist, flann::Index& flann_index) {
    // Encontra o centro aproximado da placa
    int bw = modelo.cols / 2;
    int bh = modelo.rows / 2;
    
    // Margem de segurança - Capturar o dígito sem cortar nem aparecer a borda preta
    int margem_x = bw * 0.12; 
    int margem_y = bh * 0.12;
    Rect regiao_digito(pt.x - bw/2 + margem_x, pt.y - bh/2 + margem_y, bw - 2*margem_x, bh - 2*margem_y); // retângulo para recorte
    regiao_digito &= Rect(0, 0, frame_COR.cols, frame_COR.rows); // se tentar cortar fora da câmera, ajusta
    
    if (regiao_digito.area() > 0) {
        // Converte o miolo da placa para tons de cinza (MNIST não entende cores) 
        Mat_<GRY> img_digito;
        cvtColor(frame_COR(regiao_digito), img_digito, COLOR_BGR2GRAY);
        threshold(img_digito, img_digito, 0, 255, THRESH_BINARY | THRESH_OTSU); // limiar de otsu --> encontra o melhor tom de cinza para separar a caneta do papel
        
        Mat_<GRY> img_bbox = mnist.bbox(img_digito);  // função do cekeikon para forçar imagem em 14x14
        Mat_<FLT> digito_flt;

        // Converte imagem 14x14 p/ uma única linha de 196 números decimais
        img_bbox.convertTo(digito_flt, CV_32F, 1.0/255.0, 0.0); 
        digito_flt = digito_flt.reshape(1, 1); 
        
        // KNN (k=5)
        int k = 5;
        Mat indices(1, k, CV_32S);
        Mat dists(1, k, CV_32F);
        flann_index.knnSearch(digito_flt, indices, dists, k, flann::SearchParams(32));
        
        // Olha as 5 imagens mais parecidas e decide qual o dígito por "votação" 
        vector<int> votos(10, 0);
        for(int i = 0; i < k; i++) votos[ mnist.AY[indices.at<int>(0, i)] ]++;
        return distance(votos.begin(), max_element(votos.begin(), votos.end()));
    }
    return -1;
}

void desenharDebugVisao(Mat_<COR>& frame_COR, int estado, int digito, float ncc, int escala, Point pt, const vector<EscalaModelo>& modelos) {
    // DESENHAR INFORMAÇÃO NA TELA
    if (escala != -1 && ncc > 0.50) {
        int bw = modelos[escala].cols / 2;
        int bh = modelos[escala].rows / 2;
        rectangle(frame_COR, Point(pt.x - bw, pt.y - bh), 
                             Point(pt.x + bw, pt.y + bh), Scalar(0, 255, 255), 2);
    }

    // Carimba texto de debug no topo da imagem da câmera
    char txt_status[100];
    sprintf(txt_status, "Estado: %d | Digito: %d", estado, digito);
    putText(frame_COR, txt_status, Point(10, 30), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0, 0, 255), 2);
    
    char txt_debug[100];
    sprintf(txt_debug, "NCC: %.2f | Escala: %d", ncc, escala);
    putText(frame_COR, txt_debug, Point(10, 60), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255, 255, 0), 2);
}

/* =================================================================
   =================== MAIN - MAQUINA DE ESTADOS ===================
   ================================================================= */ 

int main(int argc, char** argv) {
    // Verifica se o comando no terminal está completo
    if (argc < 3) erro("Uso: cliente7_final <IP_Raspberry> <videosaida.avi> [t/c]");

    // Instancia o cliente e tenta conectar na raspberry
    CLIENT client(argv[1]);

    // 1. CARREGAR MNIST E TREINAR IA
    cout << "Carregando modelo MNIST..." << endl;
    MNIST mnist(14, true, true);
    mnist.le("./");  // lê os arquivos de treinamento
    flann::Index flann_index(mnist.ax, flann::KDTreeIndexParams(4)); // organiza os exemplos em uma árvore de busca rápida

    vector<EscalaModelo> modelos = prepararModelos("quadrado.png");

    // 3. CONFIGURAR GRAVAÇÃO E INTERFACE
    // Abre o arquivo de vídeo (Modo C = 320x240, Modo T = 640x240 com teclado)
    bool mostrar_tela = (argc >= 4 && argv[3][0] == 't'); // se tem t no comando, abre a janela gráfica larga
    Size tamanho_video = mostrar_tela ? Size(640, 240) : Size(320, 240);
    VideoWriter vo(argv[2], VideoWriter::fourcc('X','V','I','D'), 20, tamanho_video);
    
    if (mostrar_tela) {
        namedWindow("Cliente Misto");
        setMouseCallback("Cliente Misto", cliqueMouse);
    }

    // Prepara as memórias da FSM (estado atual, cronômetro, último dígito encontrado)
    int estado = 0; 
    double t_estado = timeSinceEpoch();
    int digito_memoria = -1;
    bool auto_anterior = false; // lembra se o modo automatico estava ligado no frame anterior
    
    // 4. LOOP PRINCIPAL
    while (true) {
        // Recebe tamanho da imagem
        int tam;
        client.receiveBytes(4, (BYTE*)&tam);
        if (tam <= 0) break; // se vier zero, a conexão caiu --> quebra o loop
        
        vector<BYTE> buffer(tam);                   // cria espaço na memória para a imagem
        client.receiveBytes(tam, buffer.data());    // recebe a imagem do servidor
        Mat_<COR> frame_COR = imdecode(buffer, 1);  // descompacta os bytes recebidos
        if (frame_COR.empty()) break;

        Mat_<FLT> frame_FLT;
        float melhor_ncc = 0;    // melhor nível de confiança (NCC) 
        int melhor_escala = -1;  // guarda o tamanho com maior nota (escala de 0 a 9)
        Point melhor_pt(0,0);    // posição da placa de maior clareza

        if (modo_automatico && !auto_anterior) { // acabou de ligar o automatico
            estado = 0;                          // sai do estado 4 (manual) e volta a procurar a placa
            t_estado = timeSinceEpoch();
        }
        auto_anterior = modo_automatico;

        // Só executa o peso da visão computacional se estiver no modo automático e precisando procurar a placa
        if (modo_automatico && (estado == 0 || estado == 2)) {
            converte(frame_COR, frame_FLT); // pega a foto colorida (0 a 255) da rede e converte para float
            procurarPlaca(frame_FLT, modelos, melhor_ncc, melhor_escala, melhor_pt);
        }

        BYTE v_esq = 0, v_dir = 0;
        BYTE r_esq = 0, r_dir = 0; // velocidades de ré (so usadas no modo manual)
        double decorrido = timeSinceEpoch() - t_estado;

        // --- DIVISOR DE ÁGUAS: MODO MANUAL VS MODO AUTOMÁTICO ---
        if (!modo_automatico) {
            estado = 4; // Força o estado 4 (Manual) para a interface saber onde estamos
            traduzirComando(comandoMouse, v_esq, v_dir, r_esq, r_dir);
        } 
        else {
            // LÓGICA DA MÁQUINA DE ESTADOS FINITA (AUTOMÁTICA)
            switch(estado) {
                case 0: // procurar e seguir
                    if (melhor_escala != -1 && melhor_ncc > 0.50) { // limiar de tolerância = 0.5
                        if (melhor_escala <= 1) { // verifica se a placa tem um tamanho bom
                            estado = 1;
                            t_estado = timeSinceEpoch(); // zera o cronômetro
                        } else { // se não tiver, se aproxima da placa
                            // mini sistema de controle para ajustar rota
                            int erro_x = melhor_pt.x - 160; // vê o erro de rota (quão longe do centro = 160)
                            int base = 130; // velocidade
                            float K = 0.5;  // constante proporcional

                            // ajusta a velocidade de acordo com o erro
                            int calc_esq = base + (K * erro_x);
                            int calc_dir = base - (K * erro_x);

                            // trava de segurança: mantém na escala 0 a 255
                            v_esq = (calc_esq > 255) ? 255 : (calc_esq < 0 ? 0 : calc_esq);
                            v_dir = (calc_dir > 255) ? 255 : (calc_dir < 0 ? 0 : calc_dir);
                        }
                    }
                    break;

                case 1: // travar (esperar inércia)
                    v_esq = 0; v_dir = 0;
                    if (decorrido > 1.0) estado = 2; // 1 segundo parado
                    break;

                case 2: // ler dígito da placa
                    v_esq = 0; v_dir = 0; // para os motores enquanto a foto é processada

                    if (melhor_escala <= 1 && melhor_ncc > 0.50) { // confirma escala e qualidade antes de ler
                        digito_memoria = lerDigito(frame_COR, modelos[melhor_escala], melhor_pt, mnist, flann_index);
                        
                        // Decidir transição de estado de acordo com o dígito vencedor
                        if (digito_memoria == 0 || digito_memoria == 1) {
                            modo_automatico = false; // FIM DA PISTA: Volta para o modo manual
                            estado = 4; 
                            comandoMouse = 5; // Garante que volta parado
                        } else if (digito_memoria != -1) {
                            estado = 3; // executar manobra
                            t_estado = timeSinceEpoch();
                        } else {
                            estado = 0; // nao conseguiu ler o digito, volta a procurar
                        }
                    } else {
                        estado = 0; // se não conseguir ler, volta a procurar
                    }
                    break;

                case 3: // manobras cegas
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
                    if (estado == 5) t_estado = timeSinceEpoch(); // ao trocar de estado, reseta o cronômetro
                    break;

                case 5: // recuperação (esperar inércia e recomeçar)
                    v_esq = 0; v_dir = 0;
                    if (decorrido > 0.5) estado = 0; 
                    break;
            }
        } // Fim IF Automático

        // ENVIAR COMANDOS PELA REDE (Igual para os dois modos)
        BYTE cmd[4] = {v_esq, v_dir, r_esq, r_dir};
        cout << "auto=" << modo_automatico << " estado=" << estado
             << " cmd=" << (int)v_esq << "," << (int)v_dir
             << " re=" << (int)r_esq << "," << (int)r_dir << endl; // depuracao
        client.sendBytes(4, cmd);
        
        // --- MONTAR A INTERFACE E GRAVAR VÍDEO ---
        if (mostrar_tela) {
            Mat_<COR> tela(240, 640, COR(0, 0, 0));
            
            if (modo_automatico) {
                desenharDebugVisao(frame_COR, estado, digito_memoria, melhor_ncc, melhor_escala, melhor_pt, modelos);
            }
            
            // Copia a câmera para a metade esquerda
            frame_COR.copyTo(tela(Rect(0, 0, 320, 240)));
            
            // Desenha o teclado na metade direita
            desenharTeclado(tela);
            
            imshow("Cliente Misto", tela);
            vo << tela; // Grava a tela larga completa
        } else {
            // Se estiver rodando no modo 'c', grava apenas a câmera pura
            if (modo_automatico) {
                desenharDebugVisao(frame_COR, estado, digito_memoria, melhor_ncc, melhor_escala, melhor_pt, modelos);
            }
            vo << frame_COR;
        }
        
        // ESC --> Sai à força e encerra
        if (waitKey(1) == 27) {
            cmd[0] = 0; cmd[1] = 0; cmd[2] = 0; cmd[3] = 0;
            client.sendBytes(4, cmd); // Tenta parar o carrinho antes de fechar
            break;
        }
    }

    return 0;
}
