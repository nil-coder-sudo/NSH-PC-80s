#include <nuttx/config.h>
#include <nuttx/board.h>
#include <nuttx/spi/spi.h>
#include <nuttx/lcd/ili9341.h>
#include <syslog.h>
#include <errno.h>

/* 確保這些頭文件存在 */
#include "sam_spi.h"
#include "board.h"

/****************************************************************************
 * Name: board_lcd_initialize
 *
 * Description:
 * Initializes the ILI9341 driver and registers it to the system.
 ****************************************************************************/

int board_lcd_initialize(void)
{
  struct spi_dev_s *spi;
  int ret;

  /* 1. 獲取 SPI0 匯流排 (Due 的主 SPI 埠) */
  spi = sam_spibus_initialize(0); /* 0 for SPI0 on Due */
  if (!spi)
    {
      syslog(LOG_ERR, "ERROR: Failed to initialize SPI port 0\n");
      return -ENODEV;
    }

  /* 2. 註冊 ILI9341 驅動。它將使用 board.h 中定義的 GPIO 宏 */
  ret = ili9341_lcdinitialize(spi);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: ili9341_lcdinitialize failed: %d\n", ret);
      return ret;
    }

  return OK;
}
