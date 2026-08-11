#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

const struct device *gpio_c;

/* PINOS DA PONTE H:
   0 - Esquerda frente 
   7 - Esquerda trás 

   3 - Direita frente 
   4 - Direita trás  */

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

int main(void)
{
    printk("Iniciando programa...\n");

    gpio_c = DEVICE_DT_GET(DT_NODELABEL(gpioc));
    if (gpio_c == NULL || !device_is_ready(gpio_c))
    {
        printk("Erro ao acessar GPIOC!\n");
        return -1;
    }

    gpio_pin_configure(gpio_c, 7, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(gpio_c, 0, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(gpio_c, 3, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(gpio_c, 4, GPIO_OUTPUT_INACTIVE);

    parar();

    while (1)
    {
        k_msleep(1000);
    }

    return 0;
}