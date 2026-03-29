/****************************************************************************
 * boards/arm/sam34/arduino-due/src/sam_bringup.c
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <syslog.h>

#include <nuttx/board.h>
#include <nuttx/fs/fs.h>
#include <nuttx/leds/userled.h>

/* 必須包含的顯示器與 I2C 標頭檔 */
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/lcd/lcd.h>
#include <nuttx/lcd/ssd1306.h>
#include <nuttx/lcd/lcd_dev.h>

#include "arduino-due.h"
#include <arch/board/board.h>
#include "sam_twi.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* 定義全域指標，存放 LCD 設備實例，供系統 getdev 呼叫 */
static struct lcd_dev_s *g_lcddev = NULL;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: board_lcd_getdev
 *
 * Description:
 * 由 lcddev_register 內部呼叫，用來取得已初始化的 LCD 實例。
 *
 ****************************************************************************/

struct lcd_dev_s *board_lcd_getdev(int lcddev)
{
  return g_lcddev;
}

/****************************************************************************
 * Name: board_lcd_setdev
 ****************************************************************************/

void board_lcd_setdev(struct lcd_dev_s *dev)
{
  g_lcddev = dev;
}

/****************************************************************************
 * Name: sam_bringup
 ****************************************************************************/

int sam_bringup(void)
{
  int ret;
  struct i2c_master_s *i2c;
  struct lcd_dev_s *lcd;

#ifdef CONFIG_FS_PROCFS
  /* 掛載 proc 檔案系統 */
  ret = nx_mount(NULL, "/proc", "procfs", 0, NULL);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to mount procfs at /proc: %d\n", ret);
    }
#endif

  /* --------------------------------------------------------------------
   * SSD1306 OLED 初始化與註冊
   * -------------------------------------------------------------------- */
  
  /* 1. 初始化 TWI0 (Due 的 SDA1/SCL1 引腳) */
  i2c = sam_i2cbus_initialize(0); 
  if (!i2c)
    {
      syslog(LOG_ERR, "ERROR: Failed to initialize TWI0\n");
    }
  else
    {
      /* 2. 初始化 SSD1306 驅動 (預設 128x64) */
      lcd = ssd1306_initialize(i2c, 0, 0);
      if (!lcd)
        {
          syslog(LOG_ERR, "ERROR: Failed to initialize SSD1306 driver\n");
        }
      else
        {
          /* 3. 點亮螢幕功率 */
          lcd->setpower(lcd, CONFIG_LCD_MAXPOWER);
          
          /* 4. 將實例存入全域變數，供 board_lcd_getdev 使用 */
          board_lcd_setdev(lcd);
          
          /* 5. 註冊字元設備 /dev/lcd0 */
          ret = lcddev_register(0);
          if (ret < 0)
            {
              syslog(LOG_ERR, "ERROR: Failed to register /dev/lcd0: %d\n", ret);
            }
          else
            {
              syslog(LOG_INFO, "SSD1306 OLED /dev/lcd0 registered successfully\n");
            }
        }
    }

#ifdef HAVE_LEDS
  /* 初始化板載 LED */
  board_userled_initialize();
  userled_lower_initialize(LED_DRIVER_PATH);
#endif

  UNUSED(ret);
  return OK;
}