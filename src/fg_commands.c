/* Copyright (c) 2022 poemoep */
/* This software is released under the MIT License, see LICENSE, see LICENSE. */
/* This website content is released under the CC BY 4.0 License, see LICENSE. */


#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/sync.h" /* save_and_disable_interrupts */ 

#include "hardware/irq.h"
#include "hardware/structs/systick.h"

#include "sys_rp2040.h"
#include "fg_commands.h"
#include "com_ctrl.h"



void core1_entry_normal(void);
void core1_entry_ramp(void);
void core1_destroy(void);
int word2int(char*);
int fg_mv2digit(int);
void fg_cmdErr(const char*);
FG_VALUE_T fg_freq2datum_and_delay(FG_VALUE_T);
int getSin(int in, int min, int max);
void core1_irq_disable(void);

void ramp_buf_fill(RAMP_T ramp);

void print_fgbuf_func(char*);
void print_gpio_func(char*);

void nop_1_times(void); 
void nop_1us(void); 
void nop_10us(void); 
void nop_100us(void);

CORE1_VARIABLE_T core1_val;
RAMP_T ramp_buf;

void fg_init(void)
{
    for(volatile int i = 0; i < FG_BUF_SIZE; i++)
    {
        core1_val.fg_buf[i] = 0;
    }
    core1_val.fg_buf_max = FG_BUF_MAX;
    core1_val.count_max = FG_COUNT_CONTINUE;
    core1_val.core1_flag = CORE1_STATUS_NULL;
    core1_val.nop_func = nop_1_times;
    core1_val.core1_func = core1_entry_normal;

}

void core1_entry_normal(void)
{
    // sys_gpioIni_output(GPIO_LED_PIN);
    // sys_gpioIni_output(GPIO_DAC_PIN);

    core1_irq_disable();
    // save_and_disable_interrupts();
    unsigned short itr = 0;
    unsigned short mask = 0xFFFF;
    unsigned short count = 0; 


    while(1){

        gpio_put_all((((unsigned int)core1_val.fg_buf[itr++] & mask) | gpio.uint_io));
        if(core1_val.fg_buf_max < itr)  itr = 0;
        else                            nop(); /* 処理回数をそろえるためのnop */
        
        if(0 == itr)    count++;
        else            nop(); /* 処理回数をそろえるためのnop */
        
        if(core1_val.count_max <= count )    mask = 0;
        else                                nop(); /* 処理回数をそろえるためのnop */

        core1_val.nop_func();
    }
}

void core1_entry_ramp(void)
{
    core1_irq_disable();

    unsigned short itr = 0;
    unsigned short mask = 0xFFFF;
    unsigned short count = 0; 
    unsigned short buf_fin_val = core1_val.fg_buf[FG_BUF_MAX];

    while(1){

        gpio_put_all((((unsigned int)core1_val.fg_buf[itr] & mask) | gpio.uint_io));
        core1_val.fg_buf[itr++] = buf_fin_val;        
        core1_val.nop_func();
    }
}

void core1_irq_disable(void)
{

    /* 割り込みがあると信号がぶれるため、全割り込みを無効化する*/

    io_rw_32* p;
    p = ((io_rw_32* ) (PPB_BASE + M0PLUS_NVIC_ICER_OFFSET));
    *p = 0xFFFFFFFF;

    p = (io_rw_32 *) (PPB_BASE + M0PLUS_NVIC_ICPR_OFFSET);
    *p = 0xFFFFFFFF;
    
    systick_hw->csr = 0;

}


int word2int(char* input_str)
{


    int res = 0;

    /* その前にチェックしているから、いらないかも */
    if(*input_str == '\0') 
    {
        res = -1;
        return res;
    }

    while(*input_str != '\0' && *input_str != ' ')
    {
        int tmp = *input_str - '0'; /* *input_str - 0x30 */
        // printf("%d", tmp);

        if(tmp < 0 || tmp > 9)
        {
            res = -1;
            break;
        }

        // res = (((res<<2) + res)<<1) + tmp /* x 10 */
        res = (res * 10) + tmp;
        input_str++;
    }

    return res;
}

void fg_cmdErr(const char* usage_msg)
{
    printf("CMD ERROR\n");
    printf(usage_msg);
}

void fg_start_func(char* input_str)
{

    switch(core1_val.core1_flag){
        case CORE1_STATUS_NULL:
            printf("You have not ready fg_buf yet.\n");
            break;
        case CORE1_STATUS_START:
            printf("core1 is already started!\n");
            break;
        case CORE1_STATUS_STOP:
            printf("Start function.\n");
            if(core1_entry_ramp == core1_val.core1_func)
            {
                ramp_buf_fill(ramp_buf);
            }
            core1_val.core1_flag = CORE1_STATUS_START;
            multicore_launch_core1(core1_val.core1_func);
            break;
        default:
            printf("Error\n");
            break;
    }
}

void fg_stop_func(char* input_str)
{
    switch(core1_val.core1_flag){
        case CORE1_STATUS_NULL:
            printf("You have not ready fg_buf yet.\n");
            break;
        case CORE1_STATUS_START:
            printf("Stop function.\n");
            core1_val.core1_flag = CORE1_STATUS_STOP;
            multicore_reset_core1(); 
            gpio_put_all(0x0 | gpio.uint_io);
            break;
        case CORE1_STATUS_STOP:
            printf("core1 is not started!\n");
            break;
        default:
            printf("Error\n");
            break;
    }

}

void fg_restart_func(char* input_str)
{
    fg_stop_func(input_str);
    fg_start_func(input_str);
}

void fg_sine_func(char* input_str)
{

    if(core1_val.core1_flag == CORE1_STATUS_START)
    {
        printf("core1 is already started!\n");
        return;
    }


    FG_VALUE_T fg;
    int freq,time,n;
    char* pchar;
    unsigned int temp;

    fg.freq = -1;
    fg.cyc = -1;
    fg.out_max = -1;
    fg.out_min = -1;
    pchar = input_str;

    pchar = com_get_nextWord(pchar, word_buf);
    fg.freq = word2int(word_buf);
    if(fg.freq < 0) fg_cmdErr(FG_SINE_HELP);


    pchar = com_get_nextWord(pchar, word_buf);
    fg.cyc = word2int(word_buf);

    pchar = com_get_nextWord(pchar, word_buf);
    temp = word2int(word_buf);
    fg.out_max = fg_mv2digit((int)temp);

    pchar = com_get_nextWord(pchar, word_buf);
    temp = word2int(word_buf);
    fg.out_min = fg_mv2digit((int)temp);

    if(fg.cyc < 1) fg.cyc = FG_COUNT_CONTINUE;
    if(fg.out_max < 0) fg.out_max = DAC_OUT_MAX;
    if(fg.out_min < 0) fg.out_min = DAC_OUT_MIN;

    fg = fg_freq2datum_and_delay(fg);


    core1_val.fg_buf_max = fg.datum;
    core1_val.count_max = fg.cyc;

    short ans = 0;
    int oft = (fg.out_max + fg.out_min) >> 1;
    int wdt = (fg.out_max - fg.out_min);

    for(volatile int i = 0; i < (fg.datum >> 1); i++)
    {
        ans = getSin(i, 0, (fg.datum >> 1));
        ans = (0xFFFF)&((ans*(wdt>>1)) / FG_SINE_SIZE);
        core1_val.fg_buf[i] = ans + oft;
        core1_val.fg_buf[fg.datum - i] = oft - ans;
    }

    ans = (fg.datum >> 1);
    ans = (ans<<1);
    ans = fg.datum - ans;
    if(0 != ans){
        core1_val.fg_buf[(fg.datum>>1)+1] = oft; 
        core1_val.fg_buf[(fg.datum>>1)] = oft; 
    }
    
    
    core1_val.core1_func = core1_entry_normal;
    core1_val.core1_flag = CORE1_STATUS_STOP;

}

void fg_pulse_func(char* input_str)
{
    if(core1_val.core1_flag == CORE1_STATUS_START)
    {
        printf("core1 is already started!\n");
        return;
    }

    FG_VALUE_T fg;
    char* pchar;
    unsigned int temp;

    fg.freq = -1;
    fg.duty = -1;
    fg.cyc = -1;
    fg.out_max = -1;
    fg.out_min = -1;
    
    pchar = input_str;

    pchar = com_get_nextWord(pchar, word_buf);
    fg.freq = word2int(word_buf);
    if(fg.freq < 0) {fg_cmdErr(FG_PULSE_HELP); return;}


    pchar = com_get_nextWord(pchar, word_buf);
    fg.cyc = word2int(word_buf);

    pchar = com_get_nextWord(pchar, word_buf);
    fg.duty = word2int(word_buf);

    pchar = com_get_nextWord(pchar, word_buf);
    temp = word2int(word_buf);
    fg.out_max = fg_mv2digit((int)temp);

    pchar = com_get_nextWord(pchar, word_buf);
    temp = word2int(word_buf);
    fg.out_min = fg_mv2digit((int)temp);

    if(fg.duty < 1) fg.duty = 50; /* duty入力なかったら 50%固定値 */
    if(fg.cyc < 1 ) fg.cyc = FG_COUNT_CONTINUE; /* cyc入力なかったら、連続固定 */
    if(fg.out_max < 0) fg.out_max = DAC_OUT_MAX;
    if(fg.out_min < 0) fg.out_min = DAC_OUT_MIN;
    

    fg = fg_freq2datum_and_delay(fg);

    core1_val.fg_buf_max = fg.datum;
    core1_val.count_max = fg.cyc;

    unsigned short ans = 0;
    for(volatile int i = 0; i < (fg.datum*fg.duty)/100; i++)
    {
        core1_val.fg_buf[i] = fg.out_max;
    }
    for(volatile int i = (fg.datum*fg.duty)/100; i < fg.datum; i++)
    {
        core1_val.fg_buf[i] = fg.out_min;
    }

    
    core1_val.core1_func = core1_entry_normal;
    core1_val.core1_flag = CORE1_STATUS_STOP;
    

}

void fg_triangle_func(char* input_str)
{    
    if(core1_val.core1_flag == CORE1_STATUS_START)
    {
        printf("core1 is already started!\n");
        return;
    }

    FG_VALUE_T fg;
    char* pchar;
    unsigned int temp;

    fg.freq = -1;
    fg.cyc = -1;
    fg.out_max = -1;
    fg.out_min = -1;
    pchar = input_str;

    pchar = com_get_nextWord(pchar, word_buf);
    fg.freq = word2int(word_buf);
    if(fg.freq < 0) {fg_cmdErr(FG_TRIANGLE_HELP); return;}

    pchar = com_get_nextWord(pchar, word_buf);
    fg.cyc = word2int(word_buf);

    pchar = com_get_nextWord(pchar, word_buf);
    temp = word2int(word_buf);
    fg.out_max = fg_mv2digit((int)temp);

    pchar = com_get_nextWord(pchar, word_buf);
    temp = word2int(word_buf);
    fg.out_min = fg_mv2digit((int)temp);
    
    if(fg.cyc < 1 ) fg.cyc = FG_COUNT_CONTINUE; /* cyc入力なかったら、連続固定 */
    if(fg.out_max < 0) fg.out_max = DAC_OUT_MAX;
    if(fg.out_min < 0) fg.out_min = DAC_OUT_MIN;
    


    fg = fg_freq2datum_and_delay(fg);

    core1_val.fg_buf_max = fg.datum;
    core1_val.count_max = fg.cyc;

    int ans = 0;
    int oft = fg.out_min;
    int wdt = (fg.out_max - fg.out_min);
    for(volatile int i = 0; i < (fg.datum >> 1); i++)
    {
        ans = (i*wdt) / (fg.datum>>1);
        core1_val.fg_buf[i] = ans + oft;
        core1_val.fg_buf[fg.datum - i] = ans + oft;
    }

    ans = (fg.datum >> 1);
    ans = (ans<<1);
    ans = fg.datum - ans;    
    if(0 != ans){
        core1_val.fg_buf[(fg.datum>>1)+1] = fg.out_max; 
        core1_val.fg_buf[(fg.datum>>1)] = fg.out_max;; 
    }
    
    core1_val.core1_func = core1_entry_normal;
    core1_val.core1_flag = CORE1_STATUS_STOP;
}

