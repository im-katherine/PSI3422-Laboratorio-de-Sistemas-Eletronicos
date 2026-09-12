#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

/* Dispositivos de GPIO da FRDM-KL25Z */
const struct device *gpio_a;
const struct device *gpio_c;
const struct device *gpio_d;

/* PINOS DA PONTE H:
 * Motor esquerdo: IN1=PTD4, IN2=PTA4
 * Motor direito : IN3=PTC9, IN4=PTD1
 */

/* PINOS DOS ENCODERS (Módulo HW-201):
 * Encoder Esquerdo: PTD2
 * Encoder Direito : PTD3
 */
#define PIN_ENCODER_ESQ 2
#define PIN_ENCODER_DIR 3

/* Parâmetros físicos do sistema */
#define WHEEL_DIAMETER_M    0.065f       // 6.5 cm de diâmetro
#define PULSES_PER_REV      16           // Número de transições por volta
#define PI_VAL              3.14159265f
#define WHEEL_CIRCUMFERENCE (PI_VAL * WHEEL_DIAMETER_M)

/* Contadores voláteis incrementados pelas ISRs */
static volatile uint32_t pulses_left = 0;
static volatile uint32_t pulses_right = 0;

static struct gpio_callback cb_encoder_left;
static struct gpio_callback cb_encoder_right;

/* Callback de interrupção - Roda Esquerda */
void encoder_left_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    pulses_left++;
}

/* Callback de interrupção - Roda Direita */
void encoder_right_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    pulses_right++;
}

/* Funções de movimentação dos motores */
void parar(void)
{
    gpio_pin_set(gpio_d, 4, 0);
    gpio_pin_set(gpio_a, 4, 0);

    gpio_pin_set(gpio_c, 9, 0);
    gpio_pin_set(gpio_d, 1, 0);
}

void frente(void)
{
    gpio_pin_set(gpio_d, 4, 0);
    gpio_pin_set(gpio_a, 4, 1);

    gpio_pin_set(gpio_c, 9, 1);
    gpio_pin_set(gpio_d, 1, 0);
}

void tras(void)
{
    gpio_pin_set(gpio_d, 4, 1);
    gpio_pin_set(gpio_a, 4, 0);

    gpio_pin_set(gpio_c, 9, 0);
    gpio_pin_set(gpio_d, 1, 1);
}

void direita(void)
{
    gpio_pin_set(gpio_d, 4, 0);
    gpio_pin_set(gpio_a, 4, 1);

    gpio_pin_set(gpio_c, 9, 0);
    gpio_pin_set(gpio_d, 1, 1);
}

void esquerda(void)
{
    gpio_pin_set(gpio_d, 4, 1);
    gpio_pin_set(gpio_a, 4, 0);

    gpio_pin_set(gpio_c, 9, 1);
    gpio_pin_set(gpio_d, 1, 0);
}

int main(void)
{
    printk("Iniciando programa com 2 Encoders...\n");

    // 1. Configuração dos barramentos GPIO
    gpio_c = DEVICE_DT_GET(DT_NODELABEL(gpioc));
    if (!device_is_ready(gpio_c)) {
        printk("Erro ao acessar GPIOC!\n");
        return -1;
    }

    gpio_a = DEVICE_DT_GET(DT_NODELABEL(gpioa));
    if (!device_is_ready(gpio_a)) {
        printk("Erro ao acessar GPIOA!\n");
        return -1;
    }

    gpio_d = DEVICE_DT_GET(DT_NODELABEL(gpiod));
    if (!device_is_ready(gpio_d)) {
        printk("Erro ao acessar GPIOD!\n");
        return -1;
    }

    // 2. Configuração dos pinos dos motores (Saídas)
    gpio_pin_configure(gpio_c, 9, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(gpio_a, 4, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(gpio_d, 1, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(gpio_d, 4, GPIO_OUTPUT_INACTIVE);

    parar();

    // 3. Configuração do Encoder Esquerdo (PTD2)
    gpio_pin_configure(gpio_d, PIN_ENCODER_ESQ, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_interrupt_configure(gpio_d, PIN_ENCODER_ESQ, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&cb_encoder_left, encoder_left_isr, BIT(PIN_ENCODER_ESQ));
    gpio_add_callback(gpio_d, &cb_encoder_left);

    // 4. Configuração do Encoder Direito (PTD3)
    gpio_pin_configure(gpio_d, PIN_ENCODER_DIR, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_interrupt_configure(gpio_d, PIN_ENCODER_DIR, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&cb_encoder_right, encoder_right_isr, BIT(PIN_ENCODER_DIR));
    gpio_add_callback(gpio_d, &cb_encoder_right);

    printk("Encoders configurados com sucesso nos pinos PTD2 (Esq) e PTD3 (Dir).\n");

    // Inicia movimento para frente
    frente();

    while (1) {
        k_msleep(500);

        // Leitura atômica dos valores
        uint32_t p_left = pulses_left;
        uint32_t p_right = pulses_right;

        // Cálculos de distância em metros
        float dist_left = ((float)p_left / PULSES_PER_REV) * WHEEL_CIRCUMFERENCE;
        float dist_right = ((float)p_right / PULSES_PER_REV) * WHEEL_CIRCUMFERENCE;
        float dist_media = (dist_left + dist_right) / 2.0f;

        printk("Pulsos E: %u | Pulsos D: %u | Dist E: %.3fm | Dist D: %.3fm | Média: %.3fm\n",
               p_left, p_right, (double)dist_left, (double)dist_right, (double)dist_media);
    }

    return 0;
}