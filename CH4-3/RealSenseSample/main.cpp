#include <Windows.h>
#pragma comment(lib, "winmm.lib")

#include "pxcsensemanager.h"

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

        // IR�X�g���[����L���ɂ���
        pxcStatus sts = senseManager->EnableStream(
            PXCCapture::StreamType::STREAM_TYPE_IR,
            IR_WIDTH, IR_HEIGHT, IR_FPS );
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "IR�X�g���[���̗L�����Ɏ��s���܂���" );
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
        if ( sample ) {
            // �e�f�[�^��\������
            updateIrImage( sample->ir );
        }

        // �t���[�����������
        senseManager->ReleaseFrame();
    }

    // IR�摜���X�V����
    void updateIrImage( PXCImage* depthFrame )
    {
        if ( depthFrame == 0 ){
            return;
        }
            
        PXCImage::ImageInfo info = depthFrame->QueryInfo();

        // �f�[�^���擾����
        PXCImage::ImageData data;
        pxcStatus sts = depthFrame->AcquireAccess( 
            PXCImage::Access::ACCESS_READ,
            PXCImage::PixelFormat::PIXEL_FORMAT_Y8, &data );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error("IR�摜�̎擾�Ɏ��s");
        }

        // �f�[�^���R�s�[����
        irImage = cv::Mat( info.height, info.width, CV_8U );
        memcpy( irImage.data, data.planes[0], data.pitches[0] * info.height );

        // �f�[�^���������
        depthFrame->ReleaseAccess( &data );
    }

    // �摜��\������
    bool showImage()
    {
        // �\������
        cv::imshow( "IR Image", irImage );

        int c = cv::waitKey( 10 );
        if ( (c == 27) || (c == 'q') || (c == 'Q') ){
            // ESC|q|Q for Exit
            return false;
        }

        return true;
    }

private:

    cv::Mat irImage;
    PXCSenseManager *senseManager = 0;

    const int IR_WIDTH = 640;
    const int IR_HEIGHT = 480;
    const int IR_FPS = 30.0f;
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
