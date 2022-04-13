#include "QSkyBoxComponent.h"
#include "ECS\System\RenderSystem\QRenderSystem.h"
#include "ECS\QEntity.h"

static float cubeData[] = { // Y up, front = CCW
		// positions
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		-1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f
};

void QSkyBoxComponent::setSkyBox(std::shared_ptr<Asset::SkyBox> val) {
	mSkyBox = val;
	bNeedRecreatePipeline.active();
	bNeedRecreateResource.active();
}

void QSkyBoxComponent::recreateResource() {
	mTexture.reset(RHI->newTexture(QRhiTexture::RGBA8, mSkyBox->getImageList().front().size(), 1,
				   QRhiTexture::CubeMap
				   | QRhiTexture::MipMapped
				   | QRhiTexture::UsedWithGenerateMips));
	mTexture->create();
	mSampler.reset(RHI->newSampler(QRhiSampler::Linear,
				   QRhiSampler::Linear,
				   QRhiSampler::None,
				   QRhiSampler::Repeat,
				   QRhiSampler::Repeat));
	mSampler->create();

	mUniformBuffer.reset(RHI->newBuffer(QRhiBuffer::Type::Dynamic, QRhiBuffer::UniformBuffer, sizeof(QMatrix4x4)));
	mUniformBuffer->create();

	mVertexBuffer.reset(RHI->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(cubeData)));
	mVertexBuffer->create();
}

void QSkyBoxComponent::recreatePipeline() {
	mPipeline.reset(RHI->newGraphicsPipeline());

	const auto& blendStates = QRenderSystem::instance()->getSceneBlendStates();
	mPipeline->setTargetBlends(blendStates.begin(), blendStates.end());

	mPipeline->setTopology(QRhiGraphicsPipeline::Triangles);
	mPipeline->setDepthTest(true);
	mPipeline->setDepthWrite(true);
	mPipeline->setSampleCount(QRenderSystem::instance()->getSceneSampleCount());

	QVector<QRhiVertexInputBinding> inputBindings;
	inputBindings << QRhiVertexInputBinding{ sizeof(float) * 3 };
	QVector<QRhiVertexInputAttribute> attributeList;
	attributeList << QRhiVertexInputAttribute{ 0, 0, QRhiVertexInputAttribute::Float3, 0 };
	QRhiVertexInputLayout inputLayout;
	inputLayout.setBindings(inputBindings.begin(), inputBindings.end());
	inputLayout.setAttributes(attributeList.begin(), attributeList.end());
	mPipeline->setVertexInputLayout(inputLayout);

	QString vertexShaderCode = R"(#version 440
	layout(location = 0) in vec3 inPosition;
	layout(location = 0) out vec3 vPosition
;
	out gl_PerVertex{
		vec4 gl_Position;
	};

	layout(std140 , binding = 0) uniform buf{
		mat4 mvp;
	}ubuf;

	void main(){
		vPosition = inPosition;
		gl_Position = ubuf.mvp * vec4(inPosition,1.0f);
	}
	)";

	QShader vs = QRenderSystem::createShaderFromCode(QShader::Stage::VertexStage, vertexShaderCode.toLocal8Bit());

	QString fragShaderCode = QString(R"(#version 440
	layout(location = 0) in vec3 vPosition;
	layout(location = 0) out vec4 outColor;
	%1
	layout(binding = 1) uniform samplerCube uSkybox;
	void main(){
		outColor = texture(uSkybox,vPosition);
		%2
	}
	)");
	//if (mRenderPass->getEnableOutputDebugId()) {
	//	fragShaderCode = fragShaderCode
	//		.arg("layout (location = 1) out vec4 CompId;\n")
	//		.arg("CompId = " + mSkyBox->getCompIdVec4String() + ";\n");
	//}
	//else {
		fragShaderCode = fragShaderCode.arg("").arg("");
	//}

	QShader fs = QRenderSystem::createShaderFromCode(QShader::Stage::FragmentStage, fragShaderCode.toLocal8Bit());
	Q_ASSERT(fs.isValid());

	mPipeline->setShaderStages({
		{ QRhiShaderStage::Vertex, vs },
		{ QRhiShaderStage::Fragment, fs }
							   });
	mShaderResourceBindings.reset(RHI->newShaderResourceBindings());

	QVector<QRhiShaderResourceBinding> shaderBindings;
	shaderBindings << QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, mUniformBuffer.get());
	shaderBindings << QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, mTexture.get(), mSampler.get());
	mShaderResourceBindings->setBindings(shaderBindings.begin(), shaderBindings.end());

	mShaderResourceBindings->create();

	mPipeline->setShaderResourceBindings(mShaderResourceBindings.get());

	mPipeline->setRenderPassDescriptor(QRenderSystem::instance()->getSceneRenderPassDescriptor());

	mPipeline->create();
}

void QSkyBoxComponent::uploadResource(QRhiResourceUpdateBatch* batch) {
	batch->uploadStaticBuffer(mVertexBuffer.get(), cubeData);

	QRhiTextureSubresourceUploadDescription subresDesc[6];
	auto& imageArray = mSkyBox->getImageList();

	for (int i = 0; i < std::size(imageArray); i++) {
		subresDesc[i].setImage(imageArray[i]);
	}

	QRhiTextureUploadDescription desc({
										  { 0, 0, subresDesc[0] },  // +X
										  { 1, 0, subresDesc[1] },  // -X
										  { 2, 0, subresDesc[2] },  // +Y
										  { 3, 0, subresDesc[3] },  // -Y
										  { 4, 0, subresDesc[4] },  // +Z
										  { 5, 0, subresDesc[5] }   // -Z
									  });
	batch->uploadTexture(mTexture.get(), desc);
	batch->generateMips(mTexture.get());
}

void QSkyBoxComponent::updatePrePass(QRhiCommandBuffer* cmdBuffer) {

}

void QSkyBoxComponent::updateResourcePrePass(QRhiResourceUpdateBatch* batch) {
	QMatrix4x4 MVP = mEntity->calculateMatrixMVP();
	MVP.scale(1000);
	batch->updateDynamicBuffer(mUniformBuffer.get(), 0, sizeof(QMatrix4x4), MVP.constData());
}

void QSkyBoxComponent::renderInPass(QRhiCommandBuffer* cmdBuffer, const QRhiViewport& viewport) {
	if (mTexture->pixelSize().isEmpty())
		return;
	cmdBuffer->setGraphicsPipeline(mPipeline.get());
	cmdBuffer->setViewport(viewport);
	cmdBuffer->setShaderResources();
	const QRhiCommandBuffer::VertexInput VertexInput(mVertexBuffer.get(), 0);
	cmdBuffer->setVertexInput(0, 1, &VertexInput, mIndexBuffer.get(), 0, QRhiCommandBuffer::IndexUInt32);
	cmdBuffer->draw(36);
}

