#include <nuttx/config.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <nuttx/kmalloc.h>
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/lcd/ssd1306.h>
#include "ssd1306.h"

int ssd1306_sendbyte(FAR struct ssd1306_dev_s *priv, uint8_t regval)
{
  struct i2c_config_s config;
  uint8_t buffer[2];

  config.frequency = 400000;
  config.address   = priv->addr;
  config.addrlen   = 7;

  buffer[0] = 0x00; // Command
  buffer[1] = regval;

  return i2c_write(priv->i2c, &config, buffer, 2);
}

int ssd1306_sendblk(FAR struct ssd1306_dev_s *priv, uint8_t *data, uint8_t len)
{
  struct i2c_config_s config;
  uint8_t *buffer;
  int ret;

  buffer = (uint8_t *)kmm_malloc(len + 1);
  if (!buffer) return -ENOMEM;

  config.frequency = 400000;
  config.address   = priv->addr;
  config.addrlen   = 7;

  buffer[0] = 0x40; // Data
  memcpy(&buffer[1], data, len);

  ret = i2c_write(priv->i2c, &config, buffer, len + 1);
  kmm_free(buffer);
  return ret;
}

/* 這裡不需要定義 ssd1306_initialize，ssd1306_base.c 會處理它 */