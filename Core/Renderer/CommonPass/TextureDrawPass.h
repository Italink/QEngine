#ifndef TextureDrawPass_h__
#define TextureDrawPass_h__

#include "RHI\QRhiDefine.h"

class TextureDrawPass {
public:
	TextureDrawPass();
	void drawCommand(QRhiCommandBuffer* cmdBuffer, QRhiSPtr<QRhiTexture> texture, QRhiRenderTarget* renderTarget);
protected:
	void initRhiResource(QRhiRenderPassDescriptor* renderPassDesc, QRhiRenderTarget* renderTarget, QRhiSPtr<QRhiTexture> texture);
	void updateTexture(QRhiSPtr<QRhiTexture> texture);
private:
	QRhiSPtr<QRhiGraphicsPipeline> mPipeline;
	QRhiSPtr<QRhiSampler> mSampler;
	QRhiSPtr<QRhiShaderResourceBindings> mBindings;
	QRhiSPtr<QRhiTexture> mTexture;
};

#endif // TextureDrawPass_h__