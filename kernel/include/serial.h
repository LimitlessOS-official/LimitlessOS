#ifndef LIMITLESS_SERIAL_H
#define LIMITLESS_SERIAL_H

void serial_init(void);
void serial_write_char(char character);
void serial_write_string(const char *text);

#endif

