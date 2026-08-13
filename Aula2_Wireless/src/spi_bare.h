/*
 * spi_bare.h
 *
 * Driver SPI0 100% bare metal para o FRDM-KL25Z (MKL25Z128).
 * NAO usa chip-select automatico via hardware (SSOE) -- CSN e' controlado
 * manualmente por GPIO, porque o nRF24L01+ exige CSN em nivel baixo durante
 * toda a transacao (varios bytes), e o SS automatico do SPI0 alterna CSN a
 * cada byte transferido, quebrando o protocolo do modulo.
 *
 * Pinagem (conforme slide "Conexao do nRF24L01 com a FRDM KL25Z", SPI0):
 *   PTC5 -> SCK   (ALT2 = SPI0_SCK)
 *   PTD2 -> MOSI  (ALT2 = SPI0_MOSI)
 *   PTD3 -> MISO  (ALT2 = SPI0_MISO)
 *   PTD0 -> CSN   (GPIO manual, NAO mux'ado para SPI0_PCS0)
 *
 * OBS: SCK esta na Porta C e MOSI/MISO/CSN na Porta D. Isso e' valido no
 * KL25Z porque cada pino tem seu proprio campo MUX independente para a
 * funcao SPI0 -- nao precisam estar todos na mesma porta.
 */

#ifndef SPI_BARE_H_
#define SPI_BARE_H_

#include "MKL25Z4.h"
#include <stdint.h>

/* Pino GPIO manual do CSN (ajuste aqui se mudar a fiacao) */
#define NRF_CSN_PORT   PTD
#define NRF_CSN_PIN    0u

/* Inicializa SIM clocks, mux dos pinos SPI0 e registradores do modulo SPI0 */
void spi0_init(void);

/* Transferencia full-duplex de 1 byte (bloqueante) - CSN NAO e' tocado aqui */
uint8_t spi0_transfer(uint8_t data);

/* Controle manual do CSN (chip select do nRF24L01+) */
void nrf_csn_low(void);
void nrf_csn_high(void);

#endif /* SPI_BARE_H_ */
