#include "widget.h"
#include "ui_widget.h"
#include <QMutex>
#include <QDebug>
#include "v4l2.h"

QMutex mutexSetR;

extern struct _control* in_parameters;
extern int parametercount;

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);
    InitWidget();
}

Widget::~Widget()
{
    delete ui;
}

void Widget::InitWidget()
{
    err11 = err19 = 0;
    imageprocessthread = new MajorImageProcessingThread;

    setWindowTitle("camera demo");
    setFixedSize(1000,600);

    int camcount = GetDeviceCount();

    if(camcount > 0)
    {
        //获取所有设备
        for(int i = 0;i < camcount;i++)
            ui->cb_camera_name->addItem(QString::fromLatin1(GetCameraName(i)));

        //启动默认视频
        StartRun(2);
        imageprocessthread->init(ui->cb_camera_name->currentIndex());
        imageprocessthread->start();



        //获取所有分辨率
        ui->cb_resolution->clear();
        int rescount = GetResolutinCount();
        int curW = GetCurResWidth();
        int curH = GetCurResHeight();
        for(int i = 0; i < rescount; i++)
        {
            QString rW = QString::number(GetResolutionWidth(i));
            QString rH = QString::number(GetResolutionHeight(i));
            QString res = rW + "X" + rH;
            ui->cb_resolution->addItem(res);
            bool ok;
            if(curW == rW.toInt(&ok, 10) && curH == rH.toInt(&ok, 10))
                ui->cb_resolution->setCurrentIndex(i);
        }

        enumerateControls();

        for(int i =0;i < parametercount; i++)
        {
            if(in_parameters[i].ctrl.id == V4L2_SET_FOCUS_MODE)
            {
                int focusMode = v4l2GetControl(V4L2_SET_FOCUS_MODE);
                Qt::CheckState focusState = focusMode == 0 ? Qt::Unchecked : Qt::Checked;
                ui->checkBox_focus->setCheckState(focusState);
                ui->hs_focus->setDisabled(focusMode);

            }

            if(in_parameters[i].ctrl.id == V4L2_SET_FOCUS)
            {
                ui->hs_focus->setMinimum(in_parameters[i].ctrl.minimum);
                ui->hs_focus->setMaximum(in_parameters[i].ctrl.maximum);
                int focusValue = v4l2GetControl(V4L2_SET_FOCUS);
                ui->tE_focus->setText(QString::number(focusValue));
            }

            if(in_parameters[i].ctrl.id == V4L2_SET_EXPOSURE_MODE)
            {
                int exposureMode = v4l2GetControl(V4L2_SET_EXPOSURE_MODE);
                qDebug("exposure mode = %d\n",exposureMode);
                Qt::CheckState ExposureState = exposureMode == 1 ? Qt::Unchecked : Qt::Checked;
                ui->checkBox_exposure->setCheckState(ExposureState);
                if(exposureMode == 1)
                {
                   ui->hs_exposure->setDisabled(false);
                }
                else
                {
                    ui->hs_exposure->setDisabled(true);
                }

            }

            if(in_parameters[i].ctrl.id == V4L2_SET_EXPOSURE)
            {
                ui->hs_exposure->setMinimum(in_parameters[i].ctrl.minimum);
                ui->hs_exposure->setMaximum(in_parameters[i].ctrl.maximum);
                int exposureValue = v4l2GetControl(V4L2_SET_EXPOSURE);
                ui->tE_exposure->setText(QString::number(exposureValue));
            }

            //saturation
            if(in_parameters[i].ctrl.id == V4L2_SET_SATURATION)
            {
                ui->hs_saturation->setMinimum(in_parameters[i].ctrl.minimum);
                ui->hs_saturation->setMaximum(in_parameters[i].ctrl.maximum);
                int saturationValue = v4l2GetControl(V4L2_SET_SATURATION);
                ui->tE_saturation->setText(QString::number(saturationValue));
            }

            //contrast
            if(in_parameters[i].ctrl.id == V4L2_SET_CONTRAST)
            {
                ui->hs_contrast->setMinimum(in_parameters[i].ctrl.minimum);
                ui->hs_contrast->setMaximum(in_parameters[i].ctrl.maximum);
                int contrastValue = v4l2GetControl(V4L2_SET_CONTRAST);
                ui->tE_contrast->setText(QString::number(contrastValue));
            }

            //brightness
            if(in_parameters[i].ctrl.id == V4L2_SET_BRIGHTNESS)
            {
                ui->hs_brightness->setMinimum(in_parameters[i].ctrl.minimum);
                ui->hs_brightness->setMaximum(in_parameters[i].ctrl.maximum);
                int brightnessValue = v4l2GetControl(V4L2_SET_BRIGHTNESS);
                ui->tE_brightness->setText(QString::number(brightnessValue));
            }

            //hue
            if(in_parameters[i].ctrl.id == V4L2_SET_HUE)
            {
                ui->hs_hue->setMinimum(in_parameters[i].ctrl.minimum);
                ui->hs_hue->setMaximum(in_parameters[i].ctrl.maximum);
                int hueValue = v4l2GetControl(V4L2_SET_HUE);
                ui->tE_hue->setText(QString::number(hueValue));
            }
        }
    }

    connect(imageprocessthread, SIGNAL(SendMajorImageProcessing(QImage, int)),
            this, SLOT(ReceiveMajorImage(QImage, int)));

    connect(ui->cb_resolution, SIGNAL(currentIndexChanged(QString)), this, SLOT(SetResolution()));

    connect(ui->checkBox_focus, SIGNAL(clicked()), this, SLOT(SetFocusMode()));
    connect(ui->hs_focus, SIGNAL(valueChanged(int)), this, SLOT(SetFocusValue()));

    connect(ui->checkBox_exposure, SIGNAL(clicked()), this, SLOT(SetExposureMode()));
    connect(ui->hs_exposure, SIGNAL(valueChanged(int)), this, SLOT(SetExposureValue()));

    connect(ui->hs_saturation, SIGNAL(valueChanged(int)), this, SLOT(SetSaturation()));
    connect(ui->hs_contrast, SIGNAL(valueChanged(int)), this, SLOT(SetContrast()));
    connect(ui->hs_brightness, SIGNAL(valueChanged(int)), this, SLOT(SetBrightness()));
    connect(ui->hs_hue, SIGNAL(valueChanged(int)), this, SLOT(SetHue()));

}

