// �֊s���[�h�œ��삳����
#include "pxcsensemanager.h"
#include "pxcblobmodule.h"

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

        if ( blobData != nullptr ){
            blobData->Release();
            blobData = nullptr;
        }

        if ( blobModule != nullptr ){
            blobModule->Release();
            blobModule = nullptr;
        }
    }

    void initilize()
    {
        // SenseManager�𐶐�����
        senseManager = PXCSenseManager::CreateInstance();
        if ( senseManager == 0 ) {
            throw std::runtime_error( "SenseManager�̐����Ɏ��s���܂���" );
        }

        // Blob��L���ɂ���
        pxcStatus sts = senseManager->EnableBlob();
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "Blob�̗L�����Ɏ��s���܂���" );
        }

        // �p�C�v���C��������������
        sts = senseManager->Init();
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "�p�C�v���C���̏������Ɏ��s���܂���" );
        }

        // �~���[�\���ɂ���
        senseManager->QueryCaptureManager()->QueryDevice()->SetMirrorMode(
            PXCCapture::Device::MirrorMode::MIRROR_MODE_HORIZONTAL );

        // Blob������������
        initializeBlob();
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

    // Blob������������
    void initializeBlob()
    {
        // Blob���擾����
        blobModule = senseManager->QueryBlob();
        blobData = blobModule->CreateOutput();

        auto blobConfig = blobModule->CreateActiveConfiguration();
        blobConfig->SetSegmentationSmoothing( 1.0f );
        blobConfig->SetContourSmoothing( 1.0f );
        blobConfig->SetMaxBlobs( 4 );
        blobConfig->SetMaxDistance( 500.0f );
        blobConfig->EnableContourExtraction( true );
        blobConfig->EnableSegmentationImage( true );
        blobConfig->ApplyChanges();

        // �֊s�̓_�̔z���������
        points.resize( 4000 );
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
            updateBlobImage( sample->depth );
        }

        // �t���[�����������
        senseManager->ReleaseFrame();
    }

    // Blob���X�V����
    void updateBlobImage( PXCImage* depthFrame )
    {
        if ( depthFrame == nullptr ){
            return;
        }

        // Blob���X�V����
        auto sts = blobData->Update();
        if ( sts < PXC_STATUS_NO_ERROR ) {
            return;
        }

        // �\���p�摜������������
        PXCImage::ImageInfo depthInfo = depthFrame->QueryInfo();
        contourImage = cv::Mat::zeros( depthInfo.height, depthInfo.width, CV_8U );

        auto session = senseManager->QuerySession();
        depthInfo.format = PXCImage::PIXEL_FORMAT_Y8;
        PXCImage* blobImage = session->CreateImage( &depthInfo );

        // Blob���擾����
        int numOfBlobs = blobData->QueryNumberOfBlobs();
        for ( int i = 0; i < numOfBlobs; ++i ) {
            // Blob�f�[�^���߂����珇�Ɏ擾����
            PXCBlobData::IBlob* blob;
            sts = blobData->QueryBlobByAccessOrder( i, 
                PXCBlobData::AccessOrderType::ACCESS_ORDER_NEAR_TO_FAR, blob );
            if ( sts < PXC_STATUS_NO_ERROR ) {
                continue;
            }

            // Blob�摜���擾����
            sts = blob->QuerySegmentationImage( blobImage );
            if ( sts < PXC_STATUS_NO_ERROR ) {
                continue;
            }

            // Blob�摜��ǂݍ���
            PXCImage::ImageData data;
            pxcStatus sts = blobImage->AcquireAccess( PXCImage::Access::ACCESS_READ,
                PXCImage::PIXEL_FORMAT_Y8, &data );
            if ( sts < PXC_STATUS_NO_ERROR ) {
                continue;
            }

            // �f�[�^���R�s�[����
            for ( int j = 0; j < depthInfo.height * depthInfo.width; ++j ){
                if ( data.planes[0][j] != 0 ){
                    // �C���f�b�N�X�ɂ���āA�F����ς���
                    contourImage.data[j] = (i + 1) * 64;
                }
            }

            // Blob�摜���������
            blobImage->ReleaseAccess( &data );

            // Blob�̗֊s��\������
            updateContoursImage( blob, i );
        }

        // �������ƃG���[�ɂȂ�
        //blobImage->Release();
    }

    // Blob�̗֊s��\������
    void updateContoursImage( PXCBlobData::IBlob* blob, int index )
    {
        // �֊s��\������
        auto numOfContours = blob->QueryNumberOfContours();
        for ( int i = 0; i < numOfContours; ++i ) {
            // �֊s�̓_�̐����擾����
            pxcI32 size = blob->QueryContourSize( i );
            if ( size <= 0 ) {
                continue;
            }

            // �|�C���g�z��̊m�F
            if ( points.size() < size ){
                points.reserve( size );
            }

            // �֊s�̓_���擾����
            auto sts = blob->QueryContourPoints( i, points.size(), &points[0] );
            if ( sts < PXC_STATUS_NO_ERROR ){
                continue;
            }

            // �֊s�̓_��`�悷��
            drawContour( &points[0], size, index );
        }
    }

    // �֊s�̓_��`�悷��
    void drawContour( PXCPointI32* points, pxcI32 size, int index )
    {
        // �_�Ɠ_����Ō���
        for ( int i = 0; i < (size - 1); ++i ){
            const auto& pt1 = points[i];
            const auto& pt2 = points[i + 1];
            cv::line( contourImage, cv::Point( pt1.x, pt1.y ), cv::Point( pt2.x, pt2.y ),
                cv::Scalar( ((index + 1) * 127) ), 5 );
        }

        // �Ō�̓_�ƍŏ��̓_����Ō���
        const auto& pt1 = points[size - 1];
        const auto& pt2 = points[0];
        cv::line( contourImage, cv::Point( pt1.x, pt1.y ), cv::Point( pt2.x, pt2.y ),
            cv::Scalar( ((index + 1) * 127) ), 5 );
    }

    // �摜��\������
    bool showImage()
    {
        if ( contourImage.cols != 0 ){
            cv::imshow( "Contour Image", contourImage );
        }

        int c = cv::waitKey( 10 );
        if ( (c == 27) || (c == 'q') || (c == 'Q') ){
            // ESC|q|Q for Exit
            return false;
        }

        return true;
    }

private:

    cv::Mat contourImage;

    PXCSenseManager* senseManager = nullptr;

    PXCBlobModule* blobModule = nullptr;
    PXCBlobData* blobData = nullptr;
    std::vector<PXCPointI32> points;
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
