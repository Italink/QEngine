#include "QRenderer.h"
#include "QEngineCoreApplication.h"
#include "ECS\System\RenderSystem\Renderer\RenderPass\PixelSelectRenderPass.h"
#include "ECS\System\RenderSystem\Renderer\RenderPass\BlurRenderPass.h"
#include "ECS\System\RenderSystem\Renderer\RenderPass\BloomRenderPass.h"
#include "ECS\System\RenderSystem\Renderer\RenderPass\SwapChainRenderPass.h"
#include "ECS\System\RenderSystem\Renderer\RenderPass\LightingRenderPass.h"
#include "ECS\System\RenderSystem\QRenderSystem.h"
#include "RenderPass\DeferRenderPass.h"
#include "RenderPass\ForwardRenderPass.h"
#include "ECS\System\RenderSystem\RHI\IRenderable.h"
#include "RenderPass\ColorGradingRenderPass.h"

QRenderer::QRenderer(){}

void QRenderer::buildFrameGraph() {
	QFrameGraphBuilder builder;

	mDeferRenderPass = std::make_shared<DeferRenderPass>();
	mLightingRenderPass = std::make_shared<LightingRenderPass>();
	mForwardRenderPass = std::make_shared<ForwardRenderPass>();
	mSwapChainPass = std::make_shared<SwapChainRenderPass>();

	std::shared_ptr<PixelSelectRenderPass> bloomPixelSelectPass = std::make_shared<PixelSelectRenderPass>();
	std::shared_ptr<BlurRenderPass> bloomBlurPass = std::make_shared<BlurRenderPass>();
	std::shared_ptr<BloomMerageRenderPass> bloomMeragePass = std::make_shared<BloomMerageRenderPass>();
	std::shared_ptr<ColorGradingRenderPass> colorGradingPass = std::make_shared<ColorGradingRenderPass>();


	//	ScenePass -> LightingPass ->ForwardPass -->BloomPixelSeletorPass -> BloomBlurPass
	//	                 |														 |
	//	                 |														 V
	//					 -----------------------------------------------> BloomMeragePass ----> Swapchain( Scene + ImguiDebugDraw )

	mFrameGraph = builder.begin(this)			
		->node("Defer", mDeferRenderPass,[]() {})
		->node("Lighting", mLightingRenderPass,
			   [self = mLightingRenderPass.get(), defer = mDeferRenderPass.get()]() {
					self->setupInputTexture(LightingRenderPass::InputTextureSlot::Color, defer->getOutputTexture(DeferRenderPass::OutputTextureSlot::BaseColor));
					self->setupInputTexture(LightingRenderPass::InputTextureSlot::Position, defer->getOutputTexture(DeferRenderPass::OutputTextureSlot::Position));
					self->setupInputTexture(LightingRenderPass::InputTextureSlot::Normal_MetalnessTexture, defer->getOutputTexture(DeferRenderPass::OutputTextureSlot::NormalMetalness));
					self->setupInputTexture(LightingRenderPass::InputTextureSlot::Tangent_RoughnessTexture, defer->getOutputTexture(DeferRenderPass::OutputTextureSlot::TangentRoughness));
				})
		->dependency({ "Defer" })
		->node("Forward", mForwardRenderPass,
				[self = mForwardRenderPass.get(), lighting = mLightingRenderPass.get(), defer = mDeferRenderPass.get()]() {
					self->setupInputTexture(ForwardRenderPass::InputTextureSlot::Color, lighting->getOutputTexture(LightingRenderPass::OutputTextureSlot::LightingResult));
					self->setupInputTexture(ForwardRenderPass::InputTextureSlot::Depth, defer->getOutputTexture(DeferRenderPass::OutputTextureSlot::Depth));
					self->setupInputTexture(ForwardRenderPass::InputTextureSlot::DeferDebugId, defer->getOutputTexture(DeferRenderPass::OutputTextureSlot::DebugId));
				})
		->dependency({ "Lighting","Defer"})
		->node("BloomPixelSelector", bloomPixelSelectPass,
				[self = bloomPixelSelectPass.get(), forward = mForwardRenderPass.get()]() {
					self->setDownSamplerCount(4);
					self->setupSelectCode(R"(
						void main() {
							vec4 color = texture(uTexture, vUV);
							float value = max(max(color.r,color.g),color.b);
							outFragColor = (1-step(value,1.0f)) * color;
						}
						)");
					self->setupInputTexture(PixelSelectRenderPass::InputTextureSlot::Color,  forward->getOutputTexture(ForwardRenderPass::OutputTextureSlot::Output));
				})
			->dependency({ "Forward" })
		->node("BloomBlurPass", bloomBlurPass,
				[self = bloomBlurPass.get(), pixel = bloomPixelSelectPass.get()]() {
					self->setupInputTexture(BlurRenderPass::InputTextureSlot::Color, pixel->getOutputTexture(PixelSelectRenderPass::OutputTextureSlot::SelectResult));
					self->setupBlurSize(20);
					self->setupBlurIter(2);
				})
			->dependency({ "BloomPixelSelector" })
		->node("ToneMapping", bloomMeragePass,
				[self = bloomMeragePass.get(), forward = mForwardRenderPass.get(), blur = bloomBlurPass]() {
					self->setupInputTexture(BloomMerageRenderPass::InputTextureSlot::Bloom, blur->getOutputTexture(BlurRenderPass::OutputTextureSlot::BlurResult));
					self->setupInputTexture(BloomMerageRenderPass::InputTextureSlot::Src, forward->getOutputTexture(ForwardRenderPass::OutputTextureSlot::Output));
				})
			->dependency({ "Forward","BloomBlurPass" })
		->node("ColorGrading", colorGradingPass,
				[self = colorGradingPass.get(), toneMaping = bloomMeragePass.get()]() {
					self->setupInputTexture(ColorGradingRenderPass::InputTextureSlot::Color, toneMaping->getOutputTexture(BloomMerageRenderPass::OutputTextureSlot::BloomMerageResult));
				})
			->dependency({ "ToneMapping"})
		->node("Swapchain", mSwapChainPass,
			   [self = mSwapChainPass.get(), colorGrading = colorGradingPass.get(),  forward = mForwardRenderPass.get()]() {
					self->setupSwapChain(TheRenderSystem->window()->getSwapChain());
					self->setupInputTexture(SwapChainRenderPass::InputTextureSlot::Color, colorGrading->getOutputTexture(ColorGradingRenderPass::OutputTextureSlot::Result));
					self->setupInputTexture(SwapChainRenderPass::InputTextureSlot::DebugId, forward->getOutputTexture(ForwardRenderPass::OutputTextureSlot::DebugId));
				})
			->dependency({ "ColorGrading","Forward"})
		->end();
}