void Widget::ReceiveMajorImage(QImage image, int result)
{
    //超时后关闭视频
    //超时代表着VIDIOC_DQBUF会阻塞，直接关闭视频即可
    if(result == -1)
    {
        imageprocessthread->stop();
        imageprocessthread->wait();
        StopRun();

        ui->image_label->clear();
        ui->image_label->setText("获取设备图像超时！");
    }

    if(!image.isNull())
    {
        ui->image_label->clear();
        switch(result)
        {
        case 0:     //Success
            err11 = err19 = 0;
            if(image.isNull())
                ui->image_label->setText("画面丢失！");
            else
                ui->image_label->setPixmap(QPixmap::fromImage(image.scaled(ui->image_label->size())));

            break;
        case 11:    //Resource temporarily unavailable
            err11++;
            if(err11 == 10)
            {
                ui->image_label->clear();
                ui->image_label->setText("设备已打开，但获取视频失败！\n请尝试切换USB端口后断开重试！");
            }
            break;
        case 19:    //No such device
            err19++;
            if(err19 == 10)
            {
                ui->image_label->clear();
                ui->image_label->setText("设备丢失！");
            }
            break;
        }
    }
}

int Widget::SetResolution()
{
    int width, height;
    QString res = ui->cb_resolution->currentText();
    QStringList list = res.split("X");
    width = list[0].toInt();
    height = list[1].toInt();
    printf("set resolution width = %d, height = %d\n",width, height);

    mutexSetR.lock();
    V4L2SetResolution(width, height);
    mutexSetR.unlock();
    return 0;
}

int Widget::SetFocusMode()
{
    int focusMode = ui->checkBox_focus->isChecked() ? 1 : 0;
    v4l2SetControl(V4L2_SET_FOCUS_MODE, focusMode);
    ui->hs_focus->setDisabled(focusMode);

    return 0;
}

int Widget::SetFocusValue()
{
    int focusMode = ui->checkBox_focus->isChecked() ? 1 : 0;
    if(focusMode == 0)
    {
        int value = ui->hs_focus->value();
        ui->tE_focus->setText(QString::number(value));
        v4l2SetControl(V4L2_SET_FOCUS, value);
    }

    return 0;
}

int Widget::SetExposureMode()
{
    int exposureMode = ui->checkBox_exposure->isChecked() ? 3 : 1;
    v4l2SetControl(V4L2_SET_EXPOSURE_MODE, exposureMode);
    if(exposureMode == 1)
    {
       ui->hs_exposure->setDisabled(false);
    }
    else
    {
        ui->hs_exposure->setDisabled(true);
    }

    return 0;
}

int Widget::SetExposureValue()
{
    int ret = 0;
    int exposureMode = ui->checkBox_exposure->isChecked() ? 3 : 1;
    if(exposureMode == 1)
    {
        int value = ui->hs_exposure->value();
        ui->tE_exposure->setText(QString::number(value));
        ret = v4l2SetControl(V4L2_SET_EXPOSURE, value);
    }

    return ret;
}

int Widget::SetSaturation()
{
    int ret = 0;
    int value = ui->hs_saturation->value();
    ui->tE_saturation->setText(QString::number(value));
    ret = v4l2SetControl(V4L2_SET_SATURATION, value);
    return ret;
}

int Widget::SetContrast()
{
    int ret;
    int value = ui->hs_contrast->value();
    ui->tE_contrast->setText(QString::number(value));
    ret = v4l2SetControl(V4L2_SET_CONTRAST, value);
    return ret = 0;
}

int Widget::SetBrightness()
{
    int ret = 0;
    int value = ui->hs_brightness->value();
    ui->tE_brightness->setText(QString::number(value));
    ret = v4l2SetControl(V4L2_SET_BRIGHTNESS, value);
    return ret;
}

int Widget::SetHue()
{
    int ret = 0;
    int value = ui->hs_hue->value();
    ui->tE_hue->setText(QString::number(value));
    ret = v4l2SetControl(V4L2_SET_HUE, value);
    return ret;
}
