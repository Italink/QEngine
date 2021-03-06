//#include "TexturePainter.h"
//#include "ECS\System\RenderSystem\QRenderSystem.h"
//
//TexturePainter::TexturePainter()
//{
//}
//
//void TexturePainter::setupTexture(QRhiTexture* texture)
//{
//	mTexture = texture;
//}
//
//void TexturePainter::compile()
//{
//	mSampler.reset(RHI->newSampler(QRhiSampler::Linear,
//				   QRhiSampler::Linear,
//				   QRhiSampler::None,
//				   QRhiSampler::ClampToEdge,
//				   QRhiSampler::ClampToEdge));
//	mSampler->create();
//	mPipeline.reset(RHI->newGraphicsPipeline());
//	QRhiGraphicsPipeline::TargetBlend blendState;
//	blendState.enable = true;
//	mPipeline->setTargetBlends({ blendState });
//	mPipeline->setSampleCount(mSampleCount);
//
//	QString vsCode = R"(#version 450
//layout (location = 0) out vec2 vUV;
//out gl_PerVertex{
//	vec4 gl_Position;
//};
//void main() {
//	vUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
//	gl_Position = vec4(vUV * 2.0f - 1.0f, 0.0f, 1.0f);
//	%1
//}
//)";
//	QShader vs = QRenderSystem::createShaderFromCode(QShader::VertexStage, vsCode.arg(RHI->isYUpInNDC() ? "	vUV.y = 1 - vUV.y;":"").toLocal8Bit());
//
//	QShader fs = QRenderSystem::createShaderFromCode(QShader::FragmentStage, R"(#version 450
//layout (binding = 0) uniform sampler2D samplerColor;
//layout (location = 0) in vec2 vUV;
//layout (location = 0) out vec4 outFragColor;
//void main() {
//	outFragColor = vec4(texture(samplerColor, vUV).rgb,1.0f);
//}
//)");
//
//	mPipeline->setShaderStages({
//		{ QRhiShaderStage::Vertex, vs },
//		{ QRhiShaderStage::Fragment, fs }
//							   });
//	QRhiVertexInputLayout inputLayout;
//
//	mBindings.reset(RHI->newShaderResourceBindings());
//	mBindings->setBindings({
//		QRhiShaderResourceBinding::sampledTexture(0,QRhiShaderResourceBinding::FragmentStage,mTexture,mSampler.get())
//						   });
//	mBindings->create();
//	mPipeline->setVertexInputLayout(inputLayout);
//	mPipeline->setShaderResourceBindings(mBindings.get());
//	mPipeline->setRenderPassDescriptor(mRenderPassDesc);
//	mPipeline->create();
//}
//
//void TexturePainter::paint(QRhiCommandBuffer* cmdBuffer, QRhiRenderTarget* renderTarget) {
//
//	cmdBuffer->setGraphicsPipeline(mPipeline.get());
//	cmdBuffer->setViewport(QRhiViewport(0, 0, renderTarget->pixelSize().width(), renderTarget->pixelSize().height()));
//	cmdBuffer->setShaderResources(mBindings.get());
//	cmdBuffer->draw(4);
//}
//
