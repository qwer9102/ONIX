#ifndef ONIX_IO_H
#define ONIX_IO_H

#include <onix/types.h>

extern u8  inb(u16 port);   // Read a byte from a port
extern u16 inw(u16 port);  // Read a word from a port

extern void outb(u16 port, u8 data);  // Write a byte to a port
extern void outw(u16 port, u16 data); // Write a word to a port


#endif