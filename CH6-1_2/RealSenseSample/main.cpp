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
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "�猟�o�̗L�����Ɏ��s���܂���" );
        }

		//�猟�o��𐶐�����
        PXCFaceModule* faceModule = senseManager->QueryFace();
        if ( faceModule == 0 ) {
            throw std::runtime_error( "�猟�o��̍쐬�Ɏ��s���܂���" );
        }

		//�猟�o�̃v���p�e�B���擾
        PXCFaceConfiguration* config = faceModule->CreateActiveConfiguration();
        if ( config == 0 ) {
            throw std::runtime_error( "�猟�o�̃v���p�e�B�擾�Ɏ��s���܂���" );
        }

        config->SetTrackingMode( PXCFaceConfiguration::TrackingModeType::FACE_MODE_COLOR_PLUS_DEPTH );
        config->ApplyChanges();


        // �p�C�v���C��������������
        sts = senseManager->Init();
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "�p�C�v���C���̏������Ɏ��s���܂���" );
        }

        // �f�o�C�X���̎擾
        auto device = senseManager->QueryCaptureManager()->QueryDevice();
        if ( device == 0 ) {
            throw std::runtime_error( "�f�o�C�X�̎擾�Ɏ��s���܂���" );
        }



        // �~���[�\���ɂ���
        device->SetMirrorMode( PXCCapture::Device::MirrorMode::MIRROR_MODE_HORIZONTAL );

        PXCCapture::DeviceInfo deviceInfo;
        device->QueryDeviceInfo( &deviceInfo );
        if ( deviceInfo.model == PXCCapture::DEVICE_MODEL_IVCAM ) {
            device->SetDepthConfidenceThreshold( 1 );
            device->SetIVCAMFilterOption( 6 );
            device->SetIVCAMMotionRangeTradeOff( 21 );
        }

		config->detection.isEnabled = true;
		config->pose.isEnabled = true;					//�ǉ��F��̎p�����擾���\�ɂ���
		config->pose.maxTrackedFaces = POSE_MAXFACES;	//�ǉ��F��l�܂ł̎p�����擾�\�ɐݒ肷��
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

		//�ǉ��F��̎p�������i�[���邽�߂̕ϐ���p�ӂ���
		PXCFaceData::PoseEulerAngles poseAngle[POSE_MAXFACES] = { 0 }; //��l�ȏ�̎擾���\�ɂ���
		
		//���ꂼ��̊炲�Ƃɏ��擾����ѕ`�揈�����s��
		for (int i = 0; i < numFaces; ++i) {

			//��̏����擾����
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

			//�ǉ��F�|�[�Y(��̌������擾)�FDepth�g�p���̂�
			auto pose = face->QueryPose();
			if (pose != 0){
				auto sts = pose->QueryPoseAngles(&poseAngle[i]);
				if (sts < PXC_STATUS_NO_ERROR) {
					throw std::runtime_error("QueryPoseAngles�Ɏ��s���܂���");
				}
			}

			//�ǉ��F��̎p�����(Yaw, Pitch, Roll)�̏��
			{
				std::stringstream ss;
				ss << "Yaw:" << poseAngle[i].yaw;
				cv::putText(colorImage, ss.str(), cv::Point(faceRect.x, faceRect.y - 65), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2, CV_AA);
			}

			{
				std::stringstream ss;
				ss << "Pitch:" << poseAngle[i].pitch;
				cv::putText(colorImage, ss.str(), cv::Point(faceRect.x, faceRect.y - 40), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2, CV_AA);
			}

			{
				std::stringstream ss;
				ss << "Roll:" << poseAngle[i].roll;
				cv::putText(colorImage, ss.str(), cv::Point(faceRect.x, faceRect.y - 15), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2, CV_AA);
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
        ss << "fps:" << fps;
        cv::putText( colorImage, ss.str(), cv::Point( 50, 50 ), cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar( 0, 0, 255 ), 2, CV_AA );
    }

private:

    cv::Mat colorImage;
    PXCSenseManager* senseManager = 0;
    PXCFaceData* faceData = 0;

    const int COLOR_WIDTH = 640;
    const int COLOR_HEIGHT = 480;
    const int COLOR_FPS = 30;

	static const int POSE_MAXFACES = 2;    //�ǉ��F��̎p�������擾�ł���ő�l����ݒ�

    //const int COLOR_WIDTH = 1920;
    //const int COLOR_HEIGHT = 1080;
    //const int COLOR_FPS = 30;
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
