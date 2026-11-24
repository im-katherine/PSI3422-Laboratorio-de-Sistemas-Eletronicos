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

#include "projeto.hpp"
#include <opencv2/opencv.hpp>
#include <vector>
#include <omp.h>
#include <algorithm>

using namespace std;
using namespace cv;

// Variaveis globais (interface manual)
int comandoMouse = 5; // 5 = Parado
int mapaTeclas[9] = {7, 8, 9, 4, 5, 6, 1, 2, 3};
bool modo_automatico = false; // comeca no modo manual

// Variavel personalizada - Estrutura da placa p/ modelos de Template Matching
struct EscalaModelo {
    Mat_<FLT> T_pre;    // guarda imagem pre-processada do molde (float p/ precisao e negativos, evita overflow)
    Mat_<FLT> T_raw;    // molde "puro"
    float escala_val;   // fator de reducao
    int cols, rows;     // largura e altura desse molde
};

/* =================================================================
   ===================   FUNCOES - MODO MANUAL   ===================
   ================================================================= */ 

// Identifica cliques na janela do programa e executa de acordo
void cliqueMouse(int evento, int x, int y, int, void*) {
    if (evento == EVENT_LBUTTONUP && !modo_automatico) comandoMouse = 5; // soltou o botao = para o evento
    if (evento != EVENT_LBUTTONDOWN) return; // se o evento nao for clicar em algo, sai da funcao

    // Testa cada botao do teclado virtual
    for (int i = 0; i < 9; i++) {
        // Calcula as coordenadas X e Y do botao i
        int bx = 345 + (i % 3) * 90;
        int by = 55 + (i / 3) * 55;

        // Verifica se o mouse esta na caixa de colisao do botao (55 largura x 35 altura)
        if (x >= bx && x <= bx + 55 && y >= by && y <= by + 35) {
            if (i == 4) { 
                modo_automatico = !modo_automatico; // Se clicou no botao central, alterna o modo
                comandoMouse = 5; // para o carrinho ao trocar de modo
            } else if (!modo_automatico) { // so aceita demais comandos no modo manual
                comandoMouse = mapaTeclas[i]; // salva o n° da tecla clicada de acordo com o mapa 
            }
            return;
        }
    }
}

// Calcula o centro do botao e faz a seta na direcao dos multiplicadores de direcao (dx e dy)
void desenharSeta(Mat_<COR>& tela, int x, int y, int dx, int dy) { 
    Point centro(x + 27, y + 17);

    Point inicio( // comeco da seta, recuo no sentido oposto da direcao 
        centro.x - dx * 12,
        centro.y - dy * 9
    );

    Point fim( // ponta da seta
        centro.x + dx * 12,
        centro.y + dy * 9
    );

    arrowedLine(  // funcao que desenha a seta
        tela, inicio, fim,
        Scalar(255, 255, 255),
        2, LINE_AA, 0, 0.35
    );
}

// Monta o painel lateral na janela, exibe status do moto atual no topo e preenche botoes
void desenharTeclado(Mat_<COR>& tela) {
    rectangle(tela, Point(320, 0), Point(639, 239),
              Scalar(60, 60, 60), -1); // fundo do painel, retangulo solido

    // Seleciona texto e cor do cabecalho a partir do modo atual
    string txt_modo = modo_automatico 
        ? "MODO AUTOMATICO (IA)"
        : "MODO MANUAL";
    Scalar cor_txt = modo_automatico
        ? Scalar(0, 255, 0)
        : Scalar(255, 255, 255);
    
    // Escreve status do modo no topo do painel
    putText(tela, txt_modo, Point(345, 30),
            FONT_HERSHEY_SIMPLEX, 0.6, cor_txt, 2);

    // Direcoes das oito setas, na ordem do teclado
    int dx[9] = {-1, 0, 1, -1, 0, 1, -1, 0, 1};
    int dy[9] = {-1,-1,-1,  0, 0, 0,  1, 1, 1};

    // Percorre os 9 botoes do teclado, desenhando o conteudo
    for (int i = 0; i < 9; i++) {
        // Calcula as coordenadas do canto superior esquerdo de cada botao na grade
        int x = 345 + (i % 3) * 90;
        int y = 55 + (i / 3) * 55;
        
        // Desenha a moldura do botao
        rectangle(tela, Point(x, y),
                  Point(x + 55, y + 35),
                  Scalar(200, 200, 200), 1);

        if (i == 4) { // botao central: alterna Manual/Automatico
            putText(tela, "M/A", Point(x + 7, y + 24),
                    FONT_HERSHEY_SIMPLEX, 0.6,
                    Scalar(0, 0, 255), 2);
        } else { // demais botoes: seta
            desenharSeta(tela, x, y, dx[i], dy[i]);
        }
    }
}

