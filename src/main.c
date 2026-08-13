#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#include "ultrasound.h"

const struct device *gpio_c;

/*
 * PINOS DA PONTE H:
 *
 * 0 - Esquerda frente
 * 7 - Esquerda trás
 *
 * 3 - Direita frente
 * 4 - Direita trás
 */

void parar(void)
{
    gpio_pin_set(gpio_c, 7, 0);
    gpio_pin_set(gpio_c, 0, 0);

    gpio_pin_set(gpio_c, 3, 0);
    gpio_pin_set(gpio_c, 4, 0);
}

void frente(void)
{
    gpio_pin_set(gpio_c, 7, 0);
    gpio_pin_set(gpio_c, 0, 1);

    gpio_pin_set(gpio_c, 3, 1);
    gpio_pin_set(gpio_c, 4, 0);
}

void tras(void)
{
    gpio_pin_set(gpio_c, 7, 1);
    gpio_pin_set(gpio_c, 0, 0);

    gpio_pin_set(gpio_c, 3, 0);
    gpio_pin_set(gpio_c, 4, 1);
}

void direita(void)
{
    gpio_pin_set(gpio_c, 7, 0);
    gpio_pin_set(gpio_c, 0, 1);

    gpio_pin_set(gpio_c, 3, 0);
    gpio_pin_set(gpio_c, 4, 1);
}

void esquerda(void)
{
    gpio_pin_set(gpio_c, 7, 0);
    gpio_pin_set(gpio_c, 0, 1);

    gpio_pin_set(gpio_c, 3, 1);
    gpio_pin_set(gpio_c, 4, 0);
}

/*
 * Distância mínima (em cm) para considerar que há um obstáculo
 * à frente e iniciar a manobra de desvio.
 */
#define DISTANCIA_MINIMA_CM 15

/*
 * Tempo (em ms) que o carrinho anda de ré antes de virar,
 * e tempo que fica virando antes de seguir em frente de novo.
 */
#define TEMPO_RE_MS      400
#define TEMPO_VIRA_MS    500

int main(void)
{
    printk("Iniciando programa...\n");

    gpio_c = DEVICE_DT_GET(DT_NODELABEL(gpioc));

    if (!device_is_ready(gpio_c))
    {
        printk("Erro ao acessar GPIOC!\n");
        return -1;
    }

    gpio_pin_configure(gpio_c, 7, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(gpio_c, 0, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(gpio_c, 3, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(gpio_c, 4, GPIO_OUTPUT_INACTIVE);

    parar();

    if (config_sensor() != 0)
    {
        printk("Erro ao configurar sensor!\n");
        return -1;
    }

    while (1)
    {
        uint32_t ticks = sensor_read_distance();

        /*
         * TPM1:
         *
         * Clock = 48 MHz
         * Prescaler = 8
         *
         * Clock do contador = 48 MHz / 8
         *                  = 6 MHz
         *
         * 1 tick = 1/6 MHz
         */
        uint32_t distance =
            (ticks * 34300) / (2 * 6000000);

        printk(
            "Ticks: %u | Distancia: %u cm | ISR count: %u\n",
            ticks,
            distance,
            isr_count
        );

        /*
         * Se ainda não temos leitura válida (ticks == 0), fica parado
         * esperando a primeira medição chegar, pra não sair andando
         * "às cegas".
         */
        if (ticks == 0)
        {
            parar();
        }
        /*
         * Caminho livre: anda pra frente.
         */
        else if (distance > DISTANCIA_MINIMA_CM)
        {
            frente();
        }
        /*
         * Obstáculo detectado: para, dá ré, vira, e volta
         * a andar pra frente no próximo laço.
         */
        else
        {
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

        k_msleep(100);
    }

    return 0;
}