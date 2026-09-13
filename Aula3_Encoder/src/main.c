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
#define WHEEL_DIAMETER_M    0.065f       // 6.5 cm de diâmetro das rodas
#define AXLE_TRACK_M        0.160f       // 16 cm de eixo (distância entre rodas)
#define PULSES_PER_REV      16           // 16 pulsos por volta completa da roda
#define FATOR_CORRECAO      0.2f         // Fator de correção do cálculo de distância
#define PI_VAL              3.14159265f
#define WHEEL_CIRCUMFERENCE (PI_VAL * WHEEL_DIAMETER_M)

/* Pulsos necessários para Curva Pivô de 90° */
#define PULSES_90_DEG_PIVOT 20

/* Contadores voláteis incrementados pelas ISRs */
static volatile uint32_t pulses_left = 0;
static volatile uint32_t pulses_right = 0;

/* Acumulador global de distância retilínea percorrida (em metros) */
static float total_distance_m = 0.0f;

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

/* Zera os contadores sem somar na distância total */
void resetar_encoders(void)
{
    pulses_left = 0;
    pulses_right = 0;
}

/* Acumula a distância apenas dos trechos em linha reta */
void acumular_distancia(void)
{
    uint32_t avg_pulses = (pulses_left + pulses_right) / 2;
    float dist_trecho = ((float)avg_pulses / PULSES_PER_REV) * WHEEL_CIRCUMFERENCE * FATOR_CORRECAO;
    total_distance_m += dist_trecho;

    resetar_encoders();
}

/* Funções básicas de controle da ponte H */
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

/* Pivô para Direita: Roda Direita PARADA | Roda Esquerda AVANÇA */
void pivo_direita(void)
{
    // Motor Esquerdo FRENTE
    gpio_pin_set(gpio_d, 4, 0);
    gpio_pin_set(gpio_a, 4, 1);

    // Motor Direito PARADO
    gpio_pin_set(gpio_c, 9, 0);
    gpio_pin_set(gpio_d, 1, 0);
}

/* Pivô para Esquerda: Roda Esquerda PARADA | Roda Direita AVANÇA */
void pivo_esquerda(void)
{
    // Motor Esquerdo PARADO
    gpio_pin_set(gpio_d, 4, 0);
    gpio_pin_set(gpio_a, 4, 0);

    // Motor Direito FRENTE
    gpio_pin_set(gpio_c, 9, 1);
    gpio_pin_set(gpio_d, 1, 0);
}

/* ==========================================================
 * FUNÇÕES DE MOVIMENTAÇÃO E MANOBRA
 * ========================================================== */

/* Movimenta o carrinho para frente por tempo (ms), exibindo os pulsos em tempo real */
void andar_por_tempo_ms(uint32_t tempo_ms)
{
    resetar_encoders();

    printk(">>> Andando para frente por %u ms...\n", tempo_ms);
    frente();

    uint32_t tempo_decorrido = 0;
    const uint32_t passo_ms = 100; // Intervalo de impressão (100 ms)

    while (tempo_decorrido < tempo_ms) {
        k_msleep(passo_ms);
        tempo_decorrido += passo_ms;

        // Imprime os pulsos de cada roda em tempo real
        printk("[Em movimento] Pulsos -> Esq: %u | Dir: %u\n", pulses_left, pulses_right);
    }

    parar();
    k_msleep(300); // Pausa de estabilização

    printk(">>> Parado! Total de pulsos no trecho -> Esq: %u | Dir: %u\n", pulses_left, pulses_right);

    acumular_distancia(); // Soma a distância do trecho reto
    printk("-> Distancia acumulada (linha reta): %.3f m (%.1f cm)\n",
           (double)total_distance_m, (double)(total_distance_m * 100.0f));
}

/* Faz curva de 90° para a DIREITA pivoteando sobre a roda direita */
void virar_direita_pivo_90(void)
{
    resetar_encoders();

    printk(">>> Curva PIVÔ 90 deg DIREITA (Alvo: %u pulsos na roda ESQ)...\n", PULSES_90_DEG_PIVOT);
    pivo_direita();

    // Acompanha e imprime apenas os pulsos da roda externa (esquerda)
    while (pulses_left < PULSES_90_DEG_PIVOT) {
        printk("[Giro Direita] Pulsos Roda Esq: %u / %u\n", pulses_left, PULSES_90_DEG_PIVOT);
        k_msleep(50);
    }

    parar();
    k_msleep(300);
    printk(">>> Curva concluida! Pulsos E: %u | D: %u\n", pulses_left, pulses_right);

    resetar_encoders(); // Descarta pulsos de rotação
}

/* Faz curva de 90° para a ESQUERDA pivoteando sobre a roda esquerda */
void virar_esquerda_pivo_90(void)
{
    resetar_encoders();

    printk(">>> Curva PIVÔ 90 deg ESQUERDA (Alvo: %u pulsos na roda DIR)...\n", PULSES_90_DEG_PIVOT);
    pivo_esquerda();

    // Acompanha e imprime apenas os pulsos da roda externa (direita)
    while (pulses_right < PULSES_90_DEG_PIVOT) {
        printk("[Giro Esquerda] Pulsos Roda Dir: %u / %u\n", pulses_right, PULSES_90_DEG_PIVOT);
        k_msleep(50);
    }

    parar();
    k_msleep(300);
    printk(">>> Curva concluida! Pulsos E: %u | D: %u\n", pulses_left, pulses_right);

    resetar_encoders(); // Descarta pulsos de rotação
}

/* ==========================================================
 * MAIN
 * ========================================================== */
int main(void)
{
    printk("Iniciando programa com Encoders e Monitoramento de Pulsos...\n");

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

    printk("Encoders configurados nos pinos PTD2 (Esq) e PTD3 (Dir).\n");

    k_msleep(2000); // Aguarda 2 segundos antes de iniciar o percurso

    /* EXECUTAR PERCURSO */
    // 1. Anda retilíneo por 2 segundos (2000 ms) imprimindo os pulsos em tempo real
    andar_por_tempo_ms(2000);
    k_msleep(2000);

    // 2. Curva Pivô Direita 90°
    virar_direita_pivo_90();
    k_msleep(2000);

    // 3. Curva Pivô Esquerda 90°
    virar_esquerda_pivo_90();
    k_msleep(2000);

    /* IMPRESSÃO FINAL DA DISTÂNCIA TOTAL ACUMULADA */
    printk("\n========================================\n");
    printk("PERCURSO FINALIZADO!\n");
    printk("Distancia acumulada (Apenas Linha Reta): %.3f metros (%.1f cm)\n",
           (double)total_distance_m, (double)(total_distance_m * 100.0f));
    printk("========================================\n\n");

    // Laço final
    while (1) {
        k_msleep(1000);
        printk("Status: Parado | Distancia Linha Reta: %.2fm\n", (double)total_distance_m);
    }

    return 0;
}