// ObjectTracker 2D
#include "pxcsensemanager.h"
#include "PXCTracker.h"

#define _USE_MATH_DEFINES
#include <math.h>

#include <opencv2\opencv.hpp>

class RealSenseApp
{
public:

    ~RealSenseApp()
    {
        if ( senseManager != nullptr ){
            senseManager->Release();
            senseManager = nullptr;
        }

        if ( tracker != nullptr ){
            tracker->Release();
            tracker = nullptr;
        }
    }

    void initilize()
    {
        // SenseManager�𐶐�����
        senseManager = PXCSenseManager::CreateInstance();
        if ( senseManager == 0 ) {
            throw std::runtime_error( "SenseManager�̐����Ɏ��s���܂���" );
        }

        // �J���[�X�g���[����L���ɂ���
        auto sts = senseManager->EnableStream( PXCCapture::StreamType::STREAM_TYPE_COLOR,
            COLOR_WIDTH, COLOR_HEIGHT, COLOR_FPS );
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "�J���[�X�g���[���̗L�����Ɏ��s���܂���" );
        }

        // �I�u�W�F�N�g�g���b�J�[��L���ɂ���
        sts = senseManager->EnableTracker();
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "�I�u�W�F�N�g�g���b�J�[�̗L�����Ɏ��s���܂���" );
        }

        // �p�C�v���C��������������
        sts = senseManager->Init();
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "�p�C�v���C���̏������Ɏ��s���܂���" );
        }

        // �I�u�W�F�N�g�g���b�J�[������������
        initializeObjectTracking();
    }

    void run()
    {
        // ���C�����[�v
        while ( 1 ) {
            // �t���[���f�[�^���X�V����
            updateFrame();

            // �\������
            auto ret = showImage();
            if ( !ret ){
                break;
            }
        }
    }

