#include <iostream>
#include <sstream>

#include <windows.h>

#include "pxcsensemanager.h"

#include <opencv2\opencv.hpp>

class RealSenseAsenseManager
{
public:

    ~RealSenseAsenseManager()
    {
        if ( scanner != nullptr ){
            scanner->Release();
            scanner = nullptr;
        }

        if ( senseManager != nullptr ){
            senseManager->Release();
            senseManager = nullptr;
        }
    }

    void initilize()
    {
        // SenseManager�𐶐�����
        senseManager = PXCSenseManager::CreateInstance();
        if ( senseManager == 0 ) {
            throw std::runtime_error( "SenseManager�̐����Ɏ��s���܂���" );
        }

        // 3D�X�L������L���ɂ���
        pxcStatus sts = senseManager->Enable3DScan();
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "3D�X�L�����̗L�����Ɏ��s���܂���" );
        }

        // �p�C�v���C��������������
        sts = senseManager->Init();
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "�p�C�v���C���̏������Ɏ��s���܂���" );
        }

        // �~���[�\���ɂ���
        senseManager->QueryCaptureManager()->QueryDevice()->SetMirrorMode(
            PXCCapture::Device::MirrorMode::MIRROR_MODE_HORIZONTAL );

        // 3D�X�L�����̏�����
        initialize3dScan();
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

    void initialize3dScan()
    {
        // �X�L���i�[���擾����
        scanner = senseManager->Query3DScan();
        if ( scanner == nullptr ){
            throw std::runtime_error( "�X�L���i�[�̎擾�Ɏ��s���܂���" );
        }

        // �^�[�Q�b�g�I�v�V�����̐ݒ�
        setTargetingOption(
            PXC3DScan::TargetingOption::NO_TARGETING_OPTIONS );

        // �X�L�������[�h�̐ݒ�
        setScanMode( PXC3DScan::Mode::TARGETING );

        // ���f���쐬�I�v�V�����̕\��
        showReconstructionOption();
        showModelFormat();
    }

    void updateFrame()
    {
        // �t���[�����擾����
        pxcStatus sts = senseManager->AcquireFrame( false );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            return;
        }

        // �t���[���f�[�^���擾����
        updateColorImage( scanner->AcquirePreviewImage() );

        // �t���[�����������
        senseManager->ReleaseFrame();
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
        // �\������
        cv::imshow( "Color Image", colorImage );

        int c = cv::waitKey( 10 );
        if ( (c == 27) || (c == 'q') || (c == 'Q') ){
            // ESC|q|Q for Exit
            return false;
        }
        else if ( c == 't' ) {
            // �^�[�Q�b�g�I�v�V������ύX����
            auto option = scanner->QueryTargetingOptions();
            if ( option == PXC3DScan::TargetingOption::NO_TARGETING_OPTIONS ){
                setTargetingOption(
                    PXC3DScan::TargetingOption::OBJECT_ON_PLANAR_SURFACE_DETECTION );
            }
            else{
                setTargetingOption(
                    PXC3DScan::TargetingOption::NO_TARGETING_OPTIONS );
            }
        }
        else if ( c == 's' ) {
            // �X�L�������[�h��ύX����
            auto scanMode = scanner->QueryMode();
            if ( scanMode == PXC3DScan::Mode::TARGETING ){
                setScanMode( PXC3DScan::Mode::SCANNING );
            }
            else{
                setScanMode( PXC3DScan::Mode::TARGETING );
            }
        }
        else if ( c == 'o' ){
            // ���f���쐬�I�v�V������ύX����
            changeReconstructionOption();
        }
        else if ( c == 'f' ){
            // ���f���쐬�t�H�[�}�b�g��ύX����
            changeModelFormat();
        }
        else if ( c == 'r' ){
            // ���f�����쐬����
            reconstruct();
        }

        return true;
    }

    // �^�[�Q�b�g�I�v�V������ݒ肷��
    void setTargetingOption( PXC3DScan::TargetingOption targetingOption )
    {
        std::cout << "setTargetingOption " << targetingOption << std::endl;
        auto sts = scanner->SetTargetingOptions( targetingOption );
        if ( sts < PXC_STATUS_NO_ERROR ){
            throw std::runtime_error( "�^�[�Q�b�g�I�v�V�����̐ݒ�Ɏ��s���܂���" );
        }
    }

    // �X�L�������[�h��ݒ肷��
    void setScanMode( PXC3DScan::Mode scanMode )
    {
        std::cout << "setScanMode " << scanMode << std::endl;
        auto sts = scanner->SetMode( scanMode );
        if ( sts < PXC_STATUS_NO_ERROR ){
            throw std::runtime_error( "�X�L�������[�h�̐ݒ�Ɏ��s���܂���" );
        }
    }

    // ���f���쐬�I�v�V������ύX����
    void changeReconstructionOption()
    {
        if ( reconstructionOption ==
            PXC3DScan::ReconstructionOption::NO_RECONSTRUCTION_OPTIONS ){
            reconstructionOption = PXC3DScan::ReconstructionOption::SOLIDIFICATION;
        }
        else {
            reconstructionOption = PXC3DScan::ReconstructionOption::NO_RECONSTRUCTION_OPTIONS;
        }

        showReconstructionOption();
    }

    // ���f���쐬�I�v�V������\������
    void showReconstructionOption()
    {
        auto option = (reconstructionOption ==
            PXC3DScan::ReconstructionOption::NO_RECONSTRUCTION_OPTIONS) ?
                "NO_RECONSTRUCTION_OPTIONS" : "SOLIDIFICATION";

        std::cout << "Reconstruction Option : " << option << std::endl;
    }

    // ���f���t�H�[�}�b�g��ύX����
    void changeModelFormat()
    {
        if ( fileFormat == PXC3DScan::FileFormat::OBJ ){
            fileFormat = PXC3DScan::FileFormat::STL;
        }
        else if ( fileFormat == PXC3DScan::FileFormat::STL ){
            fileFormat = PXC3DScan::FileFormat::PLY;
        }
        else {
            fileFormat = PXC3DScan::FileFormat::OBJ;
        }

        showModelFormat();
    }

    // ���f���t�H�[�}�b�g��\������
    void showModelFormat()
    {
        std::wcout << L"Model Format          : " <<
            PXC3DScan::FileFormatToString( fileFormat ) << std::endl;
    }


    // ���f�����쐬����
    void reconstruct()
    {
        // �X�L�������ȊO�̓��f�����쐬���Ȃ�
        auto scanMode = scanner->QueryMode();
        if ( scanMode != PXC3DScan::Mode::SCANNING ){
            return;
        }

        // �t�@�C�������쐬����
        WCHAR fileTitle[MAX_PATH];
        GetTimeFormatEx( 0, 0, 0, L"hhmmss", fileTitle, _countof( fileTitle ) );

        std::wstringstream ss;
        ss << L"model-" << fileTitle << L"." << PXC3DScan::FileFormatToString( fileFormat );

        std::wcout << L"create " << ss.str() << "...";


        // 3D���f�����쐬����
        scanner->Reconstruct( fileFormat, ss.str().c_str(), reconstructionOption );

        std::cout << "done." << std::endl;
    }

private:

    cv::Mat colorImage;
    PXCSenseManager *senseManager = nullptr;
    PXC3DScan* scanner = nullptr;

    PXC3DScan::ReconstructionOption reconstructionOption =
        PXC3DScan::ReconstructionOption::NO_RECONSTRUCTION_OPTIONS;
    PXC3DScan::FileFormat fileFormat = PXC3DScan::FileFormat::OBJ;
};

void main()
{
    try{
        RealSenseAsenseManager asenseManager;
        asenseManager.initilize();
        asenseManager.run();
    }
    catch ( std::exception& ex ){
        std::cout << ex.what() << std::endl;
    }
}
