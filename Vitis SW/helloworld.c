#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <time.h>
#include <sys/ioctl.h>
#include <linux/ioctl.h>
#include <sys/stat.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>

#define ARRAY_SIZE(array) sizeof(array)/sizeof(array[0])

static const char *spi_device = "/dev/spidev1.0";
static const char *i2c_device = "/dev/i2c-3";
static const char address = 0x3F;

static uint8_t bits = 8;
static uint32_t speed = 1000000;
static uint32_t mode =SPI_MODE_3 ;
static uint16_t delay;

int main(){
	int spi;
	int i2c;
	int ret = 0;
	unsigned char spi_tx_buf[2];
	unsigned char spi_rx_buf[2];
	unsigned char i2c_tx_buf[2];
	unsigned char i2c_rx_buf[2];

	int temp;
	float spi_temp_degC;
	int16_t spi_temperature;
	float i2c_temp_degC;
	int16_t i2c_temperature;
	char temp_l;
	char temp_h;


	struct i2c_msg msgs[2];
	struct i2c_rdwr_ioctl_data msgset[1];

	struct spi_ioc_transfer tr = {
			.tx_buf = (unsigned long)spi_tx_buf,
			.rx_buf = (unsigned long)spi_rx_buf,
			.len = ARRAY_SIZE(spi_tx_buf),
			.delay_usecs = delay,
			.speed_hz = 0,
			.bits_per_word = 0,
		};

//Open SPI and I2C
	spi = open(spi_device, O_RDWR);
	if (spi < 0)
		printf("can't open SPI device\n\r");

	i2c = open(i2c_device, O_RDWR);
	if (i2c < 0)
		printf("can't open I2C device\n\r");

//configure SPI
	ret = ioctl(spi, SPI_IOC_WR_MODE32, &mode);
	if (ret == -1)
		printf("can't set spi mode\n\r");

	ret = ioctl(spi, SPI_IOC_RD_MODE32, &mode);
	if (ret == -1)
		printf("can't get spi mode\n\r");

	ret = ioctl(spi, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
		printf("can't set bits per word\n\r");

	ret = ioctl(spi, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
		printf("can't get bits per word\n\r");

	ret = ioctl(spi, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		printf("can't set max speed hz\n\r");

	ret = ioctl(spi, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		printf("can't get max speed hz\n\r");

//enable SPI sensor sampling
	spi_tx_buf[0] = (char) 0x10; //enable sampling
	spi_tx_buf[1] = (char) 0x20;

	ret = ioctl(spi, SPI_IOC_MESSAGE(1), &tr);
	if (ret == -1)
			printf("IOCTL error\n\r");

//check SPI temp sensor can be detected
	spi_tx_buf[0] = (char) 0x8F; // read who am I
	spi_tx_buf[1] = (char) 0x00;

	ret = ioctl(spi, SPI_IOC_MESSAGE(1), &tr);
	if (ret == -1)
			printf("IOCTL error\n\r");
	if (spi_rx_buf[1] == 0xb3)
		printf("LPS22 Detected\n\r");

//check I2C temp sensor can be detected
	i2c_tx_buf[0] = 0x01;

    msgs[0].addr = address;
    msgs[0].flags = 0;
    msgs[0].len = 1;
    msgs[0].buf = i2c_tx_buf;

    msgs[1].addr = address;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = 1;
    msgs[1].buf = i2c_rx_buf;

    msgset[0].msgs = msgs;
    msgset[0].nmsgs = 2;

    if (ioctl(i2c, I2C_RDWR, &msgset) < 0) {
        perror("ioctl(I2C_RDWR) in i2c_read");
    }

    if (i2c_rx_buf[0] == 0xa0)
    	printf("STTS22H Detected\n\r");


//set sampling on the I2C temp sensor
	i2c_tx_buf[0] = 0x04;
	i2c_tx_buf[1] = 0x0c;

	msgs[0].addr = address;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = i2c_tx_buf;

	msgset[0].msgs = msgs;
	msgset[0].nmsgs = 1;

	if (ioctl(i2c, I2C_RDWR, &msgset) < 0)
		perror("ioctl(I2C_RDWR) in i2c_write");

while(1){

//read temperature from the SPI sensor
		spi_tx_buf[0] = (char) 0xab;
		spi_tx_buf[1] = (char) 0x00;

		ret = ioctl(spi, SPI_IOC_MESSAGE(1), &tr);
		if (ret == -1)
				printf("IOCTL error\n\r");

		temp_l = spi_rx_buf[1];


		spi_tx_buf[0] = (char) 0xac;
		spi_tx_buf[1] = (char) 0x00;

		ret = ioctl(spi, SPI_IOC_MESSAGE(1), &tr);
		if (ret == -1)
				printf("IOCTL error\n\r");
		temp_h = spi_rx_buf[1];

		temp = ((temp_h <<8)|(temp_l));

		if ((temp & 0x8000) == 0) //msb = 0 so not negative
		    {
		    	spi_temperature = temp;
		    }
		    else
		    {
				// Otherwise perform the 2's complement math on the value
				spi_temperature = (~(temp - 0x01)) * -1;
		    }
		spi_temp_degC = (float) spi_temperature /100.0;

//Read temperature from the I2C sensor
		i2c_tx_buf[0] = 0x06;

	    msgs[0].addr = address;
	    msgs[0].flags = 0;
	    msgs[0].len = 1;
	    msgs[0].buf = i2c_tx_buf;

	    msgs[1].addr = address;
	    msgs[1].flags = I2C_M_RD;
	    msgs[1].len = 2;
	    msgs[1].buf = i2c_rx_buf;

	    msgset[0].msgs = msgs;
	    msgset[0].nmsgs = 2;

	    if (ioctl(i2c, I2C_RDWR, &msgset) < 0) {
	        perror("ioctl(I2C_RDWR) in i2c_read");
	    }

	    i2c_temperature = i2c_rx_buf[1] << 8 | i2c_rx_buf[0];
	    i2c_temp_degC = (float) i2c_temperature / 100;

		printf("SPI Temp Deg C %4.2f  I2C Temp Deg C %4.2f Difference Temp Deg C %4.2f  \n\n\r",spi_temp_degC, i2c_temp_degC, (spi_temp_degC - i2c_temp_degC) );



		usleep(1000000);
	}
}
