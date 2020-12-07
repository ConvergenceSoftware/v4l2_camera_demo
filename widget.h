#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>

#include "majorimageprocessingthread.h"

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

private slots:
    void ReceiveMajorImage(QImage image, int result);
    int SetResolution();
    int SetFocusMode();
    int SetFocusValue();
    int SetExposureMode();
    int SetExposureValue();
    int SetSaturation();
    int SetContrast();
    int SetBrightness();
    int SetHue();

private:
    Ui::Widget *ui;

    int err11, err19;

    MajorImageProcessingThread *imageprocessthread;

    void InitWidget();


};
#endif // WIDGET_H
