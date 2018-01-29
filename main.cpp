#include <iostream>
#include "multtracker.h"

#include "facedetect-dll.h"

using namespace std;
using namespace cv;
#define DETECT_BUFFER_SIZE 0x20000
int main()
{
    cv::VideoCapture *capture =new cv::VideoCapture(0);//��ȡ��Ƶ����
    cv::namedWindow("video");
    cv::Mat frame;
    cv::Mat trackImg;
    MultTracker tracker;

    if(capture->isOpened()){
        *capture >> frame;
        if(!frame.data) return -1;
    }

    if (capture->isOpened())
    {
        ofstream fout;
        fout.open("E://test.txt");
        int doLandmark = 0;
        try{
            while(1)
            {
                int * pResults = NULL;
                std::vector<cv::Rect> rectvec;
                rectvec.clear();
                *capture >> frame; //ץȡһ֡
                trackImg = frame.clone();//����ͼƬ

                unsigned char * pBuffer = (unsigned char *)malloc(DETECT_BUFFER_SIZE);
                if(!pBuffer)
                {
                    fprintf(stderr, "Can not alloc buffer.\n");
                    return -1;
                }

                cv::Mat gray;
                cvtColor(frame, gray, CV_BGR2GRAY);

                pResults = facedetect_multiview_reinforce(pBuffer, (unsigned char*)(gray.ptr(0)), gray.cols, gray.rows, (int)gray.step,
                                                            1.2f, 3, 48, 0, doLandmark);

                for(int i = 0; i < (pResults ? *pResults : 0); i++)
                {
                    short * p = ((short*)(pResults+1))+142*i;
                    int x = p[0];
                    int y = p[1];
                    int w = p[2];
                    int h = p[3];
                    cv::Rect rect;
                    rect.x = x;
                    rect.y = y;
                    rect.width = w;
                    rect.height = h;
                    if(rect.width < 0)
                        rect.width = 0;
                    if(rect.height < 0)
                        rect.height = 0;
                    if(rect.area()== 0)
                        continue;
                    rectvec.push_back(rect);
                }
                free(pBuffer);
                if(true)
                {
                    /* ����һ֡ */
                    tracker.tracking(trackImg,rectvec);
                    for(int i = 0;i < tracker.trackedFaces_.size();i++){
                        if (!tracker.trackedFaces_[i].is_disappear)  /* �ж��Ƿ�Ŀ�궪ʧ */
                        {
                            cv::rectangle(trackImg,cv::Rect(tracker.faceLocations_[i].xc-tracker.faceLocations_[i].Wx_h
                                    ,tracker.faceLocations_[i].yc-tracker.faceLocations_[i].Hy_h,
                                    tracker.faceLocations_[i].Wx_h*2,tracker.faceLocations_[i].Hy_h*2),cv::Scalar(255,0,0),2,8,0);
                        }
                    }
                    tracker.removeTwiceTrackingTarget();
                    tracker.ClearDisapperedTarget();

                }
                cv::imshow("video",trackImg);
                cv::waitKey(50);
                trackImg.release();
            }
        }catch(...){

        }


        capture->release();
        fout.close();
    }
    //�ͷ�ͼ��
    cv::destroyAllWindows();
    tracker.ClearAll();
    return 0;
}