void fg_saw_func(char* input_str)
{    
    if(core1_val.core1_flag == CORE1_STATUS_START)
    {
        printf("core1 is already started!\n");
        return;
    }

    FG_VALUE_T fg;
    char* pchar;
    unsigned int temp;

    fg.freq = -1;
    fg.cyc = -1;
    fg.out_max = -1;
    fg.out_min = -1;
    pchar = input_str;

    pchar = com_get_nextWord(pchar, word_buf);
    fg.freq = word2int(word_buf);
    if(fg.freq < 0) {fg_cmdErr(FG_SAW_HELP); return;}

    pchar = com_get_nextWord(pchar, word_buf);
    fg.cyc = word2int(word_buf);

    pchar = com_get_nextWord(pchar, word_buf);
    temp = word2int(word_buf);
    fg.out_max = fg_mv2digit((int)temp);

    pchar = com_get_nextWord(pchar, word_buf);
    temp = word2int(word_buf);
    fg.out_min = fg_mv2digit((int)temp);
    
    if(fg.cyc < 1 ) fg.cyc = FG_COUNT_CONTINUE; /* cyc入力なかったら、連続固定 */
    if(fg.out_max < 0) fg.out_max = DAC_OUT_MAX;
    if(fg.out_min < 0) fg.out_min = DAC_OUT_MIN;
    


    fg = fg_freq2datum_and_delay(fg);

    core1_val.fg_buf_max = fg.datum;
    core1_val.count_max = fg.cyc;

    int ans = 0;
    int oft = fg.out_min;
    int wdt = (fg.out_max - fg.out_min);
    for(volatile int i = 0; i < fg.datum; i++)
    {
        ans = (i*wdt) / fg.datum;
        core1_val.fg_buf[i] = ans + oft;
    }
    
    
    core1_val.core1_func = core1_entry_normal;
    core1_val.core1_flag = CORE1_STATUS_STOP;
}
 

// void fg_burst_func(char* input_str)
// {
//     printf("fg_burst_func\n");
// }

void ramp_buf_fill(RAMP_T ramp)
{
    /* 固定 0.1ms周期。時間精度は適当*/
    unsigned int time_th_wait, time_th_ramp;

    time_th_wait = (ramp.wait_time*10);
    time_th_ramp = time_th_wait + (ramp.ramp_time*10);
    
    for(int i = 0; i < FG_BUF_SIZE; i++)
    {
        if(i <= time_th_wait)
        {
            core1_val.fg_buf[i] = ramp.start_mv;
        } 
        else if(i <= time_th_ramp)
        {
            core1_val.fg_buf[i] = (unsigned short)((int)(ramp.end_mv - ramp.start_mv)*(int)(i - time_th_wait)/(int)(time_th_ramp - time_th_wait) + (int)ramp.start_mv);
        }
        else
        {
            core1_val.fg_buf[i] = ramp.end_mv;
        } 
    }

    core1_val.core1_func = core1_entry_ramp;
    core1_val.nop_func = nop_100us;
    core1_val.core1_flag = CORE1_STATUS_STOP;
}

void fg_ramp_func(char* input_str)
{
    if(core1_val.core1_flag == CORE1_STATUS_START)
    {
        printf("core1 is already started!\n");
        return;
    }

    unsigned int wait_time, ramp_time, start_mv, end_mv;
    FG_VALUE_T fg;
    char* pchar;
    unsigned int temp;

    pchar = input_str;

    pchar = com_get_nextWord(pchar, word_buf);
    wait_time = word2int(word_buf);
    if(wait_time < 0) {fg_cmdErr(FG_RAMP_HELP); return;}

    pchar = com_get_nextWord(pchar, word_buf);
    ramp_time = word2int(word_buf);
    if(ramp_time < 0) {fg_cmdErr(FG_RAMP_HELP); return;}


    pchar = com_get_nextWord(pchar, word_buf);
    temp = word2int(word_buf);
    start_mv = fg_mv2digit((int)temp);
    if(start_mv < 0) {fg_cmdErr(FG_RAMP_HELP); return;}

    pchar = com_get_nextWord(pchar, word_buf);
    temp = word2int(word_buf);
    end_mv = fg_mv2digit((int)temp);
    if(end_mv < 0) {fg_cmdErr(FG_RAMP_HELP); return;}

    if(wait_time + ramp_time > 6553)
    {
        fg_cmdErr(FG_RAMP_HELP); 
        printf("Total time is less than 6553 ms\n");
        return;
    }

    fg.ramp.wait_time = wait_time;
    fg.ramp.ramp_time = ramp_time;
    fg.ramp.start_mv = start_mv;
    fg.ramp.end_mv = end_mv;
    
    ramp_buf_fill(fg.ramp);
    ramp_buf = fg.ramp;

}

void fg_dac_func(char* input_str)
{
    if(core1_val.core1_flag == CORE1_STATUS_START)
    {
        printf("core1 is already started!\n");
        return;
    }

    FG_VALUE_T fg;
    char* pchar;
    unsigned int temp;

    fg.out_max = -1;
    pchar = input_str;

    pchar = com_get_nextWord(pchar, word_buf);
    temp = word2int(word_buf);
    fg.out_max = fg_mv2digit((int)temp);

    if(fg.out_max < 0) {fg_cmdErr(FG_DAC_HELP); return;}

    for(volatile int i = 0; i < FG_BUF_SIZE; i++ )
    {
        core1_val.fg_buf[i] = fg.out_max;
    }

    
    core1_val.core1_func = core1_entry_normal;
    core1_val.core1_flag = CORE1_STATUS_STOP;
}

void fg_led_func(char* input_str)
{
    int tmp,res;

    input_str = com_get_nextWord(input_str, word_buf);
    tmp = word2int(word_buf);
    if(tmp < 0)
    {
        printf("R:%d, G:%d, B:%d\n", gpio.myio.led.red, gpio.myio.led.green, gpio.myio.led.blue);
    }
    else
    {
        tmp = word2int(word_buf);
        sys_setLED(tmp);
    }        
}

void fg_print_func(char* input_str)
{
    typedef enum{
        PRINT_LIST_0 = 0,
        PRINT_LIST_FGBUF = 0,
        PRINT_LIST_GPIO,
        PRINT_LIST_NUM = PRINT_LIST_GPIO + 1,
        PRINT_LIST_FIN = PRINT_LIST_NUM
    } PRINT_LIST_E;

    const char* print_list[PRINT_LIST_NUM] =
    {
        "FGBUF",
        "GPIO",
    };

    const unsigned int print_list_len[PRINT_LIST_NUM] =
    {
        5,
        4
    };

    void (* const print_list_func[])(char* ) =
    {
            print_fgbuf_func,
            print_gpio_func,
    };

    volatile unsigned int print_num = 0;
    int tmp,res;

    input_str = com_get_nextWord(input_str, word_buf);
    tmp = 0;

    while(print_num < PRINT_LIST_FIN)
    {
        if( 0 == check_cmd(word_buf, print_list[print_num], print_list_len[print_num])) break;
        print_num++;
    }

    if(PRINT_LIST_FIN == print_num)
    {
        printf("INPUT PRINT-CMD ERROR\n");
    }
    else
    {
        print_list_func[print_num]((char*)(input_str));
    }

}

void print_fgbuf_func(char* input_str)
{


    printf("PRINT FG_BUF\n");
    for(volatile int i = 0; i < (FG_BUF_SIZE >> 3); i++)
    {
        printf("%5d\t%5d\t%5d\t%5d\t%5d\t%5d\t%5d\t%5d\n", 
                    core1_val.fg_buf[(i << 3)+0],
                    core1_val.fg_buf[(i << 3)+1],
                    core1_val.fg_buf[(i << 3)+2],
                    core1_val.fg_buf[(i << 3)+3],
                    core1_val.fg_buf[(i << 3)+4],
                    core1_val.fg_buf[(i << 3)+5],
                    core1_val.fg_buf[(i << 3)+6],
                    core1_val.fg_buf[(i << 3)+7]
        );
    }
}

void print_gpio_func(char* input_str)
{
    printf("PRINT GPIO\n");

    printf("DAC-OUT: 0x%x\n", gpio.myio.dac.val);
    printf("LED-OUT: RED %d, GREEN %d, BLUE %d\n",
                    gpio.myio.led.red,
                    gpio.myio.led.green,
                    gpio.myio.led.blue
                    );

    printf("gpio.uint_io: 0x%x\n", gpio.uint_io);
    printf("gpio.io.:\n");
    printf("%d %d %d %d %d %d %d %d\n",
            gpio.io.gp0, gpio.io.gp1, gpio.io.gp2, gpio.io.gp3, gpio.io.gp4, gpio.io.gp5, gpio.io.gp6, gpio.io.gp7);
    printf("%d %d %d %d %d %d %d %d\n",
            gpio.io.gp8, gpio.io.gp9, gpio.io.gp10, gpio.io.gp11, gpio.io.gp12, gpio.io.gp13, gpio.io.gp14, gpio.io.gp15);
    printf("%d %d %d %d %d %d %d %d\n",
            gpio.io.gp16, gpio.io.gp17, gpio.io.gp18, gpio.io.gp19, gpio.io.gp20, gpio.io.gp21, gpio.io.gp22, gpio.io.gp23);
    printf("%d %d %d %d %d %d %d %d\n",
            gpio.io.gp24, gpio.io.gp25, gpio.io.gp26, gpio.io.gp27, gpio.io.gp28, gpio.io.gp29, gpio.io.dummy1, gpio.io.dummy2);
    printf("gpio.myio.dac.val: 0x%x\n", gpio.myio.dac.val);

    printf("gpio.myio.dac.bit:\n");
    printf("%d %d %d %d %d %d %d %d\n",
        gpio.myio.dac.bit._0, gpio.myio.dac.bit._1, gpio.myio.dac.bit._2, gpio.myio.dac.bit._3, 
        gpio.myio.dac.bit._4, gpio.myio.dac.bit._5, gpio.myio.dac.bit._6, gpio.myio.dac.bit._7);    
    printf("%d %d %d %d %d %d %d %d\n",
        gpio.myio.dac.bit._8, gpio.myio.dac.bit._9, gpio.myio.dac.bit._A, gpio.myio.dac.bit._B, 
        gpio.myio.dac.bit._C, gpio.myio.dac.bit._D, gpio.myio.dac.bit._E, gpio.myio.dac.bit._F);    
    printf("GPIO-OUT: 0x%x\n",gpio_get_all());

}


void fg_help_func(char* input_str)
{
    printf("CMD LIST\n");
    printf(FG_START_HELP);
    printf(FG_STOP_HELP);
    printf(FG_RESTART_HELP);
    printf(FG_SINE_HELP);
    printf(FG_PULSE_HELP);
    printf(FG_TRIANGLE_HELP);
    printf(FG_SAW_HELP);
    printf(FG_RAMP_HELP);
}

