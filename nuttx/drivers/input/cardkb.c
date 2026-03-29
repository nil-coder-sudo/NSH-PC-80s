/****************************************************************************
 * drivers/input/cardkb.c
 * Final Direct-Control Version
 ****************************************************************************/

#include <nuttx/config.h>
#include <sys/types.h>
#include <stdbool.h>
#include <stdio.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <nuttx/kmalloc.h>
#include <nuttx/fs/fs.h>
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/input/cardkb.h>
#include <nuttx/sched.h>

/* 宣告外部 LCD 函數 */
void pcf8574_scroll(int direction);

#define CARDKB_I2C_ADDR 0x5f
#define CARDKB_I2C_FREQ 100000 

struct cardkb_dev_s { FAR struct i2c_master_s *i2c; uint8_t addr; };

static int cardkb_open(FAR struct file *filep) { return OK; }
static int cardkb_close(FAR struct file *filep) { return OK; }

static ssize_t cardkb_read(FAR struct file *filep, FAR char *buffer, size_t buflen)
{
  FAR struct cardkb_dev_s *priv = filep->f_inode->i_private;
  struct i2c_config_s config;
  uint8_t regval = 0;
  int ret;

  config.frequency = CARDKB_I2C_FREQ;
  config.address   = priv->addr;
  config.addrlen   = 7;

  while (1)
    {
      ret = i2c_read(priv->i2c, &config, &regval, 1);
      if (ret == OK && regval != 0)
        {
          if (regval == 0x0D) { /* Enter */
            fputs("\r\n", stdout); fflush(stdout);
            buffer[0] = '\n'; return 1;
          }
          if (regval == 0xB5) { /* Up - 直接控制 LCD */
            pcf8574_scroll(1); continue; 
          }
          if (regval == 0xB6) { /* Down - 直接控制 LCD */
            pcf8574_scroll(-1); continue;
          }
          if (regval == 0x08 || regval == 0x7F) { buffer[0] = 0x08; return 1; }
          if (regval >= 32 && regval <= 126) {
            fputc((char)regval, stdout); fflush(stdout);
            buffer[0] = (char)regval; return 1;
          }
        }
      nxsched_usleep(50000); 
    }
}

/* ... poll 和 register 保持不變 ... */

static int cardkb_poll(FAR struct file *filep, FAR struct pollfd *fds, bool setup)
{
  if (setup) { fds->revents |= (fds->events & POLLIN); poll_notify(&fds, 1, POLLIN); }
  return OK;
}

static const struct file_operations g_cardkb_fops =
{
  cardkb_open, cardkb_close, cardkb_read, NULL, NULL, NULL, NULL, NULL, cardkb_poll, NULL, NULL
};

int cardkb_register(FAR const char *devpath, FAR struct i2c_master_s *i2c)
{
  FAR struct cardkb_dev_s *priv = kmm_malloc(sizeof(struct cardkb_dev_s));
  if (!priv) return -ENOMEM;
  priv->i2c = i2c; priv->addr = CARDKB_I2C_ADDR;
  return register_driver(devpath, &g_cardkb_fops, 0666, priv);
}