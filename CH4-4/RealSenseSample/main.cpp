#include "pxcsensemanager.h"

#include <iostream>

class RealSenseApp
{
public:

    ~RealSenseApp()
    {
    }

    void initilize()
    {
        // SenseManager�𐶐�����
        senseManager = PXCSenseManager::CreateInstance();
        if ( senseManager == 0 ) {
            throw std::runtime_error( "SenseManager�̐����Ɏ��s���܂���" );
        }

        // �p�C�v���C��������������
        auto sts = senseManager->Init();
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "�p�C�v���C���̏������Ɏ��s���܂���" );
        }

        // �g�p�\�ȃf�o�C�X��񋓂���
        enumDevice();
    }

    void run()
    {
    }

private:

    void enumDevice()
    {
        // �Z�b�V�������擾����
        auto session = senseManager->QuerySession();
        if ( session == 0 ) {
            throw std::runtime_error( "�Z�b�V�����̎擾�Ɏ��s���܂���" );
        }

        // �擾����O���[�v��ݒ肷��
        PXCSession::ImplDesc mdesc = {};
        mdesc.group = PXCSession::IMPL_GROUP_SENSOR;
        mdesc.subgroup = PXCSession::IMPL_SUBGROUP_VIDEO_CAPTURE;

        for ( int i = 0;; ++i ) {
            // �Z���T�[�O���[�v���擾����
            PXCSession::ImplDesc desc1;
            auto sts = session->QueryImpl( &mdesc, i, &desc1 );
            if ( sts < PXC_STATUS_NO_ERROR ) {
                break;
            }

            // �Z���T�[�O���[�v����\������
            std::wcout << desc1.friendlyName << std::endl;

            // �L���v�`���[�I�u�W�F�N�g���쐬����
            PXCCapture* capture = 0;
            sts = session->CreateImpl<PXCCapture>( &desc1, &capture );
            if ( sts < PXC_STATUS_NO_ERROR ) {
                continue;
            }

            // �f�o�C�X��񋓂���
            enumDevice( capture );

            // �L���v�`���[�I�u�W�F�N�g���������
            capture->Release();
        }
    }

    void enumDevice( PXCCapture* capture )
    {
        for ( int i = 0;; ++i ) {
            // �f�o�C�X�����擾����
            PXCCapture::DeviceInfo dinfo;
            auto sts = capture->QueryDeviceInfo( i, &dinfo );
            if ( sts < PXC_STATUS_NO_ERROR ){
                break;
            }

            // �f�o�C�X����\������
            std::wcout << "\t" << dinfo.name << std::endl;

            // �f�o�C�X���擾����
            auto device = capture->CreateDevice( i );

            for ( int s = 0; s < PXCCapture::STREAM_LIMIT; ++s ) {
                // �X�g���[����ʂ��擾����
                PXCCapture::StreamType type = PXCCapture::StreamTypeFromIndex( s );
                if ( (dinfo.streams & type) == 0 ){
                    continue;
                }

                // �X�g���[�������擾����
                const pxcCHAR *name = PXCCapture::StreamTypeToString( type );
                std::wcout << "\t\t" << name << std::endl;

                // �X�g���[���̃t�H�[�}�b�g���擾����
                int nprofiles = device->QueryStreamProfileSetNum( type );
                for ( int p = 0; p<nprofiles; ++p ) {
                    PXCCapture::Device::StreamProfileSet profiles = {};
                    sts = device->QueryStreamProfileSet( type, p, &profiles );
                    if ( sts < PXC_STATUS_NO_ERROR ) {
                        break;
                    }

                    // �X�g���[���̃t�H�[�}�b�g��\������
                    std::wcout << "\t\t\t" << Profile2String( &profiles[type] ).c_str()
                               << std::endl;
                }
            }
        }

        std::wcout << std::endl;
    }

    // raw_streams �T���v�����
    static std::wstring Profile2String( PXCCapture::Device::StreamProfile *pinfo ) {
        pxcCHAR line[256] = L"";
        if ( pinfo->frameRate.min && pinfo->frameRate.max &&
             pinfo->frameRate.min != pinfo->frameRate.max ) {
            swprintf_s<sizeof( line ) / sizeof( pxcCHAR )>(
                line, L"%s %dx%dx%d-%d",
                PXCImage::PixelFormatToString( pinfo->imageInfo.format ),
                pinfo->imageInfo.width, pinfo->imageInfo.height,
                (int)pinfo->frameRate.min, (int)pinfo->frameRate.max );
        }
        else {
            pxcF32 frameRate = pinfo->frameRate.min ?
                pinfo->frameRate.min : pinfo->frameRate.max;

            swprintf_s<sizeof( line ) / sizeof( pxcCHAR )>(
                line, L"%s %dx%dx%d",
                PXCImage::PixelFormatToString( pinfo->imageInfo.format ),
                pinfo->imageInfo.width, pinfo->imageInfo.height,
                (int)frameRate );
        }
        return std::wstring( line );
    }


private:

    PXCSenseManager *senseManager = 0;
};

void main()
{
    try{
        // �R���\�[���ɓ��{���\��������
        setlocale( LC_ALL, "Japanese" );

        RealSenseApp app;
        app.initilize();
        app.run();
    }
    catch ( std::exception& ex ){
        std::cout << ex.what() << std::endl;
    }
}
