#include <nuttx/config.h>
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/fs/fs.h>
#include <kmm_heap.h>
#include <errno.h>

#define CARDKB_ADDR 0x5F

struct cardkb_dev_s {
  FAR struct i2c_master_s *i2c;
};

static ssize_t cardkb_read(FAR struct file *filep, FAR char *buffer, size_t buflen) {
  FAR struct inode *inode = filep->f_inode;
  FAR struct cardkb_dev_s *priv = inode->i_private;
  uint8_t key_code;
  
  /* I2C 讀取協議 */
  struct i2c_msg_s msg = {
    .frequency = 100000,
    .addr      = CARDKB_ADDR,
    .flags     = I2C_M_READ,
    .buffer    = &key_code,
    .length    = 1
  };

  if (I2C_TRANSFER(priv->i2c, &msg, 1) < 0) return 0;
  if (key_code == 0) return 0;

  buffer[0] = key_code;
  return 1;
}

static const struct file_operations g_cardkb_fops = {
  .read = cardkb_read,
};

int cardkb_register(FAR const char *devpath, FAR struct i2c_master_s *i2c) {
  FAR struct cardkb_dev_s *priv = (struct cardkb_dev_s *)kmm_zalloc(sizeof(struct cardkb_dev_s));
  if (!priv) return -ENOMEM;
  priv->i2c = i2c;
  return register_driver(devpath, &g_cardkb_fops, 0666, priv);
}
