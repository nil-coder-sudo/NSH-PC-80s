#include <nuttx/config.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

/* 手動定義指令，確保編譯不噴 _none 錯誤 */
#define MY_SETPOWER    _LCDIOC(0x0001) /* 等同於 LCDDEVIO_SETPOWER */
#define MY_SETCONTRAST _LCDIOC(0x0002) /* 等同於 LCDDEVIO_SETCONTRAST */

int main(int argc, char *argv[])
{
  int fd;

  printf("[OLED] 正在執行暴力點亮程式...\n");

  fd = open("/dev/lcd0", O_RDWR);
  if (fd < 0)
    {
      printf("[OLED] 失敗：打不開 /dev/lcd0\n");
      return -1;
    }

  /* 1. 發送點亮指令 */
  ioctl(fd, MY_SETPOWER, 1);
  
  /* 2. 把對比度拉到最高 */
  ioctl(fd, MY_SETCONTRAST, 255);

  printf("[OLED] 指令已發送。請檢查螢幕是否有任何反應。\n");
  
  close(fd);
  return 0;
}