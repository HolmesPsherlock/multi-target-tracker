#include "multtracker.h"

MultTracker::MultTracker()
{

}

float MultTracker::computeOverLap(cv::Rect &r1, cv::Rect &r2)
{
    int x1 = r1.x;
    int y1 = r1.y;
    int width1 = r1.width;
    int height1 = r1.height;

    int x2 = r2.x;
    int y2 = r2.y;
    int width2 = r2.width;
    int height2 = r2.height;

    int endx = std::max(x1+width1,x2+width2);
    int startx = std::min(x1,x2);
    int width = width1+width2-(endx-startx);

    int endy = std::max(y1+height1,y2+height2);
    int starty = std::min(y1,y2);
    int height = height1+height2-(endy-starty);

    float ratio = 0.0f;
    float Area,Area1,Area2;

    if (width<=0||height<=0)
        return 0.0f;
    else
    {
        Area = width*height;
        Area1 = width1*height1;
        Area2 = width2*height2;
//        ratio = Area /(Area1+Area2-Area);
        float ratio1 = (Area+0.0001)/Area1;
        float ratio2 = (Area+0.0001)/Area2;
        ratio = std::max(ratio1,ratio2);
    }
    std::cout << ratio << " ratio:" << std::endl;
    return ratio;
}

void MultTracker::CalcuColorHistogram(int x0, int y0, int Wx, int Hy, unsigned char *image, int W, int H, float *ColorHist, int bins)
{
    int x_begin, y_begin;  /* ָ��ͼ����������Ͻ����� */
    int y_end, x_end;
    int x, y, i, index;
    int r, g, b;
    float k, r2, f;
    int a2;

    for ( i = 0; i < bins; i++ )     /* ֱ��ͼ����ֵ��0 */
        ColorHist[i] = 0.0;
    if ( ( x0 < 0 ) || (x0 >= W) || ( y0 < 0 ) || ( y0 >= H )
        || ( Wx <= 0 ) || ( Hy <= 0 ) ) return;

    x_begin = x0 - Wx;               /* ����ʵ�ʸ߿��������ʼ�� */
    y_begin = y0 - Hy;
    if ( x_begin < 0 ) x_begin = 0;
    if ( y_begin < 0 ) y_begin = 0;
    x_end = x0 + Wx;
    y_end = y0 + Hy;
    if ( x_end >= W ) x_end = W-1;//������Χ�Ļ����û��Ŀ�ı߽�����ֵ���ӵ�����
    if ( y_end >= H ) y_end = H-1;
    a2 = Wx*Wx+Hy*Hy;                /* ����뾶ƽ��a^2 */
    f = 0.0;                         /* ��һ��ϵ�� */
    for ( y = y_begin; y <= y_end; y++ )
        for ( x = x_begin; x <= x_end; x++ )
        {
            r = image[(y*W+x)*3] >> R_SHIFT;   /* ����ֱ��ͼ */
            g = image[(y*W+x)*3+1] >> G_SHIFT; /*��λλ������R��G��B���� */
            b = image[(y*W+x)*3+2] >> B_SHIFT;
            index = r * G_BIN * B_BIN + g * B_BIN + b;//�ѵ�ǰrgb����һ������
            r2 = (float)(((y-y0)*(y-y0)+(x-x0)*(x-x0))*1.0/a2); /* ����뾶ƽ��r^2 */
            k = 1 - r2;   /* k(r) = 1-r^2, |r| < 1; ����ֵ k(r) = 0 ��Ӱ����*/
            f = f + k;
            ColorHist[index] = ColorHist[index] + k;  /* ������ܶȼ�Ȩ��ɫֱ��ͼ */
        }
    for ( i = 0; i < bins; i++ )     /* ��һ��ֱ��ͼ */
        ColorHist[i] = ColorHist[i]/f;

        return;
}