private:



    void initializeObjectTracking()
    {
        // �I�u�W�F�N�g�g���b�J�[���擾����
        tracker = senseManager->QueryTracker();
        if ( tracker == 0 ) {
            throw std::runtime_error( "�I�u�W�F�N�g�g���b�J�[�̎擾�Ɏ��s���܂���" );
        }

        // �ǐՂ���摜��ݒ�
        auto sts = tracker->Set2DTrackFromFile( L"targetEarth.jpg", targetId );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "�ǐՂ���摜�̐ݒ�Ɏ��s���܂���" );
        }
    }

    void updateFrame()
    {
        // �t���[�����擾����
        pxcStatus sts = senseManager->AcquireFrame( false );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            return;
        }

        // �t���[���f�[�^���擾����
        const PXCCapture::Sample *sample = senseManager->QueryTrackerSample();
        if ( sample ) {
            // �e�f�[�^��\������
            updateColorImage( sample->color );
        }


        // �I�u�W�F�N�g�g���b�L���O���X�V����
        updateObjectTracking();

        // �t���[�����������
        senseManager->ReleaseFrame();
    }

    // �I�u�W�F�N�g�g���b�L���O���X�V����
    void updateObjectTracking()
    {
        // �ǐՂ��Ă���I�u�W�F�N�g��\������
        PXCTracker::TrackingValues trackData;
        auto sts = tracker->QueryTrackingValues( targetId, trackData );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            return;
        }

        // �I�u�W�F�N�g��ǐՂ��Ă���Ε\������
        if ( PXCTracker::IsTracking( trackData.state ) ) {
            showTrackingValue( &trackData );
        }
    }
    
    void showTrackingValue( PXCTracker::TrackingValues* arrData )
    {
        // for the middleware being a left hand coordinate system.
        float depthnum = 630;

        //correction values for translation and movement dependent on z values
        float depthcorrectionfactor = (arrData->translation.z / depthnum);
        arrData->translation.x = arrData->translation.x / depthcorrectionfactor;
        arrData->translation.y = arrData->translation.y / depthcorrectionfactor;

        // �I�u�W�F�N�g�̃J�������W(��ʂ̒��S�����_)
        auto translation = arrData->translation;

        arrData->translation.y = -arrData->translation.y;
        arrData->translation.x += COLOR_WIDTH / 2;
        arrData->translation.y += COLOR_HEIGHT / 2;

        // ���S�_��\������
        cv::circle( colorImage,
            cv::Point( arrData->translation.x, arrData->translation.y ),
            5, cv::Scalar( 255, 0, 0 ), -1 );


        //3x1 points for scaling
        PXCPoint3DF32 trans1;
        PXCPoint3DF32 trans2;
        PXCPoint3DF32 trans3;

        //changes the size of the arrows
        int scalefactor = 150000; //150000
        float height = scalefactor / translation.z;
        float width = scalefactor / translation.z;
        float depth = scalefactor / translation.z;

        //creates the point vectors
        trans1.x = width;   trans2.x = 0;       trans3.x = 0;
        trans1.y = 0;       trans2.y = height;  trans3.y = 0;
        trans1.z = 0;       trans2.z = 0;       trans3.z = depth;

        PXCPoint4DF32 rot = arrData->rotation;

        //put into 3x1 vector point for matrix multiplication purposes
        PXCPoint3DF32 q;
        q.x = rot.x;
        q.y = rot.y;
        q.z = rot.z;
        float s = rot.w;

        // �摜�̌��o�p�x(����)�����߂�
        double heading;
        double attitude;
        double bank;

        float sqw = s*s;
        float sqx = q.x*q.x;
        float sqy = q.y*q.y;
        float sqz = q.z*q.z;
        float unit = sqx + sqy + sqz + sqw;
        float check = q.x*q.y + q.z*s;
        float rad2deg = 180 / M_PI;

        heading = atan2( 2 * q.y*s - 2 * q.x*q.z, sqx - sqy - sqz + sqw ) * rad2deg;
        attitude = asin( 2.0*check / unit ) * rad2deg;
        bank = atan2( 2 * q.x*s - 2 * q.y*q.z, -sqx + sqy - sqz + sqw ) * rad2deg;


        // �����̕����ɐL�΂����_(�����̐��̏I�_)���v�Z����
        PXCPoint3DF32 prime1; PXCPoint3DF32 prime2; PXCPoint3DF32 prime3;
        float rotmat[3][3];

        // rotation matrix using quaternion values
        rotmat[0][0] = (1 - (2 * (q.y*q.y)) - (2 * (q.z*q.z)));
        rotmat[0][1] = ((2 * q.x*q.y) - (2 * s*q.z));
        rotmat[0][2] = ((2 * q.x*q.z) + (2 * s*q.y));
        rotmat[1][0] = ((2 * q.x*q.y) + (2 * s*q.z));
        rotmat[1][1] = (1 - (2 * q.x*q.x) - (2 * q.z*q.z));
        rotmat[1][2] = ((2 * q.y*q.z) - (2 * s*q.x));
        rotmat[2][0] = ((2 * q.x*q.z) - (2 * s*q.y));
        rotmat[2][1] = ((2 * q.y*q.z) + (2 * s*q.x));
        rotmat[2][2] = (1 - (2 * q.x*q.x) - (2 * q.y*q.y));

        //rotation for x point	
        prime1.x = (rotmat[0][0] * trans1.x) +
                   (rotmat[0][1] * trans1.y) +
                   (rotmat[0][2] * trans1.z);
        prime1.y = (rotmat[1][0] * trans1.x) +
                   (rotmat[1][1] * trans1.y) +
                   (rotmat[1][2] * trans1.z);
        prime1.z = (rotmat[2][0] * trans1.x) +
                   (rotmat[1][2] * trans1.y) +
                   (rotmat[2][2] * trans1.z);

        //rotation for y point
        prime2.x = (rotmat[0][0] * trans2.x) +
                   (rotmat[0][1] * trans2.y) +
                   (rotmat[0][2] * trans2.z);
        prime2.y = (rotmat[1][0] * trans2.x) +
                   (rotmat[1][1] * trans2.y) +
                   (rotmat[1][2] * trans2.z);
        prime2.z = (rotmat[2][0] * trans2.x) +
                   (rotmat[1][2] * trans2.y) +
                   (rotmat[2][2] * trans2.z);

        //rotation for z point
        prime3.x = (rotmat[0][0] * trans3.x) +
                   (rotmat[0][1] * trans3.y) +
                   (rotmat[0][2] * trans3.z);
        prime3.y = (rotmat[1][0] * trans3.x) +
                   (rotmat[1][1] * trans3.y) +
                   (rotmat[1][2] * trans3.z);
        prime3.z = (rotmat[2][0] * trans3.x) +
                   (rotmat[1][2] * trans3.y) +
                   (rotmat[2][2] * trans3.z);

        // �e���W�̌�����\������
        translation = arrData->translation;
        cv::line( colorImage, cv::Point( translation.x, translation.y ),
            cv::Point( translation.x + prime1.x, translation.y - prime1.y ),
            cv::Scalar( 0, 0, 255 ) );
        cv::line( colorImage, cv::Point( translation.x, translation.y ),
            cv::Point( translation.x + prime2.x, translation.y - prime2.y ),
            cv::Scalar( 0, 255, 0 ) );
        cv::line( colorImage, cv::Point( translation.x, translation.y ),
            cv::Point( translation.x + prime3.x, translation.y - prime3.y ),
            cv::Scalar( 255, 0, 0 ) );
    }

    // �J���[�摜���X�V����
    void updateColorImage( PXCImage* colorFrame )
    {
        if ( colorFrame == 0 ){
            return;
        }
            
        PXCImage::ImageInfo info = colorFrame->QueryInfo();

        // �f�[�^���擾����
        PXCImage::ImageData data;
        pxcStatus sts = colorFrame->AcquireAccess( PXCImage::Access::ACCESS_READ,
            PXCImage::PixelFormat::PIXEL_FORMAT_RGB24, &data );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error("�J���[�摜�̎擾�Ɏ��s");
        }

        // �f�[�^���R�s�[����
        colorImage = cv::Mat( info.height, info.width, CV_8UC3 );
        memcpy( colorImage.data, data.planes[0], info.height * info.width * 3 );

        // �f�[�^���������
        colorFrame->ReleaseAccess( &data );
    }

    // �摜��\������
    bool showImage()
    {
        if ( colorImage.cols == 0 ){
            return true;
        }

        // �\������
        cv::imshow( "Color Image", colorImage );

        int c = cv::waitKey( 10 );
        if ( (c == 27) || (c == 'q') || (c == 'Q') ){
            // ESC|q|Q for Exit
            return false;
        }

        return true;
    }

private:

    cv::Mat colorImage;
    PXCSenseManager *senseManager = nullptr;
    
    PXCTracker* tracker = nullptr;
    pxcUID targetId = 0;

    const int COLOR_WIDTH = 640;
    const int COLOR_HEIGHT = 480;
    const int COLOR_FPS = 30;

    //const int COLOR_WIDTH = 1920;
    //const int COLOR_HEIGHT = 1080;
    //const int COLOR_FPS = 30;
};

void main()
{
    try{
        RealSenseApp asenseManager;
        asenseManager.initilize();
        asenseManager.run();
    }
    catch ( std::exception& ex ){
        std::cout << ex.what() << std::endl;
    }
}
