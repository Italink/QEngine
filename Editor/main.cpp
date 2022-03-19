#include "Window\EditorWindow\EditorWindow.h"
#include <QGuiApplication>
#include "QDateTime"
#include "QEngine.h"
#include "qrandom.h"
#include "Scene\Component\Camera\QCameraComponent.h"
#include "Scene\Component\Light\QLightComponent.h"
#include "Scene\Component\Particle\QParticleComponent.h"
#include "Scene\Component\SkyBox\QSkyBoxComponent.h"
#include "Scene\Component\StaticMesh\QCube.h"
#include "Scene\Component\StaticMesh\QSphere.h"
#include "Scene\Component\StaticMesh\QStaticModel.h"
#include "Scene\Component\StaticMesh\QText2D.h"
#include "Scene\Component\SkeletonMesh\QSkeletonMeshComponent.h"
#include "Scene\Component\AssimpToolkit\MMDVmdParser.h"
#include "Scene\Component\SkeletonMesh\QMMDModel.h"
#include "Window\MaterialEditor\QMaterialEditor.h"
#include "Renderer\Common\QDebugPainter.h"

const int CUBE_MAT_SIZE = 10;
const int CUBE_MAT_SPACING = 5;

class MyGame :public QEngine {
public:
	std::shared_ptr<QCameraComponent> mCamera;
	std::shared_ptr<QSkyBoxComponent> mSkyBox;
	std::shared_ptr<QCube> mCube[CUBE_MAT_SIZE][CUBE_MAT_SIZE];
	std::shared_ptr<QStaticModel> mStaticModel;
	std::shared_ptr<QSkeletonModelComponent> mSkeletonModel;
	std::shared_ptr<QParticleComponent> mGPUParticles;
	std::shared_ptr<QSphere> mSphere;
	std::shared_ptr<QText2D> mText;
	std::shared_ptr<QMaterial> mMaterial;

	QRandomGenerator rand;

	MyGame(int argc, char** argv)
		: QEngine(argc, argv, true) {
	}
	virtual void init() override {
		mCamera = std::make_shared<QCameraComponent>();
		mCamera->setupWindow(window().get());		//������봰�ڰ󶨣�ʹ��WASD Shift �ո�ɽ����ƶ�����������ס���ڿɵ����ӽ�
		scene()->setCamera(mCamera);				//���ó������

		mSkyBox = std::make_shared<QSkyBoxComponent>();
		mSkyBox->setSkyBoxImage(QImage(ASSET_DIR"/sky.jpeg"));
		scene()->setSkyBox(mSkyBox);

		for (int i = 0; i < CUBE_MAT_SIZE; i++) {
			for (int j = 0; j < CUBE_MAT_SIZE; j++) {
				std::shared_ptr<QCube>& cube = mCube[i][j];
				cube.reset(new QCube);
				cube->setPosition(QVector3D((i - CUBE_MAT_SIZE / 2.0) * CUBE_MAT_SPACING, (j - CUBE_MAT_SIZE / 2.0) * CUBE_MAT_SPACING, -10));
				scene()->addPrimitive(QString("cube(%1,%2)").arg(i).arg(j), cube);
			}
		}

		mStaticModel = std::make_shared<QStaticModel>();
		mStaticModel->loadFromFile(ASSET_DIR"/Model/FBX/Genji/Genji.FBX");
		mStaticModel->setRotation(QVector3D(-90, 0, 0));
		scene()->addPrimitive("StaticModel", mStaticModel);

		mText = std::make_shared<QText2D>("GPU Particles");
		mText->setPosition(QVector3D(0, -5, 0));
		mText->setRotation(QVector3D(0, 180, 0));

		mMaterial = std::make_shared<QMaterial>();
		mMaterial->addDataVec3("BaseColor", QVector3D(0.1, 0.5, 0.9));					//���ò��ʲ���
		mMaterial->setShadingCode("FragColor = vec4(UBO.BaseColor,1);");				//���ò��ʵ�Shading����
		mText->setMaterial(mMaterial);

		scene()->addPrimitive("Text", mText);

		mGPUParticles = std::make_shared<QParticleComponent>();
		mGPUParticles->setPosition(QVector3D(0, -15, 0));
		scene()->addPrimitive("GPU Particles", mGPUParticles);
	}
protected:
	void update() override
	{
	}
};
int main(int argc, char** argv) {
	MyGame engine(argc, argv);
	EditorWindow::preInitConfig();
	EditorWindow w;
	w.show();
	engine.execGame();
}