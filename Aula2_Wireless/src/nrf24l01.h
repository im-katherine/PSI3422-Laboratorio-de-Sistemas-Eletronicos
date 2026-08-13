/*
 * nrf24l01.h
 *
 * Driver minimo bare metal para o transceiver nRF24L01+ sobre SPI0
 * (FRDM-KL25Z). Auto-ACK desabilitado para simplificar o roteiro
 * (comunicacao "fire and forget", suficiente para o exercicio).
 *
 * Pinagem (conforme slide "Conexao do nRF24L01 com a FRDM KL25Z"):
 *   CE  -> PTD5 (GPIO manual, ver defines abaixo)
 *   CSN -> PTD0 (definido em spi_bare.h)
 *   IRQ -> PTA13 (NAO usado nesta versao -- driver opera por polling do
 *          registrador STATUS/FIFO_STATUS. Se quiser usar interrupcao,
 *          configure PTA13 como GPIO de entrada com interrupt on falling
 *          edge e chame nrf24_data_ready()/nrf24_transmit() dentro do
 *          handler da PORTA_IRQn).
 */

#ifndef NRF24L01_H_
#define NRF24L01_H_

#include "MKL25Z4.h"
#include <stdint.h>
#include <stdbool.h>

/* ---- Pino CE ---- */
#define NRF_CE_PORT   PTD
#define NRF_CE_PIN    5u

/* ---- Pino IRQ (opcional, nao usado no modo polling atual) ---- */
#define NRF_IRQ_PORT  PTA
#define NRF_IRQ_PIN   13u

/* ---- Registradores do nRF24L01+ ---- */
#define NRF_REG_CONFIG      0x00
#define NRF_REG_EN_AA       0x01
#define NRF_REG_EN_RXADDR   0x02
#define NRF_REG_SETUP_AW    0x03
#define NRF_REG_SETUP_RETR  0x04
#define NRF_REG_RF_CH       0x05
#define NRF_REG_RF_SETUP    0x06
#define NRF_REG_STATUS      0x07
#define NRF_REG_RX_ADDR_P0  0x0A
#define NRF_REG_TX_ADDR     0x10
#define NRF_REG_RX_PW_P0    0x11
#define NRF_REG_FIFO_STATUS 0x17

/* ---- Comandos SPI do nRF24L01+ ---- */
#define NRF_CMD_R_REGISTER    0x00
#define NRF_CMD_W_REGISTER    0x20
#define NRF_CMD_R_RX_PAYLOAD  0x61
#define NRF_CMD_W_TX_PAYLOAD  0xA0
#define NRF_CMD_FLUSH_TX      0xE1
#define NRF_CMD_FLUSH_RX      0xE2
#define NRF_CMD_NOP           0xFF

/* Bits uteis do STATUS */
#define NRF_STATUS_RX_DR   (1 << 6)
#define NRF_STATUS_TX_DS   (1 << 5)
#define NRF_STATUS_MAX_RT  (1 << 4)

#define NRF_PAYLOAD_WIDTH  1   /* 1 byte por pacote: suficiente p/ controlar 1 LED */

/* Inicializa SPI0 + pino CE + registradores do nRF24L01+.
 * addr[5]: endereco de 5 bytes usado tanto para RX quanto TX (pipe 0),
 * ambas as placas devem usar o MESMO endereco. */
void nrf24_init(const uint8_t addr[5], uint8_t channel);

/* Coloca o modulo em modo de recepcao (PRIM_RX=1, CE=1) */
void nrf24_power_up_rx(void);

/* Coloca o modulo em modo de espera para transmissao (PRIM_RX=0, CE=0) */
void nrf24_power_up_tx(void);

/* Envia 'len' bytes de 'buf' (bloqueante ate TX_DS ou MAX_RT) -> true se ack/OK */
bool nrf24_transmit(const uint8_t *buf, uint8_t len);

/* true se houver payload disponivel no FIFO de RX */
bool nrf24_data_ready(void);

/* Le 'len' bytes do FIFO de RX para 'buf' e limpa a flag RX_DR */
void nrf24_get_payload(uint8_t *buf, uint8_t len);

#endif /* NRF24L01_H_ */
