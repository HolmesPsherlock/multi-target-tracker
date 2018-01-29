#ifndef MULTTRACKER_H
#define MULTTRACKER_H
#include <opencv2/opencv.hpp>
#include <time.h>
#include <iostream>
#include<fstream>
#include <array>

#define ALPHA_COEFFICIENT      0.2     /* Ŀ��ģ�͸���Ȩ��*/
//cv1
#define B(image,x,y) ((uchar*)(image->imageData + image->widthStep*(y)))[(x)*3]		//B
#define G(image,x,y) ((uchar*)(image->imageData + image->widthStep*(y)))[(x)*3+1]	//G
#define R(image,x,y) ((uchar*)(image->imageData + image->widthStep*(y)))[(x)*3+2]	//R
//cv2
#define MB(image,i,j) image.at<cv::Vec3b>(j,i)[0]
#define MG(image,i,j) image.at<cv::Vec3b>(j,i)[1]
#define MR(image,i,j) image.at<cv::Vec3b>(j,i)[2]
# define R_BIN      8  /* ��ɫ������ֱ��ͼ���� */
# define G_BIN      8  /* ��ɫ������ֱ��ͼ���� */
# define B_BIN      8  /* ��ɫ������ֱ��ͼ���� */

# define R_SHIFT    5  /* ������ֱ��ͼ������Ӧ */
# define G_SHIFT    5  /* ��R��G��B��������λ�� */
# define B_SHIFT    5  /* log2( 256/8 )Ϊ�ƶ�λ�� */

#define MAX_WEIGHT 0.001
static float Pi_Thres = (float)0.90; /* Ȩ����ֵ   */
typedef struct __SpaceState {  /* ״̬�ռ���� */
    int xt;               /* x����λ�� */
    int yt;               /* y����λ�� */
    float v_xt;           /* x�����˶��ٶ� */
    float v_yt;           /* y�����˶��ٶ� */
    int Hxt;              /* x����봰�� */
    int Hyt;              /* y����봰�� */
    float at_dot;         /* �߶ȱ任�ٶȣ��������������һƬ����ĳ߶ȱ仯�ٶ� */
} SPACESTATE;

typedef struct __UpdataLocation{
    int xc;
    int yc;
    int Wx_h;
    int Hy_h;
    float max_weight;
}UpdataLocation;

typedef struct __SrcFace{
    cv::Mat src;
    cv::Mat face;
    cv::Rect rect;//����λ��
    bool is_disappear;//���ٶ�ʧ��־
}SrcFace;
//int xin,yin;//����ʱ��������ĵ�
//int xout,yout;//����ʱ�õ���������ĵ�
//int Wid,Hei;//ͼ��Ĵ�С
//int WidIn,HeiIn;//����İ������
//int WidOut,HeiOut;//����İ������

static float DELTA_T = (float)0.05;    /* ֡Ƶ������Ϊ30��25��15��10�� */
static float VELOCITY_DISTURB = 40.0;  /* �ٶ��Ŷ���ֵ   */
static float SCALE_DISTURB = 0.0;      /* ������Ŷ����� */
static float SCALE_CHANGE_D = (float)0.001;   /* �߶ȱ任�ٶ��Ŷ����� */

static const int NParticle = 100;
# define SIGMA2       0.02
class MultTracker
{
public:
    MultTracker();
    void ClearAll()
    {
        if(this->faceModelHist_.size()){
            for(int i = 0;i < this->faceModelHist_.size();i++){
                this->faceModelHist_[i].clear();
            }
            this->faceModelHist_.clear();
        }
        if(this->faceSpaceStates_.size()){
            for(int i = 0;i < this->faceSpaceStates_.size();i++){
                this->faceSpaceStates_[i].clear();
            }
            this->faceSpaceStates_.clear();
        }
        if(this->faceWeights_.size()){
            for(int i = 0;i < this->faceWeights_.size();i++){
                this->faceWeights_[i].clear();
            }
            this->faceWeights_.clear();
        }
        if(this->img_ != NULL) delete [] img_;
        return;
    }
    float computeOverLap(cv::Rect& rect1, cv::Rect& rect2);
    void CalcuColorHistogram( int x0, int y0, int Wx, int Hy,
                             unsigned char * image, int W, int H,
                             float * ColorHist, int bins );
    /*
    ����Bhattacharyyaϵ��
    ���������
    float * p, * q��      ������ɫֱ��ͼ�ܶȹ���
    int bins��            ֱ��ͼ����
    ����ֵ��
    Bhattacharyyaϵ��
    */
    float CalcuBhattacharyya( float * p, float * q, int bins )
    {
        int i;
        float rho;

        rho = 0.0;
        for ( i = 0; i < bins; i++ )
            rho = (float)(rho + sqrt( p[i]*q[i] ));
        return( rho );
    }

