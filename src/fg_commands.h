/* Copyright (c) 2022 poemoep */
/* This software is released under the MIT License, see LICENSE, see LICENSE. */
/* This website content is released under the CC BY 4.0 License, see LICENSE. */


#if !(__FG_COMMANDS_H__)
#define __FG_COMMANDS_H__

#define FG_BASE_CYC     (45)
#define FG_BASE_1us_CYC (SYSCLK_MHZ)
#define FG_BASE_10us_CYC (FG_BASE_1us_CYC*10)
#define FG_BASE_100us_CYC (FG_BASE_10us_CYC*10)

#define FG_BASE_RAMP_CYC     (FG_BASE_100us_CYC)

#define FG_DELAY_MIN    (FG_BASE_CYC)
#define FG_DELAY_1us    (FG_BASE_1us_CYC)
#define FG_DELAY_10us    (FG_BASE_10us_CYC)
#define FG_DELAY_100us    (FG_BASE_100us_CYC)

#define FG_BUF_SIZE     (65536)
#define FG_BUF_MAX      (65535)
#define FG_COUNT_MAX    (65534)
#define FG_COUNT_CONTINUE (65536)

/* Limmit */
#define FG_FREQ_MIN     (2)
#define FG_FREQ_MAX     (100000)
#define FG_FREQ_ASYMMETRIC_MIN  (4)  /* saw */
#define FG_FREQ_ASYMMETRIC_MAX  (50000) /* saw */

/* Help message */
#define FG_START_HELP       "USAGE: START\n"
#define FG_STOP_HELP        "USAGE: STOP\n"
#define FG_RESTART_HELP     "USAGE: RE\n"
#define FG_SINE_HELP        "USAGE: SINE FREQ-Hz [N-times] [MAX-mv] [MIN-mv] \n"
#define FG_PULSE_HELP       "USAGE: PULSE FREQ-Hz [N-times] [DUTY-%%] [MAX-mv] [MIN-mv] \n"
#define FG_TRIANGLE_HELP    "USAGE: TRI FREQ-Hz [N-times] [MAX-mv] [MIN-mv] \n"
#define FG_SAW_HELP         "USAGE: SAW FREQ-Hz [N-times] [MAX-mv] [MIN-mv] \n"
#define FG_BURST_HELP       "USAGE: BURST FREQ-0.1kHz [BurstN-times] [DELAY-ms] [RepeatN-times]\n"
#define FG_RAMP_HELP        "USAGE: RAMP WaitTime-ms RampTime-ms start-mv end-mv\n"
#define FG_DAC_HELP         "USAGE: DAC OUT-mV \n"
#define FG_TEST_HELP        "USAGE: TEST\n"

/* DAC out */
#define DAC_OUT_0_BIT   0x0001
#define DAC_OUT_1_BIT   0x0002
#define DAC_OUT_2_BIT   0x0004
#define DAC_OUT_3_BIT   0x0008
#define DAC_OUT_4_BIT   0x0018
#define DAC_OUT_5_BIT   0x0020
#define DAC_OUT_6_BIT   0x0040
#define DAC_OUT_7_BIT   0x0080
#define DAC_OUT_8_BIT   0x0100
#define DAC_OUT_9_BIT   0x0200
#define DAC_OUT_A_BIT   0x0400
#define DAC_OUT_B_BIT   0x0800
#define DAC_OUT_C_BIT   0x1000
#define DAC_OUT_D_BIT   0x2000
#define DAC_OUT_E_BIT   0x4000
#define DAC_OUT_F_BIT   0x8000

#define DAC_OUT_MIN     0x0000
#define DAC_OUT_MIDDLE  0x7FFF
#define DAC_OUT_MAX     0xFFFF

#define DAC_OUT_mV_MIN      0
#define DAC_OUT_mV_MIDDLE   5000
#define DAC_OUT_mV_MAX      10000 


/* sine buf size */
#define FG_SINE_SIZE (8192)

typedef struct{
    unsigned int wait_time;
    unsigned int ramp_time;
    unsigned int start_mv;
    unsigned int end_mv;
}RAMP_T;

typedef struct{
    unsigned short fg_buf[FG_BUF_SIZE]; /* 波形データバッファ */
    unsigned short fg_buf_max;          /* itrの上限設定パラメータ */
    unsigned int count_max;           /* 波形データ繰り返し上限設定パラメータ */
    int core1_flag;                     /* core1ステータス */
    void (* nop_func)(void);            /* delay調整用nop関数ポインタ*/
    void (* core1_func)(void);            /* Core1起動先関数ポインタ*/
}CORE1_VARIABLE_T;

extern CORE1_VARIABLE_T core1_val;

typedef struct{
    int freq;
    int freq_mod;
    int datum;
    int delay;
    int out_min;
    int out_max;
    int duty;
    int cyc;
    RAMP_T ramp;
}FG_VALUE_T;

void fg_start_func(char*);
void fg_stop_func(char*);
void fg_restart_func(char*);
void fg_sine_func(char*);
void fg_pulse_func(char*);
void fg_triangle_func(char*);
void fg_saw_func(char*);
void fg_burst_func(char*);
void fg_ramp_func(char*);
void fg_dac_func(char*);
void fg_led_func(char*);
void fg_print_func(char*);
void fg_help_func(char*);
void fg_test_func(char*);

void fg_init(void);


#endif