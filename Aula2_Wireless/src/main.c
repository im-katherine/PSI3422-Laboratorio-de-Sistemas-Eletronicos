/*
 * main.c
 *
 * Código ajustado para utilizar a biblioteca nrf24l01 baseada em strings.
 * Defina ROLE_TX ou ROLE_RX antes de compilar.
 */
#include "MKL25Z4.h"
#include "spi.h"
#include "nrf24l01.h"
#include <string.h>
#include <stdbool.h>

/* LED onboard vermelho da FRDM-KL25Z: PTB18, ativo em nível baixo */
#define LED_PORT   PTB
#define LED_PIN    18u

/* ---------------- UART0 bare metal (usado no papel TX) ---------------- */
#if defined(ROLE_TX)
static void uart0_init(void)
{
    /* Clock do UART0: MCGFLLCLK/2 (default após reset) */
    SIM->SOPT2 |= SIM_SOPT2_UART0SRC(1);
    SIM->SCGC4 |= SIM_SCGC4_UART0_MASK;
    SIM->SCGC5 |= SIM_SCGC5_PORTA_MASK;

    PORTA->PCR[1] = PORT_PCR_MUX(2); /* PTA1 = UART0_RX */
    PORTA->PCR[2] = PORT_PCR_MUX(2); /* PTA2 = UART0_TX */

    UART0->C2 = 0;                   /* desabilita TX/RX para configurar */

    /* Baud rate 115200 assumindo clock de referência de 24MHz (FEI default) */
    uint16_t sbr = 24000000u / (16u * 115200u);
    UART0->BDH = (sbr >> 8) & 0x1F;
    UART0->BDL = sbr & 0xFF;

    UART0->C1 = 0;
    UART0->C4 = 0;

    UART0->C2 = UART0_C2_TE_MASK | UART0_C2_RE_MASK;
}

static uint8_t uart0_getchar(void)
{
    while (!(UART0->S1 & UART0_S1_RDRF_MASK)) { /* espera char */ }
    return UART0->D;
}

static void uart0_putchar(uint8_t c)
{
    while (!(UART0->S1 & UART0_S1_TDRE_MASK)) { /* espera buffer livre */ }
    UART0->D = c;
}

static void uart0_puts(const char *s)
{
    while (*s) { uart0_putchar((uint8_t)*s++); }
}
#endif

/* ---------------- LED (usado no papel RX) ---------------- */
#if defined(ROLE_RX)
static void led_init(void)
{
    SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;
    PORTB->PCR[LED_PIN] = PORT_PCR_MUX(1); /* GPIO */
    LED_PORT->PDDR |= (1u << LED_PIN);
    LED_PORT->PSOR = (1u << LED_PIN);      /* começa apagado (ativo em baixo) */
}

static void led_set(bool on)
{
    if (on) {
        LED_PORT->PCOR = (1u << LED_PIN); /* nível baixo = aceso */
    } else {
        LED_PORT->PSOR = (1u << LED_PIN); /* nível alto = apagado */
    }
}
#endif

int main(void)
{
    /* Inicializa o rádio (as configurações já estão dentro de nrf24_init) */
    nrf24_init();

#if defined(ROLE_TX)
    uart0_init();
    uart0_puts("TX pronto. Digite '1' para ligar o LED remoto, '0' para desligar.\r\n");

    for (;;) {
        uint8_t c = uart0_getchar();
        if (c == '1' || c == '0') {
            /* Transforma o caractere lido em uma string (terminada em \0) */
            char msg[2] = { (char)c, '\0' };
            
            /* Envia a string através da função nrf24_send_message */
            uint8_t status = nrf24_send_message(msg);
            
            if (status == 1) {
                uart0_puts("Enviado OK\r\n");
            } else {
                uart0_puts("Falha no envio\r\n");
            }
        }
    }

#elif defined(ROLE_RX)
    led_init();

    for (;;) {
        /* Chama a função de leitura; a biblioteca já gerencia os modos do NRF */
        char *msg = nrf24_read_message();
        
        /* A função retorna "failed" se não houver mensagens ou se falhar */
        if (msg != NULL && msg[0] != 'f') { 
            if (msg[0] == '1') {
                led_set(true);
            } else if (msg[0] == '0') {
                led_set(false);
            }
        }
    }

#else
#error "Defina ROLE_TX ou ROLE_RX antes de compilar."
#endif
}