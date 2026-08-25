#include "nrf24l01.h"
#include "spi.h"
#include <string.h>

// Definições de pinos (Porta E)
#define CE_PIN   30u // PTE30
#define CSN_PIN  4u  // PTE4
#define IRQ_PIN  20u // PTE20 (Alterado de PTE5)

// Endereço e configurações de rádio
static uint8_t rx_address[5] = { 0x37, 0xa7, 0xe0, 0xb3, 0x97 };
static uint8_t tx_address[5] = { 0x37, 0xa7, 0xe0, 0xb3, 0x97 };
#define READ_PIPE       0

#define AUTO_ACK        true
#define DATARATE        RF_DR_1MBPS
#define POWER           POWER_MAX
#define CHANNEL         0x6D
#define DYN_PAYLOAD     true
#define CONTINUOUS      false

#define IRQ_RX_DR       true
#define IRQ_TX_DS       false
#define IRQ_MAX_RT      false

static uint8_t send_to_spi;

/* Delays simples Bare Metal */
static void delay_us(uint32_t us) {
    uint32_t count = us * 4;
    while (count--) {
        __asm("nop");
    }
}

static void delay_ms(uint32_t ms) {
    while (ms--) {
        delay_us(1000);
    }
}

/* Funções de controle de pinos via registrador do Kinetis (PTE) */
void ce_low(void) {
    PTE->PCOR = (1u << CE_PIN);
}

void ce_high(void) {
    PTE->PSOR = (1u << CE_PIN);
}

void csn_low(void) {
    PTE->PCOR = (1u << CSN_PIN);
}

void csn_high(void) {
    PTE->PSOR = (1u << CSN_PIN);
}

void nrf24_clear_irq_flags(void) {
    send_to_spi = (1 << RX_DR) | (1 << TX_DS) | (1 << MAX_RT);
    nrf24_write(STATUS, &send_to_spi, 1);
}

void nrf24_init_gpio(void) {
    printk("[1/3] Habilitando clock do PORTE...\n");
    SIM->SCGC5 |= SIM_SCGC5_PORTE_MASK;

    printk("[2/3] Configurando MUX dos pinos como GPIO...\n");
    PORTE->PCR[CE_PIN]  = PORT_PCR_MUX(1);
    PORTE->PCR[CSN_PIN] = PORT_PCR_MUX(1);
    PORTE->PCR[IRQ_PIN] = PORT_PCR_MUX(1);

    printk("[3/3] Configurando direcoes de entrada/saida...\n");
    PTE->PDDR |= (1u << CE_PIN) | (1u << CSN_PIN); // Saídas
    PTE->PDDR &= ~(1u << IRQ_PIN);                 // Entrada
    
    // Estado inicial dos pinos de controle
    PTE->PSOR = (1u << CSN_PIN); // CSN High (Desabilitado)
    PTE->PCOR = (1u << CE_PIN);  // CE Low (Standby)
}

void nrf24_init(void) {
    nrf24_init_gpio();

    /* Inicializa a biblioteca SPI1 (PTE1=MOSI, PTE2=SCK, PTE3=MISO) */
    spi_init(SPI_1, ALT_0, 0, 2, CS_MAN);
    
    delay_ms(100);

    /* Configura CONFIG */
    send_to_spi = (!(IRQ_RX_DR) << MASK_RX_DR) |
                  (!(IRQ_TX_DS) << MASK_TX_DS) |
                  (!(IRQ_MAX_RT) << MASK_MAX_RT) |
                  (1 << EN_CRC) |
                  (1 << CRC0) |
                  (1 << PWR_UP) |
                  (1 << PRIM_RX);
    nrf24_write(CONFIG, &send_to_spi, 1);

    /* Habilita Auto ACK */
    send_to_spi = (AUTO_ACK << ENAA_P5) | (AUTO_ACK << ENAA_P4) |
                  (AUTO_ACK << ENAA_P3) | (AUTO_ACK << ENAA_P2) |
                  (AUTO_ACK << ENAA_P1) | (AUTO_ACK << ENAA_P0);
    nrf24_write(EN_AA, &send_to_spi, 1);

    send_to_spi = 0x03; // Address width = 5 bytes
    nrf24_write(SETUP_AW, &send_to_spi, 1);

    send_to_spi = 0xfa; // Retransmissão automática
    nrf24_write(SETUP_RETR, &send_to_spi, 1);

    send_to_spi = CHANNEL;
    nrf24_write(RF_CH, &send_to_spi, 1);

    send_to_spi = (CONTINUOUS << CONT_WAVE) |
                  ((DATARATE >> RF_DR_HIGH) << RF_DR_HIGH) |
                  ((POWER >> RF_PWR) << RF_PWR);
    nrf24_write(RF_SETUP, &send_to_spi, 1);

    nrf24_clear_irq_flags();

    /* Dynamic Payload */
    send_to_spi = (DYN_PAYLOAD << DPL_P0) | (DYN_PAYLOAD << DPL_P1) |
                  (DYN_PAYLOAD << DPL_P2) | (DYN_PAYLOAD << DPL_P3) |
                  (DYN_PAYLOAD << DPL_P4) | (DYN_PAYLOAD << DPL_P5);
    nrf24_write(DYNPD, &send_to_spi, 1);

    send_to_spi = (DYN_PAYLOAD << EN_DPL) | (AUTO_ACK << EN_ACK_PAY) | (AUTO_ACK << EN_DYN_ACK);
    nrf24_write(FEATURE, &send_to_spi, 1);

    nrf24_send_spi(FLUSH_RX, 0, 0);
    nrf24_send_spi(FLUSH_TX, 0, 0);
    
    nrf24_write(RX_ADDR_P0 + READ_PIPE, rx_address, 5);
    nrf24_write(TX_ADDR, tx_address, 5);    
    send_to_spi = (1 << READ_PIPE) | 0x03;
    nrf24_write(EN_RXADDR, &send_to_spi, 1);
}