float MultTracker::randGaussian(float u, float sigma)
{
    float x1, x2, v1, v2;
    float s = 100.0;
    float y;
    /*
    ʹ��ɸѡ��������̬�ֲ�N(0,1)�������(Box-Mulles����)
    1. ����[0,1]�Ͼ����������X1,X2
    2. ����V1=2*X1-1,V2=2*X2-1,s=V1^2+V2^2
    3. ��s<=1,ת����4������ת1
    4. ����A=(-2ln(s)/s)^(1/2),y1=V1*A, y2=V2*A
    y1,y2ΪN(0,1)�������
    */
    while ( s > 1.0 )
    {
        x1 = rand0_1();
        x2 = rand0_1();
        v1 = 2 * x1 - 1;
        v2 = 2 * x2 - 1;
        s = v1*v1 + v2*v2;
    }
    y = (float)(sqrt( -2.0 * log(s)/s ) * v1);
    /*
    ���ݹ�ʽ
    z = sigma * y + u
    ��y����ת����N(u,sigma)�ֲ�
    */
    return( sigma * y + u );
}

void MultTracker::ImportanceSampling(float *c, int *ResampleIndex, int N)
{
    float rnum, * cumulateWeight;
    int i, j;

    cumulateWeight = new float [N+1]; /* �����ۼ�Ȩ�������ڴ棬��СΪN+1 */
    NormalizeCumulatedWeight( c, cumulateWeight, N ); /* �����ۼ�Ȩ�� */
    for ( i = 0; i < N; i++ )
    {
        rnum = rand0_1();       /* �������һ��[0,1]����ȷֲ����� */
        j = BinearySearch( rnum, cumulateWeight, N+1 ); /* ����<=rnum����С����j */
        if ( j == N ) j--;
        ResampleIndex[i] = j;	/* �����ز����������� */
    }

    delete[] cumulateWeight;

    return;
}

void MultTracker::ReSelect(SPACESTATE *state, float *weight, int N)
{
    SPACESTATE * tmpState;//�µķŹ��ĵط�
    int i, * rsIdx;//ͳ�Ƶ�������������������

    tmpState = new SPACESTATE[N];
    rsIdx = new int[N];

    ImportanceSampling( weight, rsIdx, N ); /* ����Ȩ�����²��� */
    for ( i = 0; i < N; i++ )
        tmpState[i] = state[rsIdx[i]];//temStateΪ��ʱ����,����state[i]��state[rsIdx[i]]������
    for ( i = 0; i < N; i++ )
        state[i] = tmpState[i];

    delete[] tmpState;
    delete[] rsIdx;

    return;
}

void MultTracker::Propagate(SPACESTATE *state, int N, cv::Mat &trackImg)
{
    int i;
    int j;
    float rn[7];

    /* ��ÿһ��״̬����state[i](��N��)���и��� */
    for ( i = 0; i < N; i++ )  /* �����ֵΪ0�������˹���� */
    {
        for ( j = 0; j < 7; j++ ) rn[j] = randGaussian( 0, (float)0.6 ); /* ����7�������˹�ֲ����� */
        state[i].xt = (int)(state[i].xt + state[i].v_xt * DELTA_T + rn[0] * state[i].Hxt + 0.5);
        state[i].yt = (int)(state[i].yt + state[i].v_yt * DELTA_T + rn[1] * state[i].Hyt + 0.5);
        state[i].v_xt = (float)(state[i].v_xt + rn[2] * VELOCITY_DISTURB);
        state[i].v_yt = (float)(state[i].v_yt + rn[3] * VELOCITY_DISTURB);
        state[i].Hxt = (int)(state[i].Hxt+state[i].Hxt*state[i].at_dot + rn[4] * SCALE_DISTURB + 0.5);
        state[i].Hyt = (int)(state[i].Hyt+state[i].Hyt*state[i].at_dot + rn[5] * SCALE_DISTURB + 0.5);
        state[i].at_dot = (float)(state[i].at_dot + rn[6] * SCALE_CHANGE_D);
//        cv::circle(trackImg,cv::Point(state[i].xt,state[i].yt),3,cv::Scalar(0,255,0),1,8,3);
    }
    return;
}

void MultTracker::Observe(SPACESTATE *state, float *weight, int N, unsigned char *image, int W, int H, float *ObjectHist, int hbins)
{
    int i;
    float * ColorHist;
    float rho;

    ColorHist = new float[hbins];

    for ( i = 0; i < N; i++ )
    {
        /* (1) �����ɫֱ��ͼ�ֲ� */
        CalcuColorHistogram( state[i].xt, state[i].yt,state[i].Hxt, state[i].Hyt,
            image, W, H, ColorHist, hbins );
        /* (2) Bhattacharyyaϵ�� */
        rho = CalcuBhattacharyya( ColorHist, ObjectHist, hbins );
        /* (3) ���ݼ���õ�Bhattacharyyaϵ���������Ȩ��ֵ */
        weight[i] = CalcuWeightedPi( rho );
    }

    delete[] ColorHist;

    return;
}

