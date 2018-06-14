#include "SPI.h"

int transfer(int fd, unsigned char send[], unsigned char receive[], int length)
{
  struct spi_ioc_transfer transfer;         // the transfer structure
  transfer.tx_buf = (unsigned long)send;    // the buffer for sending data
  transfer.rx_buf = (unsigned long)receive; // the buffer for receiving data
  transfer.len = length;                    // the length of buffer
  transfer.speed_hz = 1000000;              // the speed in Hz
  transfer.bits_per_word = 8;               // bits per word
  transfer.delay_usecs = 0;                 // delay in us
  // send the SPI message (all of the above fields, inc. buffers)
  int status = ioctl(fd, SPI_IOC_MESSAGE(1), &transfer);
  if (status < 0)
  {
    perror("SPI: SPI_IOC_MESSAGE Failed");
    return -1;
  }

  return status;
}

uint8_t readRegister(uint8_t address)
{
  //////////////
  gpio_Export("4");
  set_vlaue("4", "0");
  //////////////
  unsigned int fd; // file handle and loop counter
  unsigned char null = 0x00;       // sending only a single char
  unsigned char b[2];
  uint8_t bits = 8, mode = 0; // 8-bits per word, SPI mode 3
  uint32_t speed = 8E6;       // Speed is 1 MHz
  // The following calls set up the SPI bus properties
  fd = open(SPI_PATH, O_RDWR);
  ioctl(fd, SPI_IOC_WR_MODE, &mode);
  ioctl(fd, SPI_IOC_RD_MODE, &mode);
  ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
  ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
  ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
  ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);

  uint8_t testtemp[2];
  testtemp[0] = address & 0x7f;
  testtemp[1] = null;
  uint8_t respons[2];
  fflush(stdout); // need to flush the output, as no
  transfer(fd, (unsigned char *)&testtemp, (unsigned char *)&b, 2);
  respons[0] = b[0];
  respons[1] = b[1];


  fflush(stdout); // need to flush the output, as no
  set_vlaue("4", "1");
  close(fd); // close the file
  return respons[1];
}

void writeRegister(uint8_t address, uint8_t value)
{
  //////////////
  gpio_Export("4");
  set_vlaue("4", "0");
  //////////////
  unsigned int fd; // file handle and loop counter
  unsigned char b[2];
  uint8_t bits = 8, mode = 0; // 8-bits per word, SPI mode 3
  uint32_t speed = 8E6;       // Speed is 1 MHz
  // The following calls set up the SPI bus properties
  fd = open(SPI_PATH, O_RDWR);
  ioctl(fd, SPI_IOC_WR_MODE, &mode);
  ioctl(fd, SPI_IOC_RD_MODE, &mode);
  ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
  ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
  ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
  ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);

  uint8_t testtemp[2];
  testtemp[0] = address | 0x80;
  testtemp[1] = value;
  fflush(stdout); // need to flush the output, as no
  transfer(fd, (unsigned char *)&testtemp, (unsigned char *)&b, 2);
  fflush(stdout); // need to flush the output, as no
  set_vlaue("4", "1");
  close(fd); // close the file
}