/*
 * Copyright (c) 2013, M.Naruoka (fenrir)
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 * 
 * - Redistributions of source code must retain the above copyright notice, 
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice, 
 *   this list of conditions and the following disclaimer in the documentation 
 *   and/or other materials provided with the distribution.
 * - Neither the name of the naruoka.org nor the names of its contributors 
 *   may be used to endorse or promote products derived from this software 
 *   without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS 
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, 
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, 
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#include <stdio.h>

#include "main.h"
#include "f38x_uart1.h"
#include "fifo.h"

#define DEFAULT_BAUDRATE  9600UL          // Baud rate of UART in bps

#define CRITICAL_UART1(func) \
{\
  EIE2 &= ~0x02;\
  func;\
  EIE2 |= 0x02;\
}

__xdata fifo_char_t fifo_tx1; ///< FIFO TX
__xdata fifo_char_t fifo_rx1; ///< FIFO RX
static __xdata __at (0x100) 
  char buffer_tx1[UART1_TX_BUFFER_SIZE];
static __xdata __at (0x100 + UART1_TX_BUFFER_SIZE) 
  char buffer_rx1[UART1_RX_BUFFER_SIZE];

/**
 * Change UART1 baudrate
 * 
 */
void uart1_bauding(unsigned long baudrate){
  unsigned long selector = 0x10000UL - (SYSCLK/baudrate/2);
  
  SBCON1 = 0x03; // SB1PS[1:0] = 11;
  
  SBRLH1 = (unsigned char)((selector >> 8) & 0xFF);
  SBRLL1 = (unsigned char)(selector & 0xFF);
  
  SBCON1 |= 0x40; // SB1RUN = 1;
}


/**
 * uart1_init
 * 
 */
void uart1_init() {
  // SCON1: RX enabled
  // ninth bits are zeros
  // clear RI1 and TI1 bits
  SCON1 = 0x10;
  
  // 8-bit, no parity, 1 stop bit
  //SMOD1 = 0x0C; // default
          
  fifo_char_init(&fifo_tx1, buffer_tx1, UART1_TX_BUFFER_SIZE); 
  fifo_char_init(&fifo_rx1, buffer_rx1, UART1_RX_BUFFER_SIZE); 

  uart1_bauding(DEFAULT_BAUDRATE);

  SCON1 &= ~0x08;   // TBX1�͏����ݒ��t���O��0(�����݂��Ă��Ȃ�)
  EIE2 |= 0x02;     // ���荞�ݗL��
  //EIP2 = 0x02;      // �D��x1
}

/**
 * Regist sending data via UART1
 * 
 * @param data pointer to data
 * @param size size of data
 * @return (FIFO_SIZE_T) the size of registed data to buffer
 */
FIFO_SIZE_T uart1_write(char *buf, FIFO_SIZE_T size){
  // TB80�͏����ݒ��t���O�Ƃ��Ďg��
  // 0(�����݂��Ă��Ȃ�)��������蓮���荞�݂�������
  if(size){
    size = fifo_char_write(&fifo_tx1, buf, size);
    CRITICAL_UART1(
      if(!(SCON1 & 0x0A)){ // !TBX1 && !TI1
        SCON1 |= 0x02; // �蓮�Ŋ����݂�������
      }
    );
  }
  return size;
}

/**
 * Return the size of untransimitted data via UART1
 * 
 * @return (FIFO_SIZE_T) the size
 */
FIFO_SIZE_T uart1_tx_size(){
  return fifo_char_size(&fifo_tx1);
}

/**
 * Get the recieved data via UART1
 * 
 * @param buf buffer
 * @param size the size of buffer
 * @return (FIFO_SIZE_T) the real size of grabbed data
 */
FIFO_SIZE_T uart1_read(char *buf, FIFO_SIZE_T size){
  return fifo_char_read(&fifo_rx1, buf, size);
}

/**
 * Return the size of ungrabbed data via UART1
 * 
 * @return (FIFO_SIZE_T) the size
 */
FIFO_SIZE_T uart1_rx_size(){
  return fifo_char_size(&fifo_rx1);
}

///< Interrupt(TI1 / RI1)
void interrupt_uart1 (void) __interrupt (INTERRUPT_UART1) {
  unsigned char c;

  if(SCON1 & 0x01){   // RI1
    SCON1 &= ~0x01;
    /* �����O�o�b�t�@��1�o�C�g�����o�� */
    c = SBUF1;
    if(fifo_char_put2(&fifo_rx1, c)){
      P4 &= ~0x02;
    }else{P4 ^= 0x02;}
  }

  if(SCON1 & 0x02){   // TI1
    SCON1 &= ~0x02;
    /* �������ރf�[�^�����邩�m�F */
    if(fifo_char_size(&fifo_tx1) > 0){
      c = fifo_char_get2(&fifo_tx1);
      SCON1 |= 0x08;   // TBX1�͏����ݒ��t���O�Ƃ��Ďg���A1(�����ݒ�)��
      SBUF1 = c;
    }else{
      SCON1 &= ~0x08;  // TBX1�͏����ݒ��t���O�Ƃ��Ďg���A0(�����݂��Ă��Ȃ�)��
    }
  }
}