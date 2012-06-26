#include "types.h"
#include "registers.h"
#include "interrupt.h"
#include "usart.h"

void init_usart(void) {
    *RCC_APB2ENR |= (1 << 4);   /* Enable USART1 Clock */
    *RCC_AHB1ENR |= (1 << 1);   /* Enable GPIOB Clock */

    /* Set PB6 and PB7 to alternative function USART
     * See stm32f4_ref.pdf pg 141 and stm32f407.pdf pg 51 */

    /* Sets PB6 and PB7 to alternative function mode */
    *GPIOB_MODER &= ~((3 << (6 * 2)) | (3 << (7 * 2)));
    *GPIOB_MODER |= (GPIO_MODER_ALT << (6 * 2)) | (GPIO_MODER_ALT << (7 * 2));

    /* Sets PB6 and PB7 to USART1-3 mode */
    *GPIOB_AFRL  &= ~((0xF << (6 * 4)) | (0xF << (7 * 4)));
    *GPIOB_AFRL  |= (GPIO_AF_USART13 << (6 * 4)) | (GPIO_AF_USART13 << (7 * 4));

    /* Sets pin output to push/pull */
    *GPIOB_OTYPER &= ~((1 << 6) | (1 << 7));

    /* No pull-up, no pull-down */
    *GPIOB_PUPDR  &= ~((3 << (6 * 2)) | (3 << (7 * 2)));

    /* Speed to 50Mhz */
    *GPIOB_OSPEEDR &= ~((3 << (6 * 2)) | (3 << (7 * 2)));
    *GPIOB_OSPEEDR |= (2 << (6 * 2)) | (2 << (7 * 2));

    /* Enable USART1 */
    *USART1_CR1 |= USART_CR1_UE;

    /* 8 data bits */
    *USART1_CR1 &= ~(1 << 12);

    /* 1 stop bit */
    *USART1_CR2 &= ~(3 << 12);

    /* Set baud rate */
    *USART1_BRR = usart_baud(115200);

    /* Enable recieve interrupt */
    *USART1_CR1 |= USART_CR1_RXNEIE;
    /* Enable in NVIC.  USART1 is interrupt 37, so 37-32 is bit 5 in second ISER */
    *NVIC_ISER1 |= (1 << 5);

    /* Enable reciever */
    *USART1_CR1 |= USART_CR1_RE;
}

/* Calculates the value for the USART_BRR */
uint16_t usart_baud(uint32_t baud) {
    float usartdiv = (84000000)/(16*(float)baud);
    uint16_t mantissa = (uint16_t) usartdiv;
    float fraction = 16 * (usartdiv-mantissa);
    uint16_t int_fraction = (uint16_t) fraction;

    /* Round fraction */
    while (fraction > 1) {
        fraction--;
    }
    if (fraction >= 0.5) {
        int_fraction += 1;
    }

    if (int_fraction == 16) {
        mantissa += 1;
        int_fraction = 0;
    }

    return (mantissa << 4) | int_fraction;
}

/* Handle USART1 Global Interrupt */
void usart1_handler(void) {
    /* Receive interrupt */
    if (*USART1_SR & USART_SR_RXNE) {
        putc(*USART1_DR);       /* Echo character */
    }
    else {
        /* Something bad happened, disable interrupt to save the rest of system. */
        *NVIC_ICER1 |= (1 << 5);
        puts("\r\n----USART read error, dropping to write-only mode.----\r\n");
    }
}

#define PS 256

void printx(char *s, uint8_t *x, int n){
    char buf[PS];
    static char hextable[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

    for (int i = 0; i < PS; i++){
        if(*s == '%'){
            for(int j = n-1; j >= 0; j--){
                buf[i++%PS] = hextable[(*(x+j)>>4)&0xf];
                buf[i++%PS] = hextable[*(x+j)&0xf];
            }
            i--;
        }
        else{
            buf[i] = *s;
        }
        s++;
    }
    puts(buf);
}

void putc(char letter) {
    /* Enable transmit */
    *USART1_CR1 |= USART_CR1_TE;

    /* Wait for transmit to clear */
    while (!(*USART1_SR & USART_SR_TC));

    *USART1_DR = (uint8_t) letter;
}

void puts(char *s) {
    while (*s) {
        putc(*s++);
    }
}
