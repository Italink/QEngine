#include "QCube.h"

QCube::QCube()
{
	setTopology(QPrimitiveComponent::Topology::Triangles);

	QVector4D defaultColor = getDefaultBaseColorVec4();

	QVector<Vertex> getVertices = {
		//position					normal		tangent		bitangent   texCoord			baseColor
		  Vertex{QVector3D(-1.0f,-1.0f,-1.0f),QVector3D(),QVector3D(),QVector3D(),QVector2D(0.0f, 1.0f),defaultColor},
		  Vertex{QVector3D(-1.0f,-1.0f, 1.0f),QVector3D(),QVector3D(),QVector3D(),QVector2D(1.0f, 1.0f),defaultColor},
		  Vertex{QVector3D(-1.0f, 1.0f, 1.0f),QVector3D(),QVector3D(),QVector3D(),QVector2D(1.0f, 0.0f),defaultColor},
		  Vertex{QVector3D(-1.0f, 1.0f, 1.0f),QVector3D(),QVector3D(),QVector3D(),QVector2D(1.0f, 0.0f),defaultColor},
		  Vertex{QVector3D(-1.0f, 1.0f,-1.0f),QVector3D(),QVector3D(),QVector3D(),QVector2D(0.0f, 0.0f),defaultColor},
		  Vertex{QVector3D(-1.0f,-1.0f,-1.0f),QVector3D(),QVector3D(),QVector3D(),QVector2D(0.0f, 1.0f),defaultColor},

		  Vertex{QVector3D(-1.0f,-1.0f,-1.0f),QVector3D(),QVector3D(),QVector3D(),QVector2D(1.0f, 1.0f),defaultColor},
		  Vertex{QVector3D(1.0f, 1.0f,-1.0f), QVector3D(),QVector3D(),QVector3D(),QVector2D(0.0f, 0.0f),defaultColor},
		  Vertex{QVector3D(1.0f,-1.0f,-1.0f), QVector3D(),QVector3D(),QVector3D(),QVector2D(0.0f, 1.0f),defaultColor},
		  Vertex{QVector3D(-1.0f,-1.0f,-1.0f),QVector3D(),QVector3D(),QVector3D(),QVector2D(1.0f, 1.0f),defaultColor},
		  Vertex{QVector3D(-1.0f, 1.0f,-1.0f),QVector3D(),QVector3D(),QVector3D(),QVector2D(1.0f, 0.0f),defaultColor},
		  Vertex{QVector3D(1.0f, 1.0f,-1.0f), QVector3D(),QVector3D(),QVector3D(),QVector2D(0.0f, 0.0f),defaultColor},

		  Vertex{QVector3D(-1.0f,-1.0f,-1.0f),QVector3D(),QVector3D(),QVector3D(),QVector2D(1.0f, 0.0f),defaultColor},
		  Vertex{QVector3D(1.0f,-1.0f,-1.0f), QVector3D(),QVector3D(),QVector3D(),QVector2D(1.0f, 1.0f),defaultColor},
		  Vertex{QVector3D(1.0f,-1.0f, 1.0f), QVector3D(),QVector3D(),QVector3D(),QVector2D(0.0f, 1.0f),defaultColor},
		  Vertex{QVector3D(-1.0f,-1.0f,-1.0f),QVector3D(),QVector3D(),QVector3D(),QVector2D(1.0f, 0.0f),defaultColor},
		  Vertex{QVector3D(1.0f,-1.0f, 1.0f), QVector3D(),QVector3D(),QVector3D(),QVector2D(0.0f, 1.0f),defaultColor},
		  Vertex{QVector3D(-1.0f,-1.0f, 1.0f),QVector3D(),QVector3D(),QVector3D(),QVector2D(0.0f, 0.0f),defaultColor},

		  Vertex{QVector3D(-1.0f, 1.0f,-1.0f),QVector3D(),QVector3D(),QVector3D(),QVector2D(1.0f, 0.0f),defaultColor},
		  Vertex{QVector3D(-1.0f, 1.0f, 1.0f),QVector3D(),QVector3D(),QVector3D(),QVector2D(0.0f, 0.0f),defaultColor},
		  Vertex{QVector3D(1.0f, 1.0f, 1.0f), QVector3D(),QVector3D(),QVector3D(),QVector2D(0.0f, 1.0f),defaultColor},
		  Vertex{QVector3D(-1.0f, 1.0f,-1.0f),QVector3D(),QVector3D(),QVector3D(),QVector2D(1.0f, 0.0f),defaultColor},
		  Vertex{QVector3D(1.0f, 1.0f, 1.0f), QVector3D(),QVector3D(),QVector3D(),QVector2D(0.0f, 1.0f),defaultColor},
		  Vertex{QVector3D(1.0f, 1.0f,-1.0f), QVector3D(),QVector3D(),QVector3D(),QVector2D(1.0f, 1.0f),defaultColor},

		  Vertex{QVector3D(1.0f, 1.0f,-1.0f),QVector3D(),QVector3D(),QVector3D(),QVector2D(1.0f, 0.0f),defaultColor},
		  Vertex{QVector3D(1.0f, 1.0f, 1.0f),QVector3D(),QVector3D(),QVector3D(),QVector2D(0.0f, 0.0f),defaultColor},
		  Vertex{QVector3D(1.0f,-1.0f, 1.0f),QVector3D(),QVector3D(),QVector3D(),QVector2D(0.0f, 1.0f),defaultColor},
		  Vertex{QVector3D(1.0f,-1.0f, 1.0f),QVector3D(),QVector3D(),QVector3D(),QVector2D(0.0f, 1.0f),defaultColor},
		  Vertex{QVector3D(1.0f,-1.0f,-1.0f),QVector3D(),QVector3D(),QVector3D(),QVector2D(1.0f, 1.0f),defaultColor},
		  Vertex{QVector3D(1.0f, 1.0f,-1.0f),QVector3D(),QVector3D(),QVector3D(),QVector2D(1.0f, 0.0f),defaultColor},

		  Vertex{QVector3D(-1.0f, 1.0f, 1.0f),QVector3D(),QVector3D(),QVector3D(),QVector2D(0.0f, 0.0f),defaultColor},
		  Vertex{QVector3D(-1.0f,-1.0f, 1.0f),QVector3D(),QVector3D(),QVector3D(),QVector2D(0.0f, 1.0f),defaultColor},
		  Vertex{QVector3D(1.0f, 1.0f, 1.0f), QVector3D(),QVector3D(),QVector3D(),QVector2D(1.0f, 0.0f),defaultColor},
		  Vertex{QVector3D(-1.0f,-1.0f, 1.0f),QVector3D(),QVector3D(),QVector3D(),QVector2D(0.0f, 1.0f),defaultColor},
		  Vertex{QVector3D(1.0f,-1.0f, 1.0f), QVector3D(),QVector3D(),QVector3D(),QVector2D(1.0f, 1.0f),defaultColor},
		  Vertex{QVector3D(1.0f, 1.0f, 1.0f), QVector3D(),QVector3D(),QVector3D(),QVector2D(1.0f, 0.0f),defaultColor},
	};
	setVertices(getVertices);
}