#include "ofApp.h"

#include "pxccapture.h"
#include "pxchandmodule.h"
#include "pxchandconfiguration.h"
#include "pxchanddata.h"
#include "pxcsensemanager.h"

PXCSenseManager* senseManager = 0;
PXCHandModule* handAnalyzer = 0;
PXCHandData* handData = 0;

const int DEPTH_WIDTH = 640;
const int DEPTH_HEIGHT = 480;
const int DEPTH_FPS = 30;

int sides[2];
float openesses[2];
ofPoint centers[2];
ofColor circleColor;

void ofApp::initializeHandTracking()
{
	// ��̌��o����擾����
	handAnalyzer = senseManager->QueryHand();
	if (handAnalyzer == 0) {
		throw std::runtime_error("��̌��o��̎擾�Ɏ��s���܂���");
	}
	// ��̃f�[�^���쐬����
	handData = handAnalyzer->CreateOutput();
	if (handData == 0) {
		throw std::runtime_error("��̌��o��̍쐬�Ɏ��s���܂���");
	}
	PXCCapture::Device *device = senseManager->QueryCaptureManager()->QueryDevice();
	PXCCapture::DeviceInfo dinfo;
	device->QueryDeviceInfo(&dinfo);
	if (dinfo.model == PXCCapture::DEVICE_MODEL_IVCAM) {
		device->SetDepthConfidenceThreshold(1);
		//device->SetMirrorMode( PXCCapture::Device::MIRROR_MODE_DISABLED );
		device->SetIVCAMFilterOption(6);
	}
	// ��̌��o�̐ݒ�
	PXCHandConfiguration* config = handAnalyzer->CreateActiveConfiguration();
	config->EnableSegmentationImage(true);
	config->ApplyChanges();
	config->Update();
}

//--------------------------------------------------------------
void ofApp::setup(){
	ofBackground(0);
	ofSetFrameRate(60);

	// 1,000,000 particles
	unsigned w = 1000;
	unsigned h = 1000;

	particles.init(w, h);

	// initial positions
	// use new to allocate 4,000,000 floats on the heap rather than
	// the stack
	float* particlePosns = new float[w * h * 4];
	for (unsigned y = 0; y < h; ++y)
	{
		for (unsigned x = 0; x < w; ++x)
		{
			unsigned idx = y * w + x;
			particlePosns[idx * 4] = 400.f * x / (float)w - 200.f; // particle x
			particlePosns[idx * 4 + 1] = 400.f * y / (float)h - 200.f; // particle y
			particlePosns[idx * 4 + 2] = 0.f; // particle z
			particlePosns[idx * 4 + 3] = 0.f; // dummy
		}
	}
	particles.loadDataTexture(ofxGpuParticles::POSITION, particlePosns);
	delete[] particlePosns;

	// initial velocities
	particles.zeroDataTexture(ofxGpuParticles::VELOCITY);

	// listen for update event to set additonal update uniforms
	ofAddListener(particles.updateEvent, this, &ofApp::onParticlesUpdate);
	circleColor.r = 255;
	circleColor.g = 0;
	circleColor.b = 0;
	circleColor.a = 128;

	senseManager = PXCSenseManager::CreateInstance();
	if (senseManager == 0) {
		throw std::runtime_error("SenseManager�̐����Ɏ��s���܂���");
	}

	// Depth�X�g���[����L���ɂ���
	auto sts = senseManager->EnableStream(PXCCapture::StreamType::STREAM_TYPE_DEPTH,
		DEPTH_WIDTH, DEPTH_HEIGHT, DEPTH_FPS);
	if (sts < PXC_STATUS_NO_ERROR) {
		throw std::runtime_error("Depth�X�g���[���̗L�����Ɏ��s���܂���");
	}
	// ��̌��o��L���ɂ���
	sts = senseManager->EnableHand();
	if (sts < PXC_STATUS_NO_ERROR) {
		throw std::runtime_error("��̌��o�̗L�����Ɏ��s���܂���");
	}
	// �p�C�v���C��������������
	sts = senseManager->Init();
	if (sts < PXC_STATUS_NO_ERROR) {
		throw std::runtime_error("�p�C�v���C���̏������Ɏ��s���܂���");
	}

	initializeHandTracking();

}

// set any update uniforms in this function
void ofApp::onParticlesUpdate(ofShader& shader)
{
	// ��̈ʒu���V�F�[�_�[�̍��W�n�ɍ��킹�Čv�Z
	ofVec3f mouse(-1.0f * (centers[0].x * ofGetWidth() / DEPTH_WIDTH - .5f * ofGetWidth()), .5f * ofGetHeight() - (centers[0].y * ofGetHeight() / DEPTH_HEIGHT), 0.f);

	// �v�Z������̈ʒu���V�F�[�_�[�ɃZ�b�g
	shader.setUniform3fv("mouse", mouse.getPtr());
	shader.setUniform1f("elapsed", ofGetLastFrameTime());

	// ��̊J�l�Ńp�[�e�B�N���̏W�܂���ω�������
	float openForRad;
	if (openesses[0] < 10.0f){
		openForRad = 10.0f;
	}
	else if (openesses[0] > 100.0f){
		openForRad = 100.0f;
	}
	else{
		openForRad = openesses[0];
	}
	openForRad = 1.0f - (openForRad / 100.0f) + 0.01f;

	shader.setUniform1f("radiusSquared", 200.f * 200.f * 10.0f * openForRad);
}

void ofApp::updateHandFrame(){

	handData->Update();
	// ���o������̐����擾����
	auto numOfHands = handData->QueryNumberOfHands();
	for (int i = 0; i < numOfHands; i++) {
		// ����擾����
		pxcUID handID;
		PXCHandData::IHand* hand;
		auto sts = handData->QueryHandData(
			PXCHandData::AccessOrderType::ACCESS_ORDER_BY_ID, i, hand);
		if (sts < PXC_STATUS_NO_ERROR) {
			continue;
		}

		auto side = hand->QueryBodySide();
		auto openness = hand->QueryOpenness();
		auto center = hand->QueryMassCenterImage();

		if (i < 2){
			sides[i] = side;
			openesses[i] = openness;
			centers[i].x = center.x;
			centers[i].y = center.y;
		}
	}
}

//--------------------------------------------------------------
void ofApp::update(){

	pxcStatus sts = senseManager->AcquireFrame(false);
	if (sts < PXC_STATUS_NO_ERROR) {
		return;
	}
	// ��̃f�[�^���X�V����
	updateHandFrame();
	// �t���[�����������
	senseManager->ReleaseFrame();

	ofSetWindowTitle(ofToString(ofGetFrameRate(), 2));
	particles.update();
}

//--------------------------------------------------------------
void ofApp::draw(){
	cam.begin();
	ofEnableBlendMode(OF_BLENDMODE_ADD);
	particles.draw();
	ofDisableBlendMode();

	// �~�̈ʒu�̌v�Z
	ofVec3f handPoint(-1.0f * (centers[0].x * ofGetWidth() / DEPTH_WIDTH - .5f * ofGetWidth()), .5f * ofGetHeight() - (centers[0].y * ofGetHeight() / DEPTH_HEIGHT), 0.f);
	// �~�̐F�̎w��
	ofSetColor(circleColor);
	// �A�E�g���C���̂ݕ`��
	ofNoFill();
	// �~�̕`��
	ofCircle(handPoint.x, handPoint.y, 2.0f * openesses[0]);

	cam.end();
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){

}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}
