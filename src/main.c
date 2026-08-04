#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

// Definição dos pinos
#define PIN_IN1 4   // PTD4 	- Motor Esquerdo Horário
#define PIN_IN2 12  // PTA12 	- Motor Esquerdo Anti-Horário
#define PIN_IN3 5   // PTA5 	- Motor Direito Horário
#define PIN_IN4 4   // PTA4 	- Motor Direito Anti-Horário

static const struct device *gpioa_dev;
static const struct device *gpiod_dev;

void frente(void)
{
    // Esquerdo Avança
    gpio_pin_set_raw(gpiod_dev, PIN_IN1, 1);
    gpio_pin_set_raw(gpioa_dev, PIN_IN2, 0);
    // Direito Avança
    gpio_pin_set_raw(gpioa_dev, PIN_IN3, 1);
    gpio_pin_set_raw(gpioa_dev, PIN_IN4, 0);
}

void tras(void)
{
    // Esquerdo Recua
    gpio_pin_set_raw(gpiod_dev, PIN_IN1, 0);
    gpio_pin_set_raw(gpioa_dev, PIN_IN2, 1);
    // Direito Recua
    gpio_pin_set_raw(gpioa_dev, PIN_IN3, 0);
    gpio_pin_set_raw(gpioa_dev, PIN_IN4, 1);
}

void esquerda(void)
{
    // Esquerdo Recua
    gpio_pin_set_raw(gpiod_dev, PIN_IN1, 0);
    gpio_pin_set_raw(gpioa_dev, PIN_IN2, 1);
    // Direito Avança
    gpio_pin_set_raw(gpioa_dev, PIN_IN3, 1);
    gpio_pin_set_raw(gpioa_dev, PIN_IN4, 0);
}

void direita(void)
{
    // Esquerdo Avança
    gpio_pin_set_raw(gpiod_dev, PIN_IN1, 1);
    gpio_pin_set_raw(gpioa_dev, PIN_IN2, 0);
    // Direito Recua
    gpio_pin_set_raw(gpioa_dev, PIN_IN3, 0);
    gpio_pin_set_raw(gpioa_dev, PIN_IN4, 1);
}

void parar(void)
{
	// Para os dois motores
    gpio_pin_set_raw(gpiod_dev, PIN_IN1, 0);
    gpio_pin_set_raw(gpioa_dev, PIN_IN2, 0);
    gpio_pin_set_raw(gpioa_dev, PIN_IN3, 0);
    gpio_pin_set_raw(gpioa_dev, PIN_IN4, 0);
}

int main(void)
{
	// Verifica se os periféricos estão prontos
    gpioa_dev = DEVICE_DT_GET(DT_NODELABEL(gpioa));
    gpiod_dev = DEVICE_DT_GET(DT_NODELABEL(gpiod));
    if (!device_is_ready(gpioa_dev) || !device_is_ready(gpiod_dev)) {
        printk("Erro: Controladores GPIO nao estao prontos!\n");
        return 0;
    }

    // Configura os pinos como saída
    gpio_pin_configure(gpiod_dev, PIN_IN1, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(gpioa_dev, PIN_IN2, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(gpioa_dev, PIN_IN3, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(gpioa_dev, PIN_IN4, GPIO_OUTPUT_INACTIVE);

	printk("Carrinho pronto! Movendo para FRENTE...\n");
    frente();

	// Loop infinito
    while (1) {
        k_msleep(1000);
    }

    return 0;
}