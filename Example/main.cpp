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

const int CUBE_MAT_SIZE = 10;
const int CUBE_MAT_SPACING = 5;

class MyGame :public QEngine {
public:
	std::shared_ptr<QCameraComponent> mCamera;
	std::shared_ptr<QSkyBoxComponent> mSkyBox;
	std::shared_ptr<QCube> mCube[CUBE_MAT_SIZE][CUBE_MAT_SIZE];
	std::shared_ptr<QStaticModel> mStaticModel;
	std::shared_ptr<QParticleComponent> mGPUParticles;
	std::shared_ptr<QSphere> mSphere;
	std::shared_ptr<QText2D> mText;
	std::shared_ptr<QMaterial> mMaterial;

	QRandomGenerator rand;

	MyGame(int argc, char** argv)
		: QEngine(argc, argv) {
		mCamera = std::make_shared<QCameraComponent>();
		mCamera->setupWindow(window().get());		//������봰�ڰ󶨣�ʹ��WASD Shift �ո�ɽ����ƶ�����������ס���ڿɵ����ӽ�
		scene()->setCamera(mCamera);				//���ó������

		mSkyBox = std::make_shared<QSkyBoxComponent>();
		mSkyBox->setSkyBoxImage(QImage(PROJECT_SOURCE_DIR"/sky.jpeg"));
		scene()->setSkyBox(mSkyBox);

		mGPUParticles = std::make_shared<QParticleComponent>();
		mGPUParticles->updater().addParamVec3("force", QVector3D(0, 0.098, 0));			//Ϊ�������һ�����ϵ����������������ӵ��˶�����
		mGPUParticles->updater().setUpdateCode(R"(
			particle.position = particle.position + particle.velocity;
			particle.velocity = particle.velocity + UBO.force;
			particle.scaling  = particle.scaling;
			particle.rotation = particle.rotation;
		)");

		mGPUParticles->setLifetime(2);													//�������Ӵ��ʱ��
		mGPUParticles->setStaticMesh(std::make_shared<QSphere>());						//�������ӵ���״��ʵ����
		mGPUParticles->getStaticMesh()->setScale(QVector3D(0.05, 0.05, 0.05));
		scene()->addPrimitive(mGPUParticles);

		for (int i = 0; i < CUBE_MAT_SIZE; i++) {
			for (int j = 0; j < CUBE_MAT_SIZE; j++) {
				std::shared_ptr<QCube>& cube = mCube[i][j];
				cube.reset(new QCube);
				cube->setPosition(QVector3D((i - CUBE_MAT_SIZE / 2.0) * CUBE_MAT_SPACING, (j - CUBE_MAT_SIZE / 2.0) * CUBE_MAT_SPACING, -10));
				scene()->addPrimitive(cube);
			}
		}

		mText = std::make_shared<QText2D>("GPU Particles");
		mMaterial = std::make_shared<QMaterial>();
		mMaterial->addParamVec3("BaseColor", QVector3D(0.1, 0.5, 0.9));					//���ò��ʲ���
		mMaterial->setShadingCode("FragColor = vec4(UBO.BaseColor,1);");				//���ò��ʵ�Shading����
		mText->setMaterial(mMaterial);

		mText->setPosition(QVector3D(0, -4, 0));
		mText->setRotation(QVector3D(0, 180, 0));
		scene()->addPrimitive(mText);

		mStaticModel = std::make_shared<QStaticModel>();
		mStaticModel->loadFromFile(PROJECT_SOURCE_DIR"/Genji/Genji.FBX");
		mStaticModel->setRotation(QVector3D(-90, 0, 0));
		scene()->addPrimitive(mStaticModel);
	}
protected:
	void onGameLoop() override
	{
		float time = QTime::currentTime().msecsSinceStartOfDay();

		mMaterial->setParam<QVector3D>("BaseColor", QVector3D(0.1, 0.5, 0.9) * (sin(time / 1000) * 5 + 5));		//���ò�����ɫ
		mText->setScale(QVector3D(1, 1, 1) * (5 + 4 * sin(time / 1000)));										//RGB���ֵ����1.0����BloomЧ��

		QVector<QParticleComponent::Particle> particles(100);													//ÿ֡����100�����ӣ������ɶ�����෢����
		for (auto& particle : particles) {																		//ͨ��QParticleComponent::setUpdater������GPU���ӵ��˶����룬�Ӷ�ʵ�ָ�����������Ч��
			particle.position = QVector3D(rand.bounded(-10000, 10000) / 100.0, -200 + rand.bounded(-10000, 10000) / 100.0, rand.bounded(-10000, 10000) / 100.0).toVector4D();
			particle.velocity = QVector3D(rand.bounded(-10000, 10000) / 100.0, 0.1, 0);
		}
		mGPUParticles->createPartilces(particles);
	}
};

int main(int argc, char** argv) {
	MyGame game(argc, argv);
	game.execGame();
}