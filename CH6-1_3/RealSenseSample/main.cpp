#include <sstream>

#include <Windows.h>
#pragma comment(lib, "winmm.lib")

#include "pxcsensemanager.h"
#include "PXCFaceConfiguration.h"

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

        // �J���[�X�g���[����L���ɂ���
        pxcStatus sts = senseManager->EnableStream( PXCCapture::StreamType::STREAM_TYPE_COLOR, COLOR_WIDTH, COLOR_HEIGHT, COLOR_FPS );
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "�J���[�X�g���[���̗L�����Ɏ��s���܂���" );
        }

		initializeFace();
    }

	void initializeFace(){

		// �猟�o��L���ɂ���
		auto sts = senseManager->EnableFace();
		if (sts<PXC_STATUS_NO_ERROR) {
			throw std::runtime_error("�猟�o�̗L�����Ɏ��s���܂���");
		}

		//�猟�o��𐶐�����
		PXCFaceModule* faceModule = senseManager->QueryFace();
		if (faceModule == 0) {
			throw std::runtime_error("�猟�o��̍쐬�Ɏ��s���܂���");
		}

		//�猟�o�̃v���p�e�B���擾
		PXCFaceConfiguration* config = faceModule->CreateActiveConfiguration();
		if (config == 0) {
			throw std::runtime_error("�猟�o�̃v���p�e�B�擾�Ɏ��s���܂���");
		}

		config->SetTrackingMode(PXCFaceConfiguration::TrackingModeType::FACE_MODE_COLOR_PLUS_DEPTH);
		config->ApplyChanges();


		// �p�C�v���C��������������
		sts = senseManager->Init();
		if (sts<PXC_STATUS_NO_ERROR) {
			throw std::runtime_error("�p�C�v���C���̏������Ɏ��s���܂���");
		}

		// �f�o�C�X���̎擾
		auto device = senseManager->QueryCaptureManager()->QueryDevice();
		if (device == 0) {
			throw std::runtime_error("�f�o�C�X�̎擾�Ɏ��s���܂���");
		}

		// �~���[�\���ɂ���
		device->SetMirrorMode(PXCCapture::Device::MirrorMode::MIRROR_MODE_HORIZONTAL);

		PXCCapture::DeviceInfo deviceInfo;
		device->QueryDeviceInfo(&deviceInfo);
		if (deviceInfo.model == PXCCapture::DEVICE_MODEL_IVCAM) {
			device->SetDepthConfidenceThreshold(1);
			device->SetIVCAMFilterOption(6);
			device->SetIVCAMMotionRangeTradeOff(21);
		}

		config->detection.isEnabled = true;
		config->landmarks.isEnabled = true;         //�ǉ��F�����h�}�[�N�擾���\�ɂ���
		config->landmarks.maxTrackedFaces = LANDMARK_MAXFACES;		//�ǉ��F�����l���ɑΉ�������
		config->ApplyChanges();

		faceData = faceModule->CreateOutput();

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

		updateFaceFrame();

        // �t���[�����������
        senseManager->ReleaseFrame();

        // �t���[�����[�g��\������
        showFps();
    }

	void updateFaceFrame(){
		// �t���[���f�[�^���擾����
		const PXCCapture::Sample *sample = senseManager->QuerySample();
		if (sample) {
			// �e�f�[�^��\������
			updateColorImage(sample->color);
		}

		//SenceManager���W���[���̊�̃f�[�^���X�V����
		faceData->Update();

		//���o������̐����擾����
		const int numFaces = faceData->QueryNumberOfDetectedFaces();

		//�ǉ��F��̃����h�}�[�N�i�����_�j�̃f�[�^�̓��ꕨ��p��
		PXCFaceData::LandmarksData *landmarkData[LANDMARK_MAXFACES];
		PXCFaceData::LandmarkPoint* landmarkPoints;
		pxcI32 numPoints;

		//���ꂼ��̊炲�Ƃɏ��擾����ѕ`�揈�����s��
		for (int i = 0; i < numFaces; ++i) {
			auto face = faceData->QueryFaceByIndex(i);
			if (face == 0){
				continue;
			}

			//��̈ʒu�f�[�^�̓��ꕨ��p��
			PXCRectI32 faceRect = { 0 };


			//��̈ʒu���擾:Color�Ŏ擾����
			auto detection = face->QueryDetection();
			if (detection != 0){
				detection->QueryBoundingRect(&faceRect);
			}

			//�ǉ��F�t�F�C�X�f�[�^���烉���h�}�[�N�i�����_�Q�j�ɂ��Ă̏��𓾂�
			landmarkData[i] = face->QueryLandmarks();
			if (landmarkData[i] != NULL)
			{
				//�����h�}�[�N�f�[�^���牽�̓����_���F���ł������𒲂ׂ�
				numPoints = landmarkData[i]->QueryNumPoints();
				//�F���ł��������_�̐������A�����_���i�[����C���X�^���X�𐶐�����
				landmarkPoints = new PXCFaceData::LandmarkPoint[numPoints];
				//�����h�}�[�N�f�[�^����A�����_�̈ʒu���擾�A�\��
				if (landmarkData[i]->QueryPoints(landmarkPoints)){
					for (int j = 0; j < numPoints; j++){
						{
							std::stringstream ss;
							ss << j ;
							//ss << landmarkPoints[j].source.alias;
							//int z = landmarkPoints[j].source.alias;
							cv::putText(colorImage, ss.str(), cv::Point(landmarkPoints[j].image.x, landmarkPoints[j].image.y), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 255), 1, CV_AA);
						}
					}
				}

			}

		}

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
        pxcStatus sts = colorFrame->AcquireAccess( PXCImage::Access::ACCESS_READ, PXCImage::PixelFormat::PIXEL_FORMAT_RGB24, &data );
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

        return true;
    }

    // �t���[�����[�g�̕\��
    void showFps()
    {
        static DWORD oldTime = ::timeGetTime();
        static int fps = 0;
        static int count = 0;

        count++;

        auto _new = ::timeGetTime();
        if ( (_new - oldTime) >= 1000 ){
            fps = count;
            count = 0;

            oldTime = _new;
        }

        std::stringstream ss;
        ss << fps;
        cv::putText( colorImage, ss.str(), cv::Point( 50, 50 ), cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar( 0, 0, 255 ), 2, CV_AA );
    }

private:

    cv::Mat colorImage;
    PXCSenseManager* senseManager = 0;
    PXCFaceData* faceData = 0;

    const int COLOR_WIDTH = 640;
    const int COLOR_HEIGHT = 480;
    const int COLOR_FPS = 30;

	static const int LANDMARK_MAXFACES = 2;    //�ǉ��F��̃����h�}�[�N�����擾�ł���ő�l����ݒ�

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
