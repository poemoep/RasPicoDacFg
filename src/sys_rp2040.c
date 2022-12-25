/* Copyright (c) 2022 poemoep */
/* This software is released under the MIT License, see LICENSE, see LICENSE. */
/* This website content is released under the CC BY 4.0 License, see LICENSE. */


#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"

#include "hardware/structs/scb.h" /* m0plus cpuid */

#include "sys_rp2040.h"
#include "fg_commands.h"
#include "com_ctrl.h"

#ifdef BEST_FREQ_ACCURACY
    #define UART_ID uart0
    #define BAUD_RATE (115200)
    #define UART_TX_PIN (16)
    #define UART_RX_PIN (17)
#endif


void sys_BufIni(void);
void sys_InterruptIni(void);
char com_char[COM_CHAR_BUF_SIZE];
char word_buf[WORDBUF_SIZE];

GPIO_T gpio;

void exec_login(void);
void exec_cmd(char*);

void check_irq_en()
{

    for(volatile int i = 0 ; i < 26; i++){
        printf("%d, %d\n",i, irq_is_enabled(i));
    }

    printf("NVIC_ISER: %x\n",(io_rw_32 *) (PPB_BASE + M0PLUS_NVIC_ISER_OFFSET));
    printf("NVIC_ICER: %x\n",(io_rw_32 *) (PPB_BASE + M0PLUS_NVIC_ICER_OFFSET));
    printf("NVIC_ISPR: %x\n",(io_rw_32 *) (PPB_BASE + M0PLUS_NVIC_ISPR_OFFSET));
    printf("NVIC_ICPR: %x\n",(io_rw_32 *) (PPB_BASE + M0PLUS_NVIC_ICPR_OFFSET));
}

int main() 
{
    

#ifdef BEST_FREQ_ACCURACY
    uart_init(UART_ID, BAUD_RATE);

    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
#endif
    stdio_init_all(); /* usb seirial init */

    sys_gpioIni_output(GPIO_LED_PIN | GPIO_DAC_PIN);

    sys_BufIni();
    sys_setLED(LED_RED | LED_BLUE | LED_GREEN);

    volatile char c;

    while(1)
    {
        c = com_getChar();
        if(COM_LOGIN_CODE == c){
            exec_login();
        }
    }
    return 0;
}

void sys_gpioIni_output(unsigned int pins)
{
    int pin;
    
    pin = pins;
    for(uint i=0;i<32;i++) {
        if (pin & 1) 
        {
            gpio_init(i);
            gpio_set_dir(i, GPIO_OUT);
            gpio_disable_pulls(i);
            gpio_set_drive_strength(i, GPIO_DRIVE_STRENGTH_12MA);
            gpio_set_slew_rate(i, GPIO_SLEW_RATE_FAST);
        }
        pin >>= 1;
    }

}

void sys_BufIni(void)
{
    for(volatile int i = 0; i < COM_CHAR_BUF_SIZE; i++)
    {
        com_char[i] = 0;
    }

    fg_init();
}

void sys_setLED(int LEDs)
{
    if(0 != (LED_RED & LEDs))       gpio.myio.led.red = 1;    else    gpio.myio.led.red = 0;
    if(0 != (LED_GREEN & LEDs))     gpio.myio.led.green = 1;  else    gpio.myio.led.green = 0;
    if(0 != (LED_BLUE & LEDs))      gpio.myio.led.blue = 1;   else    gpio.myio.led.blue = 0;


    if(CORE1_STATUS_START != core1_val.core1_flag){
        gpio_put_all(gpio.uint_io);
    }
}


