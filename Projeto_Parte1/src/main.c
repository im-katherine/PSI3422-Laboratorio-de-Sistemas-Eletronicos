#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/console/console.h>
#include <soc.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "nrf24l01.h"
#include "spi.h"
#include "ultrasound.h"

#if defined(ROLE_RX)
const struct device *gpio_a;
const struct device *gpio_c;
const struct device *gpio_d;

#define PIN_ENCODER_ESQ 2
#define PIN_ENCODER_DIR 3

#define WHEEL_DIAMETER_M    0.065f
#define PULSES_PER_REV      16
#define FATOR_CORRECAO      0.8f
#define PI_VAL              3.14159265f
#define WHEEL_CIRCUMFERENCE (PI_VAL * WHEEL_DIAMETER_M)

static volatile uint32_t pulses_left = 0;
static volatile uint32_t pulses_right = 0;
static float total_distance_m = 0.0f;

static struct gpio_callback cb_encoder_left;
static struct gpio_callback cb_encoder_right;

void encoder_left_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins) { pulses_left++; }
void encoder_right_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins) { pulses_right++; }

void resetar_encoders(void) {
    pulses_left = 0;
    pulses_right = 0;
}

void acumular_distancia(void) {
    uint32_t avg_pulses = (pulses_left + pulses_right) / 2;
    float dist_trecho = ((float)avg_pulses / PULSES_PER_REV) * WHEEL_CIRCUMFERENCE * FATOR_CORRECAO;
    total_distance_m += dist_trecho;
    resetar_encoders();
}

float obter_distancia_total(void) {
    uint32_t avg_pulses = (pulses_left + pulses_right) / 2;
    float dist_trecho = ((float)avg_pulses / PULSES_PER_REV) * WHEEL_CIRCUMFERENCE * FATOR_CORRECAO;
    return total_distance_m + dist_trecho;
}

void parar(void) {
    gpio_pin_set(gpio_d, 4, 0); gpio_pin_set(gpio_a, 4, 0);
    gpio_pin_set(gpio_c, 9, 0); gpio_pin_set(gpio_d, 1, 0);
}

void frente(void) {
    gpio_pin_set(gpio_d, 4, 0); gpio_pin_set(gpio_a, 4, 1);
    gpio_pin_set(gpio_c, 9, 1); gpio_pin_set(gpio_d, 1, 0);
}

void tras(void) {
    gpio_pin_set(gpio_d, 4, 1); gpio_pin_set(gpio_a, 4, 0);
    gpio_pin_set(gpio_c, 9, 0); gpio_pin_set(gpio_d, 1, 1);
}

void direita(void) {
    gpio_pin_set(gpio_d, 4, 0); gpio_pin_set(gpio_a, 4, 1);
    gpio_pin_set(gpio_c, 9, 0); gpio_pin_set(gpio_d, 1, 1);
}

#define DISTANCIA_MINIMA_CM 15
#define TEMPO_RE_MS         400
#define TEMPO_VIRA_MS       500
#define LED_GREEN_PIN       19u

static void led_init(void) {
    SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;
    PORTB->PCR[LED_GREEN_PIN] = PORT_PCR_MUX(1);
    PTB->PDDR |= (1u << LED_GREEN_PIN);
    PTB->PSOR = (1u << LED_GREEN_PIN);
}
static void led_on(void) { PTB->PCOR = (1u << LED_GREEN_PIN); }
static void led_off(void) { PTB->PSOR = (1u << LED_GREEN_PIN); }

