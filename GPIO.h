#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>


FILE *gpio_fp;
char set_value[4];

void gpio_Export(char gpio_pin[]);
void set_vlaue(char gpio_pin[], char value[]);
void set_state(char gpio_pin[], char value[]);