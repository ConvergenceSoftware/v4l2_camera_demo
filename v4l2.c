#include "v4l2.h"
#include <jpeglib.h>
#include "sys/time.h"
#include "libv4l2.h"

#ifdef __cplusplus
extern "C" {
#endif

static struct buffer{
    void *start;
    unsigned int length;
}*buffers;
int buffers_length;

char runningDev[15] = "";
char devName[15] = "";
char camName[32] = "";
char devFmtDesc[4] = "";

int fd = -1;
int videoIsRun = -1;
int deviceIsOpen = -1;
unsigned char *rgb24 = NULL;
int WIDTH = 1280, HEIGHT = 720;

//V4l2相关结构体
static struct v4l2_capability cap;
static struct v4l2_fmtdesc fmtdesc;
static struct v4l2_frmsizeenum frmsizeenum;
static struct v4l2_format format;
static struct v4l2_requestbuffers reqbuf;
static struct v4l2_buffer buffer;

struct _control* in_parameters;
int parametercount;
struct v4l2_jpegcompression jpegcomp;

void StartVideoPrePare();
void StartVideoStream();
void EndVideoStream();
void EndVideoStreamClear();
int test_device_exist(char *devName);

long getTimeUsec()
{
    struct timeval t;
    gettimeofday(&t, 0);
    return (long)((long)t.tv_sec * 1000 * 1000 + t.tv_usec);
}

static int convert_yuv_to_rgb_pixel(int y, int u, int v)
{
    unsigned int pixel32 = 0;
    unsigned char *pixel = (unsigned char *)&pixel32;
    int r, g, b;
    r = y + (1.370705 * (v-128));
    g = y - (0.698001 * (v-128)) - (0.337633 * (u-128));
    b = y + (1.732446 * (u-128));
    if(r > 255) r = 255;
    if(g > 255) g = 255;
    if(b > 255) b = 255;
    if(r < 0) r = 0;
    if(g < 0) g = 0;
    if(b < 0) b = 0;
    pixel[0] = r ;
    pixel[1] = g ;
    pixel[2] = b ;
    return pixel32;
}

//convert mjpeg frame to RGB24
int MJPEG2RGB(unsigned char* data_frame, unsigned char *rgb, int bytesused)
{
    // variables:
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    unsigned int width, height;
    // data points to the mjpeg frame received from v4l2.
    unsigned char *data = data_frame;
    size_t data_size =  bytesused;

    // all the pixels after conversion to RGB.
    int pixel_size = 0;//size of one pixel
    if ( data == NULL  || data_size <= 0)
    {
        printf("Empty data!\n");
        return -1;
    }

//    UINT8 h1 = 0xFF;
//    UINT8 h2 = 0xD8;

//    if(*(data)!=h1 || *(data+1)==h2)
//    {
//        // error header
//        printf("wrong header\n");
//        return -2;
//    }
    // ... In the initialization of the program:
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, data, data_size);
     int rc = jpeg_read_header(&cinfo, TRUE);
     if(!(1==rc))
     {
         //printf("Not a jpg frame.\n");
         //return -2;
     }
    jpeg_start_decompress(&cinfo);
//    width = cinfo.output_width;
//	height = cinfo.output_height;
    pixel_size = cinfo.output_components;

    // ... Every frame:

    while (cinfo.output_scanline < cinfo.output_height)
    {
        unsigned char *temp_array[] ={ rgb + (cinfo.output_scanline) * WIDTH * pixel_size };
        jpeg_read_scanlines(&cinfo, temp_array, 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    return 0;
}


static int convert_yuv_to_rgb_buffer(unsigned char *yuv, unsigned char *rgb, unsigned int width, unsigned int height)
{
    unsigned int in, out = 0;
    unsigned int pixel_16;
    unsigned char pixel_24[3];
    unsigned int pixel32;
    int y0, u, y1, v;

    for(in = 0; in < width * height * 2; in += 4)
    {
        pixel_16 =
                yuv[in + 3] << 24 |
                               yuv[in + 2] << 16 |
                                              yuv[in + 1] <<  8 |
                                                              yuv[in + 0];
        y0 = (pixel_16 & 0x000000ff);
        u  = (pixel_16 & 0x0000ff00) >>  8;
        y1 = (pixel_16 & 0x00ff0000) >> 16;
        v  = (pixel_16 & 0xff000000) >> 24;
        pixel32 = convert_yuv_to_rgb_pixel(y0, u, v);
        pixel_24[0] = (pixel32 & 0x000000ff);
        pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
        pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;
        rgb[out++] = pixel_24[0];
        rgb[out++] = pixel_24[1];
        rgb[out++] = pixel_24[2];
        pixel32 = convert_yuv_to_rgb_pixel(y1, u, v);
        pixel_24[0] = (pixel32 & 0x000000ff);
        pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
        pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;
        rgb[out++] = pixel_24[0];
        rgb[out++] = pixel_24[1];
        rgb[out++] = pixel_24[2];
    }
    return 0;
}

void StartVideoPrePare()
{
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = WIDTH;
    format.fmt.pix.height = HEIGHT;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    ioctl(fd, VIDIOC_S_FMT, &format);

    //申请帧缓存区
    memset (&reqbuf, 0, sizeof (reqbuf));
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    reqbuf.count = 4;

    if (-1 == ioctl (fd, VIDIOC_REQBUFS, &reqbuf)) {
        if (errno == EINVAL)
            printf ("Video capturing or mmap-streaming is not supported\n");
        else
            perror ("VIDIOC_REQBUFS");
        return;
    }

    //分配缓存区
    buffers = calloc (reqbuf.count, sizeof (*buffers));
    if(buffers == NULL)
        perror("buffers is NULL");
    else
        assert (buffers != NULL);

    //mmap内存映射
    int i;
    for (i = 0; i < (int)reqbuf.count; i++) {
        memset (&buffer, 0, sizeof (buffer));
        buffer.type = reqbuf.type;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;

        if (-1 == ioctl (fd, VIDIOC_QUERYBUF, &buffer)) {
            perror ("VIDIOC_QUERYBUF");
            return;
        }

        buffers[i].length = buffer.length;

        buffers[i].start = mmap (NULL, buffer.length,
                                 PROT_READ | PROT_WRITE,
                                 MAP_SHARED,
                                 fd, buffer.m.offset);

        if (MAP_FAILED == buffers[i].start) {
            perror ("mmap");
            return;
        }

    }

    //将缓存帧放到队列中等待视频流到来
    unsigned int ii;
    for(ii = 0; ii < reqbuf.count; ii++){
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = ii;
        if (ioctl(fd,VIDIOC_QBUF,&buffer)==-1){
            perror("VIDIOC_QBUF failed");
        }
    }

    rgb24 = (unsigned char*)malloc(WIDTH*HEIGHT*3*sizeof(char));
    assert(rgb24 != NULL);
}

void StartVideoStream()
{
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd,VIDIOC_STREAMON,&type) == -1) {
        perror("VIDIOC_STREAMON failed");
    }
}

