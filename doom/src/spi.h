/*
 * spi.h
 *
 *  Created on: 07.06.2014
 *      Author: Florian
 */


#ifndef SPI_H_
#define SPI_H_

/*---------------------------------------------------------------------*
 *  additional includes                                                *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  global definitions                                                 *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  type declarations                                                  *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  function prototypes                                                *
 *---------------------------------------------------------------------*/

void spi_init (void);

uint8_t spi_send_receive (uint8_t data);

void spi_send (uint8_t data);

uint8_t spi_receive (void);

/*---------------------------------------------------------------------*
 *  global data                                                        *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  inline functions and function-like macros                          *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  eof                                                                *
 *---------------------------------------------------------------------*/

#endif /* SPI_H_ */
