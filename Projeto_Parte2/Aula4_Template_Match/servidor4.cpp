// servidor4.cpp
// compila servidor4 -w -ocv
#include "projeto.hpp" 
#include <opencv2/opencv.hpp>
#include <vector>
#include <wiringPi.h>
#include <softPwm.h>
#include <cstdlib>

using namespace std;
using namespace cv;

int main() {
    // Inicializar os pinos dos motores (lógica do pwmroda4.cpp)
    wiringPiSetup();
    if (softPwmCreate(0, 0, 100)) exit(1);
    if (softPwmCreate(1, 0, 100)) exit(1);
    if (softPwmCreate(2, 0, 100)) exit(1);
    if (softPwmCreate(3, 0, 100)) exit(1);

    // Configurar câmara
    VideoCapture cam(0);
    cam.set(CAP_PROP_FRAME_WIDTH, 320);
    cam.set(CAP_PROP_FRAME_HEIGHT, 240);
    if (!cam.isOpened()) erro("Erro: Camera nao encontrada!");

    // Esperar pelo cliente (computador)
    SERVER server;
    server.waitConnection();

    Mat_<COR> frame;
    vector<BYTE> buf_jpg;
    vector<int> parametros_jpg = {IMWRITE_JPEG_QUALITY, 80}; // Compactação para acelerar a rede

    while (true) {
        // 1. CAPTURAR E ENVIAR IMAGEM
        cam >> frame;
        if (frame.empty()) break;

        imencode(".jpg", frame, buf_jpg, parametros_jpg);
        int tam = buf_jpg.size();
        
        server.sendBytes(4, (BYTE*)&tam);
        server.sendBytes(tam, buf_jpg.data());

        // 2. RECEBER COMANDOS DOS MOTORES
        BYTE cmd[2];
        server.receiveBytes(2, cmd);
        
        BYTE v_esq = cmd[0];
        BYTE v_dir = cmd[1];

        // 3. APLICAR NOS MOTORES
        // O cliente envia de 0 a 255. Convertemos para a escala 0 a 100 do softPwm.
        int pwm_esq = (v_esq * 100) / 255;
        int pwm_dir = (v_dir * 100) / 255;

        // Com base no teu teste: pino 0 e 3 acionam o movimento para a frente
        softPwmWrite(0, pwm_esq); 
        softPwmWrite(1, 0);
        
        softPwmWrite(2, 0);       
        softPwmWrite(3, pwm_dir);
    }

    // Parar os motores caso o loop seja interrompido
    softPwmStop(0); softPwmStop(1); 
    softPwmStop(2); softPwmStop(3);

    return 0;
}