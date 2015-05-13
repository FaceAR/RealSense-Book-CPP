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
        if ( senseManager != nullptr ){
            senseManager->Release();
            senseManager = nullptr;
        }

        if ( handConfig != nullptr ){
            handConfig->Release();
            handConfig = nullptr;
        }

        if ( handData != nullptr ){
            handData->Release();
            handData = nullptr;
        }

        if ( handAnalyzer != nullptr ){
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

        // ��̐ݒ�
        handConfig = handAnalyzer->CreateActiveConfiguration();

        // �o�^����Ă���W�F�X�`���[��񋓂���
        auto num = handConfig->QueryGesturesTotalNumber();
        for ( int i = 0; i < num; i++ ){
            pxcCHAR gestureName[PXCHandData::MAX_NAME_SIZE];
            auto sts = handConfig->QueryGestureNameByIndex( i,
                PXCHandData::MAX_NAME_SIZE, gestureName );
            if ( sts == PXC_STATUS_NO_ERROR ){
                std::wcout << std::hex << i << " " <<  gestureName << std::endl;
            }
        }

        handConfig->ApplyChanges();
        handConfig->Update();
    }

    void updateFrame()
    {
        // �t���[�����擾����
        pxcStatus sts = senseManager->AcquireFrame( true );
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

        // �F��������̐����擾����
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
                PXCHandData::JointData jointData;
                sts = hand->QueryTrackedJoint( (PXCHandData::JointType)j, jointData );
                if ( sts != PXC_STATUS_NO_ERROR ) {
                    continue;
                }

                cv::circle( handImage,
                    cv::Point( jointData.positionImage.x, jointData.positionImage.y ),
                    5, cv::Scalar( 128, 128, 0 ) );
            }
        }

        // �F�������W�F�X�`���[�̐����擾����
        auto numOfGestures = handData->QueryFiredGesturesNumber();
        for ( int i = 0; i < numOfGestures; i++ ) {
            // �F�������W�F�X�`���[���擾����
            PXCHandData::GestureData gesture;
            auto sts = handData->QueryFiredGestureData( i, gesture );
            if ( sts < PXC_STATUS_NO_ERROR ) {
                continue;
            }

            // �W�F�X�`���[����������擾����
            PXCHandData::IHand* hand;
            sts = handData->QueryHandDataById( gesture.handId, hand );
            if ( sts < PXC_STATUS_NO_ERROR ) {
                continue;
            }

            // �ǂ���̎�ŃW�F�X�`���[�����̂�
            auto side = hand->QueryBodySide();
            if ( side == PXCHandData::BodySideType::BODY_SIDE_LEFT ){
                ++leftGestureCount;
            }
            else {
                ++rightGestureCount;
            }
        }

        // �W�F�X�`���[�̌��o����\������
        {
            std::stringstream ss;
            ss << "Left gesture  : " << leftGestureCount;
            cv::putText( handImage, ss.str(), cv::Point( 10, 40 ),
                cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar( 0, 0, 255 ), 2, CV_AA );
        }

        {
            std::stringstream ss;
            ss << "Right gesture : " << rightGestureCount;
            cv::putText( handImage, ss.str(), cv::Point( 10, 80 ),
                cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar( 0, 0, 255 ), 2, CV_AA );
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
        // 0-9�L�[�ŃW�F�X�`���[�̑I��(0-9)
        else if ( ('0' <= c) && (c <= '9') ) {
            // �L�[���C���f�b�N�X�ɕϊ�����(0-9)
            int index = c - '0';
            ChangeGesture( index );
        }
        // a-d�L�[�ŃW�F�X�`���[�̑I��(10-13)
        else if ( ('a' <= c) && (c <= 'd') ) {
            // �L�[���C���f�b�N�X�ɕϊ�����(10-13)
            int index = c - 'a' + 10;
            ChangeGesture( index );
        }

        return true;
    }

    void ChangeGesture( int index )
    {
        // �C���f�b�N�X�̃W�F�X�`���[�����擾����
        pxcCHAR gestureName[PXCHandData::MAX_NAME_SIZE];
        auto sts = handConfig->QueryGestureNameByIndex( index,
            PXCHandData::MAX_NAME_SIZE, gestureName );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            return;
        }

        // ��x���ׂẴW�F�X�`���[��؂�A�I�����ꂽ�W�F�X�`���[��L���ɂ���
        handConfig->DisableAllGestures();
        handConfig->EnableGesture( gestureName, true );

        handConfig->ApplyChanges();

        // �W�F�X�`���[���o���̏�����
        rightGestureCount = leftGestureCount = 0;

        std::wcout << gestureName << " selected" << std::endl;
    }

private:

    cv::Mat handImage;

    PXCSenseManager* senseManager = 0;

    PXCHandModule* handAnalyzer = 0;
    PXCHandData* handData = 0;

    PXCHandConfiguration* handConfig = 0;
    int rightGestureCount = 0;
    int leftGestureCount = 0;

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

