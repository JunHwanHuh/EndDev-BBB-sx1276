#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <string.h> 
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>

#include "GPIO.h"
#include "SPI.h"


//tcp setting
#define BUFSIZE 1024
#define SERVERIP ((const unsigned char *)"192.168.0.76")
//#define SERVERIP ((const unsigned char *)"13.124.103.193")
#define PORT 6000
void *send_message(void *arg);
void *recv_message(void *arg);
void error_handling(char *message);
char message[BUFSIZE];
#define MAXBUFFER 1024
#define TXSTACKSIZE 20
char transtext[TXSTACKSIZE][MAXBUFFER];
int transfer_max_index = 0;
int transfer_current_index = 0;
//tcp setting

//0=> debug
//1=> nomal
#define debug_mode 1

//1->nomal mode(sensor<=>Gateway<=>server)
//2->nomal mode(sensor<=>Gateway)
//3->
#define test_mode 2
#define SPI_PATH "/dev/spidev1.0"

int _packetIndex;
int _implicitHeaderMode = 0;

// registers
#define REG_FIFO 0x00
#define REG_OP_MODE 0x01
#define REG_FRF_MSB 0x06
#define REG_FRF_MID 0x07
#define REG_FRF_LSB 0x08
#define REG_PA_CONFIG 0x09
#define REG_LNA 0x0c
#define REG_FIFO_ADDR_PTR 0x0d
#define REG_FIFO_TX_BASE_ADDR 0x0e
#define REG_FIFO_RX_BASE_ADDR 0x0f
#define REG_FIFO_RX_CURRENT_ADDR 0x10
#define REG_IRQ_FLAGS 0x12
#define REG_RX_NB_BYTES 0x13
#define REG_PKT_SNR_VALUE 0x19
#define REG_PKT_RSSI_VALUE 0x1a
#define REG_MODEM_CONFIG_1 0x1d
#define REG_MODEM_CONFIG_2 0x1e
#define REG_PREAMBLE_MSB 0x20
#define REG_PREAMBLE_LSB 0x21
#define REG_PAYLOAD_LENGTH 0x22
#define REG_MODEM_CONFIG_3 0x26
#define REG_RSSI_WIDEBAND 0x2c
#define REG_DETECTION_OPTIMIZE 0x31
#define REG_DETECTION_THRESHOLD 0x37
#define REG_SYNC_WORD 0x39
#define REG_DIO_MAPPING_1 0x40
#define REG_VERSION 0x42

// modes
#define MODE_LONG_RANGE_MODE 0x80
#define MODE_SLEEP 0x00
#define MODE_STDBY 0x01
#define MODE_TX 0x03
#define MODE_RX_CONTINUOUS 0x05
#define MODE_RX_SINGLE 0x06

// PA config
#define PA_BOOST 0x80

// IRQ masks
#define IRQ_TX_DONE_MASK 0x08
#define IRQ_PAYLOAD_CRC_ERROR_MASK 0x20
#define IRQ_RX_DONE_MASK 0x40

#define MAX_PKT_LENGTH 255 ///////////////debug_mode

void Lora_sleep()
{
  writeRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP);
}

void setFrequency(long Lorafrequency)
{

  uint64_t frf = ((uint64_t)Lorafrequency << 19) / 32000000;
  writeRegister(REG_FRF_MSB, (uint8_t)(frf >> 16));
  writeRegister(REG_FRF_MID, (uint8_t)(frf >> 8));
  writeRegister(REG_FRF_LSB, (uint8_t)(frf >> 0));
}
void Lora_idle()
{
  writeRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
}
////////////////////////beginPacket
int beginPacket(int implicitHeader)
{
  // put in standby mode
  Lora_idle();

  if (implicitHeader)
  {
    writeRegister(REG_MODEM_CONFIG_1, readRegister(REG_MODEM_CONFIG_1) | 0x01);
  }
  else
  {
    writeRegister(REG_MODEM_CONFIG_1, readRegister(REG_MODEM_CONFIG_1) & 0xfe);
  }
  writeRegister(REG_FIFO_ADDR_PTR, 0);
  writeRegister(REG_PAYLOAD_LENGTH, 0);

  return 1;
}
void setTxPower()
{
  int level = 17;
  writeRegister(REG_PA_CONFIG, PA_BOOST | (level - 2));
}

