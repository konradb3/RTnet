#include <asm/io.h>

#ifndef COMDBG_PORT
#define COMDBG_PORT     0x3F8
#endif

const static char comdbg_hexdigits[16] =
{'0', '1', '2', '3', '4', '5', '6', '7',
     '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};



inline void comdbg_write_char(char c)
{
    while ((inb(COMDBG_PORT+5) & 0x20) == 0);
    outb(c, COMDBG_PORT);
}


void comdbg_str(char *pstr)
{
    while (*pstr != 0)
        comdbg_write_char(*(pstr++));
}



void comdbg_val(int val)
{
    int digit;
    int  i;

    comdbg_write_char(' ');
    for (i = 28; i >= 0; i-=4)
    {
        digit = (val >> i) & 0x0F;
        comdbg_write_char(comdbg_hexdigits[digit]);
    }
}
