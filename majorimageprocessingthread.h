#ifndef MajorImageProcessingThread_H
#define MajorImageProcessingThread_H

#include <QThread>
#include <QImage>
#include <QDebug>

#include "v4l2.h"

class MajorImageProcessingThread : public QThread
{
    Q_OBJECT
public:
    MajorImageProcessingThread();

    QImage majorImage;
    void stop();
    void init(int index);

protected:
    void run();

private:
    volatile int majorindex;
    volatile bool stopped;

signals:
    void SendMajorImageProcessing(QImage image, int result);
};

#endif // MajorImageProcessingThread_H
