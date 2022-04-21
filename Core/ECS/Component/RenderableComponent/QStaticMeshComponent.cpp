#include "QStaticMeshComponent.h"
#include "ECS\System\RenderSystem\QRenderSystem.h"
#include "ECS\QEntity.h"
#include "Asset\GAssetMgr.h"

void QStaticMeshComponent::setStaticMesh(std::shared_ptr<Asset::StaticMesh> val) {
	mStaticMesh = val;
	if (mStaticMesh) {
		setMaterial(TheAssetMgr->load<Asset::Material>(mStaticMesh->getMaterialPath()));
	}
	bNeedRecreatePipeline.active();
	bNeedRecreateResource.active();
}

void QStaticMeshComponent::setMaterial(std::shared_ptr<Asset::Material> val) {
	if (mMaterial) {
		mMaterial->removeRef(this);
	}
	mMaterial = val;
	if (val&&mStaticMesh) {
		mStaticMesh->setMaterialPath(val->getRefPath());
		val->addRef(this);
	}
	bNeedRecreatePipeline.active();
}

void QStaticMeshComponent::recreateResource() {
	if (!mStaticMesh || !mMaterial)
		return;
	mUniformBuffer.reset(RHI->newBuffer(QRhiBuffer::Type::Dynamic, QRhiBuffer::UniformBuffer, sizeof(QMatrix4x4)));
	mUniformBuffer->create();

	mVertexBuffer.reset(RHI->newBuffer(QRhiBuffer::Type::Static, QRhiBuffer::VertexBuffer, sizeof(Asset::StaticMesh::Vertex) * mStaticMesh->getVertices().size()));
	mVertexBuffer->create();

	mIndexBuffer.reset(RHI->newBuffer(QRhiBuffer::Type::Static, QRhiBuffer::IndexBuffer, sizeof(Asset::StaticMesh::Index) * mStaticMesh->getIndices().size()));
	mIndexBuffer->create();
}

