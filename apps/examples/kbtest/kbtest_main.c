#include <nuttx/config.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
  int fd;
  char key;

  printf("CardKB Test App Started...\n");
  printf("Opening /dev/kb0...\n");

  fd = open("/dev/kb0", O_RDONLY);
  if (fd < 0)
    {
      printf("ERROR: Failed to open /dev/kb0. Did you register the driver?\n");
      return -1;
    }

  printf("Successfully opened /dev/kb0. Press keys on CardKB (Ctrl+C to stop):\n");

  while (1)
    {
      if (read(fd, &key, 1) > 0)
        {
          /* 根據你提供的鍵位圖，直接印出字元與 Hex */
          printf("Key: '%c' | Hex: 0x%02x\n", key, key);
        }
      usleep(50000); // 50ms 輪詢一次
    }

  close(fd);
  return 0;
}