void EndVideoStream()
{
    //关闭视频流
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd,VIDIOC_STREAMOFF,&type) == -1) {
        perror("VIDIOC_STREAMOFF failed");
    }
}

void EndVideoStreamClear()
{
    //手动释放分配的内存
    int i;
    for (i = 0; i < (int)reqbuf.count; i++)
        munmap (buffers[i].start, buffers[i].length);
    //free(rgb24);
    rgb24 = NULL;
}

int test_device_exist(char *devName)
{
    struct stat st;
    if (-1 == stat(devName, &st))
        return -1;

    if (!S_ISCHR (st.st_mode))
        return -1;

    return 0;
}

int GetDeviceCount()
{
    char devname[15] = "";
    int count = 0;
    int i;
    for(i = 0; i < 100; i++)
    {
        sprintf(devname, "%s%d", "/dev/video", i);
        if(test_device_exist(devname) == 0)
            count++;

        memset(devname, 0, sizeof(devname));
    }

    return count;
}

//根据索引获取设备名称
char *GetDeviceName(int index)
{
    memset(devName, 0, sizeof(devName));

    int count = 0;
    char devname[15] = "";
    int i;
    for(i = 0; i < 100; i++)
    {
        sprintf(devname, "%s%d", "/dev/video", i);
        if(test_device_exist(devname) == 0)
        {
            if(count == index)
                break;
            count++;
        }
        else
            memset(devname, 0, sizeof(devname));
    }

    strcpy(devName, devname);

    return devName;
}

