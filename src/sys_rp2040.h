/* Copyright (c) 2022 poemoep */
/* This software is released under the MIT License, see LICENSE, see LICENSE. */
/* This website content is released under the CC BY 4.0 License, see LICENSE. */


#if !(__SYS_RP2040_H__)
#define __SYS_RP2040_H__

/* if you want to get best frequency accuracy, */
/* Uncomment out below & Use UART not USB-CDC */
// #define BEST_FREQ_ACCURACY

#define SYSCLK_MHZ  (125)
#define SYSCLK_KHZ  (SYSCLK_MHZ * 1000)
#define SYSCLK      (SYSCLK_KHZ * 1000)

#define LED_PIN_RED     (21)
#define LED_PIN_GREEN   (20)
#define LED_PIN_BLUE    (19)
#define RESTART_PIN     (16)
#define RESTART_WAIT_TIME (10)

#define LED_RED         (0x01)
#define LED_GREEN       (0x02)
#define LED_BLUE        (0x04)
#define GPIO_LED_PIN        ((1ul << LED_PIN_RED) | (1ul << LED_PIN_GREEN) | (1ul << LED_PIN_BLUE))

#define GPIO_DAC_PIN    (0x0000FFFF)
#define GPIO_DAC_PIN_NUM    16

#define CORE1_STATUS_NULL   0
#define CORE1_STATUS_START  1
#define CORE1_STATUS_STOP   2

#define WORDBUF_SIZE 16


#define nop() asm("NOP")
void sys_setLED(int);
void sys_gpioIni_output(unsigned int);
int check_cmd(char*, const char*, const unsigned int);


typedef union{
    unsigned int uint_io; /* 4byte */
    struct{
        unsigned int gp0 : 1;    /* 1bit x 32 = 4 byte */
        unsigned int gp1 : 1;
        unsigned int gp2 : 1;
        unsigned int gp3 : 1;
        unsigned int gp4 : 1;
        unsigned int gp5 : 1;
        unsigned int gp6 : 1;
        unsigned int gp7 : 1;
        unsigned int gp8 : 1;
        unsigned int gp9 : 1;
        unsigned int gp10: 1;
        unsigned int gp11: 1;
        unsigned int gp12: 1;
        unsigned int gp13: 1;
        unsigned int gp14: 1;
        unsigned int gp15: 1;
        unsigned int gp16: 1;
        unsigned int gp17: 1;
        unsigned int gp18: 1;
        unsigned int gp19: 1;
        unsigned int gp20: 1;
        unsigned int gp21: 1;
        unsigned int gp22: 1;
        unsigned int gp23: 1;
        unsigned int gp24: 1;
        unsigned int gp25: 1;
        unsigned int gp26: 1;
        unsigned int gp27: 1;
        unsigned int gp28: 1;
        unsigned int gp29: 1;        
        unsigned int dummy1: 1;
        unsigned int dummy2: 1;        
    } io;
    union{
        union{                      /* 2byte */
            // unsigned short val[2];     /* 2byte */
            unsigned short val;     /* 2byte */
//            unsigned char c_val[2]; /* 1byte x 2 = 2byte */
            struct{ 
                unsigned int _0: 1; /* 1bit x 16 = 2byte */
                unsigned int _1: 1;
                unsigned int _2: 1;
                unsigned int _3: 1;
                unsigned int _4: 1;
                unsigned int _5: 1;
                unsigned int _6: 1;
                unsigned int _7: 1;
                unsigned int _8: 1;
                unsigned int _9: 1;
                unsigned int _A: 1;
                unsigned int _B: 1;
                unsigned int _C: 1;
                unsigned int _D: 1;
                unsigned int _E: 1;
                unsigned int _F: 1;
                unsigned int dummy: 16;
            }bit;
        }dac;
        struct{
            unsigned int dummy1: 19;
            unsigned int blue: 1;
            unsigned int green: 1;
            unsigned int red: 1;
            unsigned int dummy2: 10;
        } led;
    } myio;
} GPIO_T;

extern GPIO_T gpio;

extern char word_buf[WORDBUF_SIZE];

#endif