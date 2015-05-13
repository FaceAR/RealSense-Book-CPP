// ������o����
#include "pxcsensemanager.h"
#include "pxchandconfiguration.h"

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

        if ( handAnalyzer == nullptr ){
            senseManager->Release();
            senseManager = nullptr;
        }

        if ( handData == nullptr ){
            handData->Release();
            handData = nullptr;
        }
    }

    void initilize()
    {
        // SenseManager�𐶐�����
        senseManager = PXCSenseManager::CreateInstance();
        if ( senseManager == nullptr ) {
            throw std::runtime_error( "SenseManager�̐����Ɏ��s���܂���" );
        }

        // Depth�X�g���[����L���ɂ���
        auto sts = senseManager->EnableStream( PXCCapture::StreamType::STREAM_TYPE_DEPTH,
            DEPTH_WIDTH, DEPTH_HEIGHT, DEPTH_FPS );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "Depth�X�g���[���̗L�����Ɏ��s���܂���" );
        }

        // ��̌��o��L���ɂ���
        sts = senseManager->EnableHand();
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "��̌��o�̗L�����Ɏ��s���܂���" );
        }

        // �p�C�v���C��������������
        sts = senseManager->Init();
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "�p�C�v���C���̏������Ɏ��s���܂���" );
        }

        // �~���[�\���ɂ���
        senseManager->QueryCaptureManager()->QueryDevice()->SetMirrorMode(
            PXCCapture::Device::MirrorMode::MIRROR_MODE_HORIZONTAL );


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
        PXCCapture::Device *device = senseManager->QueryCaptureManager()->QueryDevice();
        PXCCapture::DeviceInfo dinfo;
        device->QueryDeviceInfo( &dinfo );
        if ( dinfo.model == PXCCapture::DEVICE_MODEL_IVCAM ) {
            device->SetDepthConfidenceThreshold( 1 );
            //device->SetMirrorMode( PXCCapture::Device::MIRROR_MODE_DISABLED );
            device->SetIVCAMFilterOption( 6 );
        }

        // Hand Module Configuration
        PXCHandConfiguration* config = handAnalyzer->CreateActiveConfiguration();
        //config->EnableNormalizedJoints( true );
        //config->SetTrackingMode( PXCHandData::TRACKING_MODE_EXTREMITIES );
        //config->EnableAllAlerts();
        config->EnableSegmentationImage( true );

        config->ApplyChanges();
        config->Update();
    }

    void updateFrame()
    {
        // �t���[�����擾����
        pxcStatus sts = senseManager->AcquireFrame( false );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            return;
        }

        // ��̃f�[�^���X�V����
        updateHandFrame();

        // �t���[�����������
        senseManager->ReleaseFrame();
    }

    void updateHandFrame()
    {
        handData->Update();

        // �摜��������
        handImage1 = cv::Mat::zeros( DEPTH_HEIGHT, DEPTH_WIDTH, CV_8UC1 );
        handImage2 = cv::Mat::zeros( DEPTH_HEIGHT, DEPTH_WIDTH, CV_8UC1 );

        // ���o������̐����擾����
        auto numOfHands = handData->QueryNumberOfHands();
        for ( int i = 0; i < numOfHands; i++ ) {
            // ����擾����
            PXCHandData::IHand* hand;
            auto sts = handData->QueryHandData(
                PXCHandData::AccessOrderType::ACCESS_ORDER_BY_ID, i, hand );
            if ( sts < PXC_STATUS_NO_ERROR ) {
                continue;
            }

            // ��̃}�X�N�摜���擾����
            PXCImage* image = 0;
            sts = hand->QuerySegmentationImage( image );
            if ( sts < PXC_STATUS_NO_ERROR ) {
                continue;
            }

            // �}�X�N�摜���擾����
            PXCImage::ImageData data;
            sts = image->AcquireAccess( 
                PXCImage::ACCESS_READ, PXCImage::PIXEL_FORMAT_Y8, &data );
            if ( sts < PXC_STATUS_NO_ERROR ){
                continue;
            }

            // �}�X�N�摜�̃T�C�Y��Depth�Ɉˑ�
            // ���2�܂�
            PXCImage::ImageInfo info = image->QueryInfo();
            auto& handImage = (i == 0) ? handImage1 : handImage2;
            memcpy( handImage.data, data.planes[0], info.height * info.width );

            // �f�[�^��\��������(���Ԃ�������܂�)
            //for ( int i = 0; i < info.height * info.width; i++ ){
            //    std::cout << (int)data.planes[0][i] << " ";
            //}

            image->ReleaseAccess( &data );
        }
    }

    // �摜��\������
    bool showImage()
    {
        // �\������
        cv::imshow( "Hand Image 1", handImage1 );
        cv::imshow( "Hand Image 2", handImage2 );

        int c = cv::waitKey( 10 );
        if ( (c == 27) || (c == 'q') || (c == 'Q') ){
            // ESC|q|Q for Exit
            return false;
        }

        return true;
    }

private:

    PXCSenseManager* senseManager = nullptr;

    cv::Mat handImage1;
    cv::Mat handImage2;

    PXCHandModule* handAnalyzer = nullptr;
    PXCHandData* handData = nullptr;

    const int DEPTH_WIDTH = 640;
    const int DEPTH_HEIGHT = 480;
    const int DEPTH_FPS = 30;
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
