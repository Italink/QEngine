﻿#include "ISceneRenderer.h"
#include "private/qshaderbaker_p.h"
#include "QEngine.h"

ISceneRenderer::ISceneRenderer()
{
}

void ISceneRenderer::setScene(std::shared_ptr<QScene> scene)
{
	if (mScene) {
		mScene->disconnect();
	}
	mScene = scene;
}

QShader ISceneRenderer::createShaderFromCode(QShader::Stage stage, const char* code)
{
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