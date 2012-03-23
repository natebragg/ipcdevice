#ifndef __ipcdevice_h
#define __ipcdevice_h

#include <linux/ioctl.h>
#include <linux/types.h>

#define IPC_IOC_ROT13   _IOW('i', 0x70, int)
#define IPC_IOC_BASE64  _IOW('i', 0x71, int)
#define IPC_IOC_REVERSE _IOW('i', 0x72, int)

#define IPC_ENABLE 1
#define IPC_DISABLE 0

#endif /* __ipcdevice_h */
