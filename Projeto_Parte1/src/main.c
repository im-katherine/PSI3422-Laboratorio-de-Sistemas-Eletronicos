#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/console/console.h>
#include <soc.h>
#include <string.h>
#include <stdbool.h>

#include "nrf24l01.h"
#include "spi.h"
#include "ultrasound.h"

#if defined(ROLE_RX)
/* Dispositivos GPIO para a Ponte H */
const struct device *gpio_a;
const struct device *gpio_c;
const struct device *gpio_d;

/* PINOS DA PONTE H:
 * Motor esquerdo: IN1=PTD4, IN2=PTA4
 * Motor direito : IN3=PTC9, IN4=PTD1
 */

void parar(void) {
    gpio_pin_set(gpio_d, 4, 0);
    gpio_pin_set(gpio_a, 4, 0);
    gpio_pin_set(gpio_c, 9, 0);
    gpio_pin_set(gpio_d, 1, 0);
}

void frente(void) {
    gpio_pin_set(gpio_d, 4, 0);
    gpio_pin_set(gpio_a, 4, 1);
    gpio_pin_set(gpio_c, 9, 1);
    gpio_pin_set(gpio_d, 1, 0);
}

void tras(void) {
    gpio_pin_set(gpio_d, 4, 1);
    gpio_pin_set(gpio_a, 4, 0);
    gpio_pin_set(gpio_c, 9, 0);
    gpio_pin_set(gpio_d, 1, 1);
}

void direita(void) {
    gpio_pin_set(gpio_d, 4, 0);
    gpio_pin_set(gpio_a, 4, 1);
    gpio_pin_set(gpio_c, 9, 0);
    gpio_pin_set(gpio_d, 1, 1);
}

void esquerda(void) {
    gpio_pin_set(gpio_d, 4, 1);
    gpio_pin_set(gpio_a, 4, 0);
    gpio_pin_set(gpio_c, 9, 1);
    gpio_pin_set(gpio_d, 1, 0);
}

#define DISTANCIA_MINIMA_CM 15
#define TEMPO_RE_MS         400
#define TEMPO_VIRA_MS       500

/* LED Verde integrado (PTB19 - Lógica Invertida) */
#define LED_GREEN_PIN 19u

static void led_init(void) {
    SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;
    PORTB->PCR[LED_GREEN_PIN] = PORT_PCR_MUX(1);
    PTB->PDDR |= (1u << LED_GREEN_PIN);
    PTB->PSOR = (1u << LED_GREEN_PIN); // Desligado no início
}

static void led_on(void) {
    PTB->PCOR = (1u << LED_GREEN_PIN);
}

static void led_off(void) {
    PTB->PSOR = (1u << LED_GREEN_PIN);
}

static int init_ponte_h(void) {
    gpio_c = DEVICE_DT_GET(DT_NODELABEL(gpioc));
    if (!device_is_ready(gpio_c)) return -1;
    gpio_pin_configure(gpio_c, 9, GPIO_OUTPUT_INACTIVE);

    gpio_a = DEVICE_DT_GET(DT_NODELABEL(gpioa));
    if (!device_is_ready(gpio_a)) return -1;
    gpio_pin_configure(gpio_a, 4, GPIO_OUTPUT_INACTIVE);

    gpio_d = DEVICE_DT_GET(DT_NODELABEL(gpiod));
    if (!device_is_ready(gpio_d)) return -1;
    gpio_pin_configure(gpio_d, 1, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(gpio_d, 4, GPIO_OUTPUT_INACTIVE);

    parar();
    return 0;
}
#endif

int main(void) {
    printk("\n=== Sistema nRF24L01 + Carrinho Inicializado ===\n");
    nrf24_init();

#if defined(ROLE_TX)
    console_init();
    printk("[Perfil: TRANSMISSOR]\n");
    printk("Digite '1' no terminal para LIGAR o carrinho ou '0' para PARAR:\n");

    while (1) {
        uint8_t c = console_getchar();

        if (c == '1') {
            printk("Enviando '1' (ANDAR)...\n");
            nrf24_send_message("1");
        } else if (c == '0') {
            printk("Enviando '0' (PARAR)...\n");
            nrf24_send_message("0");
        }
    }

#elif defined(ROLE_RX)
    led_init();

    if (init_ponte_h() != 0) {
        printk("Erro ao inicializar a Ponte H!\n");
        return -1;
    }

    if (config_sensor() != 0) {
        printk("Erro ao configurar o sensor ultrassônico!\n");
        return -1;
    }

    printk("[Perfil: RECEPTOR - CARRINHO]\n");
    printk("Aguardando comandos RF e monitorando distância...\n");

    bool estado_andando = false;

    while (1) {
        // 1. Verificação de pacotes via rádio
        char *rx_msg = nrf24_read_message();

        if (strcmp(rx_msg, "failed") != 0) {
            printk("Pacote RF recebido: %s\n", rx_msg);

            if (rx_msg[0] == '1') {
                estado_andando = true;
                led_on();
                printk("Estado atualizado: ANDAR\n");
            } else if (rx_msg[0] == '0') {
                estado_andando = false;
                parar();
                led_off();
                printk("Estado atualizado: PARADO\n");
            }
        }

        // 2. Controle de movimentação e obstáculo
        if (estado_andando) {
            uint32_t ticks = sensor_read_distance();
            uint32_t distance = (ticks * 34300) / (2 * 6000000);

            if (ticks == 0) {
                parar(); // Sem leitura válida ainda
            } else if (distance > DISTANCIA_MINIMA_CM) {
                frente();
            } else {
                // Obstáculo detectado: Executa rotina de desvio
                printk("Obstaculo a %u cm! Desviando...\n", distance);
                parar();
                k_msleep(200);

                tras();
                k_msleep(TEMPO_RE_MS);

                parar();
                k_msleep(200);

                direita();
                k_msleep(TEMPO_VIRA_MS);

                parar();
            }
        }

        k_msleep(50);
    }

#else
    #error "Nenhum papel definido! Selecione [env:frdm_kl25z_tx] ou [env:frdm_kl25z_rx]."
#endif

    return 0;
}