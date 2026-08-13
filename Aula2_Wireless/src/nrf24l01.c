/*
 * nrf24l01.c
 * Ver nrf24l01.h para detalhes.
 */
#include "nrf24l01.h"
#include "spi_bare.h"

/* ---------- helpers internos ---------- */

static void nrf_ce_low(void)  { NRF_CE_PORT->PCOR = (1u << NRF_CE_PIN); }
static void nrf_ce_high(void) { NRF_CE_PORT->PSOR = (1u << NRF_CE_PIN); }

static uint8_t nrf_read_reg(uint8_t reg)
{
    uint8_t val;
    nrf_csn_low();
    spi0_transfer(NRF_CMD_R_REGISTER | (reg & 0x1F));
    val = spi0_transfer(NRF_CMD_NOP);
    nrf_csn_high();
    return val;
}

static void nrf_write_reg(uint8_t reg, uint8_t val)
{
    nrf_csn_low();
    spi0_transfer(NRF_CMD_W_REGISTER | (reg & 0x1F));
    spi0_transfer(val);
    nrf_csn_high();
}

static void nrf_write_reg_multi(uint8_t reg, const uint8_t *buf, uint8_t len)
{
    uint8_t i;
    nrf_csn_low();
    spi0_transfer(NRF_CMD_W_REGISTER | (reg & 0x1F));
    for (i = 0; i < len; i++) {
        spi0_transfer(buf[i]);
    }
    nrf_csn_high();
}

static void nrf_flush_tx(void)
{
    nrf_csn_low();
    spi0_transfer(NRF_CMD_FLUSH_TX);
    nrf_csn_high();
}

static void nrf_flush_rx(void)
{
    nrf_csn_low();
    spi0_transfer(NRF_CMD_FLUSH_RX);
    nrf_csn_high();
}

/* pequeno atraso ocupado, usado nos tempos de power-up/CE do datasheet */
static void nrf_delay(volatile uint32_t n)
{
    while (n--) { __asm("nop"); }
}

/* ---------- API publica ---------- */

void nrf24_init(const uint8_t addr[5], uint8_t channel)
{
    spi0_init();

    /* CE como GPIO de saida, iniciando em baixo (standby)
     * (clock da PORTD ja foi ligado em spi0_init(), mas garantimos aqui
     * tambem caso a ordem de chamada mude) */
    SIM->SCGC5 |= SIM_SCGC5_PORTD_MASK;
    PORTD->PCR[NRF_CE_PIN] = PORT_PCR_MUX(1);
    NRF_CE_PORT->PDDR |= (1u << NRF_CE_PIN);
    nrf_ce_low();

    nrf_delay(200000); /* >= 100ms de power-on reset do nRF24L01+ */

    /* Auto-ACK desligado em todos os pipes (simplifica o protocolo) */
    nrf_write_reg(NRF_REG_EN_AA, 0x00);

    /* Habilita somente o pipe 0 de RX */
    nrf_write_reg(NRF_REG_EN_RXADDR, 0x01);

    /* Endereco de 5 bytes */
    nrf_write_reg(NRF_REG_SETUP_AW, 0x03);

    /* Sem retransmissao automatica (Auto-ACK ja esta desligado) */
    nrf_write_reg(NRF_REG_SETUP_RETR, 0x00);

    /* Canal RF (0-125), mesmo canal nas duas placas */
    nrf_write_reg(NRF_REG_RF_CH, channel);

    /* 1 Mbps, potencia maxima (0dBm) */
    nrf_write_reg(NRF_REG_RF_SETUP, 0x06);

    /* Mesmo endereco usado para RX (pipe 0) e TX nas duas placas */
    nrf_write_reg_multi(NRF_REG_RX_ADDR_P0, addr, 5);
    nrf_write_reg_multi(NRF_REG_TX_ADDR,    addr, 5);

    /* Tamanho fixo do payload do pipe 0 */
    nrf_write_reg(NRF_REG_RX_PW_P0, NRF_PAYLOAD_WIDTH);

    nrf_flush_tx();
    nrf_flush_rx();

    /* Limpa flags pendentes de STATUS (escreve 1 para limpar) */
    nrf_write_reg(NRF_REG_STATUS, NRF_STATUS_RX_DR | NRF_STATUS_TX_DS | NRF_STATUS_MAX_RT);

    /* CONFIG: liga o chip (PWR_UP), CRC 1 byte habilitado, comeca em modo TX (PRIM_RX=0) */
    nrf_write_reg(NRF_REG_CONFIG, 0x0A); /* EN_CRC=1, PWR_UP=1, PRIM_RX=0 */

    nrf_delay(300000); /* >= 1.5ms Power Down -> Standby-I */
}

void nrf24_power_up_rx(void)
{
    nrf_ce_low();
    uint8_t cfg = nrf_read_reg(NRF_REG_CONFIG);
    cfg |= 0x01;              /* PRIM_RX = 1 */
    nrf_write_reg(NRF_REG_CONFIG, cfg);
    nrf_write_reg(NRF_REG_STATUS, NRF_STATUS_RX_DR | NRF_STATUS_TX_DS | NRF_STATUS_MAX_RT);
    nrf_flush_rx();
    nrf_ce_high();   /* CE alto -> entra em modo Receiving */
    nrf_delay(300);
}

void nrf24_power_up_tx(void)
{
    nrf_ce_low();
    uint8_t cfg = nrf_read_reg(NRF_REG_CONFIG);
    cfg &= ~0x01;             /* PRIM_RX = 0 */
    nrf_write_reg(NRF_REG_CONFIG, cfg);
    nrf_delay(300);
}

bool nrf24_transmit(const uint8_t *buf, uint8_t len)
{
    uint8_t status;
    uint8_t i;

    nrf24_power_up_tx();
    nrf_flush_tx();

    nrf_csn_low();
    spi0_transfer(NRF_CMD_W_TX_PAYLOAD);
    for (i = 0; i < len; i++) {
        spi0_transfer(buf[i]);
    }
    nrf_csn_high();

    /* pulso de CE >= 10us dispara a transmissao */
    nrf_ce_high();
    nrf_delay(50);
    nrf_ce_low();

    /* espera TX_DS (enviado) ou MAX_RT (falhou, nao deveria ocorrer sem Auto-ACK) */
    do {
        status = nrf_read_reg(NRF_REG_STATUS);
    } while (!(status & (NRF_STATUS_TX_DS | NRF_STATUS_MAX_RT)));

    nrf_write_reg(NRF_REG_STATUS, NRF_STATUS_TX_DS | NRF_STATUS_MAX_RT);

    return (status & NRF_STATUS_TX_DS) != 0;
}

bool nrf24_data_ready(void)
{
    uint8_t fifo = nrf_read_reg(NRF_REG_FIFO_STATUS);
    return (fifo & 0x01) == 0; /* bit0 = RX_EMPTY; 0 = tem dado */
}

void nrf24_get_payload(uint8_t *buf, uint8_t len)
{
    uint8_t i;
    nrf_csn_low();
    spi0_transfer(NRF_CMD_R_RX_PAYLOAD);
    for (i = 0; i < len; i++) {
        buf[i] = spi0_transfer(NRF_CMD_NOP);
    }
    nrf_csn_high();

    nrf_write_reg(NRF_REG_STATUS, NRF_STATUS_RX_DR);
}
