#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <soc.h>
#include "nrf24l01.h"
#include "spi.h"

// Defina apenas os NÚMEROS dos pinos (nunca passe máscaras como 1u<<30 aqui)
#define CE_PIN   30  // PTE30
#define CSN_PIN  4   // PTE4
#define IRQ_PIN  20  // PTE20

#define NRF_STATUS_REG 0x07

int main(void) {
    printk("\n=== Boot Zephyr OK - Iniciando Teste nRF24 ===\n");

    nrf24_init_gpio();
    printk("GPIOs configurados!\n");

    spi_init(SPI_1, ALT_0, 0, 2, CS_MAN);
    printk("SPI1 configurado!\n");
    
    uint8_t status = 0;
    int contador = 0;

    while (1) {
        // Tenta ler o registrador STATUS do nRF24
        nrf24_read(NRF_STATUS_REG, &status, 1);
        
        printk("[%d] Registrador STATUS do nRF24: 0x%02X\n", contador++, status);

        k_msleep(2000); // Pausa de 2 segundos sem travar o SO
    }

    return 0;
}