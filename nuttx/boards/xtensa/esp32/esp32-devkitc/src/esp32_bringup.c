/****************************************************************************
 * boards/xtensa/esp32/esp32-devkitc/src/esp32_bringup.c
 ****************************************************************************/

#include <nuttx/config.h>
#include <syslog.h>
#include <errno.h>
#include <debug.h>
#include <string.h>
#include <fcntl.h>     /* 用於 open */
#include <unistd.h>    /* 用於 dup2, close */
#include <stdio.h>     /* 用於 dprintf */

#include <nuttx/i2c/i2c_master.h>
#include <nuttx/lcd/pcf8574_lcd_backpack.h>
#include <nuttx/input/cardkb.h>  /* 🌟 引入 CardKB 頭文件 */

#include "esp32_i2c.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int esp32_bringup(void)
{
  struct i2c_master_s *i2c;
  int ret;
  int fd;

  /* 1. LCD 配置 (保留你原有的設定) */
  struct pcf8574_lcd_backpack_config_s cfg;
  
  cfg.addr   = 0x27;
  cfg.rows   = 4;
  cfg.cols   = 20;
  cfg.rs     = 0;
  cfg.rw     = 1;
  cfg.en     = 2;
  cfg.bl     = 3;
  cfg.d4     = 4;
  cfg.d5     = 5;
  cfg.d6     = 6;
  cfg.d7     = 7;
  cfg.bl_active_high = true;

  syslog(LOG_INFO, "[BRINGUP] Initializing I2C0, LCD and CardKB...\n");

  /* 2. 初始化 I2C0 匯流排 (GPIO 21/22) */
  i2c = esp32_i2cbus_initialize(0);
  if (!i2c)
    {
      syslog(LOG_ERR, "ERROR: Failed to initialize I2C0\n");
      return -ENODEV;
    }

  /* 3. 註冊 I2C 字元設備 /dev/i2c0 */
  ret = i2c_register(i2c, 0);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to register /dev/i2c0: %d\n", ret);
    }

  /* 🌟 4. 註冊 CardKB 驅動程式 /dev/kb0 */
  /* CardKB 使用同一條 i2c 總線 */
  ret = cardkb_register("/dev/kb0", i2c);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to register CardKB at /dev/kb0: %d\n", ret);
    }
  else
    {
      syslog(LOG_INFO, "[BRINGUP] CardKB registered at /dev/kb0\n");
    }

  /* 5. 註冊 LCD 驅動程式 /dev/lcd0 */
  ret = pcf8574_lcd_backpack_register("/dev/lcd0", i2c, &cfg);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to register /dev/lcd0: %d\n", ret);
      return ret;
    }

  /* 6. 奪取 Console 控制權 */
  fd = open("/dev/lcd0", O_RDWR);
  if (fd >= 0)
    {
      write(fd, "\v\f", 2); /* 開背光 + 清屏 */
      
      /* 重定向 stdout 和 stderr 到 LCD */
      dup2(fd, 1); 
      dup2(fd, 2);

     
       int kbfd = open("/dev/kb0", O_RDONLY);
       if (kbfd >= 0) dup2(kbfd, 0); 
       

      if (fd > 2) close(fd);
    }

  syslog(LOG_INFO, "[BRINGUP] All Systems Go! LCD + CardKB Ready.\n");

  return OK;
}