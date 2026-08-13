


/*
 * main.c
 *
 * Exemplo de uso do driver nrf24l01.c/.h + spi_bare.c/.h para:
 *  - Placa TRANSMISSORA: le um caractere do terminal (UART0, USB da placa)
 *    e envia por radio ('1' = liga LED remoto, '0' = desliga).
 *  - Placa RECEPTORA: recebe o byte e liga/desliga o LED onboard (PTB18,
 *    ativo em nivel baixo).
 *
 * Defina ROLE_TX ou ROLE_RX antes de compilar (ex.: nas Project Properties
 * do MCUXpresso/KDS, em Preprocessor -> Defined symbols), uma placa com
 * cada define.
 *
 * UART0 (console via OpenSDA/USB): PTA1 = RX, PTA2 = TX, 115200 8N1.
 */
#include "MKL25Z4.h"
#include "spi_bare.h"
#include "nrf24l01.h"

/* Mesmo endereco de radio (5 bytes) e canal nas DUAS placas */
static const uint8_t RADIO_ADDR[5] = {0xE7, 0xE7, 0xE7, 0xE7, 0xE7};
#define RADIO_CHANNEL   76u   /* 2.4GHz + 76 = 2.476GHz, longe do WiFi 2.4G comum */

/* LED onboard vermelho da FRDM-KL25Z: PTB18, ativo em nivel baixo */
#define LED_PORT   PTB
#define LED_PIN    18u

/* ---------------- UART0 bare metal (so' usado no papel TX) ---------------- */
#if defined(ROLE_TX)
static void uart0_init(void)
{
    /* Clock do UART0: MCGFLLCLK/2 (default apos reset) */
    SIM->SOPT2 |= SIM_SOPT2_UART0SRC(1);
    SIM->SCGC4 |= SIM_SCGC4_UART0_MASK;
    SIM->SCGC5 |= SIM_SCGC5_PORTA_MASK;

    PORTA->PCR[1] = PORT_PCR_MUX(2); /* PTA1 = UART0_RX */
    PORTA->PCR[2] = PORT_PCR_MUX(2); /* PTA2 = UART0_TX */

    UART0->C2 = 0;                   /* desabilita TX/RX p/ configurar */

    /* Baud rate 115200 assumindo clock de referencia de 24MHz (FEI default) */
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

/* ---------------- LED (so' usado no papel RX) ---------------- */
#if defined(ROLE_RX)
static void led_init(void)
{
    SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;
    PORTB->PCR[LED_PIN] = PORT_PCR_MUX(1); /* GPIO */
    LED_PORT->PDDR |= (1u << LED_PIN);
    LED_PORT->PSOR = (1u << LED_PIN);      /* comeca apagado (ativo em baixo) */
}

static void led_set(bool on)
{
    if (on) {
        LED_PORT->PCOR = (1u << LED_PIN); /* nivel baixo = aceso */
    } else {
        LED_PORT->PSOR = (1u << LED_PIN); /* nivel alto = apagado */
    }
}
#endif

int main(void)
{
    nrf24_init(RADIO_ADDR, RADIO_CHANNEL);

#if defined(ROLE_TX)
    uart0_init();
    uart0_puts("TX pronto. Digite '1' para ligar o LED remoto, '0' para desligar.\r\n");

    for (;;) {
        uint8_t c = uart0_getchar();
        if (c == '1' || c == '0') {
            uint8_t payload = c;
            bool ok = nrf24_transmit(&payload, 1);
            uart0_puts(ok ? "Enviado OK\r\n" : "Falha no envio\r\n");
        }
    }

#elif defined(ROLE_RX)
    led_init();
    nrf24_power_up_rx();

    for (;;) {
        if (nrf24_data_ready()) {
            uint8_t payload;
            nrf24_get_payload(&payload, 1);
            if (payload == '1') {
                led_set(true);
            } else if (payload == '0') {
                led_set(false);
            }
        }
    }

#else
#error "Defina ROLE_TX ou ROLE_RX antes de compilar."
#endif
}