void MultTracker::Estimation(SPACESTATE *state, float *weight, int N, SPACESTATE &EstState)
{
    int i;
    float at_dot, Hxt, Hyt, v_xt, v_yt, xt, yt;
    float weight_sum;

    at_dot = 0;
    Hxt = 0; 	Hyt = 0;
    v_xt = 0;	v_yt = 0;
    xt = 0;  	yt = 0;
    weight_sum = 0;
    for ( i = 0; i < N; i++ ) /* ��� */
    {
        at_dot += state[i].at_dot * weight[i];
        Hxt += state[i].Hxt * weight[i];
        Hyt += state[i].Hyt * weight[i];
        v_xt += state[i].v_xt * weight[i];
        v_yt += state[i].v_yt * weight[i];
        xt += state[i].xt * weight[i];
        yt += state[i].yt * weight[i];
        weight_sum += weight[i];
    }
    /* ��ƽ�� */
    if ( weight_sum <= 0 ) weight_sum = 1; /* ��ֹ��0����һ�㲻�ᷢ�� */
    EstState.at_dot = at_dot/weight_sum;
    EstState.Hxt = (int)(Hxt/weight_sum);
    EstState.Hyt = (int)(Hyt/weight_sum);
    EstState.v_xt = v_xt/weight_sum;
    EstState.v_yt = v_yt/weight_sum;
    EstState.xt = (int)(xt/weight_sum);
    EstState.yt = (int)(yt/weight_sum);

    return;
}

void MultTracker::ModelUpdate(SPACESTATE EstState, float *TargetHist, int bins, float PiT, unsigned char *img, int W, int H)
{
    float * EstHist, Bha, Pi_E;
    int i;

    EstHist = new float [bins];

    /* (1)�ڹ���ֵ������Ŀ��ֱ��ͼ */
    CalcuColorHistogram( EstState.xt, EstState.yt, EstState.Hxt,
        EstState.Hyt, img, W, H, EstHist, bins );
    /* (2)����Bhattacharyyaϵ�� */
    Bha  = CalcuBhattacharyya( EstHist, TargetHist, bins );
    /* (3)�������Ȩ�� */
    Pi_E = CalcuWeightedPi( Bha );

    if ( Pi_E > PiT )
    {
        for ( i = 0; i < bins; i++ )
        {
            TargetHist[i] = (float)((1.0 - ALPHA_COEFFICIENT) * TargetHist[i]
            + ALPHA_COEFFICIENT * EstHist[i]);
        }
    }
    delete[] EstHist;
}

int MultTracker::ColorParticleTracking(unsigned char *image, int W, int H, std::vector<UpdataLocation> &faceloca, cv::Mat &trackImg)
{
    std::vector<SPACESTATE>EStates;
    EStates.resize(this->faceSpaceStates_.size());
    int i;
    for(int sw = 0;sw < this->faceSpaceStates_.size();sw ++){
        ReSelect(this->faceSpaceStates_[sw].data(),this->faceWeights_[sw].data(),NParticle);
    }
    /* ����������״̬���̣���״̬��������Ԥ�� */
    for(int sw = 0;sw < this->faceSpaceStates_.size();sw ++){
        Propagate(this->faceSpaceStates_[sw].data(),NParticle,trackImg);
    }
    /* �۲⣺��״̬�����и��� */
    static int nbin = R_BIN * G_BIN * B_BIN;
    for(int sw = 0;sw < this->faceSpaceStates_.size();sw ++){

        Observe(this->faceSpaceStates_[sw].data(),this->faceWeights_[sw].data(),
                NParticle,this->img_,W,H,this->faceModelHist_[sw].data(),nbin);
    }
    /* ���ƣ���״̬�����й��ƣ���ȡλ���� */
    for(int sw = 0;sw < this->faceSpaceStates_.size();sw ++){
        Estimation(this->faceSpaceStates_[sw].data(),
                   this->faceWeights_[sw].data(),
                   NParticle,
                   EStates[sw]);
    }
    for(int sw = 0;sw < faceloca.size();sw ++){
        faceloca[sw].xc = EStates[sw].xt;
        faceloca[sw].yc = EStates[sw].yt;
        faceloca[sw].Wx_h = EStates[sw].Hxt;
        faceloca[sw].Hy_h = EStates[sw].Hyt;
        faceloca[sw].max_weight = this->faceWeights_[sw][0];
    }
    /* ģ�͸��� */
    for(int sw = 0;sw < this->faceModelHist_.size();sw ++){
        ModelUpdate(EStates[sw],this->faceModelHist_[sw].data(),nbin,Pi_Thres,this->img_,W,H);
    }
    /* �������Ȩ��ֵ */
    for(int i =0;i < this->faceSpaceStates_.size();i ++){
        for(int j = 0;j < NParticle;j ++){
            faceloca[i].max_weight =
                    faceloca[i].max_weight < this->faceWeights_[i][j] ? this->faceWeights_[i][j] : faceloca[i].max_weight;
        }
    }
    for(int i = 0; i< this->trackedFaces_.size();i ++){
        if(faceloca[i].xc < 0 || faceloca[i].yc < 0 || faceloca[i].xc >=W || faceloca[i].yc >=H || faceloca[i].Wx_h <=0 || faceloca[i].Hy_h <=0
                || faceloca[i].max_weight < MAX_WEIGHT)
        {
            this->trackedFaces_[i].is_disappear = true;
        }
    }
    return( 1 );
}