void fg_test_func(char* input_str)
{
    printf(FG_TEST_HELP);

        // int tmp,res;

    // res = com_skip_blank(input_str);
    // if(*input_str == '\0')
    // {
    //     fg_cmdErr(FG_TEST_HELP);
    //     return;
    // }

    // input_str = com_get_nextWord(input_str, word_buf);
    // tmp = word2int(word_buf);
    // if(tmp < 0)
    // {
    //     fg_cmdErr(FG_TEST_HELP);
    //     return;
    // }
        
    // if(core1_val.core1_flag != CORE1_STATUS_START)
    // {
    //     // printf("input_str: %s\n", input_str);
    //     // printf("word_buf: %s\n", word_buf);

    //     tmp = word2int(word_buf);
    //     // printf("tmp: %d\n", tmp);

    //     for(volatile int i = 0; i < FG_BUF_SIZE; i++)
    //     {
    //         // if(((i>>2) & 0x00000001) > 0) core1_val.fg_buf[i] = DAC_OUT_MAX;
    //         // else core1_val.fg_buf[i] = DAC_OUT_MIN;
    //         core1_val.fg_buf[i] = (i & 0x00000001) * DAC_OUT_MAX;
    //         // core1_val.fg_buf[i] = i;
    //         // core1_val.fg_buf[i] = tmp;
    //         // printf("fg_buf[%5d]: %d\n", i, core1_val.fg_buf[i]);
    //     }

    //     if(1 == tmp)        core1_val.nop_func = nop_1_times;
    //     else if (2 == tmp)  core1_val.nop_func = nop_2_times;
    //     else if (3 == tmp)  core1_val.nop_func = nop_3_times;
    //     else if (4 == tmp)  core1_val.nop_func = nop_4_times;
    //     else if (5 == tmp)  core1_val.nop_func = nop_5_times;
    //     else if (6 == tmp)  core1_val.nop_func = nop_6_times;
    //     else                core1_val.nop_func = nop_1us;
    //     // else                core1_val.nop_func = nop_1_times;

    //     // core1_val.fg_buf_max = 5;
    //     // core1_val.count_max = 2;

    //     core1_val.core1_flag = CORE1_STATUS_STOP;
    // }
    // else
    // {
    //     printf("core1 is already started!\n");
    // }

    uint16_t t = 0;
    for(int i = 0; i<65536; i++)
    {
        core1_val.fg_buf[i] = t;
        if(0 == (i%10)) {
            if( t == 0) t = 0xFFFF;
            else        t = 0;
        }
        // else core1_val.fg_buf[i] = 00;
        // core1_val.fg_buf[i] = i;
    }

    
    core1_val.count_max = 65535;
    core1_val.fg_buf_max = 65535;

    // delay = FG_DELAY_10us;
    // core1_val.nop_func = nop_10us;
    // core1_val.nop_func = nop_1us;
    core1_val.nop_func = nop_100us;
    core1_val.core1_flag = CORE1_STATUS_STOP;

}

int fg_mv2digit(int mv)
{
    int digit;

    digit = (mv*DAC_OUT_MAX + (DAC_OUT_mV_MAX >> 1))/DAC_OUT_mV_MAX;

    return digit;
}


FG_VALUE_T fg_freq2datum_and_delay(FG_VALUE_T in)
{
    FG_VALUE_T res;

    int val;
    int mod;
    int freq, delay;
    volatile int datum, i, flag, i_buf, diff;

    res.freq = in.freq;
    freq = in.freq;

    val = (int)(SYSCLK / freq);
    mod = SYSCLK - (val*freq);

    res.freq_mod = mod;

    if(freq < 16)
    {
        delay = FG_DELAY_10us;
        core1_val.nop_func = nop_10us;
    }
    else if(freq < 50)
    {
        delay = FG_DELAY_1us;
        core1_val.nop_func = nop_1us;
    }
    else
    {
        delay = FG_DELAY_MIN;
        core1_val.nop_func = nop_1_times;    
    }

    i = 65536;
    diff = i;

    while(  (i > 1) &&
            (diff > 0) )
            // (delay > FG_DELAY_MAX) )
            // (val - (i*delay) < FG_ERR_TH) |
            // ((i*delay) - val > (-1 * FG_ERR_TH))   )
    {

        diff = (diff >> 1);


        if((val / i) < delay)
        {
            i = i - diff;
        }
        else
        {
            i = i + diff;
        }

        if(i > 65536)
        { 
            i = 65536;
            break;
        }
    }

    datum = i;

    /* for check*/
    while((val - (datum*delay)) > datum)
    {
        delay++;
    }

    res.datum = datum;
    res.delay = delay;
    
    return res;
}

