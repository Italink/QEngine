#ifndef QFullSceneTexturePainter_h__
#define QFullSceneTexturePainter_h__

#include "Render\Renderer\QSceneRenderer.h"
class QFullSceneTexturePainter {
public:
	QFullSceneTexturePainter(std::shared_ptr<QRhi> rhi);
	void drawCommand(QRhiCommandBuffer* cmdBuffer, QRhiSPtr<QRhiTexture> texture, QRhiRenderTarget* renderTarget);
protected:
	void initRhiResource(QRhiRenderPassDescriptor* renderPassDesc, QRhiRenderTarget* renderTarget, QRhiSPtr<QRhiTexture> texture);
	void updateTexture(QRhiSPtr<QRhiTexture> texture);
private:
	std::shared_ptr<QRhi> mRhi;
	QRhiSPtr<QRhiGraphicsPipeline> mPipeline;
	QRhiSPtr<QRhiSampler> mSampler;
	QRhiSPtr<QRhiShaderResourceBindings> mBindings;
	QRhiSPtr<QRhiTexture> mTexture;
};

#endif // QFullSceneTexturePainter_h__