void MultTracker::tracking(cv::Mat &img, std::vector<cv::Rect> &DetectedFaces)
{
    if(!start_){
        start_ = true;
        Wid = img.cols;
        Hei = img.rows;
        img_ = new unsigned char [Wid * Hei * 3];
    }
    MatToImge(img,Wid,Hei);
    int index;
    for(int i = 0;i < DetectedFaces.size();i++){
        if(isNewFace(DetectedFaces[i],index)){
            SrcFace tmp;
            //����������λ�ò���ͼ���ڣ��������������
            cv::Rect rect = DetectedFaces[i];
            if(rect.x <0 || rect.x > img.cols)
                continue;
            if(rect.y < 0 || rect.y > img.rows)
                continue;
            if((rect.x + rect.width) <0 || (rect.x + rect.width) > img.cols)
                continue;
            if((rect.y + rect.height) < 0 || (rect.y + rect.height) > img.rows)
                continue;
            tmp.face = img(rect);
            tmp.is_disappear = false;
            tmp.rect = DetectedFaces[i];
            tmp.src = img;
            this->newfacePrepare();

            auto centerx = DetectedFaces[i].x + DetectedFaces[i].width/2;
            auto centery = DetectedFaces[i].y + DetectedFaces[i].height/2;
            auto WidIn = DetectedFaces[i].width / 2;
            auto HeiIn = DetectedFaces[i].height / 2;
            Initialize( centerx, centery, WidIn, HeiIn, img_, Wid, Hei,
                        faceSpaceStates_[faceSpaceStates_.size()-1].data(),
                    faceWeights_[faceWeights_.size()-1].data(),
                    faceModelHist_[faceModelHist_.size()-1].data());
            this->trackedFaces_.push_back(tmp);
        }else{
            auto centerx = DetectedFaces[i].x + DetectedFaces[i].width/2;
            auto centery = DetectedFaces[i].y + DetectedFaces[i].height/2;
            auto WidIn = DetectedFaces[i].width / 2;
            auto HeiIn = DetectedFaces[i].height / 2;
            Initialize( centerx, centery, WidIn, HeiIn, img_, Wid, Hei,
                        faceSpaceStates_[index].data(),
                    faceWeights_[index].data(),
                    faceModelHist_[index].data());
            //�����һ֡��Ŀ��������滻��ԭ����СĿ��.
            if(DetectedFaces[i].area() > this->trackedFaces_[index].rect.area()){
                this->trackedFaces_[index].face = img(DetectedFaces[i]);
                this->trackedFaces_[index].is_disappear = false;
                this->trackedFaces_[index].rect = DetectedFaces[i];
                this->trackedFaces_[index].src = img;
            }
        }
    }

    this->ColorParticleTracking(this->img_,this->Wid,this->Hei,this->faceLocations_,img);

}