//根据索引获取摄像头名称
char *GetCameraName(int index)
{
    if(videoIsRun > 0)
        return "";

    memset(camName, 0, sizeof(camName));

    char devname[15] = "";
    strcpy(devname, GetDeviceName(index));

    int fd = open(devname, O_RDWR);
    if(ioctl(fd, VIDIOC_QUERYCAP, &cap) != -1)
    {
        strcpy(camName, (char *)cap.card);
    }
    close(fd);

    return camName;
}

//运行指定索引的视频
int StartRun(int index)
{
    if(videoIsRun > 0)
        return -1;

    char *devname = GetDeviceName(index);
    fd = open(devname, O_RDWR);
    if(fd == -1)
        return -1;

    deviceIsOpen = 1;

    StartVideoPrePare();
    StartVideoStream();

    strcpy(runningDev, devname);
    videoIsRun = 1;

    return 0;
}

int GetFrame()
{
    if(videoIsRun > 0)
    {
        fd_set fds;
        struct timeval tv;
        int r;

        FD_ZERO (&fds);
        FD_SET (fd, &fds);


        tv.tv_sec = 7;
        tv.tv_usec = 0;

        r = select (fd + 1, &fds, NULL, NULL, &tv);

        if (0 == r)
            return -1;
        else if(-1 == r)
            return errno;

        memset(&buffer, 0, sizeof(buffer));
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;

        if (ioctl(fd, VIDIOC_DQBUF, &buffer) == -1) {
            perror("GetFrame VIDIOC_DQBUF Failed");
            return errno;
        }

        else
        {
            //convert_yuv_to_rgb_buffer((unsigned char*)buffers[buffer.index].start, rgb24, WIDTH, HEIGHT);
            MJPEG2RGB((unsigned char*)buffers[buffer.index].start, rgb24, buffer.bytesused);
            if (ioctl(fd, VIDIOC_QBUF, &buffer) < 0) {
                perror("GetFrame VIDIOC_QBUF Failed");
                return errno;
            }

            return 0;
        }
    }

    return 0;
}

int StopRun()
{
    printf("stop run\n");
    if(videoIsRun > 0)
    {
        EndVideoStream();
        EndVideoStreamClear();
    }
    memset(runningDev, 0, sizeof(runningDev));
    videoIsRun = -1;
    deviceIsOpen = -1;

    if(close(fd) != 0)
        return -1;

    return 0;
}

//切换分辨率
int V4L2SetResolution(int new_width, int new_height)
{
    StopRun();
    WIDTH = new_width;
    HEIGHT = new_height;

    StartRun(2);

    return 0;
}

char *GetDevFmtDesc(int index)
{
    memset(devFmtDesc, 0, sizeof(devFmtDesc));

    fmtdesc.index=index;
    fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if(ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) != -1)
    {
        char fmt[5] = "";
        sprintf(fmt, "%c%c%c%c",
                (__u8)(fmtdesc.pixelformat&0XFF),
                (__u8)((fmtdesc.pixelformat>>8)&0XFF),
                (__u8)((fmtdesc.pixelformat>>16)&0XFF),
                (__u8)((fmtdesc.pixelformat>>24)&0XFF));

        strncpy(devFmtDesc, fmt, 4);
    }

    return devFmtDesc;
}

//获取图像的格式属性相关
int GetDevFmtWidth()
{
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl (fd, VIDIOC_G_FMT, &format) == -1)
    {
        perror("GetDevFmtWidth:");
        return -1;
    }
    return format.fmt.pix.width;
}

int GetDevFmtHeight()
{
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl (fd, VIDIOC_G_FMT, &format) == -1)
    {
        perror("GetDevFmtHeight:");
        return -1;
    }
    return format.fmt.pix.height;
}

int GetDevFmtSize()
{
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl (fd, VIDIOC_G_FMT, &format) == -1)
    {
        perror("GetDevFmtSize:");
        return -1;
    }
    return format.fmt.pix.sizeimage;
}

int GetDevFmtBytesLine()
{
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl (fd, VIDIOC_G_FMT, &format) == -1)
    {
        perror("GetDevFmtBytesLine:");
        return -1;
    }
    return format.fmt.pix.bytesperline;
}

//设备分辨率相关
int GetResolutinCount()
{
    fmtdesc.index = 0;
    fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if(ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == -1)
        return -1;

    frmsizeenum.pixel_format = fmtdesc.pixelformat;
    int i = 0;
    for(i = 0; ; i++)
    {
        frmsizeenum.index = i;
        if(ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsizeenum) == -1)
            break;
    }
    return i;
}

