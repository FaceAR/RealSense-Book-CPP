// Depth�̋����f�[�^�𗘗p����
#include "pxcsensemanager.h"

#include <opencv2\opencv.hpp>

class RealSenseAsenseManager
{
public:

    ~RealSenseAsenseManager()
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
        pxcStatus sts = senseManager->EnableStream(
            PXCCapture::StreamType::STREAM_TYPE_DEPTH,
            DEPTH_WIDTH, DEPTH_HEIGHT, DEPTH_FPS );
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "Depth�X�g���[���̗L�����Ɏ��s���܂���" );
        }

        // �p�C�v���C��������������
        sts = senseManager->Init();
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "�p�C�v���C���̏������Ɏ��s���܂���" );
        }

        // �~���[�\���ɂ���
        senseManager->QueryCaptureManager()->QueryDevice()->SetMirrorMode(
            PXCCapture::Device::MirrorMode::MIRROR_MODE_HORIZONTAL );

        // Depth�����\���̏�����
        initializeDepth();
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

    void initializeDepth()
    {
        // �o�b�t�@���쐬����
        depthBuffer.resize( DEPTH_WIDTH * DEPTH_HEIGHT );

        // ���S�_�ŏ���������
        point.x = DEPTH_WIDTH / 2;
        point.y = DEPTH_HEIGHT / 2;

        // �}�E�X���͂��󂯎��悤�ɂ���
        cv::namedWindow( windowName, CV_WINDOW_AUTOSIZE );
        cv::setMouseCallback( windowName, &RealSenseAsenseManager::mouse_callback, this );
    }

    void updateFrame()
    {
        // �t���[�����擾����
        pxcStatus sts = senseManager->AcquireFrame( false );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            return;
        }

        // �t���[���f�[�^���擾����
        const PXCCapture::Sample *sample = senseManager->QuerySample();
        if ( sample ) {
            // �e�f�[�^��\������
            updateDepthImage( sample->depth );
            updateDepthData( sample->depth );
            showSelectedDepth();
        }

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
        depthImage = cv::Mat( info.height, info.width, CV_8UC4 );
        memcpy( depthImage.data, data.planes[0], data.pitches[0] * info.height );

        // �f�[�^���������
        depthFrame->ReleaseAccess( &data );
    }

    // Depth(�����f�[�^)���X�V����
    void updateDepthData( PXCImage* depthFrame )
    {
        if ( depthFrame == 0 ){
            return;
        }

        // �f�[�^���擾����
        PXCImage::ImageData data;
        pxcStatus sts = depthFrame->AcquireAccess(
            PXCImage::Access::ACCESS_READ,
            PXCImage::PixelFormat::PIXEL_FORMAT_DEPTH, &data );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "Depth�f�[�^�̎擾�Ɏ��s" );
        }

        // �f�[�^���R�s�[����
        PXCImage::ImageInfo info = depthFrame->QueryInfo();
        memcpy( &depthBuffer[0], data.planes[0], data.pitches[0] * info.height );

        // �f�[�^���������
        depthFrame->ReleaseAccess( &data );
    }

    // �I���ʒu�̋�����\������
    void showSelectedDepth()
    {
        cv::circle( depthImage, point, 10, cv::Scalar( 0, 0, 255 ), 3 );

        int index = (point.y * DEPTH_WIDTH) + point.x;
        auto depth = depthBuffer[index];

        {
            std::stringstream ss;
            ss << depth << "mm";
            cv::putText( depthImage, ss.str(), point,
                cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar( 128, 255, 128 ), 2, CV_AA );
        }
    }
    
    // �摜��\������
    bool showImage()
    {
        if ( depthImage.rows == 0 || (depthImage.cols == 0) ) {
            return true;
        }

        // �\������
        cv::imshow( windowName, depthImage );

        int c = cv::waitKey( 10 );
        if ( (c == 27) || (c == 'q') || (c == 'Q') ){
            // ESC|q|Q for Exit
            return false;
        }

        return true;
    }

    static void mouse_callback( int event, int x, int y, int flags, void* param )
    {
        RealSenseAsenseManager* pThis = (RealSenseAsenseManager*)param;
        pThis->mouse_callback( event, x, y, flags );
    }

    void mouse_callback( int event, int x, int y, int flags )
    {
        if ( event == cv::EVENT_LBUTTONDOWN ){
            point.x = x;
            point.y = y;
        }
    }

private:

    cv::Mat depthImage;
    PXCSenseManager *senseManager = 0;

    std::vector<unsigned short> depthBuffer;
    cv::Point point;

    const std::string windowName = "Depth Image";

    const int DEPTH_WIDTH = 640;
    const int DEPTH_HEIGHT = 480;
    const int DEPTH_FPS = 30.0f;
};

void main()
{
    try{
        RealSenseAsenseManager app;
        app.initilize();
        app.run();
    }
    catch ( std::exception& ex ){
        std::cout << ex.what() << std::endl;
    }
}
