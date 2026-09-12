/*
 * spi.c
 * Author: Evandro / Atualizado com protecao de Timeout
 * https://github.com/evandro-teixeira/frdm-kl25z-spi
 * https://embarcados.com.br/biblioteca-spi-para-a-placa-frdm-kl25z/
 */
#include "spi.h"
 
#define SPI_TIMEOUT_MAX 100000

bool spi_init(bool spi, bool alt, uint8_t pre, uint16_t div, bool cs)
{
    switch(spi)
    {
        case SPI_0:
            switch(alt)
            {
                case ALT_0:
                    SIM->SCGC5 |= SIM_SCGC5_PORTC_MASK;     // Turn on clock to C module
                    SIM->SCGC4 |= SIM_SCGC4_SPI0_MASK;      // Enable SPI0 clock
                    if(cs == CS_AUT)                        // Chip Select Auto
                        PORTC->PCR[4] = PORT_PCR_MUX(0x2);  // Set PTC4 to mux 2 [SPI0_PCS0]
                    PORTC->PCR[5] = PORT_PCR_MUX(0x2);      // Set PTC5 to mux 2 [SPI0_SCK]
                    PORTC->PCR[6] = PORT_PCR_MUX(0x2);      // Set PTC6 to mux 2 [SPI0_MOSI]
                    PORTC->PCR[7] = PORT_PCR_MUX(0x2);      // Set PTC7 to mux 2 [SPI0_MISO]
                break;

                case ALT_1:
                    SIM->SCGC5 |= SIM_SCGC5_PORTA_MASK;      // Turn on clock to A module
                    if(cs == CS_AUT)                        // Chip Select Auto
                        PORTA->PCR[14] = PORT_PCR_MUX(0x2); // Set PTA14 to mux 2 [SPI0_PCS0]
                    PORTA->PCR[15] = PORT_PCR_MUX(0x2);     // Set PTA15 to mux 2 [SPI0_SCK]
                    PORTA->PCR[16] = PORT_PCR_MUX(0x2);     // Set PTA16 to mux 2 [SPI0_MOSI]
                    PORTA->PCR[17] = PORT_PCR_MUX(0x2);     // Set PTA17 to mux 2 [SPI0_MISO]
                break;

                default:
                    return false;
                break;
            }
            // Enable SPI0 clock
            SIM->SCGC4 |= SIM_SCGC4_SPI0_MASK;

            // Chip Select Auto
            if(cs == CS_AUT)
                SPI0->C1 = SPI_C1_MSTR_MASK | SPI_C1_SSOE_MASK;
            else
                SPI0->C1 = SPI_C1_MSTR_MASK;

            // Configure SPI Register C2
            SPI0->C2 = SPI_C2_MODFEN_MASK;

            // Set baud rate prescaler and divisor
            SPI0->BR = (SPI_BR_SPPR(pre) | SPI_BR_SPR(div));

            // Enable SPI0
            SPI0->C1 |= SPI_C1_SPE_MASK;

            return true;
        break;

        case SPI_1:
			switch(alt)
			{
				case ALT_0:
					SIM->SCGC5 |= SIM_SCGC5_PORTE_MASK; // Clock do PORTE
					
					// Só configura pino de hardware se for CS Automático
					if(cs == CS_AUT) {
						PORTE->PCR[4] = PORT_PCR_MUX(0x2); // PTE4 = SPI1_PCS0
					}
					
					PORTE->PCR[2] = PORT_PCR_MUX(0x2); // PTE2 = SPI1_SCK
					PORTE->PCR[1] = PORT_PCR_MUX(0x2); // PTE1 = SPI1_MOSI
					PORTE->PCR[3] = PORT_PCR_MUX(0x2); // PTE3 = SPI1_MISO
				break;

				case ALT_1:
					SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;
					if(cs == CS_AUT) {
						PORTB->PCR[10] = PORT_PCR_MUX(0x2);
					}
					PORTB->PCR[11] = PORT_PCR_MUX(0x2);
					PORTB->PCR[16] = PORT_PCR_MUX(0x2);
					PORTB->PCR[17] = PORT_PCR_MUX(0x2);
				break;

				default:
					return false;
				break;
			}

			// Enable SPI1 clock
			SIM->SCGC4 |= SIM_SCGC4_SPI1_MASK;

			// Configuração do Master e limpa o C2 (desativa MODFEN para evitar desarmar a SPI)
			if(cs == CS_AUT) {
				SPI1->C1 = SPI_C1_MSTR_MASK | SPI_C1_SSOE_MASK;
				SPI1->C2 = SPI_C2_MODFEN_MASK;
			} else {
				SPI1->C1 = SPI_C1_MSTR_MASK;
				SPI1->C2 = 0x00; // Desativa MODFEN quando o CS é manual por GPIO
			}

			// Prescaler e Divisor de Baud Rate
			SPI1->BR = (SPI_BR_SPPR(pre) | SPI_BR_SPR(div));

			// Ativa a SPI1
			SPI1->C1 |= SPI_C1_SPE_MASK;

			return true;
		break;

        default:
            return false;
        break;
    }
}

