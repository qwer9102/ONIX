#ifndef ONIX_CONSOLE_H
#define ONIX_CONSOLE_H

#include <onix/types.h>

void console_init();
void console_clear();
int32 console_write(char *buf,u32 count);

#endif