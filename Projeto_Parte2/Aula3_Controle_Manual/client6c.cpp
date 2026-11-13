#include "../projeto.hpp"

int main(int argc, char *argv[]) {
    if (argc != 2) erro("client6c servidorIpAddr");

    CLIENT client(argv[1]);

    uint32_t u;
    client.receiveUint(u);
    cout << "Recebi o numero " << u << " de Raspberry" << endl;

    client.sendUint(3333333333U);
    cout << "Enviei numero 3333333333 para Raspberry" << endl;

    return 0;
}
