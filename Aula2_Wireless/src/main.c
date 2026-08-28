#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/console/console.h>
#include <soc.h>
#include <string.h>
#include "nrf24l01.h"
#include "spi.h"

#if defined(ROLE_RX)
// LED Verde integrado na placa FRDM-KL25Z (PTB19 - Lógica Invertida)
#define LED_GREEN_PIN 19u

static void led_init(void) {
    SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;
    PORTB->PCR[LED_GREEN_PIN] = PORT_PCR_MUX(1);
    PTB->PDDR |= (1u << LED_GREEN_PIN);
    PTB->PSOR = (1u << LED_GREEN_PIN); // Desligado no início (Active Low)
}

static void led_on(void) {
    PTB->PCOR = (1u << LED_GREEN_PIN); // Nível baixo liga o LED
}

static void led_off(void) {
    PTB->PSOR = (1u << LED_GREEN_PIN); // Nível alto desliga o LED
}
#endif

int main(void) {
    printk("\n=== Sistema nRF24L01 Inicializado ===\n");
    nrf24_init();

    // Teste de pulso nos pinos de controle
    printk("[TESTE GPIO] Alternando CSN e CE...\n");
    ce_low();
    csn_low();
    k_msleep(10);
    csn_high();
    ce_high();
    k_msleep(10);
    ce_low();

    // Leitura final do CONFIG
    uint8_t cfg = 0;
    nrf24_read(CONFIG, &cfg, 1);
    printk("[TESTE CONFIG] Valor final: 0x%02X (Esperado: 0x0E ou 0x3F)\n", cfg);

    // TESTE 0: Escrita no registrador RF_CH (Canal)
    uint8_t ch_write = 0x55; // Padrao de bits alternados (01010101)
    uint8_t ch_read = 0;

    nrf24_write(RF_CH, &ch_write, 1);
    nrf24_read(RF_CH, &ch_read, 1);

    printk("[TESTE MOSI] Canal escrito: 0x55 | Canal lido: 0x%02X\n", ch_read);

    if (ch_read == 0x55) {
        printk("[OK MOSI] Linha MOSI funcionando perfeitamente!\n");
    } else {
        printk("[ERRO MOSI] Fio MOSI (PTE1) desconectado ou com mau contato.\n");
    }

    // TESTE 1: Sanidade da Comunicação SPI
    uint8_t config_test = 0;
    nrf24_read(CONFIG, &config_test, 1);
    printk("[TESTE SPI] Valor lido do registrador CONFIG: 0x%02X\n", config_test);

    if (config_test == 0x00 || config_test == 0xFF) {
        printk("[ERRO SPI] Falha de hardware! Verifique os pinos MISO, MOSI, SCK, CSN e GND.\n");
    } else {
        printk("[OK SPI] Comunicacao SPI validada com sucesso!\n");
    }

#if defined(ROLE_TX)
    console_init();
    printk("[Perfil: TRANSMISSOR]\n");
    printk("Digite '1' no terminal para LIGAR o LED ou '0' para DESLIGAR:\n");

    while (1) {
        // Captura o caractere digitado no terminal serial
        uint8_t c = console_getchar();

        if (c == '1') {
            printk("Enviando '1'...\n");
            if (nrf24_send_message("1")) {
                printk("Comando enviado com sucesso!\n");
            } else {
                printk("Falha ao enviar o comando.\n");
            }
        } else if (c == '0') {
            printk("Enviando '0'...\n");
            if (nrf24_send_message("0")) {
                printk("Comando enviado com sucesso!\n");
            } else {
                printk("Falha ao enviar o comando.\n");
            }
        }
    }

#elif defined(ROLE_RX)
    led_init();
    printk("[Perfil: RECEPTOR]\n");
    printk("Aguardando comandos via RF...\n");

    while (1) {
        char *rx_msg = nrf24_read_message();

        if (strcmp(rx_msg, "failed") != 0) {
            printk("Pacote recebido: %s\n", rx_msg);

            if (rx_msg[0] == '1') {
                led_on();
                printk("Acao: LED LIGADO\n");
            } else if (rx_msg[0] == '0') {
                led_off();
                printk("Acao: LED DESLIGADO\n");
            }
        }

        k_msleep(20); // Evita overhead no loop do Zephyr OS
    }

#else
    #error "Nenhum papel definido! Selecione o ambiente [env:frdm_kl25z_tx] ou [env:frdm_kl25z_rx]."
#endif

    return 0;
}