void QStaticMeshComponent::recreatePipeline() {
	if (!mStaticMesh||!mMaterial)
		return;
	if (mStaticMesh->getVertices().size() == 0) {
		return;
	}
	QVector<QRhiVertexInputBinding> inputBindings;
	inputBindings << QRhiVertexInputBinding{ sizeof(Asset::StaticMesh::Vertex) };
	QVector<QRhiVertexInputAttribute> attributeList;
	attributeList << QRhiVertexInputAttribute{ 0, 0, QRhiVertexInputAttribute::Float3, offsetof(Asset::StaticMesh::Vertex, position) };
	attributeList << QRhiVertexInputAttribute{ 0, 1, QRhiVertexInputAttribute::Float3, offsetof(Asset::StaticMesh::Vertex,normal) };
	attributeList << QRhiVertexInputAttribute{ 0, 2, QRhiVertexInputAttribute::Float3, offsetof(Asset::StaticMesh::Vertex,tangent) };
	attributeList << QRhiVertexInputAttribute{ 0, 3, QRhiVertexInputAttribute::Float3, offsetof(Asset::StaticMesh::Vertex,bitangent) };
	attributeList << QRhiVertexInputAttribute{ 0, 4, QRhiVertexInputAttribute::Float3, offsetof(Asset::StaticMesh::Vertex,texCoord) };

	QString vertexShaderCode = R"(#version 440
	layout(location = 0) in vec3 inPosition;
	layout(location = 1) in vec3 inNormal;
	layout(location = 2) in vec3 inTangent;
	layout(location = 3) in vec3 inBitangent;
	layout(location = 4) in vec2 inUV;

	layout(location = 0) out vec2 vUV;

	out gl_PerVertex{
		vec4 gl_Position;
	};

	layout(std140,binding = 0) uniform buf{
		mat4 mvp;
	}ubuf;
	void main(){
		vUV = inUV;
		gl_Position = ubuf.mvp * vec4(inPosition,1.0f);
	}
	)";

	QRhiVertexInputLayout inputLayout;
	inputLayout.setBindings(inputBindings.begin(), inputBindings.end());
	inputLayout.setAttributes(attributeList.begin(), attributeList.end());
	QShader vs = QRenderSystem::createShaderFromCode(QShader::Stage::VertexStage, vertexShaderCode.toLocal8Bit());
	if (!vs.isValid()) {
		mPipeline.reset(nullptr);
		return;
	}
	const QRhiUniformProxy::UniformInfo& materialInfo = mMaterial->getProxy()->getUniformInfo(1);
	QString defineCode = materialInfo.uniformDefineCode;

	QString outputCode = mMaterial->getShadingCode();

	if (TheRenderSystem->isEnableDebug()) {
		defineCode.prepend("layout (location = 1) out vec4 CompId;\n");
		outputCode.append(QString("CompId = %1;\n").arg(getEntityIdVec4String()));
	}

	QString fragShaderCode = QString(R"(#version 440
	layout(location = 0) in vec2 vUV;
	layout(location = 0) out vec4 FragColor;
	%1
	void main(){
	    %2
	}
	)").arg(defineCode).arg(outputCode);


	QShader fs = QRenderSystem::createShaderFromCode(QShader::Stage::FragmentStage, fragShaderCode.toLocal8Bit());
	if (!fs.isValid()) {
		mPipeline.reset(nullptr);
		return;
	}

	mPipeline.reset(RHI->newGraphicsPipeline());
	
	mPipeline->setVertexInputLayout(inputLayout);
	const auto& blendStates = TheRenderSystem->getSceneBlendStates();
	mPipeline->setTargetBlends(blendStates.begin(), blendStates.end());
	mPipeline->setTopology(QRhiGraphicsPipeline::Topology::Triangles);
	mPipeline->setDepthOp(QRhiGraphicsPipeline::LessOrEqual);
	mPipeline->setDepthTest(true);
	mPipeline->setDepthWrite(true);
	mPipeline->setSampleCount(TheRenderSystem->getSceneSampleCount());

	mPipeline->setShaderStages({
		{ QRhiShaderStage::Vertex, vs },
		{ QRhiShaderStage::Fragment, fs }
							   });
	mShaderResourceBindings.reset(RHI->newShaderResourceBindings());
	QVector<QRhiShaderResourceBinding> shaderBindings;
	shaderBindings << QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, mUniformBuffer.get());
	shaderBindings << materialInfo.bindings;
	mShaderResourceBindings->setBindings(shaderBindings.begin(), shaderBindings.end());
	mShaderResourceBindings->create();

	mPipeline->setShaderResourceBindings(mShaderResourceBindings.get());

	mPipeline->setRenderPassDescriptor(TheRenderSystem->getSceneRenderPassDescriptor());

	mPipeline->create();
}

void QStaticMeshComponent::uploadResource(QRhiResourceUpdateBatch* batch) {
	if (!mStaticMesh || !mMaterial)
		return;
	batch->uploadStaticBuffer(mVertexBuffer.get(), mStaticMesh->getVertices().constData());
	batch->uploadStaticBuffer(mIndexBuffer.get(), mStaticMesh->getIndices().constData());
}

void QStaticMeshComponent::updatePrePass(QRhiCommandBuffer* cmdBuffer) {
}

void QStaticMeshComponent::updateResourcePrePass(QRhiResourceUpdateBatch* batch) {
	if (!mStaticMesh || !mMaterial)
		return;
	mMaterial->getProxy()->updateResource(batch);
	QMatrix4x4 MVP = mEntity->calculateMatrixMVP();
	batch->updateDynamicBuffer(mUniformBuffer.get(), 0, sizeof(QMatrix4x4), MVP.constData());
}

void QStaticMeshComponent::renderInPass(QRhiCommandBuffer* cmdBuffer, const QRhiViewport& viewport) {
	if (!mPipeline)
		return;
	cmdBuffer->setGraphicsPipeline(mPipeline.get());
	cmdBuffer->setViewport(viewport);
	cmdBuffer->setShaderResources();
	const QRhiCommandBuffer::VertexInput VertexInput(mVertexBuffer.get(), 0);
	cmdBuffer->setVertexInput(0, 1, &VertexInput, mIndexBuffer.get(), 0, QRhiCommandBuffer::IndexUInt32);
	cmdBuffer->drawIndexed(mStaticMesh->getIndices().size());
}

