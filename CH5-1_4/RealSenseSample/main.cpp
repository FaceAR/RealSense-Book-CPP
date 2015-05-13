// �w�̃f�[�^��\������
#include "pxcsensemanager.h"
#include "pxchandconfiguration.h"

#include <opencv2\opencv.hpp>

class RealSenseApp
{
public:

    ~RealSenseApp()
    {
        if ( senseManager != 0 ){
            senseManager->Release();
        }
    }

    void initilize()
    {
        // SenseManager�𐶐�����
        senseManager = PXCSenseManager::CreateInstance();
        if ( senseManager == 0 ) {
            throw std::runtime_error( "SenseManager�̐����Ɏ��s���܂���" );
        }

        // Depth�X�g���[����L���ɂ���
        pxcStatus sts = senseManager->EnableStream( PXCCapture::StreamType::STREAM_TYPE_DEPTH,
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
        pxcStatus sts = senseManager->AcquireFrame( false );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            return;
        }

        // ��̍X�V
        updateHandFrame();

        // �t���[�����������
        senseManager->ReleaseFrame();
    }

    void updateHandFrame()
    {
        // ��̃f�[�^���X�V����
        handData->Update();

        // �摜��������
        handImage = cv::Mat::zeros( DEPTH_HEIGHT, DEPTH_WIDTH, CV_8UC3 );

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
            if ( sts != PXC_STATUS_NO_ERROR ) {
                continue;
            }

            // ��̉摜�f�[�^���擾����
            PXCImage::ImageData data;
            sts = image->AcquireAccess( PXCImage::ACCESS_READ,
                PXCImage::PIXEL_FORMAT_Y8, &data );
            if ( sts != PXC_STATUS_NO_ERROR ){
                continue;
            }

            // ��̍��E���擾����
            auto side = hand->QueryBodySide();

            // ��̊J�x(0-100)���擾����
            auto openness = hand->QueryOpenness();


            // �}�X�N�摜�̃T�C�Y��Depth�Ɉˑ�
            // ���2�܂Ō��o����
            PXCImage::ImageInfo info = image->QueryInfo();
            for ( int j = 0; j < info.height * info.width; ++j ){
                if ( data.planes[0][j] != 0 ){
                    auto index = j * 3;

                    // ��̍��E����ю�̊J�x�ŐF���������߂�
                    // ����=1�F0-127�͈̔�
                    // �E��=2�F0-254�͈̔�
                    auto value = (uchar)((side * 127) * (openness / 100.0f));

                    handImage.data[index + 0] = value;
                    handImage.data[index + 1] = value;
                    handImage.data[index + 2] = value;
                }
            }

            // �w�̊֐߂�񋓂���
            for ( int j = 0; j < PXCHandData::NUMBER_OF_JOINTS; j++ ) {
                PXCHandData::JointData jointData;
                sts = hand->QueryTrackedJoint( (PXCHandData::JointType)j, jointData );
                if ( sts != PXC_STATUS_NO_ERROR ) {
                    continue;
                }

                cv::circle( handImage,
                    cv::Point( jointData.positionImage.x, jointData.positionImage.y ),
                    5, cv::Scalar( 128, 128, 0 ) );
            }

            // ��̉摜�f�[�^���������
            image->ReleaseAccess( &data );

            // ��̒��S��\������
            auto center = hand->QueryMassCenterImage();
            cv::circle( handImage, cv::Point( center.x, center.y ), 5,
                cv::Scalar( 255, 0, 0 ), -1 );

            // ��͈̔͂�\������
            auto boundingbox = hand->QueryBoundingBoxImage();
            cv::rectangle( handImage,
                cv::Rect( boundingbox.x, boundingbox.y, boundingbox.w, boundingbox.h ),
                cv::Scalar( 0, 0, 255 ), 2 );
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

    PXCHandModule* handAnalyzer = 0;
    PXCHandData* handData = 0;

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
