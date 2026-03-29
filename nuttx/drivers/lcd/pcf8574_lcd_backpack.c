/****************************************************************************
 * drivers/lcd/pcf8574_lcd_backpack.c
 * Final Stable Version - Double Newline Fix & Scroll Intercept
 ****************************************************************************/

#include <nuttx/config.h>
#include <ctype.h>
#include <poll.h>
#include <string.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/arch.h>
#include <nuttx/kmalloc.h>
#include <nuttx/mutex.h>
#include <nuttx/ascii.h>
#include <nuttx/fs/fs.h>
#include <nuttx/lcd/pcf8574_lcd_backpack.h>

#define I2C_FREQ            100000
#define DELAY_US_NYBBLE0    20
#define DELAY_US_NYBBLE1    10
#define DELAY_US_WRITE      40
#define DELAY_US_HOMECLEAR  2000

#define CMD_CLEAR           0x01
#define CMD_SET_DDADDR      0x80

#define HISTORY_LINES       64    
#define LCD_ROWS            4     
#define LCD_COLS            20    
#define PROMPT_LEN          5     

struct pcf8574_lcd_dev_s
{
  FAR struct i2c_master_s *i2c;
  struct pcf8574_lcd_backpack_config_s cfg;
  uint8_t bl_bit;
  uint8_t refs;
  mutex_t lock;

  char history[HISTORY_LINES][LCD_COLS];
  int  head_row;     
  int  cur_col;      
  int  view_offset;  
};

/* --- 全域指標：讓鍵盤驅動能直接找到 LCD --- */
FAR struct pcf8574_lcd_dev_s *g_lcd_dev = NULL;

/****************************************************************************
 * Private Functions (Hardware Control)
 ****************************************************************************/

static void pca8574_write(FAR struct pcf8574_lcd_dev_s *priv, uint8_t data)
{
  struct i2c_config_s config;
  config.frequency = I2C_FREQ;
  config.address = priv->cfg.addr;
  config.addrlen = 7;
  i2c_write(priv->i2c, &config, &data, 1);
}

static void latch_nybble(FAR struct pcf8574_lcd_dev_s *priv, uint8_t nybble, bool rs)
{
  uint8_t lcddata;
  uint8_t en_bit = 1 << priv->cfg.en;
  uint8_t rs_bit = rs ? (1 << priv->cfg.rs) : 0;
  uint8_t prep = 0;
  if (nybble & 0x08) prep |= (1 << priv->cfg.d7);
  if (nybble & 0x04) prep |= (1 << priv->cfg.d6);
  if (nybble & 0x02) prep |= (1 << priv->cfg.d5);
  if (nybble & 0x01) prep |= (1 << priv->cfg.d4);
  lcddata = prep | priv->bl_bit | rs_bit;
  pca8574_write(priv, lcddata);
  lcddata |= en_bit; pca8574_write(priv, lcddata);
  up_udelay(DELAY_US_NYBBLE0);
  lcddata &= ~en_bit; pca8574_write(priv, lcddata);
  up_udelay(DELAY_US_NYBBLE1);
}

static void lcd_putcmd(FAR struct pcf8574_lcd_dev_s *priv, uint8_t data)
{
  latch_nybble(priv, data >> 4, false); latch_nybble(priv, data, false);
  up_udelay(DELAY_US_WRITE);
}

static void lcd_putdata(FAR struct pcf8574_lcd_dev_s *priv, uint8_t data)
{
  latch_nybble(priv, data >> 4, true); latch_nybble(priv, data, true);
  up_udelay(DELAY_US_WRITE);
}

static void lcd_set_curpos(FAR struct pcf8574_lcd_dev_s *priv, uint8_t row, uint8_t col)
{
  uint8_t addr;
  if (row < 2) addr = row * 0x40 + col;
  else addr = (row - 2) * 0x40 + (col + priv->cfg.cols);
  lcd_putcmd(priv, CMD_SET_DDADDR | addr);
}

void pcf8574_refresh_screen(FAR struct pcf8574_lcd_dev_s *priv)
{
  int r, c;
  for (r = 0; r < LCD_ROWS; r++)
    {
      int hist_idx = (priv->head_row - (LCD_ROWS - 1) - priv->view_offset + r + HISTORY_LINES) % HISTORY_LINES;
      lcd_set_curpos(priv, r, 0);
      for (c = 0; c < LCD_COLS; c++)
        lcd_putdata(priv, priv->history[hist_idx][c]);
    }
  if (priv->view_offset == 0) lcd_set_curpos(priv, LCD_ROWS - 1, priv->cur_col);
}

/* 外部捲動接口 */
void pcf8574_scroll(int direction)
{
  if (g_lcd_dev == NULL) return;
  nxmutex_lock(&g_lcd_dev->lock);
  if (direction > 0) { /* Up */
    if (g_lcd_dev->view_offset < (HISTORY_LINES - LCD_ROWS)) g_lcd_dev->view_offset++;
  } else { /* Down */
    if (g_lcd_dev->view_offset > 0) g_lcd_dev->view_offset--;
  }
  pcf8574_refresh_screen(g_lcd_dev);
  nxmutex_unlock(&g_lcd_dev->lock);
}

/****************************************************************************
 * 核心寫入函數 (含換行去重)
 ****************************************************************************/

