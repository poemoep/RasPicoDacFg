/* Copyright (c) 2022 poemoep */
/* This software is released under the MIT License, see LICENSE, see LICENSE. */
/* This website content is released under the CC BY 4.0 License, see LICENSE. */

#if !(__COM_CTRL_H__)
#define __COM_CTRL_H__

char com_getChar(void);
void com_upper(char*);

char* com_skip_blank(char*);
char* com_get_nextWord(char*,char*);

#define COM_CHAR_BUF_SIZE 64

#define ASCII_BACKSPACE (0x08)
#define ASCII_DOWN      (0x0A)
#define ASCII_UP        (0x0B)
#define ASCII_RIGHT     (0x0C)
#define ASCII_CR        (0x0D)

#define COM_LOGIN_CODE '\r'
#define COM_RETURN_CODE '\r'
#define COM_CR_CODE     '\r'
#define COM_LF_CODE     '\n'
#define COM_NULL_CODE '\0'
#define COM_WAIT_CMD "CMD: >"

#endif