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

	void initializeFace()
	{
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

		//�ǉ��F��F���̃v���p�e�B���擾
		PXCFaceConfiguration::RecognitionConfiguration *rcfg = config->QueryRecognition();

		//�ǉ��F��F����L����
		rcfg->Enable();

		//�ǉ��F��F���p�f�[�^�x�[�X�̗p��
		PXCFaceConfiguration::RecognitionConfiguration::RecognitionStorageDesc desc = {0};
		desc.maxUsers = 10;
		rcfg->CreateStorage(L"MyDB", &desc);
		rcfg->UseStorage(L"MyDB");

		//�ǉ��F��F���̓o�^�̐ݒ���s��
		rcfg->SetRegistrationMode(PXCFaceConfiguration::RecognitionConfiguration::REGISTRATION_MODE_CONTINUOUS);

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

		//��̗̈�������l�p�`��p�ӂ���
		PXCRectI32 faceRect = { 0 };

		//���ꂼ��̊炲�Ƃɏ��擾����ѕ`�揈�����s��
		for (int i = 0; i < numFaces; ++i) {
			auto face = faceData->QueryFaceByIndex(i);
			if (face == 0){
				continue;
			}
			
			// ��̈ʒu���擾:Color�Ŏ擾����
			auto detection = face->QueryDetection();
			if (detection != 0){
				//��̑傫�����擾����
				detection->QueryBoundingRect(&faceRect);
			}

			//��̈ʒu�Ƒ傫������A��̗̈�������l�p�`��`�悷��
			cv::rectangle(colorImage, cv::Rect(faceRect.x, faceRect.y, faceRect.w, faceRect.h), cv::Scalar(255, 0, 0));

			//�ǉ��F�环�ʂ̌��ʂ��i�[���邽�߂̕ϐ���p�ӂ���
			auto *rdata = face->QueryRecognition();

			if (rdata->IsRegistered()){
				//�ǉ��F���ʂ���ID���ǂ������m�F����
				pxcI32 uid = rdata->QueryUserID();
				if (uid != -1) {
					{
						std::stringstream ss;
						ss << "Recognition:" << uid;
						cv::putText(colorImage, ss.str(), cv::Point(faceRect.x, faceRect.y), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2, CV_AA);
					}
				}
			}
			
			std::stringstream id_ss;	//�ǉ��F�o�^�E�����̃��b�Z�[�W�p
			
			//�ǉ��F�L�[�{�[�h�̓��͑҂�
			int c = cv::waitKey(10);

			//�ǉ��F���o�^����
			if ((c == 'r') || (c == 'R')){
				int id = rdata->RegisterUser();
				id_ss << id << "Regist";
			}
			//�ǉ��F��̎��ʂ���������
			else if ((c == 'u') || (c == 'U')){
				rdata->UnregisterUser();
				id_ss << "Users Unregisted!!!";
			}

			//�ǉ��F�o�^�E�����̃��b�Z�[�W��\��
			cv::putText(colorImage, id_ss.str(), cv::Point(50, 125), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2, CV_AA);

			//�ǉ��F������@�̐����̕\��
			{
				std::stringstream ss;
				ss << "Regist User : Key R push";
				cv::putText(colorImage, ss.str(), cv::Point(50, 75), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2, CV_AA);
			}

			{
				std::stringstream ss;
				ss << "Unregist Users : Key U push";
				cv::putText(colorImage, ss.str(), cv::Point(50, 100), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2, CV_AA);
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