bool MultTracker::isNewFace(cv::Rect &rect,int &index)
{
    if(this->trackedFaces_.size() == 0){
        index = 0;
        return true;
    }
    float max_overlop = 0.0;
    for(int i = 0;i < this->trackedFaces_.size();i++){
        if((trackedFaces_[i].rect.contains(cv::Point(rect.x,rect.y) )&&
            trackedFaces_[i].rect.contains(cv::Point(rect.x+rect.width,rect.y+rect.height)))
                ||
                (rect.contains(cv::Point(trackedFaces_[i].rect.x,trackedFaces_[i].rect.y) )&&
                 rect.contains(cv::Point(trackedFaces_[i].rect.x+trackedFaces_[i].rect.width,trackedFaces_[i].rect.y+trackedFaces_[i].rect.height)))
                ){
            index = i;
            return false;
        }

//        float jl = std::sqrt((rect.x-trackedFaces_[i].rect.x)*(rect.x-trackedFaces_[i].rect.x)
//                             + (rect.y-trackedFaces_[i].rect.y)*(rect.y-trackedFaces_[i].rect.y));
//        float r1 = std::sqrt(rect.width*rect.width + rect.height*rect.height);
//        float r2 = std::sqrt(trackedFaces_[i].rect.width*trackedFaces_[i].rect.width+ trackedFaces_[i].rect.height*trackedFaces_[i].rect.height);
//        if(r1+r2 != 0)
//            std::cout << jl/(r1+r2) << std::endl;
        float olap = computeOverLap(trackedFaces_[i].rect,rect);
        if(olap > max_overlop){
            max_overlop = olap;
            index = i;
        }
    }
//    std::cout << "Max overlap:" << max_overlop << std::endl;
    if(max_overlop > 0.40){
        return false;
    }
    else{
        index = -1;
        return true;
    }
}

/*
��ʼ��ϵͳ
int x0, y0��        ��ʼ������ͼ��Ŀ����������
int Wx, Hy��        Ŀ��İ���
unsigned char * img��ͼ�����ݣ�RGB��ʽ
int W, H��          ͼ����
*/
int MultTracker::Initialize(int x0, int y0, int Wx, int Hy, unsigned char *img, int W, int H,SPACESTATE*states,float* weights,float*ModelHist)
{
    int i, j;
    srand((unsigned int)(time(NULL)));
    if ( ModelHist == NULL ) return( -1 );
    /* ����Ŀ��ģ��ֱ��ͼ */
    static int nbin = R_BIN * G_BIN * B_BIN;
    CalcuColorHistogram( x0, y0, Wx, Hy, img, W, H, ModelHist, nbin );
    /* ��ʼ������״̬(��(x0,y0,1,1,Wx,Hy,0.1)Ϊ���ĳ�N(0,0.4)��̬�ֲ�) */
    states[0].xt = x0;
    states[0].yt = y0;
    states[0].v_xt = (float)0.0; // 1.0
    states[0].v_yt = (float)0.0; // 1.0
    states[0].Hxt = Wx;
    states[0].Hyt = Hy;
    states[0].at_dot = (float)0.0; // 0.1
    weights[0] = (float)(1.0/NParticle); /* 0.9; */
    float rn[7];
    for ( i = 1; i < NParticle; i++ )
    {
        for ( j = 0; j < 7; j++ )
            rn[j] = randGaussian( 0, (float)0.6 ); /* ����7�������˹�ֲ����� */
        states[i].xt = (int)( states[0].xt + rn[0] * Wx );
        states[i].yt = (int)( states[0].yt + rn[1] * Hy );
        states[i].v_xt = (float)( states[0].v_xt + rn[2] * VELOCITY_DISTURB );
        states[i].v_yt = (float)( states[0].v_yt + rn[3] * VELOCITY_DISTURB );
        states[i].Hxt = (int)( states[0].Hxt + rn[4] * SCALE_DISTURB );
        states[i].Hyt = (int)( states[0].Hyt + rn[5] * SCALE_DISTURB );
        states[i].at_dot = (float)( states[0].at_dot + rn[6] * SCALE_CHANGE_D );
        /* Ȩ��ͳһΪ1/N����ÿ����������ȵĻ��� */
        weights[i] = (float)(1.0/NParticle);
    }
    return( 1 );
}
int MultTracker::Initialize( cv::Rect &rect,
               unsigned char * img, int W, int H,SPACESTATE*states,float* weights,float*ModelHist ){
    Initialize(rect.x,rect.y,rect.width,rect.height,img,W,H,states,weights,ModelHist);
    return (1);
}

