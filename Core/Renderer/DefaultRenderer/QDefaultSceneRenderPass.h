﻿#ifndef QDefaultSceneRenderPass_h__
#define QDefaultSceneRenderPass_h__

#include "Scene\QSceneComponent.h"
#include "Renderer\ISceneRenderPass.h"
#include "Renderer\IRhiProxy.h"

class QDefaultRenderer;

class QDefaultSceneRenderPass :public ISceneRenderPass {
public:
	QDefaultSceneRenderPass(std::shared_ptr<QScene> scene);
	std::shared_ptr<IRhiProxy> createStaticMeshProxy(std::shared_ptr<QStaticMeshComponent>) override;
	std::shared_ptr<IRhiProxy> createSkeletonMeshProxy(std::shared_ptr<QSkeletonModelComponent>) override;
	std::shared_ptr<IRhiProxy> createParticleProxy(std::shared_ptr<QParticleComponent>) override;
	std::shared_ptr<IRhiProxy> createSkyBoxProxy(std::shared_ptr<QSkyBoxComponent>) override;

	virtual void compile() override;

	void setupSceneFrameSize(QSize size);
	void setupSampleCount(int count);

	QRhiTexture* getOutputTexture();
	QRhiTexture* getDebugTexutre();

	virtual QVector<QRhiGraphicsPipeline::TargetBlend> getBlendStates() override;
	virtual QRhiRenderTarget* getRenderTarget() override;
private:
	QSize mSceneFrameSize;
	int mSampleCount = 1;
	struct RTResource {
		QRhiSPtr<QRhiTexture> colorAttachment;
		QRhiSPtr<QRhiRenderBuffer> msaaBuffer;
		QRhiSPtr<QRhiRenderBuffer> depthStencil;
		QRhiSPtr<QRhiTextureRenderTarget> renderTarget;
		QRhiSPtr<QRhiRenderPassDescriptor> renderPassDesc;

		QRhiSPtr<QRhiTexture> debugTexture;
		QRhiSPtr<QRhiRenderBuffer> debugMsaaBuffer;
	};
	RTResource mRT;
	QVector<QRhiGraphicsPipeline::TargetBlend> mBlendStates;
};

#endif // QDefaultSceneRenderPass_h__