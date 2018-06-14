#include "GPIO.h"
//////////////////////////////////////////gpio_controll start
void gpio_Export(char gpio_pin[])
{
  if ((gpio_fp = fopen("/sys/class/gpio/export", "ab")) == NULL)
  {
    printf("check sudo  & Cannot open export file.\n");
    exit(1);
  }
  rewind(gpio_fp);
  strcpy(set_value, gpio_pin);
  fwrite(&set_value, sizeof(char), 2, gpio_fp);
  fclose(gpio_fp);
}
void set_vlaue(char gpio_pin[], char value[])
{
  char gpiodir[64];
  snprintf(gpiodir, sizeof(gpiodir), "/sys/class/gpio/gpio%s/value", gpio_pin);
  if ((gpio_fp = fopen(gpiodir, "rb+")) == NULL)
  {
    printf("Cannot open value file.\n");
    exit(1);
  }
  rewind(gpio_fp);
  strcpy(set_value, value);
  fwrite(&set_value, sizeof(char),  sizeof(value), gpio_fp);
  fclose(gpio_fp);
}
void set_state(char gpio_pin[], char value[])
{
  char gpiodir[64];
  snprintf(gpiodir, sizeof(gpiodir), "/sys/class/gpio/gpio%s/direction", gpio_pin);

  if ((gpio_fp = fopen(gpiodir, "rb+")) == NULL)
  {
    printf("Cannot open value file.\n");
    exit(1);
  }
  rewind(gpio_fp);
  strcpy(set_value, value);
  fwrite(&set_value, sizeof(char),  sizeof(value), gpio_fp);
  fclose(gpio_fp);

}