size_t Lora_write(const uint8_t *buffer, size_t size)
{

  int currentLength = readRegister(REG_PAYLOAD_LENGTH);

  // check size
  if ((currentLength + size) > MAX_PKT_LENGTH)
  {
    size = MAX_PKT_LENGTH - currentLength;
  }
  size_t i = 0;
  // write data
  for (i = 0; i < size; i++)
  {
    writeRegister(REG_FIFO, buffer[i]);
  }

  // update length
  writeRegister(REG_PAYLOAD_LENGTH, currentLength + size);

  return size;
}
///////////////////////////////////////////
int endPacket()
{
  writeRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_TX);
  while ((readRegister(REG_IRQ_FLAGS) & IRQ_TX_DONE_MASK) == 0)
  {
    sleep(1);
  }

  // clear IRQ's
  writeRegister(REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);

  return 1;
}

int parsePacket()
{
  int size = 0;
  int packetLength = 0;
  int irqFlags = readRegister(REG_IRQ_FLAGS);

  if (size > 0)
  {
    //implicitHeaderMode();
    writeRegister(REG_MODEM_CONFIG_1, readRegister(REG_MODEM_CONFIG_1) | 0x01);
    writeRegister(REG_PAYLOAD_LENGTH, size & 0xff);
  }
  else
  {
    //explicitHeaderMode();
    writeRegister(REG_MODEM_CONFIG_1, readRegister(REG_MODEM_CONFIG_1) & 0xfe);
  }

  // clear IRQ's
  writeRegister(REG_IRQ_FLAGS, irqFlags);

  if ((irqFlags & IRQ_RX_DONE_MASK) && (irqFlags & IRQ_PAYLOAD_CRC_ERROR_MASK) == 0)
  {
    // received a packet
    _packetIndex = 0;

    // read packet length
    if (_implicitHeaderMode)
    {
      packetLength = readRegister(REG_PAYLOAD_LENGTH);
    }
    else
    {
      packetLength = readRegister(REG_RX_NB_BYTES);
    }

    // set FIFO address to current RX address
    writeRegister(REG_FIFO_ADDR_PTR, readRegister(REG_FIFO_RX_CURRENT_ADDR));

    // put in standby mode
    Lora_idle();
  }
  else if (readRegister(REG_OP_MODE) != (MODE_LONG_RANGE_MODE | MODE_RX_SINGLE))
  {
    // not currently in RX mode

    // reset FIFO address
    writeRegister(REG_FIFO_ADDR_PTR, 0);

    // put in single RX mode
    writeRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_RX_SINGLE);
  }

  return packetLength;
}

int available()
{
  return (readRegister(REG_RX_NB_BYTES) - _packetIndex);
}
int Lora_read()
{

  if (!available())
  {
    return -1;
  }

  _packetIndex++;
  return readRegister(REG_FIFO);
}

