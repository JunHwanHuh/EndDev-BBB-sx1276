#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include"GPIO.h"
#include <linux/spi/spidev.h>
#define SPI_PATH "/dev/spidev1.0"


int transfer(int fd, unsigned char send[], unsigned char receive[], int length);
uint8_t readRegister(uint8_t address);
void writeRegister(uint8_t address, uint8_t value);