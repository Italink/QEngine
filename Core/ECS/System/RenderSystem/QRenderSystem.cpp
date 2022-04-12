#include "QRenderSystem.h"
#include "IRenderable.h"
#include "private\qshaderbaker_p.h"
#include "Renderer\IRenderer.h"
#include "Renderer\RenderPass\ISceneRenderPass.h"

QRenderSystem::QRenderSystem()
	:mWindow(std::make_shared<QRhiWindow>(QRhi::Implementation::Vulkan)) {
}

QRenderSystem* QRenderSystem::instance() {
	static QRenderSystem ins;
	return &ins;
}

QShader QRenderSystem::createShaderFromCode(QShader::Stage stage, const char* code) {
	QShaderBaker baker;
	baker.setGeneratedShaderVariants({ QShader::StandardShader });
	baker.setGeneratedShaders({
		QShaderBaker::GeneratedShader{QShader::Source::SpirvShader,QShaderVersion(100)},
		QShaderBaker::GeneratedShader{QShader::Source::GlslShader,QShaderVersion(430)},
		QShaderBaker::GeneratedShader{QShader::Source::MslShader,QShaderVersion(12)},
		QShaderBaker::GeneratedShader{QShader::Source::HlslShader,QShaderVersion(50)},
							  });

	baker.setSourceString(code, stage);
	QShader shader = baker.bake();
	if (!shader.isValid()) {
		qWarning(code);
		qWarning(baker.errorMessage().toLocal8Bit());
	}

	return shader;
}

void QRenderSystem::addRenderItem(IRenderable* comp) {
	mRenderer->mScenePass->mRenderItemList << comp;
}

void QRenderSystem::removeRenderItem(IRenderable* comp) {
	mRenderer->mScenePass->mRenderItemList.removeOne(comp);
}

QRhiWindow* QRenderSystem::window() {
	return mWindow.get();
}

QRhi* QRenderSystem::getRHI() {
	return mWindow->mRhi.get();
}

bool QRenderSystem::isEnableDebug() {
	return mEnableDebug;
}