void setSpreadingFactor(int sf)
{  if (sf < 6) {
    sf = 6;
  } else if (sf > 12) {
    sf = 12;
  }
  if (sf == 6) {
    writeRegister(REG_DETECTION_OPTIMIZE, 0xc5);
    writeRegister(REG_DETECTION_THRESHOLD, 0x0c);
  } else {
    writeRegister(REG_DETECTION_OPTIMIZE, 0xc3);
    writeRegister(REG_DETECTION_THRESHOLD, 0x0a);
  }
  writeRegister(REG_MODEM_CONFIG_2, (readRegister(REG_MODEM_CONFIG_2) & 0x0f) | ((sf << 4) & 0xf0));
}
void setSignalBandwidth(long sbw)
{  int bw;
  if (sbw <= 7.8E3) {
    bw = 0;
  } else if (sbw <= 10.4E3) {
    bw = 1;
  } else if (sbw <= 15.6E3) {
    bw = 2;
  } else if (sbw <= 20.8E3) {
    bw = 3;
  } else if (sbw <= 31.25E3) {
    bw = 4;
  } else if (sbw <= 41.7E3) {
    bw = 5;
  } else if (sbw <= 62.5E3) {
    bw = 6;
  } else if (sbw <= 125E3) {
    bw = 7;
  } else if (sbw <= 250E3) {
    bw = 8;
  } else /*if (sbw <= 250E3)*/ {
    bw = 9;
  }
  writeRegister(REG_MODEM_CONFIG_1, (readRegister(REG_MODEM_CONFIG_1) & 0x0f) | (bw << 4));
}
void setCodingRate4(int denominator)
{  
  if (denominator < 5) {
    denominator = 5;
  } else if (denominator > 8) {
    denominator = 8;
  }
  int cr = denominator - 4;
  writeRegister(REG_MODEM_CONFIG_1, (readRegister(REG_MODEM_CONFIG_1) & 0xf1) | (cr << 1));
}

void setPreambleLength(long length)
{  
  writeRegister(REG_PREAMBLE_MSB, (uint8_t)(length >> 8));
  writeRegister(REG_PREAMBLE_LSB, (uint8_t)(length >> 0));
}  
void Lora_begin(  long fre,long band, int coding,int sf)
{
  //Lora_begin start
  gpio_Export("60");
  set_vlaue("60", "0");
  sleep(1);
  set_vlaue("60", "1");
  if (readRegister(0x42) != 0x12)
  {
    printf("Spi or Reset not work\n");
  }
  Lora_sleep();
  // setFrequency(868000000);
  // setSpreadingFactor(10);
  // setCodingRate4(5);
  // setSignalBandwidth(125000);

   long frequency=fre*1000000;
  setFrequency(frequency);//long // set frequency
  long bandwidth=band*1000;
  setSignalBandwidth(bandwidth);//long
  setSpreadingFactor(sf);//int
  setCodingRate4(coding);////int

  // set base addresses
  writeRegister(REG_FIFO_TX_BASE_ADDR, 0);
  writeRegister(REG_FIFO_RX_BASE_ADDR, 0);

  // set LNA boost
  writeRegister(REG_LNA, readRegister(REG_LNA) | 0x03);

  // set auto AGC
  writeRegister(REG_MODEM_CONFIG_3, 0x04);

  // set output power to 17 dBm
  setTxPower();

  // put in standby mode
  Lora_idle();
  //Lora_begin end
}

////////////////////////////////////////////TCP
void *send_message(void *arg)
{
  int sock = (int)arg;

  if (!strcmp(message, "q\n"))
  {
    close(sock);
    exit(0);
  }
  write(sock, transtext[transfer_current_index], strlen(transtext[transfer_current_index]));
  if (transfer_current_index == 19)
  {
    transfer_current_index = 0;
  }
  else
  {
    transfer_current_index++;
  }
  return NULL; 
}
void *recv_message(void *arg)
{
  int sock = (int)arg;
  char name_message[BUFSIZE];
  int str_len;
  while (1)
  {
    memset(name_message, 0x00, BUFSIZE - 1);
    str_len = read(sock, name_message, BUFSIZE - 1);
    if (str_len == -1)
      return (void *)1;
    printf("server:%s\n", name_message);
  }
}
void error_handling(char *message)
{
  fputs(message, stderr);
  fputc('\n', stderr);
}
////////////////////////////////////////////TCP
/////////////////parse_date////////////////////////////
//Read Lora Data Pasing & tcp/ip transfer to thread////
//pthread_t,sock,LoraReaddate//////////////////////////
///////////////////////////////////////////////////////
void parse_date(pthread_t temp_snd_thread, int tempsock, char *doc)
{
  char divice[32];
  char x[5];
  char y[5];
  char z[5];
  int a;
  int parsecount = 1; //처음에 parsing 할때 쓰레기값들어온다..
  char *ptr = strtok(doc, "D");
  while (ptr != NULL) // 자른 문자열이 나오지 않을 때까지 반복
  {
    if (parsecount == 1)
    {
      parsecount++;
    }
    else
    {
      //printf("D cuting=>%s\n", ptr); // 자른 문자열 출력
      a = sscanf(ptr, ":%3sx:%4sy:%4sz:%4s", divice, x, y, z);
      //printf("parse device=>%s x=> %s y=> %s z => %s \n", divice, x, y, z);//일정한 형태로 된 부분만 파싱가능(sscanf를 통하여)

      sprintf(transtext[transfer_max_index], "Device num =%s, X=%s, Y=%s, Z=%s \n", divice, x, y, z); //보내고싶은 메시지 세팅.......
      printf("%s\n", transtext[transfer_max_index]);
      if (transfer_max_index == 19)
      {
        transfer_max_index = 0;
      }
      else{
          transfer_max_index++;
      }

      pthread_create(&temp_snd_thread, NULL, send_message, (void *)tempsock);
      pthread_detach(temp_snd_thread);
    }
    ptr = strtok(NULL, "D"); // 다음 문자열을 잘라서 포인터를 반환
  }
  //socket setting

}

