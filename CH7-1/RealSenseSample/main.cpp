#include "pxcsensemanager.h"
#include "PXC3DSeg.h"

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

        if ( segmentation != nullptr ){
            segmentation->Release();
            segmentation = nullptr;
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

        // �Z�O�����e�[�V������L���ɂ���
        sts = senseManager->Enable3DSeg();
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "�Z�O�����e�[�V�����̗L�����Ɏ��s���܂���" );
        }

        // �p�C�v���C��������������
        sts = senseManager->Init();
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "�p�C�v���C���̏������Ɏ��s���܂���" );
        }

        // �~���[�\���ɂ���
        senseManager->QueryCaptureManager()->QueryDevice()->SetMirrorMode(
            PXCCapture::Device::MirrorMode::MIRROR_MODE_HORIZONTAL );

        // �Z�O�����e�[�V�����I�u�W�F�N�g���擾����
        segmentation = senseManager->Query3DSeg();
        if ( segmentation == 0 ) {
            throw std::runtime_error( "�Z�O�����e�[�V�����̎擾�Ɏ��s���܂���" );
        }
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
        auto image = segmentation->AcquireSegmentedImage();
        // �e�f�[�^��\������
        updateSegmentationImage( image );

        // �t���[�����������
        senseManager->ReleaseFrame();
    }

    // �Z�O�����e�[�V�����摜���X�V����
    void updateSegmentationImage( PXCImage* colorFrame )
    {
        if ( colorFrame == 0 ){
            return;
        }
            
        PXCImage::ImageInfo info = colorFrame->QueryInfo();

        // �f�[�^���擾����
        PXCImage::ImageData data;
        pxcStatus sts = colorFrame->AcquireAccess( PXCImage::Access::ACCESS_READ,
            PXCImage::PixelFormat::PIXEL_FORMAT_RGB32, &data );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error("�J���[�摜�̎擾�Ɏ��s");
        }

        // �f�[�^���R�s�[����
        colorImage = cv::Mat::zeros( info.height, info.width, CV_8UC4 );

        auto dst = colorImage.data;
        auto src = data.planes[0];

        for ( int i = 0; i < (info.height * info.width); i++ ) {
            auto index = i * BYTE_PER_PIXEL;

            // ���l��0�łȂ��ꍇ�ɂ͗L���ȏꏊ�Ƃ��ĐF���R�s�[����
            if ( src[index + 3] > 0 ){
                dst[index + 0] = src[index + 0];
                dst[index + 1] = src[index + 1];
                dst[index + 2] = src[index + 2];
            }
            // ���l��0�̏ꍇ�͔��ɂ���
            else {
                dst[index + 0] = 255;
                dst[index + 1] = 255;
                dst[index + 2] = 255;
            }
        }

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
    PXCSenseManager *senseManager = 0;
    PXC3DSeg* segmentation = 0;

    // �s�N�Z��������̃o�C�g��
    const int BYTE_PER_PIXEL = 4;

    const int COLOR_WIDTH = 640;
    const int COLOR_HEIGHT = 480;
    const int COLOR_FPS = 30;

    //const int COLOR_WIDTH = 1280;
    //const int COLOR_HEIGHT = 720;
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
