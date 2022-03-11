﻿#ifndef QSceneRenderer_h__
#define QSceneRenderer_h__

#include "Scene/QScene.h"
#include "Common/QBloomPainter.h"
#include "RHI/QRhiDefine.h"

class QPrimitiveComponent;
class QStaticMeshComponent;
class QStaticMeshComponent;
class QSkeletonMeshComponent;
class QParticleComponent;
class QSkyBoxComponent;
class QSceneRenderer;

class QRhiProxy {
public:
	enum PipelineUsageFlag {
		Normal = 1,
		Instancing = 1 << 1,
		Debug = 1 << 2,
	};
	Q_DECLARE_FLAGS(PipelineUsageFlags, PipelineUsageFlag);

	virtual void recreateResource() {}
	virtual void recreatePipeline(PipelineUsageFlags flags = PipelineUsageFlag::Normal) {}
	virtual void uploadResource(QRhiResourceUpdateBatch* batch) {}
	virtual void updateResource(QRhiResourceUpdateBatch* batch) {}
	virtual void updatePrePass(QRhiCommandBuffer* cmdBuffer) {}
	virtual void drawInPass(QRhiCommandBuffer* cmdBuffer, const QRhiViewport& viewport) {}
public:
	QSceneRenderer* mRenderer;
	std::shared_ptr<QSceneComponent> mComponent;
	QRhiSPtr<QRhiGraphicsPipeline> mPipeline;
	QRhiSPtr<QRhiBuffer> mUniformBuffer;
	QRhiSPtr<QRhiBuffer> mVertexBuffer;
	QRhiSPtr<QRhiBuffer> mIndexBuffer;
};

class QSceneRenderer :public QObject {
	Q_OBJECT
public:
	QSceneRenderer(int sampleCount, QRhiSPtr<QRhiRenderPassDescriptor> renderPassDescriptor);
	void setScene(std::shared_ptr<QScene> scene);

	std::shared_ptr<QScene> getScene() { return mScene; }
	virtual QRhiSPtr<QRhiRenderPassDescriptor> getRenderPassDescriptor() const { return mRootRenderPassDescriptor; }

	void renderInternal(QRhiCommandBuffer* cmdBuffer, QRhiRenderTarget* renderTarget);
	virtual void render(QRhiCommandBuffer* cmdBuffer, QRhiRenderTarget* renderTarget) = 0;
	QMatrix4x4 getViewMatrix();
	QMatrix4x4 getClipMatrix() const;
	QMatrix4x4 getVP();
	int getSampleCount() const { return mSampleCount; }

	std::shared_ptr<QRhiProxy> createPrimitiveProxy(std::shared_ptr<QPrimitiveComponent> component);
	static QShader createShaderFromCode(QShader::Stage stage, const char* code);
private:
	void onPrimitiveInserted(uint32_t index, std::shared_ptr<QPrimitiveComponent> primitive);
	void onPrimitiveRemoved(std::shared_ptr<QPrimitiveComponent> primitive);
	void onLightChanged();
	void onSkyBoxChanged();
	void tryResetUniformProxy();
	void tryResetPrimitiveProxy();
	void tryResetSkyBox(QRhiResourceUpdateBatch* batch);
	void resetPrimitiveProxy(std::shared_ptr<QPrimitiveComponent> component);
	virtual std::shared_ptr<QRhiProxy> createStaticMeshProxy(std::shared_ptr<QStaticMeshComponent>) = 0;
	virtual std::shared_ptr<QRhiProxy> createSkeletonMeshProxy(std::shared_ptr<QSkeletonMeshComponent>) = 0;
	virtual std::shared_ptr<QRhiProxy> createParticleProxy(std::shared_ptr<QParticleComponent>) = 0;
	virtual std::shared_ptr<QRhiProxy> createSkyBoxProxy(std::shared_ptr<QSkyBoxComponent>) = 0;
protected:
	std::shared_ptr<QScene> mScene;
	QRhiSPtr<QRhiRenderPassDescriptor> mRootRenderPassDescriptor;

	QHash<QSceneComponent::ComponentId, std::shared_ptr<QRhiProxy>> mPrimitiveProxyMap;	//图元组件代理
	QList<std::shared_ptr<QRhiProxy>> mProxyUploadList;
	std::shared_ptr<QRhiProxy> mSkyBoxProxy;
	std::shared_ptr<QBloomPainter> mBloomPainter;
	int mSampleCount;
};

#endif // QSceneRenderer_h__
