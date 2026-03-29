#include <nuttx/config.h>
#include <debug.h>
#include <stddef.h>
#include <syslog.h>      /* 新增：用於日誌輸出 */

#include <nuttx/fs/fs.h>
#include <nuttx/i2c/i2c_master.h> /* 新增：I2C 支持 */
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/lcd/ssd1306.h>
#include <nuttx/lcd/lcd_dev.h>
#include <nuttx/lcd/lcd_dev.h>
#include <nuttx/lcd/lcd_dev.h>
#include <nuttx/lcd/lcd_dev.h>  /* 新增：OLED 支持 */
#include <nuttx/lcd/lcd.h>       /* 新增：LCD 註冊 */

#include <arch/board/board.h>
#include "rp23xx_pico.h"
#include "rp23xx_i2c.h"
#include "rp23xx_i2c.h"           /* 新增：Pico 2 I2C 驅動 */

#ifdef CONFIG_ARCH_BOARD_COMMON
#include "rp23xx_common_bringup.h"
#endif

#ifdef CONFIG_USERLED
#  include <nuttx/leds/userled.h>
#endif

/****************************************************************************
 * Name: rp23xx_bringup
 ****************************************************************************/

int rp23xx_bringup(void)
{
  int ret = OK;

#ifdef CONFIG_ARCH_BOARD_COMMON
  ret = rp23xx_common_bringup();
  if (ret < 0)
    {
      return ret;
    }
#endif

  /* --- 你的 Mini PC 硬體初始化開始 --- */

  struct i2c_master_s *i2c;
  struct lcd_dev_s *lcd;

  /* 1. 初始化 I2C0 (預設 GP4=SDA, GP5=SCL) */
  i2c = rp23xx_i2cbus_initialize(0);
  if (i2c == NULL)
    {
      syslog(LOG_ERR, "ERROR: MiniPC Failed to initialize I2C0\n");
    }
  else
    {
      /* 2. 註冊 I2C 設備，讓 /dev/i2c0 出現 */
      i2c_register(i2c, 0);

      /* 3. 初始化 0.42" OLED (SSD1306, 72x40) */
      /* 0x3c 是 0.42 吋螢幕最常見的地址 */
      lcd = ssd1306_initialize(i2c, NULL, 0);
      if (lcd != NULL)
        {
          /* 將 OLED 註冊為 /dev/lcd0 */
          ret = lcddev_register(0);
          if (ret < 0)
            {
              syslog(LOG_ERR, "ERROR: LCD registration failed: %d\n", ret);
            }
          else
            {
              syslog(LOG_INFO, "MiniPC: OLED 0.42 initialized at /dev/lcd0\n");
            }
        }
    }

  /* --- 原有的其他驅動初始化 --- */

#ifdef CONFIG_USERLED
  ret = userled_lower_initialize("/dev/userleds");
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: userled_lower_initialize() failed: %d\n", ret);
    }
#endif

#ifdef CONFIG_INPUT_BUTTONS
  ret = btn_lower_initialize("/dev/buttons");
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: btn_lower_initialize() failed: %d\n", ret);
    }
#endif

  return ret;
}