int GetResolutionWidth(int index)
{
    fmtdesc.index = 0;
    fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if(ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == -1)
        return -1;

    frmsizeenum.pixel_format = fmtdesc.pixelformat;

    frmsizeenum.index = index;
    if(ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsizeenum) != -1)
        return frmsizeenum.discrete.width;
    else
        return -1;
}

int GetResolutionHeight(int index)
{
    fmtdesc.index = 0;
    fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if(ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == -1)
        return -1;

    frmsizeenum.pixel_format = fmtdesc.pixelformat;

    frmsizeenum.index = index;
    if(ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsizeenum) != -1)
        return frmsizeenum.discrete.height;
    else
        return -1;
}

int GetCurResWidth()
{
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_G_FMT, &format) == -1)
        return -1;
    return format.fmt.pix.width;
}

int GetCurResHeight()
{
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_G_FMT, &format) == -1)
        return -1;
    return format.fmt.pix.height;
}

void control_readed(struct v4l2_queryctrl* ctrl)
{
    struct v4l2_control c;
    memset(&c, 0, sizeof(struct v4l2_control));
    c.id = ctrl->id;

    if (in_parameters == NULL) {
        in_parameters = (control*)calloc(1, sizeof(control));
    }
    else {
        in_parameters =
            (control*)realloc(in_parameters, (parametercount + 1) * sizeof(control));
    }

    if (in_parameters == NULL) {
        printf("Calloc failed\n");
        return;
    }

    memcpy(&in_parameters[parametercount].ctrl, ctrl, sizeof(struct v4l2_queryctrl));
    in_parameters[parametercount].group = IN_CMD_V4L2;
    in_parameters[parametercount].value = c.value;
    if (ctrl->type == V4L2_CTRL_TYPE_MENU) {
        in_parameters[parametercount].menuitems =
            (struct v4l2_querymenu*)malloc((ctrl->maximum + 1) * sizeof(struct v4l2_querymenu));
        int i;
        for (i = ctrl->minimum; i <= ctrl->maximum; i++) {
            struct v4l2_querymenu qm;
            memset(&qm, 0, sizeof(struct v4l2_querymenu));
            qm.id = ctrl->id;
            qm.index = i;
            if (ioctl(fd, VIDIOC_QUERYMENU, &qm) == 0) {
                memcpy(&in_parameters[parametercount].menuitems[i], &qm, sizeof(struct v4l2_querymenu));
                printf("Menu item %d: %s\n", qm.index, qm.name);
            }
            else {
                printf("Unable to get menu item for %s, index=%d\n", ctrl->name, qm.index);
            }
        }
    }
    else {
        in_parameters[parametercount].menuitems = NULL;
    }

    in_parameters[parametercount].value = 0;
    in_parameters[parametercount].class_id = (ctrl->id & 0xFFFF0000);
    #ifndef V4L2_CTRL_FLAG_NEXT_CTRL
    in_parameters[parametercount].class_id = V4L2_CTRL_CLASS_USER;
    #endif

    int ret = -1;
    if (in_parameters[parametercount].class_id == V4L2_CTRL_CLASS_USER) {
        printf("V4L2 parameter found: %s value %d Class: USER \n", ctrl->name, c.value);
        ret = ioctl(fd, VIDIOC_G_CTRL, &c);
        if (ret == 0) {
            in_parameters[parametercount].value = c.value;
        }
        else {
            printf("Unable to get the value of %s retcode: %d  %s\n", ctrl->name, ret, strerror(errno));
        }
    }
    else {
        printf("V4L2 parameter found: %s value %d Class: EXTENDED \n", ctrl->name, c.value);
        struct v4l2_ext_controls ext_ctrls = { 0 };
        struct v4l2_ext_control ext_ctrl = { 0 };
        ext_ctrl.id = ctrl->id;
        #ifdef V4L2_CTRL_TYPE_STRING
        ext_ctrl.size = 0;
        if (ctrl.type == V4L2_CTRL_TYPE_STRING) {
            ext_ctrl.size = ctrl->maximum + 1;
            // FIXMEEEEext_ctrl.string = control->string;
        }
        #endif
        ext_ctrls.count = 1;
        ext_ctrls.controls = &ext_ctrl;
        ret = ioctl(fd, VIDIOC_G_EXT_CTRLS, &ext_ctrls);
        if (ret) {
            switch (ext_ctrl.id) {
            case V4L2_CID_PAN_RESET:
                in_parameters[parametercount].value = 1;
                printf("Setting PAN reset value to 1\n");
                break;
            case V4L2_CID_TILT_RESET:
                in_parameters[parametercount].value = 1;
                printf("Setting the Tilt reset value to 2\n");
                break;
            default:
                printf("control id: 0x%08x failed to get value (error %i)\n", ext_ctrl.id, ret);
            }
        }
        else {
            switch (ctrl->type)
            {
            #ifdef V4L2_CTRL_TYPE_STRING
            case V4L2_CTRL_TYPE_STRING:
                //string gets set on VIDIOC_G_EXT_CTRLS
                //add the maximum size to value
                in_parameters[parametercount].value = ext_ctrl.size;
                break;
            #endif
            case V4L2_CTRL_TYPE_INTEGER64:
                in_parameters[parametercount].value = ext_ctrl.value64;
                break;
            default:
                in_parameters[parametercount].value = ext_ctrl.value;
                break;
            }
        }
    }

    parametercount++;
}

