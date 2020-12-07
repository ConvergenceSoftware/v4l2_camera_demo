#ifndef V4L2_H
#define V4L2_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned char *rgb24;
extern int videoIsRun;


enum _cmd_group {
    IN_CMD_GENERIC = 0, // if you use non V4L2 input plugin you not need to deal the groups.
    IN_CMD_V4L2 = 1,
    IN_CMD_RESOLUTION = 2,
    IN_CMD_JPEG_QUALITY = 3,
    IN_CMD_PWC = 4,
};

struct _control {
    struct v4l2_queryctrl ctrl;
    int value;
    struct v4l2_querymenu* menuitems;
    /*  In the case the control a V4L2 ctrl this variable will specify
        that the control is a V4L2_CTRL_CLASS_USER control or not.
        For non V4L2 control it is not acceptable, leave it 0.
    */
    int class_id;
    int group;
};

typedef struct _control control;


//control_id
#define V4L2_SET_BRIGHTNESS                             0x00980900 //设置亮度
#define V4L2_SET_CONTRAST                               0x00980901 //设置对比度
#define V4L2_SET_SATURATION                             0x00980902 //设置饱和度
#define V4L2_SET_HUE                                    0x00980903 //设置色调
#define V4L2_SET_WHITE_BALANCE_TEMPERATURE_MODE         0x0098090C //设置白平衡模式 0-手动 1-自动
#define V4L2_SET_GAMMA                                  0x00980910 //设置gamma
#define V4L2_SET_WHITE_BALANCE_TEMPERATURE              0x0098091A //设置白平衡值
#define V4L2_SET_SHARTNESS                              0x0098091B //设置清晰度
#define V4L2_SET_EXPOSURE_MODE                          0x009A0901 //设置曝光模式 1-手动 3-自动
#define V4L2_SET_EXPOSURE                               0x009A0902 //设置曝光值
#define V4L2_SET_FOCUS_MODE                             0x009A090C //设置对焦模式 0-手动 1-自动
#define V4L2_SET_FOCUS                                  0x009A090A //设置对焦值

long getTimeUsec();
int GetDeviceCount();
char *GetDeviceName(int index);
char *GetCameraName(int index);
int StartRun(int index);
int GetFrame();
int StopRun();

int GetDevFmtWidth();
int GetDevFmtHeight();
int GetDevFmtSize();
int GetDevFmtBytesLine();

int GetResolutinCount();
int GetResolutionWidth(int index);
int GetResolutionHeight(int index);
int GetCurResWidth();
int GetCurResHeight();

void enumerateControls();
int v4l2GetControl(int control);
int v4l2SetControl(int control_id, int value);

int V4L2SetResolution(int new_width, int new_height);

#ifdef  __cplusplus
}
#endif

#endif // V4L2_H
