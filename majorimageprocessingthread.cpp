#include "majorimageprocessingthread.h"
#include <QMutex>
#include "v4l2.h"
#include <QDebug>

extern QMutex mutexSetR;
long start_time;
int frame_count;

MajorImageProcessingThread::MajorImageProcessingThread()
{
    stopped = false;
    majorindex = -1;
}

void MajorImageProcessingThread::stop()
{
    stopped = true;
}

void MajorImageProcessingThread::init(int index)
{
    stopped = false;
    majorindex = index;
}

void MajorImageProcessingThread::run()
{
    if(majorindex != -1)
    {
        start_time = getTimeUsec();
        while(!stopped)
        {
            //msleep(5);

            QImage img;
            mutexSetR.lock();
            int ret = GetFrame();
            mutexSetR.unlock();
            if(ret == 0)
            {
                frame_count +=1;
                if(frame_count == 30)
                {
                    frame_count =0;
                    //qDebug("fps is: %.4f fps\n", 30.0 / ((getTimeUsec() - start_time) * 1.0 / 1000000));
                    start_time = getTimeUsec();
                }
                int WV = GetCurResWidth();
                int HV = GetCurResHeight();
                img = QImage(rgb24, WV, HV, QImage::Format_RGB888);
            }

            emit SendMajorImageProcessing(img, ret);
        }
    }
}

