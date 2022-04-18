#include "ImporterModelTask.h"
#include "Asset\Material.h"
#include "Asset\StaticMesh.h"
#include "assimp\Importer.hpp"
#include "assimp\postprocess.h"
#include "assimp\scene.h"
#include "QQueue"
#include "Utils\FileUtils.h"
#include "..\SkeletonModel\SkeletonModel.h"
#include "..\SkeletonModel\Skeleton.h"
#include "..\GAssetMgr.h"

QMatrix4x4 converter(const aiMatrix4x4& aiMat4) {
	QMatrix4x4 mat4;
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			mat4(i, j) = aiMat4[i][j];
		}
	}
	return mat4;
}

QVector3D converter(const aiVector3D& aiVec3) {
	return QVector3D(aiVec3.x, aiVec3.y, aiVec3.z);
}

std::shared_ptr<Asset::StaticMesh> createStaticMeshFromAssimpMesh(aiMesh* mesh) {
	std::shared_ptr<Asset::StaticMesh> staticMesh = std::make_shared<Asset::StaticMesh>();
	QVector<Asset::StaticMesh::Vertex> vertices(mesh->mNumVertices);
	for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
		Asset::StaticMesh::Vertex& vertex = vertices[i];
		vertex.position = converter(mesh->mVertices[i]);
		if (mesh->mNormals)
			vertex.normal = converter(mesh->mNormals[i]);
		if (mesh->mTextureCoords[0]) {
			vertex.texCoord.setX(mesh->mTextureCoords[0][i].x);
			vertex.texCoord.setY(mesh->mTextureCoords[0][i].y);
		}
		if (mesh->mTangents)
			vertex.tangent = converter(mesh->mTangents[i]);
		if (mesh->mBitangents)
			vertex.bitangent = converter(mesh->mBitangents[i]);
	}
	staticMesh->setVertices(std::move(vertices));
	QVector<Asset::StaticMesh::Index> indices;
	for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
		aiFace face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++) {
			indices.push_back(face.mIndices[j]);
		}
	}
	staticMesh->setIndices(std::move(indices));
	return staticMesh;
}

std::shared_ptr<Asset::Material> createMaterialFromAssimpMaterial(aiMaterial* material, const aiScene* scene, QDir dir) {
	auto qMaterial = std::make_shared<Asset::Material>();
	int diffuseCount = material->GetTextureCount(aiTextureType_DIFFUSE);
	for (int j = 0; j < diffuseCount; j++) {
		aiString path;
		material->GetTexture(aiTextureType_DIFFUSE, j, &path);
		QString realPath = dir.filePath(path.C_Str());
		QImage image;
		if (QFile::exists(realPath)) {
			image.load(realPath);
		}
		else {
			const aiTexture* embTexture = scene->GetEmbeddedTexture(path.C_Str());
			if (embTexture->mHeight == 0) {
				image.loadFromData((uchar*)embTexture->pcData, embTexture->mWidth, embTexture->achFormatHint);
			}
			else {
				image = QImage((uchar*)embTexture->pcData, embTexture->mWidth, embTexture->mHeight, QImage::Format_ARGB32);
			}
		}
		qMaterial->addTextureSampler("Diffuse", image);
	}
	if (diffuseCount) {
		qMaterial->setShadingCode("FragColor = texture(Diffuse,vUV); ");
	}
	else {
		qMaterial->setShadingCode("FragColor = vec4(1.0); ");
	}
	return qMaterial;
}

std::shared_ptr<Asset::Skeleton::ModelNode> processSkeletonModelNode(aiNode* node) {
	std::shared_ptr<Asset::Skeleton::ModelNode> boneNode = std::make_shared<Asset::Skeleton::ModelNode>();
	boneNode->name = node->mName.C_Str();
	boneNode->localMatrix = converter(node->mTransformation);
	for (unsigned int i = 0; i < node->mNumChildren; i++) {
		boneNode->children.push_back(processSkeletonModelNode(node->mChildren[i]));
	}
	return boneNode;
}