    float CalcuWeightedPi( float rho )
    {
        float pi_n, d2;
        d2 = 1 - rho;
        pi_n = (float)(exp( - d2/SIGMA2 ));
        return( pi_n );
    }
    /*
    ���һ��[0,1]֮��������
    */
    inline float rand0_1()
    {
        return(rand()/float(RAND_MAX));
    }
    /*
    ���һ��x - N(u,sigma)Gaussian�ֲ��������
    */
    float randGaussian( float u, float sigma );
    /*
    �����һ���ۼƸ���c'_i
    ���������
    float * weight��    Ϊһ����N��Ȩ�أ����ʣ�������
    int N��             ����Ԫ�ظ���
    ���������
    float * cumulateWeight�� Ϊһ����N+1���ۼ�Ȩ�ص����飬
    cumulateWeight[0] = 0;
    */
    void NormalizeCumulatedWeight( float * weight, float * cumulateWeight, int N )
    {
        int i;

        for ( i = 0; i < N+1; i++ )
            cumulateWeight[i] = 0;
        for ( i = 0; i < N; i++ )
            cumulateWeight[i+1] = cumulateWeight[i] + weight[i];
        for ( i = 0; i < N+1; i++ )
            cumulateWeight[i] = cumulateWeight[i]/ cumulateWeight[N];

        return;
    }

    /*
    �۰���ң�������NCumuWeight[N]��Ѱ��һ����С��j��ʹ��
    NCumuWeight[j] <=v
    float v��              һ�������������
    float * NCumuWeight��  Ȩ������
    int N��                ����ά��
    ����ֵ��
    �����±����
    */
    int BinearySearch( float v, float * NCumuWeight, int N )
    {
        int l, r, m;

        l = 0; 	r = N-1;   /* extreme left and extreme right components' indexes */
        while ( r >= l)
        {
            m = (l+r)/2;
            if ( v >= NCumuWeight[m] && v < NCumuWeight[m+1] ) return( m );
            if ( v < NCumuWeight[m] ) r = m - 1;
            else l = m + 1;
        }
        return( 0 );
    }

    /*
    ���½�����Ҫ�Բ���
    ���������
    float * c��          ��Ӧ����Ȩ������pi(n)
    int N��              Ȩ�����顢�ز�����������Ԫ�ظ���
    ���������
    int * ResampleIndex���ز�����������
    */
    void ImportanceSampling( float * c, int * ResampleIndex, int N );

    /*
    ����ѡ�񣬴�N�����������и���Ȩ��������ѡ��N��
    ���������
    SPACESTATE * state��     ԭʼ�������ϣ���N����
    float * weight��         N��ԭʼ������Ӧ��Ȩ��
    int N��                  ��������
    ���������
    SPACESTATE * state��     ���¹���������
    */
    void ReSelect( SPACESTATE * state, float * weight, int N );

    /*
    ����������ϵͳ״̬������ȡ״̬Ԥ����
    ״̬����Ϊ�� S(t) = A S(t-1) + W(t-1)
    W(t-1)Ϊ��˹����
    ���������
    SPACESTATE * state��      �����״̬������
    int N��                   ����״̬����
    ���������
    SPACESTATE * state��      ���º��Ԥ��״̬������
    */
    void Propagate( SPACESTATE * state, int N,cv::Mat& trackImg );