// Traduz a tecla do controle manual em velocidade PWM
void traduzirComando(int comando, BYTE& v_esq, BYTE& v_dir, BYTE& r_esq, BYTE& r_dir) {
    // v_* = velocidade para a frente   /   r_* = velocidade para tras (re)
    v_esq = 0; v_dir = 0; r_esq = 0; r_dir = 0;
    switch (comando) {
        case 8: v_esq = 200; v_dir = 200; break; // Frente
        case 4: v_esq = 200; v_dir = 0; break;   // Esquerda
        case 6: v_esq = 0; v_dir = 200; break;   // Direita
        case 7: v_esq = 200; v_dir = 100; break; // Frente-Esquerda
        case 9: v_esq = 100; v_dir = 200; break; // Frente-Direita
        case 2: r_esq = 200; r_dir = 200; break; // Re
        case 1: r_esq = 200; r_dir = 100; break; // Re-Esquerda
        case 3: r_esq = 100; r_dir = 200; break; // Re-Direita
        default: break;                          // Parado
    }
}


/* =================================================================
   =================== FUNCOES - MODO AUTOMATICO ===================
   ================================================================= */ 

// Prepara as 10 escalas da placa
vector<EscalaModelo> prepararModelos(const string& nome_arquivo) {
    // Converte a matriz colorida da placa em um float
    Mat_<COR> temp_COR = imread(nome_arquivo, 1); // le a imagem em padrao BGR e armazena na matriz
    if (temp_COR.total() == 0) erro("Erro: molde nao encontrado!"); 
    Mat_<FLT> T_original;
    converte(temp_COR, T_original); // converte a imagem colorida (0 a 255) em tons de cinza (0.0 a 1.0). Evita overflow e permite netagivos
    
    vector<EscalaModelo> modelos(10); // prepara um vetor p/ guardar as 10 escalas
    float fator = 0.30;  //// fator de reducao inicial (maior molde tem 30% do arquivo)
    float decrecimo = pow(0.095 / 0.30, 1.0/9.0); // redu em PG
    
    // Constroi os moldes: redimensiona a imagem, remove cinza medio (rejeicao DC) e salva a versao otimizada
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

// Encontrar os picos de deteccao no mapa do OpenCV
vector<Point> acharPicos(const Mat_<FLT>& mapa, int limite, int dist) {
    vector<Point> picos; // vetor de coordenadas
    Mat_<FLT> work_map = mapa.clone(); // para poder apagar os picos da copia ao encontrar 
    
    // Loop ate acabar ou atingir o limite de picos
    for(int i = 0; i < limite; i++) {
        double maxVal; Point maxLoc;  // pontuação e coordenadas do pico 
        minMaxLoc(work_map, NULL, &maxVal, NULL, &maxLoc); // varre a matriz encontrando maximo (nao quero minimos -> null)
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

// Localiza a placa na imagem (CC para achar candidatos rapido e NCC para confirmar)
void procurarPlaca(const Mat_<FLT>& frame_FLT, const vector<EscalaModelo>& modelos, float& melhor_ncc, int& melhor_escala, Point& melhor_pt) {
    vector<Mat_<FLT>> R_cc(10);   // cross-Correlation - armazena os mapas da busca preliminar
    vector<Mat_<FLT>> R_ncc(10);  // normalized cross-correlation - mapas da busca refinada
    
    melhor_ncc = 0; melhor_escala = -1; melhor_pt = Point(0,0);
    
    #pragma omp parallel for // executa o código paralelamente nos núcleos do processador
    for (int s = 0; s < 10; s++) {
        R_cc[s] = matchTemplateSame(frame_FLT, modelos[s].T_pre, TM_CCORR, 0.0); // compara com o modelo pré processado e guarda o mapa de calor em Rcc
        R_ncc[s] = matchTemplateSame(frame_FLT, modelos[s].T_raw, TM_CCOEFF_NORMED, 0.0); // busca refinada, compara o frame com o modelo cru e gera um segundo mapa
    }
    
    // Segundo loop, sem paralelismo, para evitar condição de corrida
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

// Aplica KNN para classificar os digitos manuscritos na placa
int lerDigito(const Mat_<COR>& frame_COR, const EscalaModelo& modelo, Point pt, MNIST& mnist, flann::Index& flann_index) {
    // Encontra o centro aproximado da placa
    int bw = modelo.cols / 2;
    int bh = modelo.rows / 2;
    
    // Margem de segurança - Capturar o digito sem cortar nem aparecer a borda preta
    int margem_x = bw * 0.12; 
    int margem_y = bh * 0.12;
    Rect regiao_digito(pt.x - bw/2 + margem_x, pt.y - bh/2 + margem_y, bw - 2*margem_x, bh - 2*margem_y); // retangulo para recorte
    regiao_digito &= Rect(0, 0, frame_COR.cols, frame_COR.rows); // se tentar cortar fora da camera, ajusta
    
    if (regiao_digito.area() > 0) {
        // Converte o miolo da placa para tons de cinza (MNIST nao entende cores) 
        Mat_<GRY> img_digito;
        cvtColor(frame_COR(regiao_digito), img_digito, COLOR_BGR2GRAY);
        threshold(img_digito, img_digito, 0, 255, THRESH_BINARY | THRESH_OTSU); // limiar de otsu --> encontra o melhor tom de cinza para separar a caneta do papel
        
        Mat_<GRY> img_bbox = mnist.bbox(img_digito);  // funcao do cekeikon para forçar imagem em 14x14
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
        vector<int> votos(10, 0); // cria uma "urna" com 10 posicoes (digitos de 0 a 9)
        for(int i = 0; i < k; i++) votos[ mnist.AY[indices.at<int>(0, i)] ]++; // distribui os votos
        return distance(votos.begin(), max_element(votos.begin(), votos.end())); // retorna o indice da posicao mais voatada
    }
    return -1;
}

// Desenha na camera o resultado final do rastreamento da placa e reconhecimento do digito
void desenharDebugVisao(Mat_<COR>& frame_COR, int estado, int digito, float ncc, int escala, Point pt, const vector<EscalaModelo>& modelos) {
    // Desenha condorno da placa caso a confianca passe do limiar de confianca (50%)
    if (escala != -1 && ncc > 0.50) {
        int bw = modelos[escala].cols / 2;
        int bh = modelos[escala].rows / 2;
        rectangle(frame_COR, Point(pt.x - bw, pt.y - bh), 
                             Point(pt.x + bw, pt.y + bh), Scalar(0, 255, 255), 2);
    }

    // Carimba texto de debug no topo da imagem da camera
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
    // Verifica se o comando no terminal esta completo
    if (argc < 3) erro("Uso: cliente7_final <IP_Raspberry> <videosaida.avi> [t/c]");

    // Instancia o cliente e tenta conectar na raspberry
    CLIENT client(argv[1]);

    // 1. CARREGAR MNIST E TREINAR IA
    cout << "Carregando modelo MNIST..." << endl;
    MNIST mnist(14, true, true); // inicializa mnist, imagem 14x14 com inversao de cores e recorte automatico bounding box
    mnist.le("./");  // le os arquivos de treinamento
    flann::Index flann_index(mnist.ax, flann::KDTreeIndexParams(4)); // 4 arvores de busca paralelas

    // Pre calcula 10 escalas em PG a partir do molde da placa
    vector<EscalaModelo> modelos = prepararModelos("quadrado.png");

    // 2. CONFIGURAR GRAVACAO E INTERFACE
    // Abre o arquivo de video (Modo C = 320x240, Modo T = 640x240 com teclado)
    bool mostrar_tela = (argc >= 4 && argv[3][0] == 't'); // se tem t no comando, abre a janela grafica larga
    Size tamanho_video = mostrar_tela ? Size(640, 240) : Size(320, 240);
    VideoWriter vo(argv[2], VideoWriter::fourcc('X','V','I','D'), 20, tamanho_video);
    
    // Cria janela grafica com callback para capturar eventos do mouse
    if (mostrar_tela) {
        namedWindow("Cliente Misto");
        setMouseCallback("Cliente Misto", cliqueMouse);
    }

    // 3. PREPARA MEMORIAS DA FSM (estado atual, cronometro, ultimo digito encontrado)
    int estado = 0; 
    double t_estado = timeSinceEpoch();
    int digito_memoria = -1;
    bool auto_anterior = false; // lembra se o modo automatico estava ligado no frame anterior
    
    // 4. LOOP PRINCIPAL
    while (true) {
        // Recebe o tamanho da imagem
        int tam;
        client.receiveBytes(4, (BYTE*)&tam);
        if (tam <= 0) break; // se vier zero, a conexao caiu --> quebra o loop
        
        vector<BYTE> buffer(tam);                   // cria espaço na memoria para a imagem
        client.receiveBytes(tam, buffer.data());    // recebe a imagem do servidor
        Mat_<COR> frame_COR = imdecode(buffer, 1);  // descompacta os bytes recebidos
        if (frame_COR.empty()) break;

        Mat_<FLT> frame_FLT;
        float melhor_ncc = 0;    // melhor nivel de confiança (NCC) 
        int melhor_escala = -1;  // guarda o tamanho com maior nota (escala de 0 a 9)
        Point melhor_pt(0,0);    // posição da placa de maior clareza

        if (modo_automatico && !auto_anterior) { // apenas ao ligar o automatico
            estado = 0;                          // sai do estado 4 (manual) e volta a procurar a placa
            t_estado = timeSinceEpoch();         // zera o cronometro 
        }
        auto_anterior = modo_automatico;         // garante que o if so eh executado na borda de subida

        // Executa a visao computacional apenas no modo automatico e precisando procurar a placa
        if (modo_automatico && (estado == 0 || estado == 2)) {
            converte(frame_COR, frame_FLT); // pega a foto colorida (0 a 255) da rede e converte para float
            procurarPlaca(frame_FLT, modelos, melhor_ncc, melhor_escala, melhor_pt);
        }

        BYTE v_esq = 0, v_dir = 0;  // se nenhuma condicao ativar o carrinho, ele para
        BYTE r_esq = 0, r_dir = 0;  // velocidades de re (somente no manual)
        double decorrido = timeSinceEpoch() - t_estado;

        // ------- MODO MANUAL VS MODO AUTOMATICO -------
        if (!modo_automatico) { // MODO MANUAL
            estado = 4; // desativar modo auto forca a entrar no estado 4 (manual)
            traduzirComando(comandoMouse, v_esq, v_dir, r_esq, r_dir); // passa a ler comandos do mouse
        } 
        else {
            // MAQUINA DE ESTADOS FINITA (MODO AUTOMATICO)
            switch(estado) {
                case 0: // procurar e seguir
                    if (melhor_escala != -1 && melhor_ncc > 0.50) { // limiar de tolerancia = 0.5
                        if (melhor_escala <= 2) { // verifica se a placa tem um tamanho bom
                            estado = 1;
                            t_estado = timeSinceEpoch(); // zera o cronometro
                        } else { // se nao tiver, se aproxima da placa
                            // mini sistema de controle para ajustar rota
                            int erro_x = melhor_pt.x - 160; // ve o erro de rota (quao longe do centro = 160)
                            int base = 100; // velocidade
                            float K = 0.35;  // constante proporcional

                            // ajusta a velocidade de acordo com o erro
                            int calc_esq = base - (K * erro_x);
                            int calc_dir = base + (K * erro_x);

                            // trava de seguranca: mantem sempre na escala 0 a 255
                            v_esq = (calc_esq > 255) ? 255 : (calc_esq < 0 ? 0 : calc_esq);
                            v_dir = (calc_dir > 255) ? 255 : (calc_dir < 0 ? 0 : calc_dir);
                        }
                    }
                    break;

                case 1: // travar (esperar inercia)
                    v_esq = 0; v_dir = 0;
                    if (decorrido > 1.0) { // 1 segundo parado
                        estado = 2;
                        t_estado = timeSinceEpoch();
                    }
                    break;

                case 2: // ler digito da placa
                    v_esq = 0; v_dir = 0; // para os motores enquanto a foto eh processada

                    if (melhor_escala != -1 && melhor_escala <= 2 && melhor_ncc > 0.50) { // confirma escala e qualidade antes de ler
                        digito_memoria = lerDigito(frame_COR, modelos[melhor_escala], melhor_pt, mnist, flann_index);
                        
                        // Decidir transicao de estado de acordo com o digito vencedor
                        if (digito_memoria == 0 || digito_memoria == 1) { // FIM: volta para o manual
                            modo_automatico = false; 
                            estado = 4; 
                            comandoMouse = 5; // garante que volta parado
                        } else if (digito_memoria != -1) {
                            estado = 3; // executar manobra
                            t_estado = timeSinceEpoch();
                        } else if (decorrido > 0.5) { // perdeu o digito por mais de 0.5s, volta a procurar
                            estado = 0;
                        }
                    } else if (decorrido > 0.5) { // nao conseguiu qualidade boa o bastante para seguir, volta a procurar
                        estado = 0; 
                    }
                    break;

                case 3: // manobras automaticas
                    if (digito_memoria == 6 || digito_memoria == 7) { // 90 esq
                        v_esq = 200; v_dir = 0;
                        if (decorrido > 1.0) estado = 5; 
                    } 
                    else if (digito_memoria == 8 || digito_memoria == 9) { // 90 dir
                        v_esq = 0; v_dir = 200;
                        if (decorrido > 1.2) estado = 5; 
                    } 
                    else if (digito_memoria == 2) { // 180 esq
                        v_esq = 200; v_dir = 0;
                        if (decorrido > 2.0) estado = 5; 
                    } 
                    else if (digito_memoria == 3) { // 180 dir
                        v_esq = 0; v_dir = 200;
                        if (decorrido > 2.4) estado = 5; 
                    } 
                    else if (digito_memoria == 4 || digito_memoria == 5) { // seguir em frente
                        v_esq = 180; v_dir = 180;
                        if (decorrido > 1.2) estado = 5;
                    }
                    if (estado == 5) t_estado = timeSinceEpoch(); // ao trocar de estado, reseta o cronometro
                    break;

                case 5: // recuperacao (espera inercia e reseta)
                    v_esq = 0; v_dir = 0;
                    if (decorrido > 0.5) estado = 0; 
                    break;
            }
        } // Fim do modo automatico

        // 5. ENVIAR COMANDOS PELA REDE
        BYTE cmd[4] = {v_esq, v_dir, r_esq, r_dir};
        cout << "auto=" << modo_automatico << " estado=" << estado
             << " cmd=" << (int)v_esq << "," << (int)v_dir
             << " re=" << (int)r_esq << "," << (int)r_dir << endl; // depuracao
        client.sendBytes(4, cmd);
        
        // 6. MONTAR A INTERFACE E GRAVAR VIDEO
        if (mostrar_tela) { // verifica se esta no modo t
            Mat_<COR> tela(240, 640, COR(0, 0, 0)); // cria uma "tela em branco" para o video e teclado
            
            if (modo_automatico) { // se no modo auto, adiciona os debugs na tela
                desenharDebugVisao(frame_COR, estado, digito_memoria, melhor_ncc, melhor_escala, melhor_pt, modelos);
            }
            
            // Copia a camera para a metade esquerda
            frame_COR.copyTo(tela(Rect(0, 0, 320, 240)));
            
            // Desenhar o teclado na metade direita
            desenharTeclado(tela);
            
            imshow("Cliente Misto", tela);
            vo << tela; // Grava a tela larga completa
        } else { // Se estiver rodando no modo 'c', grava apenas a camera pura
            if (modo_automatico) {
                desenharDebugVisao(frame_COR, estado, digito_memoria, melhor_ncc, melhor_escala, melhor_pt, modelos);
            }
            vo << frame_COR;
        }
        
        // ESC --> Sai a força e encerra
        if (waitKey(1) == 27) {
            cmd[0] = 0; cmd[1] = 0; cmd[2] = 0; cmd[3] = 0;
            client.sendBytes(4, cmd); // Para o carrinho antes de fechar
            break;
        }
    }

    return 0;
}