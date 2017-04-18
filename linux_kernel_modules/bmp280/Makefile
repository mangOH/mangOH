#
# Makefile for industrial I/O pressure drivers
#

# When adding new entries keep the list in alphabetical order
obj-$(CONFIG_BMP280) += bmp280.o
bmp280-objs := bmp280-core.o bmp280-regmap.o
obj-$(CONFIG_BMP280_I2C) += bmp280-i2c.o
obj-$(CONFIG_BMP280_SPI) += bmp280-spi.o
