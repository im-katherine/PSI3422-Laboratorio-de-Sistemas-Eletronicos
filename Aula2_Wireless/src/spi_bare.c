/*
 * spi_bare.c
 * Driver SPI0 bare metal, ver spi_bare.h para detalhes.
 */
#include "spi_bare.h"

void spi0_init(void)
{
    /* 1. Clocks: PORTC (SCK) + PORTD (MOSI/MISO/CSN) + SPI0 (modulo) */
    SIM->SCGC5 |= SIM_SCGC5_PORTC_MASK | SIM_SCGC5_PORTD_MASK;
    SIM->SCGC4 |= SIM_SCGC4_SPI0_MASK;

    /* 2. Mux dos pinos de SPI (ALT2 no KL25Z) */
    PORTC->PCR[5] = PORT_PCR_MUX(2);   /* PTC5 = SCK  */
    PORTD->PCR[2] = PORT_PCR_MUX(2);   /* PTD2 = MOSI */
    PORTD->PCR[3] = PORT_PCR_MUX(2);   /* PTD3 = MISO */

    /* 3. CSN como GPIO comum (NAO SPI0_PCS0!) */
    PORTD->PCR[NRF_CSN_PIN] = PORT_PCR_MUX(1);      /* GPIO */
    NRF_CSN_PORT->PDDR     |= (1u << NRF_CSN_PIN);  /* saida */
    nrf_csn_high();                                 /* idle = alto */

    /* 4. Configuracao do modulo SPI0
     *    - Master mode
     *    - SEM SSOE (nao usamos SS automatico de hardware)
     *    - CPOL=0, CPHA=0 (modo 0, o que o nRF24L01+ espera)
     */
    SPI0->C1 = SPI_C1_MSTR_MASK;   /* master, modo 0,0, sem SSOE */
    SPI0->C2 = 0x00;               /* 8 bits, sem MODFEN (SS nao usado) */

    /* Baud rate: SPIbaseClock / ((SPPR+1) * 2^(SPR+1))
     * Prescale=0, Divisor=2^(3+1)=16 -> ~1.3 MHz com bus clock de ~21MHz.
     * O nRF24L01+ aceita ate 10 MHz, entao qualquer valor conservador serve. */
    SPI0->BR = SPI_BR_SPPR(0) | SPI_BR_SPR(3);

    /* 5. Habilita o modulo */
    SPI0->C1 |= SPI_C1_SPE_MASK;
}

uint8_t spi0_transfer(uint8_t data)
{
    while (!(SPI0->S & SPI_S_SPTEF_MASK)) { /* espera buffer TX livre */ }
    SPI0->D = data;

    while (!(SPI0->S & SPI_S_SPRF_MASK)) { /* espera byte recebido */ }
    return SPI0->D;
}

void nrf_csn_low(void)
{
    NRF_CSN_PORT->PCOR = (1u << NRF_CSN_PIN);
}

void nrf_csn_high(void)
{
    NRF_CSN_PORT->PSOR = (1u << NRF_CSN_PIN);
}
