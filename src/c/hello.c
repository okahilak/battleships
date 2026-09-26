#include <stdio.h>
#include <conio.h>
#include <c64.h>

int main(void)
{
    unsigned char i;

    bordercolor(COLOR_BLACK);
    bgcolor(COLOR_BLUE);
    clrscr();

    textcolor(COLOR_WHITE);
    cputs("hello from cc65!\r\n\r\n");

    for (i = 0; i < 8; ++i) {
        textcolor(i + 1 == COLOR_BLUE ? COLOR_LIGHTBLUE : i + 1);
        cprintf("line %u\r\n", i);
    }

    textcolor(COLOR_YELLOW);
    cputs("\r\npress any key...");
    cgetc();
    return 0;
}
