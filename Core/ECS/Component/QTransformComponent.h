#ifndef QTransformComponent_h__
#define QTransformComponent_h__

#include "qvectornd.h"
#include "IComponent.h"
#include "QMatrix4x4"

class QTransformComponent:public IComponent {
	Q_OBJECT
		Q_PROPERTY(QVector3D Position READ getPosition WRITE setPosition)
		Q_PROPERTY(QVector3D Rotation READ getRotation WRITE setRotation)
		Q_PROPERTY(QVector3D Scale READ getScale WRITE setScale)
public:
	QVector3D getPosition() const { return mPosition; }
	void setPosition(QVector3D val) { mPosition = val; }

	QVector3D getRotation() const { return mRotation; }
	void setRotation(QVector3D val) { mRotation = val; }

	QVector3D getScale() const { return mScale; }
	void setScale(QVector3D val) { mScale = val; }

	void setMatrix(QMatrix4x4 matrix);

	QMatrix4x4 calculateMatrix();
private:
	QVector3D mPosition = QVector3D(0.0f, 0.0f, 0.0f);
	QVector3D mRotation = QVector3D(0.0f, 0.0f, 0.0f);
	QVector3D mScale = QVector3D(1.0f, 1.0f, 1.0f);
};

#endif // QTransformComponent_h__
