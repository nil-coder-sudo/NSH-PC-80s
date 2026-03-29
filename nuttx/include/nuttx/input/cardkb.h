#ifndef __INCLUDE_NUTTX_INPUT_CARDKB_H
#define __INCLUDE_NUTTX_INPUT_CARDKB_H

#include <nuttx/i2c/i2c_master.h>

int cardkb_register(FAR const char *devpath, FAR struct i2c_master_s *i2c);

#endif