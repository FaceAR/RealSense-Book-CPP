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
		
		//���ӁF�\��o���s���ꍇ�́A�J���[�X�g���[����L�������Ȃ�
		/*
		// �J���[�X�g���[����L���ɂ���
		pxcStatus sts = senseManager->EnableStream(PXCCapture::StreamType::STREAM_TYPE_COLOR, COLOR_WIDTH, COLOR_HEIGHT, COLOR_FPS);
		if (sts<PXC_STATUS_NO_ERROR) {
			throw std::runtime_error("�J���[�X�g���[���̗L�����Ɏ��s���܂���");
		}
		*/

		initializeFace();

    }

	void initializeFace(){

		// �猟�o��L���ɂ���
		auto sts = senseManager->EnableFace();
		if (sts<PXC_STATUS_NO_ERROR) {
			throw std::runtime_error("�猟�o�̗L�����Ɏ��s���܂���");
		}

		// �ǉ��F�\��o��L���ɂ���
		sts = senseManager->EnableEmotion();
		if (sts<PXC_STATUS_NO_ERROR) {
			throw std::runtime_error("�\��o�̗L�����Ɏ��s���܂���");
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

		// �猟�o��̐ݒ�
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
		//��������͕\�o(Expression)���̐ݒ���Q�Ƃ��Ă�������
		config->QueryExpressions()->Enable();    //��̕\�o���̗L����
		config->QueryExpressions()->EnableAllExpressions();    //���ׂĂ̕\�o���̗L����
		config->QueryExpressions()->properties.maxTrackedFaces = 2;    //��̕\�o���̍ő�F���l��

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
        // �ύX�Ftrue�ɂ��Ċ�ƕ\��𓯊����Ȃ���QueryEmotion��0���Ԃ�
        pxcStatus sts = senseManager->AcquireFrame( true );
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

		//�ǉ��F�\��o�̌��ʂ��i�[���邽�߂̓��ꕨ��p�ӂ���
		PXCEmotion::EmotionData arrData[NUM_TOTAL_EMOTIONS];

		//�ǉ��F�\������o����
		emotionDet = senseManager->QueryEmotion();
		if (emotionDet == 0) {
			std::cout << "�\��o�Ɏ��s���܂���" << std::endl;
			return;
		}

		//�ǉ��F�\��̃��x���Q
		const char *EmotionLabels[NUM_PRIMARY_EMOTIONS] = {
			"ANGER",
			"CONTEMPT",
			"DISGUST",
			"FEAR",
			"JOY",
			"SADNESS",
			"SURPRISE"
		};

		//�ǉ��F����̃��x���Q
		const char *SentimentLabels[NUM_SENTIMENT_EMOTIONS] = {
			"NEGATIVE",
			"POSITIVE",
			"NEUTRAL"
		};

		//////////////////////////////////////
		//��������͊猟�o�̋@�\
		
		//SenceManager���W���[���̊�̃f�[�^���X�V����
		faceData->Update();

		//���o������̐����擾����
		const int numFaces = faceData->QueryNumberOfDetectedFaces();

		//���ꂼ��̊炲�Ƃɏ��擾����ѕ`�揈�����s��
		for (int i = 0; i < numFaces; ++i) {
			auto face = faceData->QueryFaceByIndex(i);
			if (face == 0){
				continue;
			}

			PXCRectI32 faceRect = { 0 };
			PXCFaceData::PoseEulerAngles poseAngle = { 0 };

			//��̊���̃f�[�^�A����ъp�x�̃f�[�^�̓��ꕨ��p��
			PXCFaceData::ExpressionsData *expressionData;
			PXCFaceData::ExpressionsData::FaceExpressionResult expressionResult;

			// ��̈ʒu���擾:Color�Ŏ擾����
			auto detection = face->QueryDetection();
			if (detection != 0){
				detection->QueryBoundingRect(&faceRect);
			}

			//��̈ʒu�Ƒ傫������A��̗̈�������l�p�`��`�悷��
			cv::rectangle(colorImage, cv::Rect(faceRect.x, faceRect.y, faceRect.w, faceRect.h), cv::Scalar(255, 0, 0));

			//��̃f�[�^���\�o���̃f�[�^�̏��𓾂�
			expressionData = face->QueryExpressions();
			if (expressionData != NULL)
			{
				//���̊J���
				if (expressionData->QueryExpression(PXCFaceData::ExpressionsData::EXPRESSION_MOUTH_OPEN, &expressionResult)){
					{
						std::stringstream ss;
						ss << "Mouth_Open:" << expressionResult.intensity;
						cv::putText(colorImage, ss.str(), cv::Point(faceRect.x, faceRect.y + faceRect.h + 15), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 255), 2, CV_AA);
					}
				}

				//��̏o���
				if (expressionData->QueryExpression(PXCFaceData::ExpressionsData::EXPRESSION_TONGUE_OUT, &expressionResult)){
					{
						std::stringstream ss;
						ss << "TONGUE_Out:" << expressionResult.intensity;
						cv::putText(colorImage, ss.str(), cv::Point(faceRect.x, faceRect.y + faceRect.h + 40), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 255), 2, CV_AA);
					}
				}

				//�Ί�̓x��
				if (expressionData->QueryExpression(PXCFaceData::ExpressionsData::EXPRESSION_SMILE, &expressionResult)){
					{
						std::stringstream ss;
						ss << "SMILE:" << expressionResult.intensity;
						cv::putText(colorImage, ss.str(), cv::Point(faceRect.x, faceRect.y + faceRect.h + 65), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 255), 2, CV_AA);
					}
				}

			}
			//�����܂ł��\�o��񌟏o�̋@�\
			//////////////////////////////////////

			//////////////////////////////////////
			//�ǉ��F�������炪�\��(Emotion)�F��

			//�ǉ��F����̃f�[�^�𓾂�
			emotionDet->QueryAllEmotionData(i, &arrData[0]);

			//�ǉ��F�\��(PRIMARY)�𐄒肷��
			int idx_outstanding_emotion = -1;		//�ŏI�I�Ɍ��肳���\��̒l
			bool IsSentimentPresent = false;			//�\��͂����肵�Ă��邩�ǂ���
			pxcI32 maxscoreE = -3; pxcF32 maxscoreI = 0;	//evidence,intencity��for���ł̍ő�l(�����l�͍ŏ��l)

			// arrData�Ɋi�[�������ׂĂ̕\��̃p�����[�^�ɂ��Č��Ă���
			for (int i = 0; i<NUM_PRIMARY_EMOTIONS; i++) {
				if (arrData[i].evidence < maxscoreE)  continue;	//�\��̌`��(evidence)���r
				if (arrData[i].intensity < maxscoreI) continue; //�\��̋���(intensity)���r
				//��̒l���A�Ƃ��ɍł��傫���ꍇ�̒l�֍X�V
				maxscoreE = arrData[i].evidence;
				maxscoreI = arrData[i].intensity;
				idx_outstanding_emotion = i;	
			}

			//�ǉ��F�\��(PRIMARY)�̕\��
			if (idx_outstanding_emotion != -1) {
				{
					std::stringstream ss;
					ss << "Emotion_PRIMARY:" << EmotionLabels[idx_outstanding_emotion];
					cv::putText(colorImage, ss.str(), cv::Point(faceRect.x, faceRect.y - 40), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2, CV_AA);
				}
			}

			//�\��̋���(intensity)������l�ȏ�̎��A�������Ɣ��f
			if (maxscoreI > 0.4){
				IsSentimentPresent = true;
			}

			//�ǉ��F����(Sentiment)�𐄒肷��
			//�\��(PRIMARY)�̐���Ɠ��l�Ȃ̂ŁA�R�����g�͏ȗ�
			if (IsSentimentPresent){
				int idx_sentiment_emotion = -1;
				maxscoreE = -3; maxscoreI = 0;
				for (int i = 0; i<(10 - NUM_PRIMARY_EMOTIONS); i++) {
					if (arrData[NUM_PRIMARY_EMOTIONS + i].evidence  < maxscoreE) continue;
					if (arrData[NUM_PRIMARY_EMOTIONS + i].intensity < maxscoreI) continue;
					maxscoreE = arrData[NUM_PRIMARY_EMOTIONS + i].evidence;
					maxscoreI = arrData[NUM_PRIMARY_EMOTIONS + i].intensity;
					idx_sentiment_emotion = i;
				}
				if (idx_sentiment_emotion != -1){
					{
						std::stringstream ss;
						ss << "Emo_SENTIMENT:" << SentimentLabels[idx_sentiment_emotion];
						cv::putText(colorImage, ss.str(), cv::Point(faceRect.x, faceRect.y - 15), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2, CV_AA);
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
	PXCEmotion* emotionDet = 0; //�ǉ��F�\��o�̌��ʂ��i�[���邽�߂̓��ꕨ

    const int COLOR_WIDTH = 640;
	const int COLOR_HEIGHT = 480;
    const int COLOR_FPS = 30;
	static const int NUM_TOTAL_EMOTIONS = 10;		//�ǉ��F�擾�ł���\���ъ���̂��ׂĂ̎�ނ̐�
	static const int NUM_PRIMARY_EMOTIONS = 7;		//�ǉ��F�\��̐�
	static const int NUM_SENTIMENT_EMOTIONS = 3;	//�ǉ��F����̐�

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
