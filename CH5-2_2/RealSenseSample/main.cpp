// ��w�̈ʒu��Depth�摜�ɍ��킹��
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

        // ��̌��o�̐ݒ�
        PXCHandConfiguration* config = handAnalyzer->CreateActiveConfiguration();
        config->SetTrackingMode( PXCHandData::TRACKING_MODE_EXTREMITIES );
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

        // �摜��������
        handImage = cv::Mat::zeros( DEPTH_HEIGHT, DEPTH_WIDTH, CV_8UC4 );

        // �t���[���f�[�^���擾����
        const PXCCapture::Sample *sample = senseManager->QuerySample();
        if ( sample ) {
            // �e�f�[�^��\������
            updateDepthImage( sample->depth );
        }

        // ��̍X�V
        updateHandFrame();

        // �t���[�����������
        senseManager->ReleaseFrame();
    }


    // Depth�摜���X�V����
    void updateDepthImage( PXCImage* depthFrame )
    {
        if ( depthFrame == 0 ){
            return;
        }

        // �f�[�^���擾����
        PXCImage::ImageData data;
        pxcStatus sts = depthFrame->AcquireAccess(
            PXCImage::Access::ACCESS_READ,
            PXCImage::PixelFormat::PIXEL_FORMAT_RGB32, &data );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "Depth�摜�̎擾�Ɏ��s" );
        }

        // �f�[�^���R�s�[����
        PXCImage::ImageInfo info = depthFrame->QueryInfo();
        memcpy( handImage.data, data.planes[0], data.pitches[0] * info.height );

        // �f�[�^���������
        depthFrame->ReleaseAccess( &data );
    }

    void updateHandFrame()
    {
        // ��̃f�[�^���X�V����
        handData->Update();

        // �_�ɐF��t����
        const cv::Scalar colors[] = {
            cv::Scalar( 255, 0, 0 ),
            cv::Scalar( 0, 255, 0 ),
            cv::Scalar( 0, 0, 255 ),
            cv::Scalar( 255, 255, 0 ),
            cv::Scalar( 255, 0, 255 ),
            cv::Scalar( 0, 255, 255 ),
        };

        auto numOfHands = handData->QueryNumberOfHands();
        for ( int i = 0; i < numOfHands; i++ ) {
            // ����擾����
            PXCHandData::IHand* hand;
            auto sts = handData->QueryHandData(
                PXCHandData::AccessOrderType::ACCESS_ORDER_BY_ID, i, hand );
            if ( sts < PXC_STATUS_NO_ERROR ) {
                continue;
            }

            // �ʒu��񋓂���
            for ( int j = 0; j < PXCHandData::NUMBER_OF_EXTREMITIES; j++ ) {
                PXCHandData::ExtremityData extremityData;
                sts = hand->QueryExtremityPoint(
                    (PXCHandData::ExtremityType)j, extremityData );
                if ( sts != PXC_STATUS_NO_ERROR ) {
                    continue;
                }

                cv::circle( handImage,
                    cv::Point( extremityData.pointImage.x, extremityData.pointImage.y ),
                    10, colors[j], -1 );
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