bool spi_send(bool spi, uint8_t data)
{
    uint32_t timeout = SPI_TIMEOUT_MAX;

    switch(spi)
    {
        case SPI_0:
            while(!(SPI_S_SPTEF_MASK & SPI0->S) && --timeout)
            {
                __asm("nop");
            }
            if (timeout == 0) return false;

            SPI0->D = data;
            return true;
        break;

        case SPI_1:
            while(!(SPI_S_SPTEF_MASK & SPI1->S) && --timeout)
            {
                __asm("nop");
            }
            if (timeout == 0) return false;

            SPI1->D = data;
            return true;
        break;

        default:
            return false;
        break;
    }
}

uint8_t spi_read(bool spi)
{
    uint32_t timeout = SPI_TIMEOUT_MAX;

    switch(spi)
    {
        case SPI_0:
            while(!(SPI0->S & SPI_S_SPRF_MASK) && --timeout)
            {
                __asm("nop");
            }
            if (timeout == 0) return 0xFF;

            return SPI0->D;
        break;

        case SPI_1:
            while(!(SPI1->S & SPI_S_SPRF_MASK) && --timeout)
            {
                __asm("nop");
            }
            if (timeout == 0) return 0xFF;

            return SPI1->D;
        break;

        default:
            return 0xFF;
        break;
    }
}

uint8_t spi_exchange(bool spi, uint8_t data)
{
    uint32_t timeout = SPI_TIMEOUT_MAX;

    switch(spi)
    {
        case SPI_0:
            // Aguarda o buffer de envio liberar
            while(!(SPI_S_SPTEF_MASK & SPI0->S) && --timeout)
            {
                __asm("nop");
            }
            if (timeout == 0) return 0xFF;

            SPI0->D = data;

            // Aguarda a recepção do dado
            timeout = SPI_TIMEOUT_MAX;
            while(!(SPI0->S & SPI_S_SPRF_MASK) && --timeout)
            {
                __asm("nop");
            }
            if (timeout == 0) return 0xFF;

            return SPI0->D;
        break;

        case SPI_1:
            // Aguarda o buffer de envio liberar
            while(!(SPI_S_SPTEF_MASK & SPI1->S) && --timeout)
            {
                __asm("nop");
            }
            if (timeout == 0) return 0xFF;

            SPI1->D = data;

            // Aguarda a recepção do dado
            timeout = SPI_TIMEOUT_MAX;
            while(!(SPI1->S & SPI_S_SPRF_MASK) && --timeout)
            {
                __asm("nop");
            }
            if (timeout == 0) return 0xFF;

            return SPI1->D;
        break;

        default:
            return 0xFF;
        break;
    }
}