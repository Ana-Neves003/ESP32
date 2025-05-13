#ifndef SD_DRIVER_H
#define SD_DRIVER_H

#include <stdio.h>
#include <stdbool.h>

void sd_mount(void);
bool sd_is_mounted(void);
FILE* sd_open_file(void);

#endif // SD_DRIVER_H