    /*
    �۲⣬����״̬����St�е�ÿһ���������۲�ֱ��ͼ��Ȼ��
    ���¹�����������µ�Ȩ�ظ���
    ���������
    SPACESTATE * state��      ״̬������
    int N��                   ״̬������ά��
    unsigned char * image��   ͼ�����ݣ����������ң��������µ�˳��ɨ�裬
    ��ɫ���д���RGB, RGB, ...
    int W, H��                ͼ��Ŀ�͸�
    float * ObjectHist��      Ŀ��ֱ��ͼ
    int hbins��               Ŀ��ֱ��ͼ����
    ���������
    float * weight��          ���º��Ȩ��
    */
    void Observe( SPACESTATE * state, float * weight, int N,
                 unsigned char * image, int W, int H,
                 float * ObjectHist, int hbins );

    /*
    ���ƣ�����Ȩ�أ�����һ��״̬����Ϊ�������
    ���������
    SPACESTATE * state��      ״̬������
    float * weight��          ��ӦȨ��
    int N��                   ״̬������ά��
    ���������
    SPACESTATE * EstState��   ���Ƴ���״̬��
    */
    void Estimation( SPACESTATE * state, float * weight, int N,
                    SPACESTATE & EstState );
    /*
    ģ�͸���
    ���������
    SPACESTATE EstState��   ״̬���Ĺ���ֵ
    float * TargetHist��    Ŀ��ֱ��ͼ
    int bins��              ֱ��ͼ����
    float PiT��             ��ֵ��Ȩ����ֵ��
    unsigned char * img��   ͼ�����ݣ�RGB��ʽ
    int W, H��              ͼ����
    �����
    float * TargetHist��    ���µ�Ŀ��ֱ��ͼ
    ************************************************************/
    void ModelUpdate( SPACESTATE EstState, float * TargetHist, int bins, float PiT,
                     unsigned char * img, int W, int H );
    int ColorParticleTracking( unsigned char * image, int W, int H,
                              std::vector<UpdataLocation>&faceloca ,cv::Mat& trackImg);

    void MatToImge(cv::Mat & mat,int w,int h){
        int i,j;
        for(j = 0;j < h;j++)
            for(i = 0;i < w; i++){
                img_[(j*w + i)*3] = MR(mat,i,j);
                img_[(j*w + i)*3 + 1] = MG(mat,i,j);
                img_[(j*w + i)*3 + 2] = MB(mat,i,j);
            }
    }

/*
 * ���룺��ǰ֡ͼ��
 * ���룺��ǰ֡��⵽��Ŀ���б�
 * �����Ŀ��λ�� ��С
 * �����������Ҫ����������б�
*/
    void tracking(cv::Mat& img,std::vector<cv::Rect>&DetectedFaces);
    //����ж�������������뵽�����б��У�����ж������ڸ��ٵ������򽫵�ǰ���ٵ���λ�ý���һ�£���������Сȷ���Ƿ��滻��ǰ����
    bool isNewFace(cv::Rect& rect,int &index);

    /*
    ��ʼ��ϵͳ
    int x0, y0��        ��ʼ������ͼ��Ŀ����������
    int Wx, Hy��        Ŀ��İ���
    unsigned char * img��ͼ�����ݣ�RGB��ʽ
    int W, H��          ͼ����
    */
    int Initialize( int x0, int y0, int Wx, int Hy,
                   unsigned char * img, int W, int H,SPACESTATE*states,float* weights,float*ModelHist );
    int Initialize( cv::Rect &rect,
                   unsigned char * img, int W, int H ,SPACESTATE*states,float* weights,float*ModelHist);
    void newfacePrepare();

    /*
     * �����ڸ��ٵ������ظ���Ŀ��ȥ�ز�����
    */
    void removeTwiceTrackingTarget();

    /*
     * ��������ĸ���Ŀ��
    */
    void ClearDisapperedTarget();
public:
    std::vector<SrcFace> trackedFaces_;
    int face_nums;//��ǰ���ٵ���������
    //ÿһ������spaceState����һ�����飬������N����
    std::vector<std::vector<SPACESTATE> > faceSpaceStates_;
    std::vector<std::vector<float> > faceWeights_;
    std::vector<std::vector<float> > faceModelHist_;//��������ֱ��ͼ�б�
    std::vector<UpdataLocation> faceLocations_;

    unsigned char * img_;
    bool start_ = false;
    int Wid;
    int Hei;

};

#endif // MULTTRACKER_H


















