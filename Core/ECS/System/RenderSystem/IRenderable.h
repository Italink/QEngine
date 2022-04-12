#ifndef IRenderable_h__
#define IRenderable_h__

#include "RHI/QRhiDefine.h"

class IRenderable{
public:
	virtual void recreateResource() {}
	virtual void recreatePipeline() {}
	virtual void uploadResource(QRhiResourceUpdateBatch* batch) {}
	virtual void updatePrePass(QRhiCommandBuffer* cmdBuffer) {}
	virtual void updateResourcePrePass(QRhiResourceUpdateBatch* batch) {}
	virtual void renderInPass(QRhiCommandBuffer* cmdBuffer) = 0;

	QRhiSignal bNeedRecreateResource;
	QRhiSignal bNeedRecreatePipeline;

	void active();
	void deactive();
};

#endif // IRenderable_h__