void QRenderer::render(QRhiCommandBuffer* cmdBuffer) {
	if(mFrameGraph)
		mFrameGraph->executable(cmdBuffer);
}

void QRenderer::resize(QSize size) {
	mFrameSize = size;
	if (mFrameGraph)
		mFrameGraph->compile();
}

void QRenderer::addRenderItem(IRenderable* comp) {
	mRenderableItemList << comp;
}

void QRenderer::removeRenderItem(IRenderable* comp) {
	mRenderableItemList.removeOne(comp);
}

void QRenderer::addLightItem(ILightComponent* item) {
	mLightingRenderPass->addLightItem(item);
}

void QRenderer::removeLightItem(ILightComponent* item) {
	mLightingRenderPass->removeLightItem(item);
}

QList<IRenderable*> QRenderer::getDeferItemList() {
	QList<IRenderable*> deferList;
	for (auto& item : mRenderableItemList) {
		if (item->isDefer())
			deferList << item;
	}
	return deferList;
}

QList<IRenderable*> QRenderer::getForwardItemList() {
	QList<IRenderable*> forwardList;
	for (auto& item : mRenderableItemList) {
		if (!item->isDefer())
			forwardList << item;
	}
	return forwardList;
}

int QRenderer::getDeferPassSampleCount() {
	return 1;
}

QVector<QRhiGraphicsPipeline::TargetBlend> QRenderer::getDeferPassBlendStates() {
	return mDeferRenderPass->getBlendStates();
}

QRhiRenderPassDescriptor* QRenderer::getDeferPassDescriptor() {
	return mDeferRenderPass->getRenderTarget()->renderPassDescriptor();
}

int QRenderer::getForwardSampleCount() {
	return 1;
}

QVector<QRhiGraphicsPipeline::TargetBlend> QRenderer::getForwardPassBlendStates() {
	return mForwardRenderPass->getBlendStates();
}


QRhiRenderPassDescriptor* QRenderer::getForwardRenderPassDescriptor() {
	return mForwardRenderPass->getRenderTarget()->renderPassDescriptor();
}

void QRenderer::setCurrentRenderPass(IRenderPassBase* val) {
	if (mCurrentRenderPass != val) {
		mCurrentRenderPass = val;
		Q_EMIT currentRenderPassChanged(val);
	}
}
