// servidor6.cpp - Igual ao servidor4, nao foram feitas alteracoes
// compila servidor6 -w -ocv
#include "projeto.hpp" 
#include <opencv2/opencv.hpp>
#include <vector>
#include <wiringPi.h>
#include <softPwm.h>
#include <cstdlib>

using namespace std;
using namespace cv;

int main() {
    // Inicializar os pinos dos motores (logica do pwmroda4.cpp)
    wiringPiSetup(); // biblioteca que configura a numeracao dos pinos
    if (softPwmCreate(0, 0, 100)) exit(1); // gerador de pwm -> (pino, min pwm, max pwm)
    if (softPwmCreate(1, 0, 100)) exit(1);
    if (softPwmCreate(2, 0, 100)) exit(1);
    if (softPwmCreate(3, 0, 100)) exit(1);

    // Configurar camera
    VideoCapture cam(0); // abre comunicacao com a camera da rasp
    cam.set(CAP_PROP_FRAME_WIDTH, 320);
    cam.set(CAP_PROP_FRAME_HEIGHT, 240);
    if (!cam.isOpened()) erro("Erro: Camera nao encontrada!");

    // Instancia o servidor e espera pelo cliente
    SERVER server;
    server.waitConnection();

    Mat_<COR> frame;  // matriz para armazenar as imagens (guarda ate serem processadas)
    vector<BYTE> buf_jpg;  // guarda temporariamente a imagem compactada em texto/bytes
    vector<int> parametros_jpg = {IMWRITE_JPEG_QUALITY, 80}; // regra de compactacao da imagem

    while (true) {
        // 1. CAPTURAR E ENVIAR IMAGEM
        cam >> frame; // captura um frame e guarda na matriz "frame"
        if (frame.empty()) break; // trava de seguranca

        imencode(".jpg", frame, buf_jpg, parametros_jpg); // comprime a imagem, reduzindo a qualidade, e salva os bytes no vetor
        int tam = buf_jpg.size();  // quantos bytes a imagem comprimida tem
        
        // Envia os dados da imagem comprimida 
        server.sendBytes(4, (BYTE*)&tam);  // o tamanho eh para o cliente saber quando termina de receber
        server.sendBytes(tam, buf_jpg.data()); 

        // 2. RECEBER COMANDOS DOS MOTORES
        BYTE cmd[2]; // reserva espaco na memoria para as instrucoes de velocidade 
        server.receiveBytes(2, cmd); // pausa e escuta a rede ate receber os 2 bytes de instrucao
        
        // Descompactar nas variaveis (nao salvar direto garante que a memoria nao seja corrompida)
        BYTE v_esq = cmd[0]; 
        BYTE v_dir = cmd[1];

        // 3. APLICAR NOS MOTORES
        // Cliente envia de 0 a 255 (tamanho maximo = 1 byte), conversao para a escala do softPwm:
        int pwm_esq = (v_esq * 100) / 255;
        int pwm_dir = (v_dir * 100) / 255;

        // Coloca a velocidade nos motores. Pinos 0 e 3 = movimento para a frente
        softPwmWrite(0, pwm_esq); 
        softPwmWrite(1, 0);
        
        softPwmWrite(2, 0);       
        softPwmWrite(3, pwm_dir);
    }

    // Para os motores caso o loop seja interrompido
    softPwmStop(0); softPwmStop(1); 
    softPwmStop(2); softPwmStop(3);

    return 0;
}