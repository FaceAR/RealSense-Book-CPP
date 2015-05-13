// ��w�̈ʒu���J���[�摜�ɍ��킹��
#include "pxcsensemanager.h"
#include "pxchandconfiguration.h"
#include "PXCProjection.h"

#include <opencv2\opencv.hpp>

class RealSenseApp
{
public:

    ~RealSenseApp()
    {
        if ( senseManager != 0 ){
            senseManager->Release();
            senseManager = nullptr;
        }

        if ( projection != 0 ){
            projection->Release();
            projection = nullptr;
        }

        if ( handData != 0 ){
            handData->Release();
            handData = nullptr;
        }

        if ( handAnalyzer != 0 ){
            handAnalyzer->Release();
            handAnalyzer = nullptr;
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

        // Depth�X�g���[����L���ɂ���
        sts = senseManager->EnableStream( PXCCapture::StreamType::STREAM_TYPE_DEPTH,
            DEPTH_WIDTH, DEPTH_HEIGHT, DEPTH_FPS );
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "Depth�X�g���[���̗L�����Ɏ��s���܂���" );
        }

        // ��̌��o��L���ɂ���
        sts = senseManager->EnableHand();
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "��̌��o�̗L�����Ɏ��s���܂���" );
        }

        // �p�C�v���C��������������
        sts = senseManager->Init();
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "�p�C�v���C���̏������Ɏ��s���܂���" );
        }

        // �f�o�C�X���擾����
        auto device = senseManager->QueryCaptureManager()->QueryDevice();

        // �~���[�\���ɂ���
        device->SetMirrorMode(
            PXCCapture::Device::MirrorMode::MIRROR_MODE_HORIZONTAL );

        // ���W�ϊ��I�u�W�F�N�g���쐬
        projection = device->CreateProjection();

        // ��̌��o�̏�����
        initializeHandTracking();
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

    void initializeHandTracking()
    {
        // ��̌��o����쐬����
        handAnalyzer = senseManager->QueryHand();
        if ( handAnalyzer == 0 ) {
            throw std::runtime_error( "��̌��o��̎擾�Ɏ��s���܂���" );
        }

        // ��̃f�[�^���擾����
        handData = handAnalyzer->CreateOutput();
        if ( handData == 0 ) {
            throw std::runtime_error( "��̌��o��̍쐬�Ɏ��s���܂���" );
        }

        // RealSense �J�����ł���΁A�v���p�e�B��ݒ肷��
        PXCCapture::DeviceInfo dinfo;
        senseManager->QueryCaptureManager()->QueryDevice()->QueryDeviceInfo( &dinfo );
        if ( dinfo.model == PXCCapture::DEVICE_MODEL_IVCAM ) {
            PXCCapture::Device *device = senseManager->QueryCaptureManager()->QueryDevice();
            device->SetDepthConfidenceThreshold( 1 );
            //device->SetMirrorMode( PXCCapture::Device::MIRROR_MODE_DISABLED );
            device->SetIVCAMFilterOption( 6 );
        }

        // Hand Module Configuration
        PXCHandConfiguration* config = handAnalyzer->CreateActiveConfiguration();
        //config->EnableNormalizedJoints( showNormalizedSkeleton );
        //config->SetTrackingMode( PXCHandData::TRACKING_MODE_EXTREMITIES );
        //config->EnableAllAlerts();
        config->EnableSegmentationImage( true );

        config->ApplyChanges();
        config->Update();
    }

    void updateFrame()
    {
        // �t���[�����擾����
        pxcStatus sts = senseManager->AcquireFrame( true );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            return;
        }

        // �摜��������
        handImage = cv::Mat::zeros( COLOR_HEIGHT, COLOR_WIDTH, CV_8UC3 );

        // �t���[���f�[�^���擾����
        const PXCCapture::Sample *sample = senseManager->QuerySample();
        if ( sample ) {
            // �e�f�[�^��\������
            updateColorImage( sample->color );
        }

        // ��̍X�V
        updateHandFrame();

        // �t���[�����������
        senseManager->ReleaseFrame();
    }


    // �J���[�摜���X�V����
    void updateColorImage( PXCImage* colorFrame )
    {
        if ( colorFrame == 0 ){
            return;
        }

        // �f�[�^���擾����
        PXCImage::ImageData data;
        pxcStatus sts = colorFrame->AcquireAccess( PXCImage::Access::ACCESS_READ,
            PXCImage::PixelFormat::PIXEL_FORMAT_RGB24, &data );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "�J���[�摜�̎擾�Ɏ��s" );
        }

        // �f�[�^���R�s�[����
        PXCImage::ImageInfo info = colorFrame->QueryInfo();
        memcpy( handImage.data, data.planes[0], info.height * info.width * 3 );

        // �f�[�^���������
        colorFrame->ReleaseAccess( &data );
    }

    void updateHandFrame()
    {
        // ��̃f�[�^���X�V����
        handData->Update();

        auto numOfHands = handData->QueryNumberOfHands();
        for ( int i = 0; i < numOfHands; i++ ) {
            // ����擾����
            PXCHandData::IHand* hand;
            auto sts = handData->QueryHandData(
                PXCHandData::AccessOrderType::ACCESS_ORDER_BY_ID, i, hand );
            if ( sts < PXC_STATUS_NO_ERROR ) {
                continue;
            }

            // �w�̊֐߂�񋓂���
            for ( int j = 0; j < PXCHandData::NUMBER_OF_JOINTS; j++ ) {
                //  �w�̃f�[�^���擾����
                PXCHandData::JointData jointData;
                sts = hand->QueryTrackedJoint( (PXCHandData::JointType)j, jointData );
                if ( sts != PXC_STATUS_NO_ERROR ) {
                    continue;
                }

                // Depth���W�n���J���[���W�n�ɕϊ�����
                PXCPointF32 colorPoint = { 0 };
                auto depthPoint = jointData.positionImage;
                depthPoint.z = jointData.positionWorld.z * 1000;
                projection->MapDepthToColor( 1, &depthPoint, &colorPoint );

                // �w�̍��W��\������
                cv::circle( handImage,
                    cv::Point( colorPoint.x, colorPoint.y ),
                    5, cv::Scalar( 255, 255, 0 ), -1 );
            }
        }
    }

    // �摜��\������
    bool showImage()
    {
        // �\������
        cv::imshow( "Hand Image", handImage );

        int c = cv::waitKey( 10 );
        if ( (c == 27) || (c == 'q') || (c == 'Q') ){
            // ESC|q|Q for Exit
            return false;
        }

        return true;
    }

private:

    PXCSenseManager* senseManager = 0;

    cv::Mat handImage;

    PXCProjection *projection = 0;

    PXCHandModule* handAnalyzer = 0;
    PXCHandData* handData = 0;

    const int DEPTH_WIDTH = 640;
    const int DEPTH_HEIGHT = 480;
    const int DEPTH_FPS = 30;

    const int COLOR_WIDTH = 1280;
    const int COLOR_HEIGHT = 720;
    const int COLOR_FPS = 30;

    //const int COLOR_WIDTH = 1920;
    //const int COLOR_HEIGHT = 1080;
    //const int COLOR_FPS = 30;
};

void main()
{
    try{
        RealSenseApp app;
        app.initilize();
        app.run();
    }
    catch ( std::exception& ex ){
        std::cout << ex.what() << std::endl;
    }
}
