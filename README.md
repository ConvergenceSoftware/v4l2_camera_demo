# v4l2_camera_demo
linux V4L2调用UVC摄像头QT示例,可预览、切换分辨率、获取和修改摄像头参数
# 环境
ubuntu18.04 + QT5.9.9
# 安装依赖库
sudo apt-get install libjpeg-dev libv4l-dev
# 使用
camera_demo.pro中添加libjpeg.so的路径： 将unix:LIBS += **/usr/lib/x86_64-linux-gnu/libjpeg.so**替换为自己的路径
# API
|                             接口                             |         功能         |
| :----------------------------------------------------------: | :------------------: |
|                     StartVideoPrePare()                      |       准备工作       |
|                      StartVideoStream()                      |      打开视频流      |
|                       EndVideoStream()                       |      关闭视频流      |
|                          GetFrame()                          |     获取一帧图片     |
| MJPEG2RGB(unsigned char* data_frame, unsigned char *rgb, int bytesused) |      MJPEG转RGB      |
|       V4L2SetResolution(int new_width, int new_height)       |      切换分辨率      |
|                     enumerateControls()                      | 列举摄像头所有控制项 |
|                 v4l2GetControl(int control)                  |  获取指定控制项的值  |
|          v4l2SetControl(int control_id, int value)           |  设置指定控制项的值  |
|                v4l2ResetControl(int control)                 |  重置指定控制项的值  |
# V4L2控制项
|                 控制id                  |            控制项            |    id值    |
| :-------------------------------------: | :--------------------------: | :--------: |
|           V4L2_SET_BRIGHTNESS           |           设置亮度           | 0x00980900 |
|            V4L2_SET_CONTRAST            |          设置对比度          | 0x00980901 |
|           V4L2_SET_SATURATION           |          设置饱和度          | 0x00980902 |
|              V4L2_SET_HUE               |           设置色调           | 0x00980903 |
| V4L2_SET_WHITE_BALANCE_TEMPERATURE_MODE | 设置白平衡模式 0-手动 1-自动 | 0x0098090C |
|             V4L2_SET_GAMMA              |          设置gamma           | 0x00980910 |
|   V4L2_SET_WHITE_BALANCE_TEMPERATURE    |         设置白平衡值         | 0x0098091A |
|           V4L2_SET_SHARTNESS            |          设置清晰度          | 0x0098091B |
|         V4L2_SET_EXPOSURE_MODE          |  设置曝光模式 1-手动 3-自动  | 0x009A0901 |
|            V4L2_SET_EXPOSURE            |          设置曝光值          | 0x009A0902 |
|           V4L2_SET_FOCUS_MODE           |  设置对焦模式 0-手动 1-自动  | 0x009A090C |
|             V4L2_SET_FOCUS              |          设置对焦值          | 0x009A090A |

**Note:**
-  GetFrame()是阻塞读取视频帧的，需在线程中调用
-  GetFrame()获取到视频帧后就可以实现更多的功能,如拍照、录像等

# 参考项目
- https://github.com/jacksonliam/mjpg-streamer
- https://github.com/ZXX521/V4L2-Qt5.6.0-Ubuntu16.04

# 联系我们
邮箱:  
software@cvgc.cn  
官方网站:  
http://www.cvgc.cn/  
http://www.tipscope.com/  
https://www.tinyscope.com/  