static int init_hardware_rx(void) {
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

    gpio_pin_configure(gpio_d, PIN_ENCODER_ESQ, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_interrupt_configure(gpio_d, PIN_ENCODER_ESQ, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&cb_encoder_left, encoder_left_isr, BIT(PIN_ENCODER_ESQ));
    gpio_add_callback(gpio_d, &cb_encoder_left);

    gpio_pin_configure(gpio_d, PIN_ENCODER_DIR, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_interrupt_configure(gpio_d, PIN_ENCODER_DIR, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&cb_encoder_right, encoder_right_isr, BIT(PIN_ENCODER_DIR));
    gpio_add_callback(gpio_d, &cb_encoder_right);

    resetar_encoders();
    parar();
    return 0;
}
#endif

int main(void) {
    printk("\n========================================\n");
    printk("=== Sistema nRF24L01 + Carrinho + Encoders ===\n");
    printk("========================================\n");
    nrf24_init();

#if defined(ROLE_TX)
    console_init();
    printk("[Perfil: TRANSMISSOR]\n");
    printk("Comandos: '0'=PARAR, '1'=ANDAR, '2'=DISTANCIA, '3'=ZERAR\n");

    while (1) {
        uint8_t c = console_getchar();

        if (c == '0') {
            printk("[TX] Enviando '0' (PARAR)...\n");
            nrf24_send_message("0");
        } else if (c == '1') {
            printk("[TX] Enviando '1' (ANDAR)...\n");
            nrf24_send_message("1");
        } else if (c == '2') {
            printk("[TX] Enviando '2' (PEDIR DISTANCIA)...\n");
            nrf24_send_message("2");

            bool recebeu_resposta = false;
            for (int i = 0; i < 20; i++) {
                k_msleep(50);
                char *resp = nrf24_read_message();

                if (strcmp(resp, "failed") != 0) {
                    printk("\n========================================\n");
                    printk("[TX RECEBIDO] Distancia do Carrinho: %s cm\n", resp);
                    printk("========================================\n\n");
                    recebeu_resposta = true;
                    break;
                }
            }

            if (!recebeu_resposta) {
                printk("[TX ERRO] Tempo esgotado! Sem resposta do receptor.\n");
            }
        } else if (c == '3') {
            printk("[TX] Enviando '3' (ZERAR DISTANCIA)...\n");
            nrf24_send_message("3");
        } else if (c != '\n' && c != '\r') {
            printk("[TX] Tecla desconsiderada: '%c'\n", c);
        }
    }

#elif defined(ROLE_RX)
    led_init();
    if (init_hardware_rx() != 0 || config_sensor() != 0) {
        printk("[RX ERRO] Falha de inicializacao de hardware!\n");
        return -1;
    }

    printk("[Perfil: RECEPTOR - CARRINHO]\n");
    bool estado_andando = false;

    while (1) {
        // 1. Processamento de comandos do Radio
        char *rx_msg = nrf24_read_message();

        if (strcmp(rx_msg, "failed") != 0) {
            printk("[RX DEBUG] Pacote RF recebido: '%c'\n", rx_msg[0]);

            if (rx_msg[0] == '1') {
                resetar_encoders();
                estado_andando = true;
                led_on();
                printk("[RX STATE] Estado: ANDAR\n");
            } else if (rx_msg[0] == '0') {
                acumular_distancia();
                estado_andando = false;
                parar();
                led_off();
                printk("[RX STATE] Estado: PARADO\n");
            } else if (rx_msg[0] == '2') {
                float dist = obter_distancia_total();
                int centimetros_totais = (int)(dist * 100.0f);

                printk("[RX DISTANCIA CALCULADA] %d cm\n", centimetros_totais);

                char buffer_envio[16];
                snprintf(buffer_envio, sizeof(buffer_envio), "%d", centimetros_totais);

                k_msleep(20);
                nrf24_send_message(buffer_envio);
                printk("[RX] Resposta enviada ao TX: %s cm\n", buffer_envio);
            } else if (rx_msg[0] == '3') {
                total_distance_m = 0.0f;
                resetar_encoders();
                printk("[RX STATE] Distancia ZERADA\n");
            }
        }

        // 2. Navegacao com tratamento de estouro e monitoramento
        if (estado_andando) {
            uint32_t ticks = sensor_read_distance();

            if (ticks == 0) {
                printk("[RX WARN] Leitura zerada (Timeout/Falha no sensor HC-SR04)!\n");
                parar();
            } else {
                uint32_t distance = (uint32_t)(((uint64_t)ticks * 343) / 120000);

                if (distance > DISTANCIA_MINIMA_CM) {
                    frente();
                } else {
                    acumular_distancia();
                    printk("[RX NAV] Obstaculo a %u cm! Executando manobra...\n", distance);
                    parar();
                    k_msleep(200);
                    tras();
                    k_msleep(TEMPO_RE_MS);
                    parar();
                    k_msleep(200);
                    direita();
                    k_msleep(TEMPO_VIRA_MS);
                    parar();
                    k_msleep(200);
                    resetar_encoders();
                }
            }
        }

        k_msleep(50);
    }
#endif

    return 0;
}