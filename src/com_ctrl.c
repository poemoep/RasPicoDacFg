/* Copyright (c) 2022 poemoep */
/* This software is released under the MIT License, see LICENSE, see LICENSE. */
/* This website content is released under the CC BY 4.0 License, see LICENSE. */

#include <stdio.h>
#include "pico/stdlib.h"

#include "sys_rp2040.h"
#include "fg_commands.h" /* FG_WORDBUB_SIZE */
#include "com_ctrl.h"

char com_getChar(void){
    char c;
    scanf("%c",&c);
    if(COM_CR_CODE == c)    printf("%c",COM_LF_CODE);
    else                    printf("%c",c);

    return c;
}

void com_upper(char* input_char)
{
    unsigned int i = 0;;

    while( '\0' != input_char[i])
    {
        /* 0x61 ~ 0x7A: a ~ z */
        if((0x61 <= input_char[i]) && (input_char[i] <= 0x7A))
        {
            input_char[i] -= 0x20;
        }
        i++;
    }
}

char* com_skip_blank(char* input_str)
{
    /* 複数スペースをスキップしたいけどうまくいかない */
    int num;
    char* pchar;

    pchar = input_str;
    num = 0;
    
    while(*pchar == ' ')
    {
        if(*pchar == '\0') break;
        pchar++;
        num++;
    }

    return pchar;
}

char* com_get_nextWord(char* input_str, char* output_str)
{
    int check_strNum = 0;
    int res;
    char* pchar_in;

    pchar_in = input_str;

    pchar_in = com_skip_blank(input_str);

    while(*pchar_in != '\0' && *pchar_in != ' ')
    {
        *output_str = *pchar_in;
        pchar_in++;
        output_str++;
        check_strNum++;

        if(WORDBUF_SIZE == check_strNum)
        {
            printf("BUF ERROR\n");
            break;
        }
    }

    if(WORDBUF_SIZE == check_strNum)
    {
        printf("FB_WORDBUF OVERFLOW\n");
        return input_str;
    }
    else
    {
        *output_str = '\0';
        input_str = pchar_in;
        return pchar_in;
    
    }
}