int getSin(int in, int min, int max)
{
    
    static const short sin_array[FG_SINE_SIZE] = {
        0,   3,   6,   9,  13,  16,  19,  22,  25,  28,  31,  35,  38,  41,  44,  47,
        50,  53,  57,  60,  63,  66,  69,  72,  75,  79,  82,  85,  88,  91,  94,  97,
        101, 104, 107, 110, 113, 116, 119, 123, 126, 129, 132, 135, 138, 141, 145, 148,
        151, 154, 157, 160, 163, 166, 170, 173, 176, 179, 182, 185, 188, 192, 195, 198,
        201, 204, 207, 210, 214, 217, 220, 223, 226, 229, 232, 236, 239, 242, 245, 248,
        251, 254, 258, 261, 264, 267, 270, 273, 276, 280, 283, 286, 289, 292, 295, 298,
        302, 305, 308, 311, 314, 317, 320, 323, 327, 330, 333, 336, 339, 342, 345, 349,
        352, 355, 358, 361, 364, 367, 371, 374, 377, 380, 383, 386, 389, 393, 396, 399,
        402, 405, 408, 411, 415, 418, 421, 424, 427, 430, 433, 436, 440, 443, 446, 449,
        452, 455, 458, 462, 465, 468, 471, 474, 477, 480, 484, 487, 490, 493, 496, 499,
        502, 505, 509, 512, 515, 518, 521, 524, 527, 531, 534, 537, 540, 543, 546, 549,
        553, 556, 559, 562, 565, 568, 571, 574, 578, 581, 584, 587, 590, 593, 596, 600,
        603, 606, 609, 612, 615, 618, 621, 625, 628, 631, 634, 637, 640, 643, 646, 650,
        653, 656, 659, 662, 665, 668, 672, 675, 678, 681, 684, 687, 690, 693, 697, 700,
        703, 706, 709, 712, 715, 719, 722, 725, 728, 731, 734, 737, 740, 744, 747, 750,
        753, 756, 759, 762, 765, 769, 772, 775, 778, 781, 784, 787, 790, 794, 797, 800,
        803, 806, 809, 812, 815, 819, 822, 825, 828, 831, 834, 837, 840, 844, 847, 850,
        853, 856, 859, 862, 865, 869, 872, 875, 878, 881, 884, 887, 890, 894, 897, 900,
        903, 906, 909, 912, 915, 919, 922, 925, 928, 931, 934, 937, 940, 944, 947, 950,
        953, 956, 959, 962, 965, 968, 972, 975, 978, 981, 984, 987, 990, 993, 997,1000,
        1003,1006,1009,1012,1015,1018,1021,1025,1028,1031,1034,1037,1040,1043,1046,1050,
        1053,1056,1059,1062,1065,1068,1071,1074,1078,1081,1084,1087,1090,1093,1096,1099,
        1102,1106,1109,1112,1115,1118,1121,1124,1127,1130,1134,1137,1140,1143,1146,1149,
        1152,1155,1158,1162,1165,1168,1171,1174,1177,1180,1183,1186,1190,1193,1196,1199,
        1202,1205,1208,1211,1214,1218,1221,1224,1227,1230,1233,1236,1239,1242,1246,1249,
        1252,1255,1258,1261,1264,1267,1270,1273,1277,1280,1283,1286,1289,1292,1295,1298,
        1301,1304,1308,1311,1314,1317,1320,1323,1326,1329,1332,1335,1339,1342,1345,1348,
        1351,1354,1357,1360,1363,1366,1370,1373,1376,1379,1382,1385,1388,1391,1394,1397,
        1401,1404,1407,1410,1413,1416,1419,1422,1425,1428,1431,1435,1438,1441,1444,1447,
        1450,1453,1456,1459,1462,1465,1469,1472,1475,1478,1481,1484,1487,1490,1493,1496,
        1499,1503,1506,1509,1512,1515,1518,1521,1524,1527,1530,1533,1537,1540,1543,1546,
        1549,1552,1555,1558,1561,1564,1567,1570,1574,1577,1580,1583,1586,1589,1592,1595,
        1598,1601,1604,1607,1611,1614,1617,1620,1623,1626,1629,1632,1635,1638,1641,1644,
        1647,1651,1654,1657,1660,1663,1666,1669,1672,1675,1678,1681,1684,1687,1691,1694,
        1697,1700,1703,1706,1709,1712,1715,1718,1721,1724,1727,1730,1734,1737,1740,1743,
        1746,1749,1752,1755,1758,1761,1764,1767,1770,1773,1776,1780,1783,1786,1789,1792,
        1795,1798,1801,1804,1807,1810,1813,1816,1819,1822,1826,1829,1832,1835,1838,1841,
        1844,1847,1850,1853,1856,1859,1862,1865,1868,1871,1874,1878,1881,1884,1887,1890,
        1893,1896,1899,1902,1905,1908,1911,1914,1917,1920,1923,1926,1929,1933,1936,1939,
        1942,1945,1948,1951,1954,1957,1960,1963,1966,1969,1972,1975,1978,1981,1984,1987,
        1990,1994,1997,2000,2003,2006,2009,2012,2015,2018,2021,2024,2027,2030,2033,2036,
        2039,2042,2045,2048,2051,2054,2057,2061,2064,2067,2070,2073,2076,2079,2082,2085,
        2088,2091,2094,2097,2100,2103,2106,2109,2112,2115,2118,2121,2124,2127,2130,2133,
        2136,2139,2142,2146,2149,2152,2155,2158,2161,2164,2167,2170,2173,2176,2179,2182,
        2185,2188,2191,2194,2197,2200,2203,2206,2209,2212,2215,2218,2221,2224,2227,2230,
        2233,2236,2239,2242,2245,2248,2251,2254,2257,2261,2264,2267,2270,2273,2276,2279,
        2282,2285,2288,2291,2294,2297,2300,2303,2306,2309,2312,2315,2318,2321,2324,2327,
        2330,2333,2336,2339,2342,2345,2348,2351,2354,2357,2360,2363,2366,2369,2372,2375,
        2378,2381,2384,2387,2390,2393,2396,2399,2402,2405,2408,2411,2414,2417,2420,2423,
        2426,2429,2432,2435,2438,2441,2444,2447,2450,2453,2456,2459,2462,2465,2468,2471,
        2474,2477,2480,2483,2486,2489,2492,2495,2498,2501,2504,2507,2510,2513,2516,2519,
        2522,2525,2528,2531,2534,2537,2540,2543,2546,2549,2552,2555,2558,2561,2564,2567,
        2570,2573,2576,2579,2582,2585,2588,2591,2594,2597,2599,2602,2605,2608,2611,2614,
        2617,2620,2623,2626,2629,2632,2635,2638,2641,2644,2647,2650,2653,2656,2659,2662,
        2665,2668,2671,2674,2677,2680,2683,2686,2689,2692,2695,2698,2701,2704,2706,2709,
        2712,2715,2718,2721,2724,2727,2730,2733,2736,2739,2742,2745,2748,2751,2754,2757,
        2760,2763,2766,2769,2772,2775,2778,2780,2783,2786,2789,2792,2795,2798,2801,2804,
        2807,2810,2813,2816,2819,2822,2825,2828,2831,2834,2837,2840,2842,2845,2848,2851,
        2854,2857,2860,2863,2866,2869,2872,2875,2878,2881,2884,2887,2890,2892,2895,2898,
        2901,2904,2907,2910,2913,2916,2919,2922,2925,2928,2931,2934,2937,2939,2942,2945,
        2948,2951,2954,2957,2960,2963,2966,2969,2972,2975,2978,2980,2983,2986,2989,2992,
        2995,2998,3001,3004,3007,3010,3013,3016,3018,3021,3024,3027,3030,3033,3036,3039,
        3042,3045,3048,3051,3053,3056,3059,3062,3065,3068,3071,3074,3077,3080,3083,3086,
        3088,3091,3094,3097,3100,3103,3106,3109,3112,3115,3118,3120,3123,3126,3129,3132,
        3135,3138,3141,3144,3147,3149,3152,3155,3158,3161,3164,3167,3170,3173,3176,3178,
        3181,3184,3187,3190,3193,3196,3199,3202,3204,3207,3210,3213,3216,3219,3222,3225,
        3228,3230,3233,3236,3239,3242,3245,3248,3251,3254,3256,3259,3262,3265,3268,3271,
        3274,3277,3279,3282,3285,3288,3291,3294,3297,3300,3302,3305,3308,3311,3314,3317,
        3320,3323,3325,3328,3331,3334,3337,3340,3343,3346,3348,3351,3354,3357,3360,3363,
        3366,3368,3371,3374,3377,3380,3383,3386,3389,3391,3394,3397,3400,3403,3406,3409,
        3411,3414,3417,3420,3423,3426,3429,3431,3434,3437,3440,3443,3446,3448,3451,3454,
        3457,3460,3463,3466,3468,3471,3474,3477,3480,3483,3485,3488,3491,3494,3497,3500,
        3503,3505,3508,3511,3514,3517,3520,3522,3525,3528,3531,3534,3537,3539,3542,3545,
        3548,3551,3554,3556,3559,3562,3565,3568,3571,3573,3576,3579,3582,3585,3587,3590,
        3593,3596,3599,3602,3604,3607,3610,3613,3616,3619,3621,3624,3627,3630,3633,3635,
        3638,3641,3644,3647,3650,3652,3655,3658,3661,3664,3666,3669,3672,3675,3678,3680,
        3683,3686,3689,3692,3694,3697,3700,3703,3706,3708,3711,3714,3717,3720,3722,3725,
        3728,3731,3734,3736,3739,3742,3745,3748,3750,3753,3756,3759,3762,3764,3767,3770,
        3773,3776,3778,3781,3784,3787,3789,3792,3795,3798,3801,3803,3806,3809,3812,3814,
        3817,3820,3823,3826,3828,3831,3834,3837,3839,3842,3845,3848,3851,3853,3856,3859,
        3862,3864,3867,3870,3873,3876,3878,3881,3884,3887,3889,3892,3895,3898,3900,3903,
        3906,3909,3911,3914,3917,3920,3922,3925,3928,3931,3934,3936,3939,3942,3945,3947,
        3950,3953,3956,3958,3961,3964,3967,3969,3972,3975,3978,3980,3983,3986,3989,3991,
        3994,3997,3999,4002,4005,4008,4010,4013,4016,4019,4021,4024,4027,4030,4032,4035,
        4038,4041,4043,4046,4049,4051,4054,4057,4060,4062,4065,4068,4071,4073,4076,4079,
        4081,4084,4087,4090,4092,4095,4098,4101,4103,4106,4109,4111,4114,4117,4120,4122,
        4125,4128,4130,4133,4136,4139,4141,4144,4147,4149,4152,4155,4158,4160,4163,4166,
        4168,4171,4174,4176,4179,4182,4185,4187,4190,4193,4195,4198,4201,4203,4206,4209,
        4212,4214,4217,4220,4222,4225,4228,4230,4233,4236,4238,4241,4244,4247,4249,4252,
        4255,4257,4260,4263,4265,4268,4271,4273,4276,4279,4281,4284,4287,4289,4292,4295,
        4297,4300,4303,4305,4308,4311,4313,4316,4319,4321,4324,4327,4329,4332,4335,4337,
        4340,4343,4345,4348,4351,4353,4356,4359,4361,4364,4367,4369,4372,4375,4377,4380,
        4383,4385,4388,4391,4393,4396,4399,4401,4404,4407,4409,4412,4415,4417,4420,4422,
        4425,4428,4430,4433,4436,4438,4441,4444,4446,4449,4451,4454,4457,4459,4462,4465,
        4467,4470,4473,4475,4478,4480,4483,4486,4488,4491,4494,4496,4499,4501,4504,4507,
        4509,4512,4515,4517,4520,4522,4525,4528,4530,4533,4536,4538,4541,4543,4546,4549,
        4551,4554,4556,4559,4562,4564,4567,4569,4572,4575,4577,4580,4583,4585,4588,4590,
        4593,4596,4598,4601,4603,4606,4609,4611,4614,4616,4619,4622,4624,4627,4629,4632,
        4634,4637,4640,4642,4645,4647,4650,4653,4655,4658,4660,4663,4666,4668,4671,4673,
        4676,4678,4681,4684,4686,4689,4691,4694,4696,4699,4702,4704,4707,4709,4712,4714,
        4717,4720,4722,4725,4727,4730,4732,4735,4738,4740,4743,4745,4748,4750,4753,4755,
        4758,4761,4763,4766,4768,4771,4773,4776,4778,4781,4784,4786,4789,4791,4794,4796,
        4799,4801,4804,4806,4809,4812,4814,4817,4819,4822,4824,4827,4829,4832,4834,4837,
        4840,4842,4845,4847,4850,4852,4855,4857,4860,4862,4865,4867,4870,4872,4875,4877,
        4880,4882,4885,4888,4890,4893,4895,4898,4900,4903,4905,4908,4910,4913,4915,4918,
        4920,4923,4925,4928,4930,4933,4935,4938,4940,4943,4945,4948,4950,4953,4955,4958,
        4960,4963,4965,4968,4970,4973,4975,4978,4980,4983,4985,4988,4990,4993,4995,4998,
        5000,5003,5005,5008,5010,5013,5015,5018,5020,5023,5025,5028,5030,5033,5035,5038,
        5040,5042,5045,5047,5050,5052,5055,5057,5060,5062,5065,5067,5070,5072,5075,5077,
        5080,5082,5084,5087,5089,5092,5094,5097,5099,5102,5104,5107,5109,5111,5114,5116,
        5119,5121,5124,5126,5129,5131,5134,5136,5138,5141,5143,5146,5148,5151,5153,5156,
        5158,5160,5163,5165,5168,5170,5173,5175,5177,5180,5182,5185,5187,5190,5192,5195,
        5197,5199,5202,5204,5207,5209,5212,5214,5216,5219,5221,5224,5226,5228,5231,5233,
        5236,5238,5241,5243,5245,5248,5250,5253,5255,5257,5260,5262,5265,5267,5269,5272,
        5274,5277,5279,5281,5284,5286,5289,5291,5293,5296,5298,5301,5303,5305,5308,5310,
        5313,5315,5317,5320,5322,5325,5327,5329,5332,5334,5337,5339,5341,5344,5346,5348,
        5351,5353,5356,5358,5360,5363,5365,5367,5370,5372,5375,5377,5379,5382,5384,5386,
        5389,5391,5393,5396,5398,5401,5403,5405,5408,5410,5412,5415,5417,5419,5422,5424,
        5427,5429,5431,5434,5436,5438,5441,5443,5445,5448,5450,5452,5455,5457,5459,5462,
        5464,5466,5469,5471,5473,5476,5478,5480,5483,5485,5487,5490,5492,5494,5497,5499,
        5501,5504,5506,5508,5511,5513,5515,5518,5520,5522,5525,5527,5529,5532,5534,5536,
        5539,5541,5543,5545,5548,5550,5552,5555,5557,5559,5562,5564,5566,5569,5571,5573,
        5575,5578,5580,5582,5585,5587,5589,5592,5594,5596,5598,5601,5603,5605,5608,5610,
        5612,5614,5617,5619,5621,5624,5626,5628,5630,5633,5635,5637,5640,5642,5644,5646,
        5649,5651,5653,5656,5658,5660,5662,5665,5667,5669,5671,5674,5676,5678,5680,5683,
        5685,5687,5690,5692,5694,5696,5699,5701,5703,5705,5708,5710,5712,5714,5717,5719,
        5721,5723,5726,5728,5730,5732,5735,5737,5739,5741,5744,5746,5748,5750,5752,5755,
        5757,5759,5761,5764,5766,5768,5770,5773,5775,5777,5779,5782,5784,5786,5788,5790,
        5793,5795,5797,5799,5801,5804,5806,5808,5810,5813,5815,5817,5819,5821,5824,5826,
        5828,5830,5832,5835,5837,5839,5841,5843,5846,5848,5850,5852,5854,5857,5859,5861,
        5863,5865,5868,5870,5872,5874,5876,5879,5881,5883,5885,5887,5890,5892,5894,5896,
        5898,5900,5903,5905,5907,5909,5911,5914,5916,5918,5920,5922,5924,5927,5929,5931,
        5933,5935,5937,5940,5942,5944,5946,5948,5950,5952,5955,5957,5959,5961,5963,5965,
        5968,5970,5972,5974,5976,5978,5980,5983,5985,5987,5989,5991,5993,5995,5998,6000,
        6002,6004,6006,6008,6010,6013,6015,6017,6019,6021,6023,6025,6027,6030,6032,6034,
        6036,6038,6040,6042,6044,6047,6049,6051,6053,6055,6057,6059,6061,6064,6066,6068,
        6070,6072,6074,6076,6078,6080,6083,6085,6087,6089,6091,6093,6095,6097,6099,6101,
        6104,6106,6108,6110,6112,6114,6116,6118,6120,6122,6124,6127,6129,6131,6133,6135,
        6137,6139,6141,6143,6145,6147,6149,6151,6154,6156,6158,6160,6162,6164,6166,6168,
        6170,6172,6174,6176,6178,6180,6182,6185,6187,6189,6191,6193,6195,6197,6199,6201,
        6203,6205,6207,6209,6211,6213,6215,6217,6219,6221,6224,6226,6228,6230,6232,6234,
        6236,6238,6240,6242,6244,6246,6248,6250,6252,6254,6256,6258,6260,6262,6264,6266,
        6268,6270,6272,6274,6276,6278,6280,6282,6284,6286,6288,6290,6292,6294,6296,6298,
        6300,6303,6305,6307,6309,6311,6313,6315,6317,6319,6321,6323,6325,6327,6329,6331,
        6333,6334,6336,6338,6340,6342,6344,6346,6348,6350,6352,6354,6356,6358,6360,6362,
        6364,6366,6368,6370,6372,6374,6376,6378,6380,6382,6384,6386,6388,6390,6392,6394,
        6396,6398,6400,6402,6404,6406,6408,6410,6411,6413,6415,6417,6419,6421,6423,6425,
        6427,6429,6431,6433,6435,6437,6439,6441,6443,6445,6447,6448,6450,6452,6454,6456,
        6458,6460,6462,6464,6466,6468,6470,6472,6474,6475,6477,6479,6481,6483,6485,6487,
        6489,6491,6493,6495,6497,6499,6500,6502,6504,6506,6508,6510,6512,6514,6516,6518,
        6519,6521,6523,6525,6527,6529,6531,6533,6535,6537,6538,6540,6542,6544,6546,6548,
        6550,6552,6554,6555,6557,6559,6561,6563,6565,6567,6569,6571,6572,6574,6576,6578,
        6580,6582,6584,6585,6587,6589,6591,6593,6595,6597,6599,6600,6602,6604,6606,6608,
        6610,6612,6613,6615,6617,6619,6621,6623,6625,6626,6628,6630,6632,6634,6636,6637,
        6639,6641,6643,6645,6647,6648,6650,6652,6654,6656,6658,6659,6661,6663,6665,6667,
        6669,6670,6672,6674,6676,6678,6680,6681,6683,6685,6687,6689,6690,6692,6694,6696,
        6698,6699,6701,6703,6705,6707,6708,6710,6712,6714,6716,6717,6719,6721,6723,6725,
        6726,6728,6730,6732,6734,6735,6737,6739,6741,6743,6744,6746,6748,6750,6751,6753,
        6755,6757,6759,6760,6762,6764,6766,6767,6769,6771,6773,6775,6776,6778,6780,6782,
        6783,6785,6787,6789,6790,6792,6794,6796,6797,6799,6801,6803,6804,6806,6808,6810,
        6811,6813,6815,6817,6818,6820,6822,6824,6825,6827,6829,6831,6832,6834,6836,6837,
        6839,6841,6843,6844,6846,6848,6850,6851,6853,6855,6856,6858,6860,6862,6863,6865,
        6867,6868,6870,6872,6874,6875,6877,6879,6880,6882,6884,6886,6887,6889,6891,6892,
        6894,6896,6897,6899,6901,6902,6904,6906,6908,6909,6911,6913,6914,6916,6918,6919,
        6921,6923,6924,6926,6928,6929,6931,6933,6934,6936,6938,6939,6941,6943,6944,6946,
        6948,6949,6951,6953,6954,6956,6958,6959,6961,6963,6964,6966,6968,6969,6971,6973,
        6974,6976,6978,6979,6981,6983,6984,6986,6987,6989,6991,6992,6994,6996,6997,6999,
        7001,7002,7004,7005,7007,7009,7010,7012,7014,7015,7017,7018,7020,7022,7023,7025,
        7027,7028,7030,7031,7033,7035,7036,7038,7039,7041,7043,7044,7046,7047,7049,7051,
        7052,7054,7055,7057,7059,7060,7062,7063,7065,7067,7068,7070,7071,7073,7074,7076,
        7078,7079,7081,7082,7084,7086,7087,7089,7090,7092,7093,7095,7097,7098,7100,7101,
        7103,7104,7106,7108,7109,7111,7112,7114,7115,7117,7118,7120,7122,7123,7125,7126,
        7128,7129,7131,7132,7134,7135,7137,7139,7140,7142,7143,7145,7146,7148,7149,7151,
        7152,7154,7155,7157,7159,7160,7162,7163,7165,7166,7168,7169,7171,7172,7174,7175,
        7177,7178,7180,7181,7183,7184,7186,7187,7189,7190,7192,7193,7195,7196,7198,7199,
        7201,7202,7204,7205,7207,7208,7210,7211,7213,7214,7216,7217,7219,7220,7222,7223,
        7225,7226,7228,7229,7231,7232,7234,7235,7237,7238,7239,7241,7242,7244,7245,7247,
        7248,7250,7251,7253,7254,7256,7257,7258,7260,7261,7263,7264,7266,7267,7269,7270,
        7272,7273,7274,7276,7277,7279,7280,7282,7283,7285,7286,7287,7289,7290,7292,7293,
        7295,7296,7297,7299,7300,7302,7303,7305,7306,7307,7309,7310,7312,7313,7314,7316,
        7317,7319,7320,7322,7323,7324,7326,7327,7329,7330,7331,7333,7334,7336,7337,7338,
        7340,7341,7343,7344,7345,7347,7348,7349,7351,7352,7354,7355,7356,7358,7359,7361,
        7362,7363,7365,7366,7367,7369,7370,7372,7373,7374,7376,7377,7378,7380,7381,7382,
        7384,7385,7387,7388,7389,7391,7392,7393,7395,7396,7397,7399,7400,7401,7403,7404,
        7405,7407,7408,7410,7411,7412,7414,7415,7416,7418,7419,7420,7422,7423,7424,7426,
        7427,7428,7429,7431,7432,7433,7435,7436,7437,7439,7440,7441,7443,7444,7445,7447,
        7448,7449,7451,7452,7453,7454,7456,7457,7458,7460,7461,7462,7464,7465,7466,7467,
        7469,7470,7471,7473,7474,7475,7476,7478,7479,7480,7482,7483,7484,7485,7487,7488,
        7489,7490,7492,7493,7494,7496,7497,7498,7499,7501,7502,7503,7504,7506,7507,7508,
        7509,7511,7512,7513,7514,7516,7517,7518,7519,7521,7522,7523,7524,7526,7527,7528,
        7529,7531,7532,7533,7534,7536,7537,7538,7539,7540,7542,7543,7544,7545,7547,7548,
        7549,7550,7551,7553,7554,7555,7556,7558,7559,7560,7561,7562,7564,7565,7566,7567,
        7568,7570,7571,7572,7573,7574,7576,7577,7578,7579,7580,7582,7583,7584,7585,7586,
        7588,7589,7590,7591,7592,7593,7595,7596,7597,7598,7599,7600,7602,7603,7604,7605,
        7606,7607,7609,7610,7611,7612,7613,7614,7616,7617,7618,7619,7620,7621,7623,7624,
        7625,7626,7627,7628,7629,7631,7632,7633,7634,7635,7636,7637,7639,7640,7641,7642,
        7643,7644,7645,7646,7648,7649,7650,7651,7652,7653,7654,7655,7657,7658,7659,7660,
        7661,7662,7663,7664,7665,7667,7668,7669,7670,7671,7672,7673,7674,7675,7676,7678,
        7679,7680,7681,7682,7683,7684,7685,7686,7687,7688,7690,7691,7692,7693,7694,7695,
        7696,7697,7698,7699,7700,7701,7702,7704,7705,7706,7707,7708,7709,7710,7711,7712,
        7713,7714,7715,7716,7717,7718,7719,7721,7722,7723,7724,7725,7726,7727,7728,7729,
        7730,7731,7732,7733,7734,7735,7736,7737,7738,7739,7740,7741,7742,7743,7744,7745,
        7746,7747,7748,7749,7750,7752,7753,7754,7755,7756,7757,7758,7759,7760,7761,7762,
        7763,7764,7765,7766,7767,7768,7769,7770,7771,7772,7773,7774,7775,7776,7777,7778,
        7779,7780,7781,7781,7782,7783,7784,7785,7786,7787,7788,7789,7790,7791,7792,7793,
        7794,7795,7796,7797,7798,7799,7800,7801,7802,7803,7804,7805,7806,7807,7808,7809,
        7809,7810,7811,7812,7813,7814,7815,7816,7817,7818,7819,7820,7821,7822,7823,7824,
        7825,7825,7826,7827,7828,7829,7830,7831,7832,7833,7834,7835,7836,7837,7837,7838,
        7839,7840,7841,7842,7843,7844,7845,7846,7847,7847,7848,7849,7850,7851,7852,7853,
        7854,7855,7855,7856,7857,7858,7859,7860,7861,7862,7863,7863,7864,7865,7866,7867,
        7868,7869,7870,7870,7871,7872,7873,7874,7875,7876,7877,7877,7878,7879,7880,7881,
        7882,7883,7883,7884,7885,7886,7887,7888,7889,7889,7890,7891,7892,7893,7894,7894,
        7895,7896,7897,7898,7899,7899,7900,7901,7902,7903,7904,7904,7905,7906,7907,7908,
        7909,7909,7910,7911,7912,7913,7913,7914,7915,7916,7917,7917,7918,7919,7920,7921,
        7921,7922,7923,7924,7925,7925,7926,7927,7928,7929,7929,7930,7931,7932,7933,7933,
        7934,7935,7936,7936,7937,7938,7939,7940,7940,7941,7942,7943,7943,7944,7945,7946,
        7946,7947,7948,7949,7950,7950,7951,7952,7953,7953,7954,7955,7956,7956,7957,7958,
        7959,7959,7960,7961,7962,7962,7963,7964,7964,7965,7966,7967,7967,7968,7969,7970,
        7970,7971,7972,7972,7973,7974,7975,7975,7976,7977,7978,7978,7979,7980,7980,7981,
        7982,7982,7983,7984,7985,7985,7986,7987,7987,7988,7989,7989,7990,7991,7992,7992,
        7993,7994,7994,7995,7996,7996,7997,7998,7998,7999,8000,8000,8001,8002,8002,8003,
        8004,8004,8005,8006,8006,8007,8008,8008,8009,8010,8010,8011,8012,8012,8013,8014,
        8014,8015,8016,8016,8017,8018,8018,8019,8020,8020,8021,8021,8022,8023,8023,8024,
        8025,8025,8026,8027,8027,8028,8028,8029,8030,8030,8031,8032,8032,8033,8033,8034,
        8035,8035,8036,8036,8037,8038,8038,8039,8039,8040,8041,8041,8042,8042,8043,8044,
        8044,8045,8045,8046,8047,8047,8048,8048,8049,8050,8050,8051,8051,8052,8052,8053,
        8054,8054,8055,8055,8056,8056,8057,8058,8058,8059,8059,8060,8060,8061,8062,8062,
        8063,8063,8064,8064,8065,8065,8066,8067,8067,8068,8068,8069,8069,8070,8070,8071,
        8071,8072,8072,8073,8074,8074,8075,8075,8076,8076,8077,8077,8078,8078,8079,8079,
        8080,8080,8081,8081,8082,8082,8083,8083,8084,8084,8085,8085,8086,8086,8087,8087,
        8088,8088,8089,8089,8090,8090,8091,8091,8092,8092,8093,8093,8094,8094,8095,8095,
        8096,8096,8097,8097,8098,8098,8099,8099,8100,8100,8101,8101,8101,8102,8102,8103,
        8103,8104,8104,8105,8105,8106,8106,8107,8107,8107,8108,8108,8109,8109,8110,8110,
        8111,8111,8111,8112,8112,8113,8113,8114,8114,8114,8115,8115,8116,8116,8117,8117,
        8117,8118,8118,8119,8119,8120,8120,8120,8121,8121,8122,8122,8122,8123,8123,8124,
        8124,8124,8125,8125,8126,8126,8126,8127,8127,8128,8128,8128,8129,8129,8130,8130,
        8130,8131,8131,8132,8132,8132,8133,8133,8133,8134,8134,8135,8135,8135,8136,8136,
        8136,8137,8137,8137,8138,8138,8139,8139,8139,8140,8140,8140,8141,8141,8141,8142,
        8142,8142,8143,8143,8143,8144,8144,8144,8145,8145,8145,8146,8146,8146,8147,8147,
        8147,8148,8148,8148,8149,8149,8149,8150,8150,8150,8151,8151,8151,8152,8152,8152,
        8153,8153,8153,8153,8154,8154,8154,8155,8155,8155,8156,8156,8156,8156,8157,8157,
        8157,8158,8158,8158,8158,8159,8159,8159,8160,8160,8160,8160,8161,8161,8161,8162,
        8162,8162,8162,8163,8163,8163,8163,8164,8164,8164,8164,8165,8165,8165,8165,8166,
        8166,8166,8166,8167,8167,8167,8167,8168,8168,8168,8168,8169,8169,8169,8169,8170,
        8170,8170,8170,8170,8171,8171,8171,8171,8172,8172,8172,8172,8172,8173,8173,8173,
        8173,8174,8174,8174,8174,8174,8175,8175,8175,8175,8175,8176,8176,8176,8176,8176,
        8177,8177,8177,8177,8177,8178,8178,8178,8178,8178,8178,8179,8179,8179,8179,8179,
        8180,8180,8180,8180,8180,8180,8181,8181,8181,8181,8181,8181,8182,8182,8182,8182,
        8182,8182,8182,8183,8183,8183,8183,8183,8183,8183,8184,8184,8184,8184,8184,8184,
        8184,8185,8185,8185,8185,8185,8185,8185,8185,8186,8186,8186,8186,8186,8186,8186,
        8186,8187,8187,8187,8187,8187,8187,8187,8187,8187,8188,8188,8188,8188,8188,8188,
        8188,8188,8188,8188,8189,8189,8189,8189,8189,8189,8189,8189,8189,8189,8189,8189,
        8190,8190,8190,8190,8190,8190,8190,8190,8190,8190,8190,8190,8190,8190,8190,8191,
        8191,8191,8191,8191,8191,8191,8191,8191,8191,8191,8191,8191,8191,8191,8191,8191,
        8191,8191,8191,8191,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,
        8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,
        8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,
        8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8191,8191,8191,
        8191,8191,8191,8191,8191,8191,8191,8191,8191,8191,8191,8191,8191,8191,8191,8191,
        8191,8191,8190,8190,8190,8190,8190,8190,8190,8190,8190,8190,8190,8190,8190,8190,
        8190,8189,8189,8189,8189,8189,8189,8189,8189,8189,8189,8189,8189,8188,8188,8188,
        8188,8188,8188,8188,8188,8188,8188,8187,8187,8187,8187,8187,8187,8187,8187,8187,
        8186,8186,8186,8186,8186,8186,8186,8186,8185,8185,8185,8185,8185,8185,8185,8185,
        8184,8184,8184,8184,8184,8184,8184,8183,8183,8183,8183,8183,8183,8183,8182,8182,
        8182,8182,8182,8182,8182,8181,8181,8181,8181,8181,8181,8180,8180,8180,8180,8180,
        8180,8179,8179,8179,8179,8179,8178,8178,8178,8178,8178,8178,8177,8177,8177,8177,
        8177,8176,8176,8176,8176,8176,8175,8175,8175,8175,8175,8174,8174,8174,8174,8174,
        8173,8173,8173,8173,8172,8172,8172,8172,8172,8171,8171,8171,8171,8170,8170,8170,
        8170,8170,8169,8169,8169,8169,8168,8168,8168,8168,8167,8167,8167,8167,8166,8166,
        8166,8166,8165,8165,8165,8165,8164,8164,8164,8164,8163,8163,8163,8163,8162,8162,
        8162,8162,8161,8161,8161,8160,8160,8160,8160,8159,8159,8159,8158,8158,8158,8158,
        8157,8157,8157,8156,8156,8156,8156,8155,8155,8155,8154,8154,8154,8153,8153,8153,
        8153,8152,8152,8152,8151,8151,8151,8150,8150,8150,8149,8149,8149,8148,8148,8148,
        8147,8147,8147,8146,8146,8146,8145,8145,8145,8144,8144,8144,8143,8143,8143,8142,
        8142,8142,8141,8141,8141,8140,8140,8140,8139,8139,8139,8138,8138,8137,8137,8137,
        8136,8136,8136,8135,8135,8135,8134,8134,8133,8133,8133,8132,8132,8132,8131,8131,
        8130,8130,8130,8129,8129,8128,8128,8128,8127,8127,8126,8126,8126,8125,8125,8124,
        8124,8124,8123,8123,8122,8122,8122,8121,8121,8120,8120,8120,8119,8119,8118,8118,
        8117,8117,8117,8116,8116,8115,8115,8114,8114,8114,8113,8113,8112,8112,8111,8111,
        8111,8110,8110,8109,8109,8108,8108,8107,8107,8107,8106,8106,8105,8105,8104,8104,
        8103,8103,8102,8102,8101,8101,8101,8100,8100,8099,8099,8098,8098,8097,8097,8096,
        8096,8095,8095,8094,8094,8093,8093,8092,8092,8091,8091,8090,8090,8089,8089,8088,
        8088,8087,8087,8086,8086,8085,8085,8084,8084,8083,8083,8082,8082,8081,8081,8080,
        8080,8079,8079,8078,8078,8077,8077,8076,8076,8075,8075,8074,8074,8073,8072,8072,
        8071,8071,8070,8070,8069,8069,8068,8068,8067,8067,8066,8065,8065,8064,8064,8063,
        8063,8062,8062,8061,8060,8060,8059,8059,8058,8058,8057,8056,8056,8055,8055,8054,
        8054,8053,8052,8052,8051,8051,8050,8050,8049,8048,8048,8047,8047,8046,8045,8045,
        8044,8044,8043,8042,8042,8041,8041,8040,8039,8039,8038,8038,8037,8036,8036,8035,
        8035,8034,8033,8033,8032,8032,8031,8030,8030,8029,8028,8028,8027,8027,8026,8025,
        8025,8024,8023,8023,8022,8021,8021,8020,8020,8019,8018,8018,8017,8016,8016,8015,
        8014,8014,8013,8012,8012,8011,8010,8010,8009,8008,8008,8007,8006,8006,8005,8004,
        8004,8003,8002,8002,8001,8000,8000,7999,7998,7998,7997,7996,7996,7995,7994,7994,
        7993,7992,7992,7991,7990,7989,7989,7988,7987,7987,7986,7985,7985,7984,7983,7982,
        7982,7981,7980,7980,7979,7978,7978,7977,7976,7975,7975,7974,7973,7972,7972,7971,
        7970,7970,7969,7968,7967,7967,7966,7965,7964,7964,7963,7962,7962,7961,7960,7959,
        7959,7958,7957,7956,7956,7955,7954,7953,7953,7952,7951,7950,7950,7949,7948,7947,
        7946,7946,7945,7944,7943,7943,7942,7941,7940,7940,7939,7938,7937,7936,7936,7935,
        7934,7933,7933,7932,7931,7930,7929,7929,7928,7927,7926,7925,7925,7924,7923,7922,
        7921,7921,7920,7919,7918,7917,7917,7916,7915,7914,7913,7913,7912,7911,7910,7909,
        7909,7908,7907,7906,7905,7904,7904,7903,7902,7901,7900,7899,7899,7898,7897,7896,
        7895,7894,7894,7893,7892,7891,7890,7889,7889,7888,7887,7886,7885,7884,7883,7883,
        7882,7881,7880,7879,7878,7877,7877,7876,7875,7874,7873,7872,7871,7870,7870,7869,
        7868,7867,7866,7865,7864,7863,7863,7862,7861,7860,7859,7858,7857,7856,7855,7855,
        7854,7853,7852,7851,7850,7849,7848,7847,7847,7846,7845,7844,7843,7842,7841,7840,
        7839,7838,7837,7837,7836,7835,7834,7833,7832,7831,7830,7829,7828,7827,7826,7825,
        7825,7824,7823,7822,7821,7820,7819,7818,7817,7816,7815,7814,7813,7812,7811,7810,
        7809,7809,7808,7807,7806,7805,7804,7803,7802,7801,7800,7799,7798,7797,7796,7795,
        7794,7793,7792,7791,7790,7789,7788,7787,7786,7785,7784,7783,7782,7781,7781,7780,
        7779,7778,7777,7776,7775,7774,7773,7772,7771,7770,7769,7768,7767,7766,7765,7764,
        7763,7762,7761,7760,7759,7758,7757,7756,7755,7754,7753,7752,7750,7749,7748,7747,
        7746,7745,7744,7743,7742,7741,7740,7739,7738,7737,7736,7735,7734,7733,7732,7731,
        7730,7729,7728,7727,7726,7725,7724,7723,7722,7721,7719,7718,7717,7716,7715,7714,
        7713,7712,7711,7710,7709,7708,7707,7706,7705,7704,7702,7701,7700,7699,7698,7697,
        7696,7695,7694,7693,7692,7691,7690,7688,7687,7686,7685,7684,7683,7682,7681,7680,
        7679,7678,7676,7675,7674,7673,7672,7671,7670,7669,7668,7667,7665,7664,7663,7662,
        7661,7660,7659,7658,7657,7655,7654,7653,7652,7651,7650,7649,7648,7646,7645,7644,
        7643,7642,7641,7640,7639,7637,7636,7635,7634,7633,7632,7631,7629,7628,7627,7626,
        7625,7624,7623,7621,7620,7619,7618,7617,7616,7614,7613,7612,7611,7610,7609,7607,
        7606,7605,7604,7603,7602,7600,7599,7598,7597,7596,7595,7593,7592,7591,7590,7589,
        7588,7586,7585,7584,7583,7582,7580,7579,7578,7577,7576,7574,7573,7572,7571,7570,
        7568,7567,7566,7565,7564,7562,7561,7560,7559,7558,7556,7555,7554,7553,7551,7550,
        7549,7548,7547,7545,7544,7543,7542,7540,7539,7538,7537,7536,7534,7533,7532,7531,
        7529,7528,7527,7526,7524,7523,7522,7521,7519,7518,7517,7516,7514,7513,7512,7511,
        7509,7508,7507,7506,7504,7503,7502,7501,7499,7498,7497,7496,7494,7493,7492,7490,
        7489,7488,7487,7485,7484,7483,7482,7480,7479,7478,7476,7475,7474,7473,7471,7470,
        7469,7467,7466,7465,7464,7462,7461,7460,7458,7457,7456,7454,7453,7452,7451,7449,
        7448,7447,7445,7444,7443,7441,7440,7439,7437,7436,7435,7433,7432,7431,7429,7428,
        7427,7426,7424,7423,7422,7420,7419,7418,7416,7415,7414,7412,7411,7410,7408,7407,
        7405,7404,7403,7401,7400,7399,7397,7396,7395,7393,7392,7391,7389,7388,7387,7385,
        7384,7382,7381,7380,7378,7377,7376,7374,7373,7372,7370,7369,7367,7366,7365,7363,
        7362,7361,7359,7358,7356,7355,7354,7352,7351,7349,7348,7347,7345,7344,7343,7341,
        7340,7338,7337,7336,7334,7333,7331,7330,7329,7327,7326,7324,7323,7322,7320,7319,
        7317,7316,7314,7313,7312,7310,7309,7307,7306,7305,7303,7302,7300,7299,7297,7296,
        7295,7293,7292,7290,7289,7287,7286,7285,7283,7282,7280,7279,7277,7276,7274,7273,
        7272,7270,7269,7267,7266,7264,7263,7261,7260,7258,7257,7256,7254,7253,7251,7250,
        7248,7247,7245,7244,7242,7241,7239,7238,7237,7235,7234,7232,7231,7229,7228,7226,
        7225,7223,7222,7220,7219,7217,7216,7214,7213,7211,7210,7208,7207,7205,7204,7202,
        7201,7199,7198,7196,7195,7193,7192,7190,7189,7187,7186,7184,7183,7181,7180,7178,
        7177,7175,7174,7172,7171,7169,7168,7166,7165,7163,7162,7160,7159,7157,7155,7154,
        7152,7151,7149,7148,7146,7145,7143,7142,7140,7139,7137,7135,7134,7132,7131,7129,
        7128,7126,7125,7123,7122,7120,7118,7117,7115,7114,7112,7111,7109,7108,7106,7104,
        7103,7101,7100,7098,7097,7095,7093,7092,7090,7089,7087,7086,7084,7082,7081,7079,
        7078,7076,7074,7073,7071,7070,7068,7067,7065,7063,7062,7060,7059,7057,7055,7054,
        7052,7051,7049,7047,7046,7044,7043,7041,7039,7038,7036,7035,7033,7031,7030,7028,
        7027,7025,7023,7022,7020,7018,7017,7015,7014,7012,7010,7009,7007,7005,7004,7002,
        7001,6999,6997,6996,6994,6992,6991,6989,6987,6986,6984,6983,6981,6979,6978,6976,
        6974,6973,6971,6969,6968,6966,6964,6963,6961,6959,6958,6956,6954,6953,6951,6949,
        6948,6946,6944,6943,6941,6939,6938,6936,6934,6933,6931,6929,6928,6926,6924,6923,
        6921,6919,6918,6916,6914,6913,6911,6909,6908,6906,6904,6902,6901,6899,6897,6896,
        6894,6892,6891,6889,6887,6886,6884,6882,6880,6879,6877,6875,6874,6872,6870,6868,
        6867,6865,6863,6862,6860,6858,6856,6855,6853,6851,6850,6848,6846,6844,6843,6841,
        6839,6837,6836,6834,6832,6831,6829,6827,6825,6824,6822,6820,6818,6817,6815,6813,
        6811,6810,6808,6806,6804,6803,6801,6799,6797,6796,6794,6792,6790,6789,6787,6785,
        6783,6782,6780,6778,6776,6775,6773,6771,6769,6767,6766,6764,6762,6760,6759,6757,
        6755,6753,6751,6750,6748,6746,6744,6743,6741,6739,6737,6735,6734,6732,6730,6728,
        6726,6725,6723,6721,6719,6717,6716,6714,6712,6710,6708,6707,6705,6703,6701,6699,
        6698,6696,6694,6692,6690,6689,6687,6685,6683,6681,6680,6678,6676,6674,6672,6670,
        6669,6667,6665,6663,6661,6659,6658,6656,6654,6652,6650,6648,6647,6645,6643,6641,
        6639,6637,6636,6634,6632,6630,6628,6626,6625,6623,6621,6619,6617,6615,6613,6612,
        6610,6608,6606,6604,6602,6600,6599,6597,6595,6593,6591,6589,6587,6585,6584,6582,
        6580,6578,6576,6574,6572,6571,6569,6567,6565,6563,6561,6559,6557,6555,6554,6552,
        6550,6548,6546,6544,6542,6540,6538,6537,6535,6533,6531,6529,6527,6525,6523,6521,
        6519,6518,6516,6514,6512,6510,6508,6506,6504,6502,6500,6499,6497,6495,6493,6491,
        6489,6487,6485,6483,6481,6479,6477,6475,6474,6472,6470,6468,6466,6464,6462,6460,
        6458,6456,6454,6452,6450,6448,6447,6445,6443,6441,6439,6437,6435,6433,6431,6429,
        6427,6425,6423,6421,6419,6417,6415,6413,6411,6410,6408,6406,6404,6402,6400,6398,
        6396,6394,6392,6390,6388,6386,6384,6382,6380,6378,6376,6374,6372,6370,6368,6366,
        6364,6362,6360,6358,6356,6354,6352,6350,6348,6346,6344,6342,6340,6338,6336,6334,
        6333,6331,6329,6327,6325,6323,6321,6319,6317,6315,6313,6311,6309,6307,6305,6303,
        6300,6298,6296,6294,6292,6290,6288,6286,6284,6282,6280,6278,6276,6274,6272,6270,
        6268,6266,6264,6262,6260,6258,6256,6254,6252,6250,6248,6246,6244,6242,6240,6238,
        6236,6234,6232,6230,6228,6226,6224,6221,6219,6217,6215,6213,6211,6209,6207,6205,
        6203,6201,6199,6197,6195,6193,6191,6189,6187,6185,6182,6180,6178,6176,6174,6172,
        6170,6168,6166,6164,6162,6160,6158,6156,6154,6151,6149,6147,6145,6143,6141,6139,
        6137,6135,6133,6131,6129,6127,6124,6122,6120,6118,6116,6114,6112,6110,6108,6106,
        6104,6101,6099,6097,6095,6093,6091,6089,6087,6085,6083,6080,6078,6076,6074,6072,
        6070,6068,6066,6064,6061,6059,6057,6055,6053,6051,6049,6047,6044,6042,6040,6038,
        6036,6034,6032,6030,6027,6025,6023,6021,6019,6017,6015,6013,6010,6008,6006,6004,
        6002,6000,5998,5995,5993,5991,5989,5987,5985,5983,5980,5978,5976,5974,5972,5970,
        5968,5965,5963,5961,5959,5957,5955,5952,5950,5948,5946,5944,5942,5940,5937,5935,
        5933,5931,5929,5927,5924,5922,5920,5918,5916,5914,5911,5909,5907,5905,5903,5900,
        5898,5896,5894,5892,5890,5887,5885,5883,5881,5879,5876,5874,5872,5870,5868,5865,
        5863,5861,5859,5857,5854,5852,5850,5848,5846,5843,5841,5839,5837,5835,5832,5830,
        5828,5826,5824,5821,5819,5817,5815,5813,5810,5808,5806,5804,5801,5799,5797,5795,
        5793,5790,5788,5786,5784,5782,5779,5777,5775,5773,5770,5768,5766,5764,5761,5759,
        5757,5755,5752,5750,5748,5746,5744,5741,5739,5737,5735,5732,5730,5728,5726,5723,
        5721,5719,5717,5714,5712,5710,5708,5705,5703,5701,5699,5696,5694,5692,5690,5687,
        5685,5683,5680,5678,5676,5674,5671,5669,5667,5665,5662,5660,5658,5656,5653,5651,
        5649,5646,5644,5642,5640,5637,5635,5633,5630,5628,5626,5624,5621,5619,5617,5614,
        5612,5610,5608,5605,5603,5601,5598,5596,5594,5592,5589,5587,5585,5582,5580,5578,
        5575,5573,5571,5569,5566,5564,5562,5559,5557,5555,5552,5550,5548,5545,5543,5541,
        5539,5536,5534,5532,5529,5527,5525,5522,5520,5518,5515,5513,5511,5508,5506,5504,
        5501,5499,5497,5494,5492,5490,5487,5485,5483,5480,5478,5476,5473,5471,5469,5466,
        5464,5462,5459,5457,5455,5452,5450,5448,5445,5443,5441,5438,5436,5434,5431,5429,
        5427,5424,5422,5419,5417,5415,5412,5410,5408,5405,5403,5401,5398,5396,5393,5391,
        5389,5386,5384,5382,5379,5377,5375,5372,5370,5367,5365,5363,5360,5358,5356,5353,
        5351,5348,5346,5344,5341,5339,5337,5334,5332,5329,5327,5325,5322,5320,5317,5315,
        5313,5310,5308,5305,5303,5301,5298,5296,5293,5291,5289,5286,5284,5281,5279,5277,
        5274,5272,5269,5267,5265,5262,5260,5257,5255,5253,5250,5248,5245,5243,5241,5238,
        5236,5233,5231,5228,5226,5224,5221,5219,5216,5214,5212,5209,5207,5204,5202,5199,
        5197,5195,5192,5190,5187,5185,5182,5180,5177,5175,5173,5170,5168,5165,5163,5160,
        5158,5156,5153,5151,5148,5146,5143,5141,5138,5136,5134,5131,5129,5126,5124,5121,
        5119,5116,5114,5111,5109,5107,5104,5102,5099,5097,5094,5092,5089,5087,5084,5082,
        5080,5077,5075,5072,5070,5067,5065,5062,5060,5057,5055,5052,5050,5047,5045,5042,
        5040,5038,5035,5033,5030,5028,5025,5023,5020,5018,5015,5013,5010,5008,5005,5003,
        5000,4998,4995,4993,4990,4988,4985,4983,4980,4978,4975,4973,4970,4968,4965,4963,
        4960,4958,4955,4953,4950,4948,4945,4943,4940,4938,4935,4933,4930,4928,4925,4923,
        4920,4918,4915,4913,4910,4908,4905,4903,4900,4898,4895,4893,4890,4888,4885,4882,
        4880,4877,4875,4872,4870,4867,4865,4862,4860,4857,4855,4852,4850,4847,4845,4842,
        4840,4837,4834,4832,4829,4827,4824,4822,4819,4817,4814,4812,4809,4806,4804,4801,
        4799,4796,4794,4791,4789,4786,4784,4781,4778,4776,4773,4771,4768,4766,4763,4761,
        4758,4755,4753,4750,4748,4745,4743,4740,4738,4735,4732,4730,4727,4725,4722,4720,
        4717,4714,4712,4709,4707,4704,4702,4699,4696,4694,4691,4689,4686,4684,4681,4678,
        4676,4673,4671,4668,4666,4663,4660,4658,4655,4653,4650,4647,4645,4642,4640,4637,
        4634,4632,4629,4627,4624,4622,4619,4616,4614,4611,4609,4606,4603,4601,4598,4596,
        4593,4590,4588,4585,4583,4580,4577,4575,4572,4569,4567,4564,4562,4559,4556,4554,
        4551,4549,4546,4543,4541,4538,4536,4533,4530,4528,4525,4522,4520,4517,4515,4512,
        4509,4507,4504,4501,4499,4496,4494,4491,4488,4486,4483,4480,4478,4475,4473,4470,
        4467,4465,4462,4459,4457,4454,4451,4449,4446,4444,4441,4438,4436,4433,4430,4428,
        4425,4422,4420,4417,4415,4412,4409,4407,4404,4401,4399,4396,4393,4391,4388,4385,
        4383,4380,4377,4375,4372,4369,4367,4364,4361,4359,4356,4353,4351,4348,4345,4343,
        4340,4337,4335,4332,4329,4327,4324,4321,4319,4316,4313,4311,4308,4305,4303,4300,
        4297,4295,4292,4289,4287,4284,4281,4279,4276,4273,4271,4268,4265,4263,4260,4257,
        4255,4252,4249,4247,4244,4241,4238,4236,4233,4230,4228,4225,4222,4220,4217,4214,
        4212,4209,4206,4203,4201,4198,4195,4193,4190,4187,4185,4182,4179,4176,4174,4171,
        4168,4166,4163,4160,4158,4155,4152,4149,4147,4144,4141,4139,4136,4133,4130,4128,
        4125,4122,4120,4117,4114,4111,4109,4106,4103,4101,4098,4095,4092,4090,4087,4084,
        4081,4079,4076,4073,4071,4068,4065,4062,4060,4057,4054,4051,4049,4046,4043,4041,
        4038,4035,4032,4030,4027,4024,4021,4019,4016,4013,4010,4008,4005,4002,3999,3997,
        3994,3991,3989,3986,3983,3980,3978,3975,3972,3969,3967,3964,3961,3958,3956,3953,
        3950,3947,3945,3942,3939,3936,3934,3931,3928,3925,3922,3920,3917,3914,3911,3909,
        3906,3903,3900,3898,3895,3892,3889,3887,3884,3881,3878,3876,3873,3870,3867,3864,
        3862,3859,3856,3853,3851,3848,3845,3842,3839,3837,3834,3831,3828,3826,3823,3820,
        3817,3814,3812,3809,3806,3803,3801,3798,3795,3792,3789,3787,3784,3781,3778,3776,
        3773,3770,3767,3764,3762,3759,3756,3753,3750,3748,3745,3742,3739,3736,3734,3731,
        3728,3725,3722,3720,3717,3714,3711,3708,3706,3703,3700,3697,3694,3692,3689,3686,
        3683,3680,3678,3675,3672,3669,3666,3664,3661,3658,3655,3652,3650,3647,3644,3641,
        3638,3635,3633,3630,3627,3624,3621,3619,3616,3613,3610,3607,3604,3602,3599,3596,
        3593,3590,3587,3585,3582,3579,3576,3573,3571,3568,3565,3562,3559,3556,3554,3551,
        3548,3545,3542,3539,3537,3534,3531,3528,3525,3522,3520,3517,3514,3511,3508,3505,
        3503,3500,3497,3494,3491,3488,3485,3483,3480,3477,3474,3471,3468,3466,3463,3460,
        3457,3454,3451,3448,3446,3443,3440,3437,3434,3431,3429,3426,3423,3420,3417,3414,
        3411,3409,3406,3403,3400,3397,3394,3391,3389,3386,3383,3380,3377,3374,3371,3368,
        3366,3363,3360,3357,3354,3351,3348,3346,3343,3340,3337,3334,3331,3328,3325,3323,
        3320,3317,3314,3311,3308,3305,3302,3300,3297,3294,3291,3288,3285,3282,3279,3277,
        3274,3271,3268,3265,3262,3259,3256,3254,3251,3248,3245,3242,3239,3236,3233,3230,
        3228,3225,3222,3219,3216,3213,3210,3207,3204,3202,3199,3196,3193,3190,3187,3184,
        3181,3178,3176,3173,3170,3167,3164,3161,3158,3155,3152,3149,3147,3144,3141,3138,
        3135,3132,3129,3126,3123,3120,3118,3115,3112,3109,3106,3103,3100,3097,3094,3091,
        3088,3086,3083,3080,3077,3074,3071,3068,3065,3062,3059,3056,3053,3051,3048,3045,
        3042,3039,3036,3033,3030,3027,3024,3021,3018,3016,3013,3010,3007,3004,3001,2998,
        2995,2992,2989,2986,2983,2980,2978,2975,2972,2969,2966,2963,2960,2957,2954,2951,
        2948,2945,2942,2939,2937,2934,2931,2928,2925,2922,2919,2916,2913,2910,2907,2904,
        2901,2898,2895,2892,2890,2887,2884,2881,2878,2875,2872,2869,2866,2863,2860,2857,
        2854,2851,2848,2845,2842,2840,2837,2834,2831,2828,2825,2822,2819,2816,2813,2810,
        2807,2804,2801,2798,2795,2792,2789,2786,2783,2780,2778,2775,2772,2769,2766,2763,
        2760,2757,2754,2751,2748,2745,2742,2739,2736,2733,2730,2727,2724,2721,2718,2715,
        2712,2709,2706,2704,2701,2698,2695,2692,2689,2686,2683,2680,2677,2674,2671,2668,
        2665,2662,2659,2656,2653,2650,2647,2644,2641,2638,2635,2632,2629,2626,2623,2620,
        2617,2614,2611,2608,2605,2602,2599,2597,2594,2591,2588,2585,2582,2579,2576,2573,
        2570,2567,2564,2561,2558,2555,2552,2549,2546,2543,2540,2537,2534,2531,2528,2525,
        2522,2519,2516,2513,2510,2507,2504,2501,2498,2495,2492,2489,2486,2483,2480,2477,
        2474,2471,2468,2465,2462,2459,2456,2453,2450,2447,2444,2441,2438,2435,2432,2429,
        2426,2423,2420,2417,2414,2411,2408,2405,2402,2399,2396,2393,2390,2387,2384,2381,
        2378,2375,2372,2369,2366,2363,2360,2357,2354,2351,2348,2345,2342,2339,2336,2333,
        2330,2327,2324,2321,2318,2315,2312,2309,2306,2303,2300,2297,2294,2291,2288,2285,
        2282,2279,2276,2273,2270,2267,2264,2261,2257,2254,2251,2248,2245,2242,2239,2236,
        2233,2230,2227,2224,2221,2218,2215,2212,2209,2206,2203,2200,2197,2194,2191,2188,
        2185,2182,2179,2176,2173,2170,2167,2164,2161,2158,2155,2152,2149,2146,2142,2139,
        2136,2133,2130,2127,2124,2121,2118,2115,2112,2109,2106,2103,2100,2097,2094,2091,
        2088,2085,2082,2079,2076,2073,2070,2067,2064,2061,2057,2054,2051,2048,2045,2042,
        2039,2036,2033,2030,2027,2024,2021,2018,2015,2012,2009,2006,2003,2000,1997,1994,
        1990,1987,1984,1981,1978,1975,1972,1969,1966,1963,1960,1957,1954,1951,1948,1945,
        1942,1939,1936,1933,1929,1926,1923,1920,1917,1914,1911,1908,1905,1902,1899,1896,
        1893,1890,1887,1884,1881,1878,1874,1871,1868,1865,1862,1859,1856,1853,1850,1847,
        1844,1841,1838,1835,1832,1829,1826,1822,1819,1816,1813,1810,1807,1804,1801,1798,
        1795,1792,1789,1786,1783,1780,1776,1773,1770,1767,1764,1761,1758,1755,1752,1749,
        1746,1743,1740,1737,1734,1730,1727,1724,1721,1718,1715,1712,1709,1706,1703,1700,
        1697,1694,1691,1687,1684,1681,1678,1675,1672,1669,1666,1663,1660,1657,1654,1651,
        1647,1644,1641,1638,1635,1632,1629,1626,1623,1620,1617,1614,1611,1607,1604,1601,
        1598,1595,1592,1589,1586,1583,1580,1577,1574,1570,1567,1564,1561,1558,1555,1552,
        1549,1546,1543,1540,1537,1533,1530,1527,1524,1521,1518,1515,1512,1509,1506,1503,
        1499,1496,1493,1490,1487,1484,1481,1478,1475,1472,1469,1465,1462,1459,1456,1453,
        1450,1447,1444,1441,1438,1435,1431,1428,1425,1422,1419,1416,1413,1410,1407,1404,
        1401,1397,1394,1391,1388,1385,1382,1379,1376,1373,1370,1366,1363,1360,1357,1354,
        1351,1348,1345,1342,1339,1335,1332,1329,1326,1323,1320,1317,1314,1311,1308,1304,
        1301,1298,1295,1292,1289,1286,1283,1280,1277,1273,1270,1267,1264,1261,1258,1255,
        1252,1249,1246,1242,1239,1236,1233,1230,1227,1224,1221,1218,1214,1211,1208,1205,
        1202,1199,1196,1193,1190,1186,1183,1180,1177,1174,1171,1168,1165,1162,1158,1155,
        1152,1149,1146,1143,1140,1137,1134,1130,1127,1124,1121,1118,1115,1112,1109,1106,
        1102,1099,1096,1093,1090,1087,1084,1081,1078,1074,1071,1068,1065,1062,1059,1056,
        1053,1050,1046,1043,1040,1037,1034,1031,1028,1025,1021,1018,1015,1012,1009,1006,
        1003,1000, 997, 993, 990, 987, 984, 981, 978, 975, 972, 968, 965, 962, 959, 956,
        953, 950, 947, 944, 940, 937, 934, 931, 928, 925, 922, 919, 915, 912, 909, 906,
        903, 900, 897, 894, 890, 887, 884, 881, 878, 875, 872, 869, 865, 862, 859, 856,
        853, 850, 847, 844, 840, 837, 834, 831, 828, 825, 822, 819, 815, 812, 809, 806,
        803, 800, 797, 794, 790, 787, 784, 781, 778, 775, 772, 769, 765, 762, 759, 756,
        753, 750, 747, 744, 740, 737, 734, 731, 728, 725, 722, 719, 715, 712, 709, 706,
        703, 700, 697, 693, 690, 687, 684, 681, 678, 675, 672, 668, 665, 662, 659, 656,
        653, 650, 646, 643, 640, 637, 634, 631, 628, 625, 621, 618, 615, 612, 609, 606,
        603, 600, 596, 593, 590, 587, 584, 581, 578, 574, 571, 568, 565, 562, 559, 556,
        553, 549, 546, 543, 540, 537, 534, 531, 527, 524, 521, 518, 515, 512, 509, 505,
        502, 499, 496, 493, 490, 487, 484, 480, 477, 474, 471, 468, 465, 462, 458, 455,
        452, 449, 446, 443, 440, 436, 433, 430, 427, 424, 421, 418, 415, 411, 408, 405,
        402, 399, 396, 393, 389, 386, 383, 380, 377, 374, 371, 367, 364, 361, 358, 355,
        352, 349, 345, 342, 339, 336, 333, 330, 327, 323, 320, 317, 314, 311, 308, 305,
        302, 298, 295, 292, 289, 286, 283, 280, 276, 273, 270, 267, 264, 261, 258, 254,
        251, 248, 245, 242, 239, 236, 232, 229, 226, 223, 220, 217, 214, 210, 207, 204,
        201, 198, 195, 192, 188, 185, 182, 179, 176, 173, 170, 166, 163, 160, 157, 154,
        151, 148, 145, 141, 138, 135, 132, 129, 126, 123, 119, 116, 113, 110, 107, 104,
        101,  97,  94,  91,  88,  85,  82,  79,  75,  72,  69,  66,  63,  60,  57,  53,
        50,  47,  44,  41,  38,  35,  31,  28,  25,  22,  19,  16,  13,   9,   6,   3
    };

    unsigned int deg = ((in*FG_SINE_SIZE) / (max - min));
    int rem = (in*FG_SINE_SIZE) - (deg*(max-min));


    int base = sin_array[deg];
    int opt, temp;

    if(rem > 0)
    {
        opt = sin_array[deg+1];
    }
    else
    {
        opt = sin_array[deg-1];
    }

    temp = opt - base;
    if(temp < 0) temp = (-1) * temp;

    base += (temp*rem) / (max - min);

    return base;
}

/* nop + return cyc delay */
void nop_1_times(void){ return;}

/* nop回数は実測調整 */
void nop_1us(void)
{

        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop();
        nop(); nop(); /* nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop();*/
        // nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop();
        // nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 

        // nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        // nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 

        // nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop();
}

/* nop回数は実測調整 */
void nop_10us(void)
{

        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop();
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 

        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop();
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 

        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop();
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 

        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop();
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 

        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop();
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 

        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop();
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 

        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop();
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 

        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop();
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 

        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop();
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 

        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop();
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 



        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop();
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); /* nop(); */ 
        // nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 

        // nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        // nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        // nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        // nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        // nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        // nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        // nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        // nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        // nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        // nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 

        // nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop();
        // nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        // nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        // nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        // nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
}

/* 100us delay for RAMP function*/
/* nop回数は実測調整 */
void nop_100us(void)
{
    for(int i = 0; i < 100; i++)
    {
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop();
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 

        nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 
        // nop(); nop(); nop(); nop(); nop();  nop(); nop(); nop(); nop(); nop(); 

        // nop(); nop(); nop(); nop(); nop();
    }

}