void MultTracker::newfacePrepare()
{
    faceSpaceStates_.resize(faceSpaceStates_.size()+1);
    faceSpaceStates_[faceSpaceStates_.size()-1].resize(NParticle);

    faceWeights_.resize(faceWeights_.size()+1);
    faceWeights_[faceWeights_.size()-1].resize(NParticle);

    faceModelHist_.resize(faceModelHist_.size()+1);
    int nbin = R_BIN * G_BIN * B_BIN;
    faceModelHist_[faceModelHist_.size()-1].resize(nbin);

    faceLocations_.resize(faceLocations_.size() + 1);
}

void MultTracker::removeTwiceTrackingTarget()
{
    bool end = false;
    static int name = 0;
    while(true){
        int newsizse = this->trackedFaces_.size();
        std::cout <<newsizse << std::endl;
        if(this->trackedFaces_.size() == 0 || this->trackedFaces_.size() == 1)
            end = true;
        if(end)
            break;
        for(int i = 0;i < newsizse;i ++){
            for(int j = 1;j < newsizse;j++){
                cv::Rect r1;
                cv::Rect r2;
                r1.x = faceLocations_[i].xc-faceLocations_[i].Wx_h;
                r1.y = faceLocations_[i].yc-faceLocations_[i].Hy_h;
                r1.width = faceLocations_[i].Wx_h*2;
                r1.height = faceLocations_[i].Hy_h*2;

                r1.x = faceLocations_[j].xc-faceLocations_[i].Wx_h;
                r1.y = faceLocations_[j].yc-faceLocations_[i].Hy_h;
                r1.width = faceLocations_[j].Wx_h*2;
                r1.height = faceLocations_[j].Hy_h*2;

                float olap = computeOverLap(r1,r2);
                if(olap>0.5 && (i != j))
                {
                    int index = trackedFaces_[i].rect.area() > trackedFaces_[j].rect.area() ?
                              j : i;
                    std::stringstream ss;
                    ss << "E:/genzong_lyx/tcf/" << name << ".jpg";
                    cv::imwrite(ss.str(),trackedFaces_[i].src);
                    this->faceLocations_.erase(this->faceLocations_.begin() + index);
                    this->faceModelHist_.erase(this->faceModelHist_.begin() + index);
                    this->faceSpaceStates_.erase(this->faceSpaceStates_.begin() + index);
                    this->faceWeights_.erase(this->faceWeights_.begin() + index);
                    this->trackedFaces_.erase(this->trackedFaces_.begin() + index);
                    end = false;
                    break;
                }else{
                    end = true;
                }
            }
        }
    }

}

void MultTracker::ClearDisapperedTarget()
{
    //states
    assert(trackedFaces_.size() == faceSpaceStates_.size());
    assert(trackedFaces_.size() == faceModelHist_.size());
    assert(trackedFaces_.size() == faceWeights_.size());
    assert(trackedFaces_.size() == faceLocations_.size());
    bool bianli_end = false;
    while(true){
        int newsize = trackedFaces_.size();
        static int name = 0;
        for(int i = 0;i < newsize; i++){
            if(trackedFaces_[i].is_disappear){
                //������ٶ���Ŀ��������Ϣ.
                std::stringstream ss;
                ss << "E:/genzong_lyx/src/" << name << ".jpg";
                cv::imwrite(ss.str(),trackedFaces_[i].src);
                ss.str("");
                ss << "E:/genzong_lyx/dst/" << name << ".jpg";
                cv::imwrite(ss.str(),trackedFaces_[i].face);
                ss.str("");
                ++name;
                this->faceLocations_.erase(this->faceLocations_.begin() + i);
                this->faceModelHist_.erase(this->faceModelHist_.begin() + i);
                this->faceSpaceStates_.erase(this->faceSpaceStates_.begin() + i);
                this->faceWeights_.erase(this->faceWeights_.begin() + i);
                this->trackedFaces_.erase(this->trackedFaces_.begin() + i);
                bianli_end = false;
                std::cout << "clear disapeared target." << std::endl;
                break;
            }else {
                bianli_end = true;
            }
        }

        if(bianli_end || newsize == 0)
            break;
    }

}




