void enumerateControls()
{
    // enumerating v4l2 controls
    struct v4l2_queryctrl ctrl;
    memset(&ctrl, 0, sizeof(struct v4l2_queryctrl));
    parametercount = 0;
    in_parameters = malloc(0 * sizeof(control));
    /* Enumerate the v4l2 controls
     Try the extended control API first */
    #ifdef V4L2_CTRL_FLAG_NEXT_CTRL
    printf("V4L2 API's V4L2_CTRL_FLAG_NEXT_CTRL is supported\n");
    ctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
    if (0 == ioctl(fd, VIDIOC_QUERYCTRL, &ctrl)) {
        do {
            control_readed(&ctrl);
            ctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
        } while (0 == ioctl(fd, VIDIOC_QUERYCTRL, &ctrl));
    }
    else
    #endif
    {
        printf("V4L2 API's V4L2_CTRL_FLAG_NEXT_CTRL is NOT supported\n");
        /* Fall back on the standard API */
        /* Check all the standard controls */
        int i;
        for (i = V4L2_CID_BASE; i < V4L2_CID_LASTP1; i++) {
            ctrl.id = i;
            if (ioctl(fd, VIDIOC_QUERYCTRL, &ctrl) == 0) {
                control_readed(&ctrl);
            }
        }

        /* Check any custom controls */
        for (i = V4L2_CID_PRIVATE_BASE; ; i++) {
            ctrl.id = i;
            if (ioctl(fd, VIDIOC_QUERYCTRL, &ctrl) == 0) {
                control_readed(&ctrl);
            }
            else {
                break;
            }
        }
    }

    memset(&jpegcomp, 0, sizeof(struct v4l2_jpegcompression));
    if (ioctl(fd, VIDIOC_G_JPEGCOMP, &jpegcomp) != EINVAL) {

        struct v4l2_queryctrl ctrl_jpeg;
        ctrl_jpeg.id = 1;
        sprintf((char*)&ctrl_jpeg.name, "JPEG quality");
        ctrl_jpeg.minimum = 0;
        ctrl_jpeg.maximum = 100;
        ctrl_jpeg.step = 1;
        ctrl_jpeg.default_value = 50;
        ctrl_jpeg.flags = 0;
        ctrl_jpeg.type = V4L2_CTRL_TYPE_INTEGER;
        if (in_parameters == NULL) {
            in_parameters = (control*)calloc(1, sizeof(control));
        }
        else {
            in_parameters = (control*)realloc(in_parameters, (parametercount + 1) * sizeof(control));
        }

        if (in_parameters == NULL) {
            printf("Calloc/realloc failed\n");
            return;
        }

        memcpy(&in_parameters[parametercount].ctrl, &ctrl_jpeg, sizeof(struct v4l2_queryctrl));
        in_parameters[parametercount].group = IN_CMD_JPEG_QUALITY;
        in_parameters[parametercount].value = jpegcomp.quality;
        parametercount++;
    }
    else {
        printf("Modifying the setting of the JPEG compression is not supported\n");
        jpegcomp.quality = -1;
    }
}

