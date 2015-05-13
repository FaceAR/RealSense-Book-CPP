#include "pxcsensemanager.h"

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
    }

    void initilize()
    {
        // SenseManager�𐶐�����
        senseManager = PXCSenseManager::CreateInstance();
        if ( senseManager == nullptr ) {
            throw std::runtime_error( "SenseManager�̐����Ɏ��s���܂���" );
        }

        // �J���[�X�g���[����L���ɂ���
        pxcStatus sts = senseManager->EnableStream(
            PXCCapture::StreamType::STREAM_TYPE_COLOR,
            COLOR_WIDTH, COLOR_HEIGHT, COLOR_FPS );
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "�J���[�X�g���[���̗L�����Ɏ��s���܂���" );
        }

        // �p�C�v���C��������������
        sts = senseManager->Init();
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "�p�C�v���C���̏������Ɏ��s���܂���" );
        }

        // �~���[�\���ɂ���
        senseManager->QueryCaptureManager()->QueryDevice()->SetMirrorMode(
            PXCCapture::Device::MirrorMode::MIRROR_MODE_HORIZONTAL );
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

    void updateFrame()
    {
        // �t���[�����擾����
        pxcStatus sts = senseManager->AcquireFrame( false );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            return;
        }

        // �t���[���f�[�^���擾����
        const PXCCapture::Sample *sample = senseManager->QuerySample();
        if ( sample != nullptr ) {
            // �e�f�[�^��\������
            updateColorImage( sample->color );
        }

        // �t���[�����������
        senseManager->ReleaseFrame();
    }

#if 1
    // �J���[�摜���X�V����(32�r�b�g�t�H�[�}�b�g)
    void updateColorImage( PXCImage* colorFrame )
    {
        if ( colorFrame == nullptr ){
            return;
        }

        // �f�[�^���擾����
        PXCImage::ImageData data;
        pxcStatus sts = colorFrame->AcquireAccess(
            PXCImage::Access::ACCESS_READ,
            PXCImage::PixelFormat::PIXEL_FORMAT_RGB32, &data );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "�J���[�摜�̎擾�Ɏ��s" );
        }

        // �f�[�^���R�s�[����
        PXCImage::ImageInfo info = colorFrame->QueryInfo();

        colorImage = cv::Mat( info.height, info.width, CV_8UC4 );
        memcpy( colorImage.data, data.planes[0], data.pitches[0] * info.height );

        // �f�[�^���������
        colorFrame->ReleaseAccess( &data );
    }

#else
    // �J���[�摜���X�V����(24�r�b�g�t�H�[�}�b�g)
    void updateColorImage( PXCImage* colorFrame )
    {
        if ( colorFrame == nullptr ){
            return;
        }

        // �f�[�^���擾����
        PXCImage::ImageData data;
        pxcStatus sts = colorFrame->AcquireAccess(
            PXCImage::Access::ACCESS_READ,
            PXCImage::PixelFormat::PIXEL_FORMAT_RGB24, &data );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "�J���[�摜�̎擾�Ɏ��s" );
        }

        // �f�[�^���R�s�[����
        PXCImage::ImageInfo info = colorFrame->QueryInfo();

        colorImage = cv::Mat( info.height, info.width, CV_8UC3 );
        memcpy( colorImage.data, data.planes[0], data.pitches[0] * info.height );

        // �f�[�^���������
        colorFrame->ReleaseAccess( &data );
    }
#endif

    // �摜��\������
    bool showImage()
    {
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
        RealSenseApp app;
        app.initilize();
        app.run();
    }
    catch ( std::exception& ex ){
        std::cout << ex.what() << std::endl;
    }
}