uint8_t nrf24_send_spi(uint8_t register_address, void *data, unsigned int bytes) {
    uint8_t status;
    csn_low();
    status = spi_exchange(SPI_1, register_address);
    for (unsigned int i = 0; i < bytes; i++) {
        ((uint8_t*)data)[i] = spi_exchange(SPI_1, ((uint8_t*)data)[i]);
    }
    csn_high();
    return status;
}

uint8_t nrf24_read(uint8_t register_address, uint8_t *data, unsigned int bytes) {
    return nrf24_send_spi(R_REGISTER | register_address, data, bytes);
}

uint8_t nrf24_write(uint8_t register_address, uint8_t *data, unsigned int bytes) {
    return nrf24_send_spi(W_REGISTER | register_address, data, bytes);
}

uint8_t nrf24_send_message(char *tx_message) {
    uint8_t status, fifo_status, config_register;
    uint8_t length = strlen(tx_message);

    nrf24_read(CONFIG, &config_register, 1);
    config_register &= ~(1 << PRIM_RX);
    nrf24_write(CONFIG, &config_register, 1);

    nrf24_send_spi(FLUSH_RX, 0, 0);
    nrf24_send_spi(FLUSH_TX, 0, 0);
    nrf24_clear_irq_flags();

    csn_low();
    if (AUTO_ACK) spi_send(SPI_1, W_TX_PAYLOAD);
    else spi_send(SPI_1, W_TX_PAYLOAD_NOACK);
    
    uint8_t i = 0;
    while (i <= length) {
        spi_send(SPI_1, tx_message[i]);
        i++;
    }
    spi_send(SPI_1, 0);
    csn_high();

    ce_high();
    delay_us(15);
    
    nrf24_read(STATUS, &status, 1);
    while (!(status & (1 << TX_DS)) && !(status & (1 << MAX_RT))) {
        nrf24_read(STATUS, &status, 1);
    }

    nrf24_read(FIFO_STATUS, &fifo_status, 1);

    if ((status & (1 << MAX_RT)) || (status & (TX_FULL))) {
        nrf24_send_spi(FLUSH_TX, 0, 0);
        nrf24_clear_irq_flags();
        ce_low();
        return 0;
    }

    nrf24_send_spi(FLUSH_TX, 0, 0);
    nrf24_clear_irq_flags();
    ce_low();
    
    return 1;
}

char * nrf24_read_message(void) {
    uint8_t config_register, width, status;
    static char rx_message[32];
    memset(rx_message, 0, 32);

    ce_low();
    nrf24_read(CONFIG, &config_register, 1);
    config_register |= (1 << PRIM_RX);
    nrf24_write(CONFIG, &config_register, 1);

    ce_high();
    delay_us(130);

    nrf24_read(STATUS, &status, 1);
    if (!(status & (1 << RX_DR))) {
        return "failed";
    }

    ce_low();

    nrf24_read(R_RX_PL_WID, &width, 1);

    if (width > 32) {
        nrf24_send_spi(FLUSH_RX, 0, 0);
        nrf24_clear_irq_flags();
        return "failed";
    }

    if (width > 0) {
        nrf24_send_spi(R_RX_PAYLOAD, &rx_message, width);
    }

    nrf24_send_spi(FLUSH_RX, 0, 0);
    nrf24_clear_irq_flags();

    if (strlen(rx_message) > 0) return rx_message;
    return "failed";
}