void exec_login(void)
{
    printf("\n\nWelcom to Raspberry pi Pico\nR-2R DAC start!\n\n");
    printf("Cortex M0+ CPUID: 0x%x\n", scb_hw->cpuid);
    printf(COM_WAIT_CMD);

    volatile unsigned int com_num = 0;
    volatile char c;

    while(1){

        c = com_getChar();
        switch(c)
        {
            case COM_RETURN_CODE:
                if(com_num > 0){ 
                           
                    com_char[com_num] = COM_NULL_CODE;
                    com_upper(com_char);
                    exec_cmd(com_char);

                    com_num = 0;
                    for(volatile int i = 0; i < COM_CHAR_BUF_SIZE; i++)
                    {
                        com_char[i] = 0;
                    }

                    printf(COM_WAIT_CMD);
                }
                else
                {
                    printf(COM_WAIT_CMD);
                }
                break;

            case ASCII_BACKSPACE:
                if(com_num > 0){
                    com_char[com_num] = 0;
                    printf(" %c", ASCII_BACKSPACE);
                    com_num--;
                }
                break;

            default:
                if((' ' <= c) && (c <= '~'))
                {
                    com_char[com_num] = c;
                    com_num++;
                }
                break;
        }
    }
}

void exec_cmd(char* input_char)
{
    typedef enum{
        CMD_LIST_0 = 0,
        CMD_LIST_START = 0,
        CMD_LIST_STOP,
        CMD_LIST_RESTART,
        CMD_LIST_SINE,
        CMD_LIST_PULSE,
        CMD_LIST_TRI,
        CMD_LIST_SAW,
        // CMD_LIST_BURST,
        CMD_LIST_RAMP,
        CMD_LIST_DAC,
        CMD_LIST_LED,
        CMD_LIST_PRINT,
        CMD_LIST_HELP,
        // CMD_LIST_TEST,
        CMD_LIST_NUM,
        CMD_LIST_FIN = CMD_LIST_NUM
    } CMD_LIST_E;

    const char* cmd_list[CMD_LIST_NUM] =
    {
        "START",
        "STOP",
        "RE",
        "SINE",
        "PULSE",
        "TRI",
        "SAW",
        // "BURST",
        "RAMP",
        "DAC",
        "LED",
        "PRINT",
        "HELP",
        // "TEST",
    };

    const unsigned int cmd_list_len[CMD_LIST_NUM] =
    {
        5,
        4,
        2,
        4,
        5,
        3,
        3,
        // 5,
        4,
        3,
        3,
        5,
        4,
        // 4,
    };

    void (* const cmd_list_func[])(char* ) =
    {
            fg_start_func,
            fg_stop_func,
            fg_restart_func,
            fg_sine_func,
            fg_pulse_func,
            fg_triangle_func,
            fg_saw_func,
            // fg_burst_func,
            fg_ramp_func,
            fg_dac_func,
            fg_led_func,
            fg_print_func,
            fg_help_func,
            // fg_test_func
    };

    /* check cmd list */

    volatile unsigned int cmd_num = 0;
    while(cmd_num < CMD_LIST_FIN)
    {
        if( 0 == check_cmd(input_char, cmd_list[cmd_num], cmd_list_len[cmd_num])) break;
        cmd_num++;
    }

    if(CMD_LIST_FIN == cmd_num)
    {
        printf("INPUT CMD ERROR\n");
    }
    else
    {
        cmd_list_func[cmd_num]((char*)(input_char + cmd_list_len[cmd_num] + 1));
    }


}

int check_cmd(char* input_str, const char* cmd, const unsigned int cmd_len){

    int str_len = 0;

    if(*input_str == '\0') return -1;
    if(*input_str == ' ') return -1;

    while(*input_str != '\0' && *input_str != ' ')
    {

        if(*cmd != *input_str) return -2;

        input_str++;
        cmd++;
        str_len++;
    }

    if(cmd_len == str_len) return 0;
    else return -3;
}


void sys_restartSwitchItr(unsigned gpio, unsigned long events)
{
    printf("GPIO IRQ\n");
    sys_setLED(1);
    if(gpio == RESTART_PIN)
    {
        for(int i = 0; i < 2500000; i++)
        {
            nop();

        }
        if(gpio_get(RESTART_PIN) == 0)
        {
            char* dmy ="dmy" ;
            fg_restart_func(dmy);
            sys_setLED(7);
        }
        else
        {sys_setLED(2);}

    }
    
}