void ImporterModelTask::executable() {
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(mFilePath.toUtf8().constData(), aiProcess_Triangulate | aiProcess_FlipUVs);
	if (!scene) {
		return;
	}

	QList<std::shared_ptr<Asset::Material>> mMaterialList;
	QStringList mMaterialPathList;
	for (uint i = 0; i < scene->mNumMaterials; i++) {
		aiMaterial* material = scene->mMaterials[i];
		auto qMaterial = createMaterialFromAssimpMaterial(material, scene, QFileInfo(mFilePath).dir());
		mMaterialList << qMaterial;
		QString vaildPath = FileUtils::getVaildPath(mDestDir.filePath(QString("%1.%2").arg(material->GetName().C_Str()).arg(qMaterial->getExtName())));;
		qMaterial->setRefPath(vaildPath);
		qMaterial->save(vaildPath);
		mMaterialPathList << qMaterial->getRefPath();
	}

	bool hasBone = false;

	QQueue<QPair<aiNode*, aiMatrix4x4>> qNode;
	qNode.push_back({ scene->mRootNode ,aiMatrix4x4() });
	while (!qNode.isEmpty()) {
		QPair<aiNode*, aiMatrix4x4> node = qNode.takeFirst();
		for (unsigned int i = 0; i < node.first->mNumMeshes; i++) {
			aiMesh* mesh = scene->mMeshes[node.first->mMeshes[i]];
			std::shared_ptr<Asset::StaticMesh> staticMesh = createStaticMeshFromAssimpMesh(mesh);
			staticMesh->setMaterialPath(mMaterialList[mesh->mMaterialIndex]->getRefPath());
			QString vaildPath = FileUtils::getVaildPath(mDestDir.filePath(QString("%1.%2").arg(mesh->mName.C_Str()).arg(staticMesh->getExtName())));
			staticMesh->setRefPath(vaildPath);
			staticMesh->save(vaildPath);

			if (mesh->HasBones()) {
				hasBone = true;
			}
		}
		for (unsigned int i = 0; i < node.first->mNumChildren; i++) {
			qNode.push_back({ node.first->mChildren[i] ,node.second * node.first->mChildren[i]->mTransformation });
		}
	}

	if (!hasBone) 
		return;

	QVector<Asset::SkeletonModel::Mesh> mMeshList;
	std::shared_ptr<Asset::Skeleton::ModelNode> mRootNode = processSkeletonModelNode(scene->mRootNode);
	QHash<QString, std::shared_ptr<Asset::Skeleton::BoneNode>> mBoneMap;
	QVector<Asset::Skeleton::Mat4> mBoneOffsetMatrix;
	QVector<Asset::Skeleton::Mat4> mCurrentPosesMatrix;

	qNode.push_back({ scene->mRootNode ,aiMatrix4x4() });
	while (!qNode.isEmpty()) {
		QPair<aiNode*, aiMatrix4x4> node = qNode.takeFirst();
		for (unsigned int i = 0; i < node.first->mNumMeshes; i++) {
			aiMesh* mesh = scene->mMeshes[node.first->mMeshes[i]];
			Asset::SkeletonModel::Mesh mMesh;
			mMesh.mVertices.resize(mesh->mNumVertices);
			for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
				Asset::SkeletonModel::Vertex& vertex = mMesh.mVertices[i];
				vertex.position = converter(mesh->mVertices[i]);
				if (mesh->mNormals)
					vertex.normal = converter(mesh->mNormals[i]);
				if (mesh->mTextureCoords[0]) {
					vertex.texCoord.setX(mesh->mTextureCoords[0][i].x);
					vertex.texCoord.setY(mesh->mTextureCoords[0][i].y);
				}
				if (mesh->mTangents)
					vertex.tangent = converter(mesh->mTangents[i]);
				if (mesh->mBitangents)
					vertex.bitangent = converter(mesh->mBitangents[i]);
			}

			for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
				aiFace face = mesh->mFaces[i];
				for (unsigned int j = 0; j < face.mNumIndices; j++) {
					mMesh.mIndices.push_back(face.mIndices[j]);
				}
			}

			for (unsigned int i = 0; i < mesh->mNumBones; i++) {
				for (unsigned int j = 0; j < mesh->mBones[i]->mNumWeights; j++) {
					int vertexId = mesh->mBones[i]->mWeights[j].mVertexId;
					aiBone* bone = mesh->mBones[i];
					std::shared_ptr<Asset::Skeleton::BoneNode> boneNode;
					if (mBoneMap.contains(bone->mName.C_Str()))
						boneNode =  mBoneMap[bone->mName.C_Str()];
					else {
						boneNode = std::make_shared<Asset::Skeleton::BoneNode>();
						boneNode->name = bone->mName.C_Str();
						boneNode->offsetMatrix = converter(bone->mOffsetMatrix);
						boneNode->index = mBoneOffsetMatrix.size();
						mBoneOffsetMatrix << boneNode->offsetMatrix.toGenericMatrix<4, 4>();
						mBoneMap[boneNode->name] = boneNode;
					}
					int slot = 0;
					while (slot < std::size(mMesh.mVertices[vertexId].boneIndex) && (mMesh.mVertices[vertexId].boneWeight[slot] > 0.000001)) {
						slot++;
					}
					if (slot < std::size(mMesh.mVertices[vertexId].boneIndex)) {
						mMesh.mVertices[vertexId].boneIndex[slot] = boneNode->index;
						mMesh.mVertices[vertexId].boneWeight[slot] = mesh->mBones[i]->mWeights[j].mWeight;;
					}
					else {
						qWarning("Lack of slot");
					}
				}
			}
			mMesh.mMaterialIndex = mesh->mMaterialIndex;
			mMeshList.emplace_back(std::move(mMesh));
		}
		for (unsigned int i = 0; i < node.first->mNumChildren; i++) {
			qNode.push_back({ node.first->mChildren[i] ,node.second * node.first->mChildren[i]->mTransformation });
		}
	}

	std::shared_ptr<Asset::Skeleton> mSkeleton = std::make_shared<Asset::Skeleton>();
	mSkeleton->setBoneMap(std::move(mBoneMap));
	mSkeleton->setBoneOffsetMatrix(std::move(mBoneOffsetMatrix));
	mSkeleton->setRootNode(mRootNode);
	mSkeleton->setRefPath(mDestDir.filePath("%1.%2").arg(QFileInfo(mFilePath).baseName()).arg(mSkeleton->getExtName()));
	mSkeleton->save(mSkeleton->getRefPath());

	std::shared_ptr<Asset::SkeletonModel> mSkeletonModel = std::make_shared<Asset::SkeletonModel>();
	mSkeletonModel->setMeshList(std::move(mMeshList));
	mSkeletonModel->setSkeletonPath(mSkeleton->getRefPath());
	mSkeletonModel->setRefPath(mDestDir.filePath("%1.%2").arg(QFileInfo(mFilePath).baseName()).arg(mSkeletonModel->getExtName()));
	mSkeletonModel->setMaterialPathList(std::move(mMaterialPathList));
	mSkeletonModel->save(mSkeletonModel->getRefPath());
}
