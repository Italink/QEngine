#ifndef QPrimitiveComponent_h__
#define QPrimitiveComponent_h__

#include "Render/Scene/QSceneComponent.h"

const uint32_t QMATERIAL_DIFFUSE = 0;

typedef QHash<uint32_t, QImage> QMaterial;

class QPrimitiveComponent :public QVisibleComponent {
	Q_OBJECT
public:
	struct Vertex {
		QVector3D position;
		QVector3D normal;
		QVector3D tangent;
		QVector3D bitangent;
		QVector2D texCoord;
		QVector4D baseColor;
	};

	enum Topology {		//ͼԪ���˽ṹ
		Triangles,
		TriangleStrip,
		TriangleFan,
		Lines,
		LineStrip,
		Points
	};
	enum Type {
		Static,			//�������ݲ������
		Dynamic			//�������ݽ�û֡������
	};

private:
	Topology mTopology;
	QVector<Vertex> mVertices;
	QVector<uint32_t> mIndices;
};

#endif // QPrimitiveComponent_h__