void parse_date_NOTX(char *doc)
{
  char divice[32];
  char x[5];
  char y[5];
  char z[5];
  int a;
  int parsecount = 1; //처음에 parsing 할때 쓰레기값들어온다..
  char *ptr = strtok(doc, "D");
  while (ptr != NULL) // 자른 문자열이 나오지 않을 때까지 반복
  {
    if (parsecount == 1)
    {
      parsecount++;
    }
    else
    {
      //printf("D cuting=>%s\n", ptr); // 자른 문자열 출력
      a = sscanf(ptr, ":%3sx:%4sy:%4sz:%4s", divice, x, y, z);
     // printf("parse device xyz =>%s, %s, %s, %s \n", divice, x, y, z);//일정한 형태로 된 부분만 파싱가능(sscanf를 통하여)

      sprintf(transtext[transfer_max_index], "Device num =%s, X=%s, Y=%s, Z=%s \n", divice, x, y, z); //보내고싶은 메시지 세팅.......
      printf("%s\n", transtext[transfer_max_index]);
      if (transfer_max_index == 19)
      {
        transfer_max_index = 0;
      }
      else{
          transfer_max_index++;
      }
    }
    ptr = strtok(NULL, "D"); // 다음 문자열을 잘라서 포인터를 반환
  }
  //socket setting

}

int main()
{
  pthread_t snd_thread;
  int sock;





  if (test_mode != 2)
  {
    ////////////////////////////////tehread
    struct sockaddr_in serv_addr;
    pthread_t rcv_thread;
    ///////////////////////////////thread setting
    sock = socket(PF_INET, SOCK_STREAM, 0);

    if (sock == -1)
      error_handling("socket() error");
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(SERVERIP);
    serv_addr.sin_port = htons(PORT);
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
      error_handling("connect() error!");
    }
    pthread_create(&rcv_thread, NULL, recv_message, (void *)sock);
    ////////////////////////////////tehread+
  }
  long fre,band,pream;
  int sf,coding;
  char senddate[MAX_PKT_LENGTH];
  memset(senddate, 0, sizeof(char) * 128); //char pointer 초기화
  printf("Please enter frequency(Mhz) :\n");
  scanf("%ld",&fre);
  printf("Please enter bandwiSignalBandwidth(KHz) :\n");
  scanf("%ld",&band);
  printf("Please enter SpreadingFactor :\n");
  scanf("%i",&sf);
  printf("Please enter CodingRate(4/) :\n");
  scanf("%i",&coding);
  printf("Please enter Send Text :\n");
  scanf("%s",senddate);
  usleep(200);
  printf("input text %s\n",senddate);
  printf("input text size  %d\n",strlen(senddate));

  // Lora_begin();
  Lora_begin(fre,band,coding,sf);

  while(1){
  beginPacket(0);
  Lora_write(senddate,strlen(senddate));
  endPacket();
  printf("send [%s] done \n",senddate);
  usleep(500);
  }  
  return 0; 

}