int isv4l2Control(int control, struct v4l2_queryctrl* queryctrl)
{
    int err = 0;

    queryctrl->id = control;
    if ((err = ioctl(fd, VIDIOC_QUERYCTRL, queryctrl)) < 0) {
        //fprintf(stderr, "ioctl querycontrol error %d \n",errno);
        return -1;
    }

    if (queryctrl->flags & V4L2_CTRL_FLAG_DISABLED) {
        //fprintf(stderr, "control %s disabled \n", (char *) queryctrl->name);
        return -1;
    }

    if (queryctrl->type & V4L2_CTRL_TYPE_BOOLEAN) {
        return 1;
    }

    if (queryctrl->type & V4L2_CTRL_TYPE_INTEGER) {
        return 0;
    }

    fprintf(stderr, "contol %s unsupported  \n", (char*)queryctrl->name);
    return -1;
}

int v4l2ResetControl(int control)
{
    struct v4l2_control control_s;
    struct v4l2_queryctrl queryctrl;
    int val_def;
    int err;

    if (isv4l2Control(control, &queryctrl) < 0)
        return -1;

    val_def = queryctrl.default_value;
    control_s.id = control;
    control_s.value = val_def;

    if ((err = ioctl(fd, VIDIOC_S_CTRL, &control_s)) < 0) {
        return -1;
    }

    return 0;
}

int v4l2GetControl(int control)
{
    struct v4l2_queryctrl queryctrl;
    struct v4l2_control control_s;
    int err;

    if ((err = isv4l2Control(control, &queryctrl)) < 0) {
        return -1;
    }

    control_s.id = control;
    if ((err = ioctl(fd, VIDIOC_G_CTRL, &control_s)) < 0) {
        return -1;
    }

    return control_s.value;
}

int v4l2SetControl(int control_id, int value)
{
    struct v4l2_control control_s;
    int min, max;
    int ret = 0;
    int err;
    int i;
    int got = -1;
    printf("Looking for the 0x%08x V4L2 control\n", control_id);
    for (i = 0; i < parametercount; i++) {
        if (in_parameters[i].ctrl.id == control_id) {
            got = 0;
            break;
        }
    }

    if (got == 0) { // we have found the control with the specified id
        printf("V4L2 ctrl 0x%08x found\n", control_id);
        if (in_parameters[i].class_id == V4L2_CTRL_CLASS_USER) {
            printf("Control type: USER\n");
            min = in_parameters[i].ctrl.minimum;
            max = in_parameters[i].ctrl.maximum;

            if ((value >= min) && (value <= max)) {
                control_s.id = control_id;
                control_s.value = value;
                if ((err = ioctl(fd, VIDIOC_S_CTRL, &control_s)) < 0) {
                    printf("VIDIOC_S_CTRL failed\n");
                    return -1;
                }
                else {
                    printf("V4L2 ctrl 0x%08x new value: %d\n", control_id, value);
                    in_parameters[i].value = value;
                }
            }
            else {
                printf("Value (%d) out of range (%d .. %d)\n", value, min, max);
            }
            return 0;
        }
        else { // not user class controls
            printf("Control type: EXTENDED\n");
            struct v4l2_ext_controls ext_ctrls = { 0 };
            struct v4l2_ext_control ext_ctrl = { 0 };
            ext_ctrl.id = in_parameters[i].ctrl.id;

            switch (in_parameters[i].ctrl.type) {
            #ifdef V4L2_CTRL_TYPE_STRING
            case V4L2_CTRL_TYPE_STRING:
                //string gets set on VIDIOC_G_EXT_CTRLS
                //add the maximum size to value
                ext_ctrl.size = value;
                printf("STRING extended controls are currently broken\n");
                //ext_ctrl.string = control->string; // FIXMEE
                break;
            #endif
            case V4L2_CTRL_TYPE_INTEGER64:
                ext_ctrl.value64 = value;
                break;
            default:
                ext_ctrl.value = value;
                break;
            }

            ext_ctrls.count = 1;
            ext_ctrls.controls = &ext_ctrl;
            ret = ioctl(fd, VIDIOC_S_EXT_CTRLS, &ext_ctrls);
            if (ret) {
                printf("control id: 0x%08x failed to set value (error %i)\n", ext_ctrl.id, ret);
                return -1;
            }
            else {
                printf("control id: 0x%08x new value: %d\n", ext_ctrl.id, ext_ctrl.value);
            }
            return 0;
        }
    }
    else {
        printf("Invalid V4L2_set_control request for the id: 0x%08x. Control cannot be found in the list\n", control_id);
        return -1;
    }
}

#ifdef __cplusplus
}
#endif