static ssize_t pcf8574_lcd_write(FAR struct file *filep, FAR const char *buffer, size_t buflen)
{
  FAR struct inode *inode = filep->f_inode;
  FAR struct pcf8574_lcd_dev_s *priv = inode->i_private;
  size_t i;
  nxmutex_lock(&priv->lock);
  for (i = 0; i < buflen; i++)
    {
      unsigned char ch = buffer[i];

      /* 處理 ANSI 過濾 (防止 [K 亂碼) */
      if (ch == '\x1b' && i + 1 < buflen && buffer[i+1] == '[')
        {
          i += 2; while (i < buflen && !isalpha(buffer[i])) { i++; }
          continue;
        }

      /* 退格保護 (不刪除 nsh>) */
      if (ch == '\b' || ch == 0x7f)
        {
          if (priv->cur_col > PROMPT_LEN)
            {
              priv->cur_col--;
              priv->history[priv->head_row][priv->cur_col] = ' ';
              if (priv->view_offset == 0)
                {
                  lcd_set_curpos(priv, LCD_ROWS - 1, priv->cur_col);
                  lcd_putdata(priv, ' ');
                  lcd_set_curpos(priv, LCD_ROWS - 1, priv->cur_col);
                }
            }
          continue;
        }

      /* --- 換行處理：解決雙重換行問題 --- */
      if (ch == '\n' || ch == '\r')
        {
          /* 只有當目前游標不在行首時才換行，防止 cardkb 與 nsh 同時觸發換行 */
          if (priv->cur_col > 0) 
            {
              priv->cur_col = 0;
              priv->head_row = (priv->head_row + 1) % HISTORY_LINES;
              memset(priv->history[priv->head_row], ' ', LCD_COLS);
              if (priv->view_offset == 0) pcf8574_refresh_screen(priv);
            }
          continue;
        }

      /* 普通字元顯示 */
      if (isprint(ch))
        {
          /* 自動換行 */
          if (priv->cur_col >= LCD_COLS)
            {
              priv->cur_col = 0;
              priv->head_row = (priv->head_row + 1) % HISTORY_LINES;
              memset(priv->history[priv->head_row], ' ', LCD_COLS);
              if (priv->view_offset == 0) pcf8574_refresh_screen(priv);
            }
          
          priv->history[priv->head_row][priv->cur_col] = ch;
          if (priv->view_offset == 0)
            {
              lcd_set_curpos(priv, LCD_ROWS - 1, priv->cur_col);
              lcd_putdata(priv, ch);
            }
          priv->cur_col++;
        }
    }
  nxmutex_unlock(&priv->lock);
  return buflen;
}

/* --- 系統介面函數 (Open, Close, Seek, Poll, IOCTL) --- */

static int pcf8574_lcd_open(FAR struct file *filep) {
  FAR struct pcf8574_lcd_dev_s *priv = filep->f_inode->i_private;
  nxmutex_lock(&priv->lock); priv->refs++; nxmutex_unlock(&priv->lock);
  return OK;
}
static int pcf8574_lcd_close(FAR struct file *filep) {
  FAR struct pcf8574_lcd_dev_s *priv = filep->f_inode->i_private;
  nxmutex_lock(&priv->lock); if (priv->refs > 0) priv->refs--; nxmutex_unlock(&priv->lock);
  return OK;
}
static ssize_t pcf8574_lcd_read(FAR struct file *filep, FAR char *buffer, size_t buflen) { return 0; }
static off_t pcf8574_lcd_seek(FAR struct file *filep, off_t offset, int whence) { return 0; }
static int pcf8574_lcd_poll(FAR struct file *filep, FAR struct pollfd *fds, bool setup) { return OK; }
static int pcf8574_lcd_ioctl(FAR struct file *filep, int cmd, unsigned long arg) {
  if (cmd == SLCDIOC_GETATTRIBUTES) {
    FAR struct slcd_attributes_s *attr = (FAR struct slcd_attributes_s *)arg;
    attr->nrows = LCD_ROWS; attr->ncolumns = LCD_COLS; return OK;
  }
  return -ENOTTY;
}

static const struct file_operations g_pcf8574_lcd_fops =
{
  pcf8574_lcd_open, pcf8574_lcd_close, pcf8574_lcd_read, pcf8574_lcd_write,
  pcf8574_lcd_seek, pcf8574_lcd_ioctl, NULL, NULL, pcf8574_lcd_poll, NULL, NULL
};

int pcf8574_lcd_backpack_register(FAR const char *devpath, FAR struct i2c_master_s *i2c, FAR struct pcf8574_lcd_backpack_config_s *cfg)
{
  FAR struct pcf8574_lcd_dev_s *priv = kmm_zalloc(sizeof(struct pcf8574_lcd_dev_s));
  if (!priv) return -ENOMEM;
  priv->i2c = i2c; priv->cfg = *cfg;
  priv->bl_bit = priv->cfg.bl_active_high ? (1 << priv->cfg.bl) : 0;
  priv->head_row = LCD_ROWS - 1;
  memset(priv->history, ' ', sizeof(priv->history));
  nxmutex_init(&priv->lock);
  g_lcd_dev = priv; /* 賦值全域指標 */
  
  nxsched_usleep(50000);
  latch_nybble(priv, 0x30 >> 4, false); nxsched_usleep(5000);
  latch_nybble(priv, 0x30 >> 4, false); nxsched_usleep(5000);
  latch_nybble(priv, 0x20 >> 4, false); nxsched_usleep(5000);
  lcd_putcmd(priv, 0x28); lcd_putcmd(priv, 0x08);
  lcd_putcmd(priv, CMD_CLEAR); up_udelay(DELAY_US_HOMECLEAR);
  lcd_putcmd(priv, 0x06); lcd_putcmd(priv, 0x0c);
  return register_driver(devpath, &g_pcf8574_lcd_fops, 0666, priv);
}