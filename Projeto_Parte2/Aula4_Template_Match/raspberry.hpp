//raspberry.hpp
#ifndef RASPBERRY_HPP
#define RASPBERRY_HPP

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <chrono>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

void erro(string s1="") {
    cerr << s1 << endl;
    exit(1);
}

#define xdebug { string st = "File="+string(__FILE__)+" line="+to_string(__LINE__)+"\n"; cout << st; }
#define xprint(x) { ostringstream os; os << #x "=" << x << '\n'; cout << os.str(); }

double timeSinceEpoch() {
    using namespace std::chrono;
    return duration_cast<duration<double>>( system_clock::now().time_since_epoch() ).count();
}

typedef uint8_t BYTE;
typedef uint8_t GRY;
typedef Vec3b COR;
typedef float FLT;

// Converte a imagem colorida para matriz de ponto flutuante em escala de cinzentos
void converte(Mat_<COR> ent, Mat_<FLT>& sai) {
    Mat_<Vec3f> temp; 
    ent.convertTo(temp, CV_32F, 1.0/255.0, 0.0);
    cvtColor(temp, sai, COLOR_BGR2GRAY);
}

// Pré-processa o modelo, subtraindo a média e zerando os pixéis "don't care"
Mat_<FLT> dcReject(Mat_<FLT> T, float dc_val = -1.0f) {
    Mat_<FLT> out = T.clone();
    float sum = 0.0f;
    int count = 0;
    
    for(int r=0; r<out.rows; r++) {
        for(int c=0; c<out.cols; c++) {
            if(out(r,c) != dc_val) {
                sum += out(r,c);
                count++;
            }
        }
    }
    
    float mean = (count > 0) ? (sum / count) : 0.0f;
    
    for(int r=0; r<out.rows; r++) {
        for(int c=0; c<out.cols; c++) {
            if(out(r,c) == dc_val) {
                out(r,c) = 0.0f; 
            } else {
                out(r,c) -= mean; 
            }
        }
    }
    return out;
}

// Normaliza o modelo para que a soma absoluta seja igual a 2
Mat_<FLT> somaAbsDois(Mat_<FLT> T) {
    Mat_<FLT> out = T.clone();
    float sum_abs = 0.0f;
    
    for(int r=0; r<out.rows; r++) {
        for(int c=0; c<out.cols; c++) {
            sum_abs += abs(out(r,c));
        }
    }
    
    if (sum_abs > 0) {
        for(int r=0; r<out.rows; r++) {
            for(int c=0; c<out.cols; c++) {
                out(r,c) = (out(r,c) / sum_abs) * 2.0f;
            }
        }
    }
    return out;
}

// Faz o Template Matching e devolve o resultado centrado com o mesmo tamanho da entrada
Mat_<FLT> matchTemplateSame(Mat_<FLT> I, Mat_<FLT> T, int method, FLT backg=0.0) {
    Mat_<FLT> R;
    matchTemplate(I, T, R, method);
    
    Mat_<FLT> out(I.rows, I.cols, backg);
    int dx = T.cols / 2;
    int dy = T.rows / 2;
    
    R.copyTo(out(Rect(dx, dy, R.cols, R.rows)));
    return out;
}

#endif
