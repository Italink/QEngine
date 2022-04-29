﻿#ifndef IRenderPassBase_h__
#define IRenderPassBase_h__

#include "ECS/System/RenderSystem/RHI/QRhiDefine.h"

class QRenderer;

class IRenderPassBase {
	friend class QFrameGraphBuilder;
public:
	void setup() {
		if(mFuncSetup)
			mFuncSetup();
	}

	virtual void compile() = 0;
	virtual void execute(QRhiCommandBuffer* cmdBuffer) = 0;

	virtual QRhiTexture* getOutputTexture(int slot = 0) {
		return mOutputTextures.value(slot, nullptr);
	}
	virtual void setupInputTexture(int slot, QRhiTexture* texture){
		mInputTextures[slot] = texture;
	}

	void setFuncSetup(std::function<void()> val) { 
		mFuncSetup = val; 
	}

	const QHash<int, QRhiTexture*>& getInputTextures() {
		return mInputTextures;
	}
	const QHash<int, QRhiTexture*>& getOutputTextures() {
		return mOutputTextures;
	}
private:
	std::function<void()> mFuncSetup;
protected:
	QRenderer* mRenderer = nullptr;
	QHash<int, QRhiTexture*> mInputTextures;
	QHash<int, QRhiTexture*> mOutputTextures;
};

#endif // IRenderPassBase_h__
