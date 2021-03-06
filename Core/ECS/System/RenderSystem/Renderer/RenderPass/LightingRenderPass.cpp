#include "LightingRenderPass.h"
#include "ECS\System\RenderSystem\QRenderSystem.h"
#include "ECS\Component\LightComponent\ILightComponent.h"
#include "QEngineCoreApplication.h"
#include "ECS\Component\QCameraComponent.h"
#include "ECS\Component\RenderableComponent\QSkyBoxComponent.h"

LightingRenderPass::LightingRenderPass(){}

void LightingRenderPass::addLightItem(ILightComponent* item) {
	mLightItemList << item;
	rebuildLight();
}

void LightingRenderPass::removeLightItem(ILightComponent* item) {
	mLightItemList.removeOne(item);
	rebuildLight();
}

void LightingRenderPass::compile() {
	mRT.colorAttachment.reset(RHI->newTexture(QRhiTexture::RGBA32F, mInputTextures[InputTextureSlot::Color]->pixelSize(), 1, QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
	mRT.colorAttachment->create();
	mRT.renderTarget.reset(RHI->newTextureRenderTarget({ mRT.colorAttachment.get() }));
	mRT.renderPassDesc.reset(mRT.renderTarget->newCompatibleRenderPassDescriptor());
	mRT.renderTarget->setRenderPassDescriptor(mRT.renderPassDesc.get());
	mRT.renderTarget->create();

	mSampler.reset(RHI->newSampler(QRhiSampler::Linear,
				   QRhiSampler::Linear,
				   QRhiSampler::None,
				   QRhiSampler::ClampToEdge,
				   QRhiSampler::ClampToEdge));
	mSampler->create();
	mUniformBuffer.reset(RHI->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(UniformBlock)));
	mUniformBuffer->create();
	rebuildLight();
	mOutputTextures[OutputTextureSlot::LightingResult] = mRT.colorAttachment.get();
}

void LightingRenderPass::rebuildLight() {
	mPipeline.reset(RHI->newGraphicsPipeline());
	QRhiGraphicsPipeline::TargetBlend blendState;
	blendState.enable = true;
	mPipeline->setTargetBlends({ blendState });
	mPipeline->setSampleCount(mRT.renderTarget->sampleCount());

	mBindings.reset(RHI->newShaderResourceBindings());
	QVector<QRhiShaderResourceBinding> shaderBindings;
	shaderBindings << QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage, mInputTextures[InputTextureSlot::Color], mSampler.get());
	shaderBindings << QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, mInputTextures[InputTextureSlot::Position], mSampler.get());
	shaderBindings << QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage, mInputTextures[InputTextureSlot::Normal_MetalnessTexture], mSampler.get());
	shaderBindings << QRhiShaderResourceBinding::sampledTexture(3, QRhiShaderResourceBinding::FragmentStage, mInputTextures[InputTextureSlot::Tangent_RoughnessTexture], mSampler.get());
	shaderBindings << QRhiShaderResourceBinding::uniformBuffer(4, QRhiShaderResourceBinding::FragmentStage, mUniformBuffer.get(), 0, sizeof(UniformBlock));
	uint8_t bindingOffset = 5;

	if (mSkyCube) {
		shaderBindings << QRhiShaderResourceBinding::sampledTexture(bindingOffset++, QRhiShaderResourceBinding::FragmentStage, mSkyCube, mSampler.get());
		/*shaderBindings << QRhiShaderResourceBinding::sampledTexture(bindingOffset++, QRhiShaderResourceBinding::FragmentStage, irmapTexture.get(), mSampler.get());
		shaderBindings << QRhiShaderResourceBinding::sampledTexture(bindingOffset++, QRhiShaderResourceBinding::FragmentStage, BRDFTexture.get(), mSampler.get());*/
	}

	QByteArray lightDefineCode;
	QByteArray lightingCode;

	for (int i = 0; i < mLightItemList.size(); i++) {
		auto& lightItem = mLightItemList[i];

		QString name = QString("Light%1").arg(i);
		const auto& lightInfo = lightItem->getUniformInfo(bindingOffset, name);

		lightDefineCode += lightInfo.uniformDefineCode;
		lightingCode += lightItem->getLightingCode(name);

		bindingOffset = lightInfo.bindingOffset;

		shaderBindings << lightInfo.bindings;
	}
	mBindings->setBindings(shaderBindings.begin(), shaderBindings.end());
	mBindings->create();

	QString vsCode = R"(#version 450
layout (location = 0) out vec2 vUV;
out gl_PerVertex{
	vec4 gl_Position;
};
void main() {
	vUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(vUV * 2.0f - 1.0f, 0.0f, 1.0f);
	%1
}
)";
	QShader vs = QRenderSystem::createShaderFromCode(QShader::VertexStage, vsCode.arg(RHI->isYUpInNDC() ? "	vUV.y = 1 - vUV.y;" : "").toLocal8Bit());
	QByteArray shadingCode;
	if (mSkyCube) {
		lightDefineCode.prepend(R"( 
			layout(binding = 5) uniform samplerCube uSkybox;
		)");
		shadingCode = R"(
			vec3 skyColor =  texture(uSkybox,reflect(Lo,N)).rgb;
			
			outFragColor = vec4(mix(skyColor,baseColor,roughness) + directLighting, 1.0);
		)";
	}
	else {
		shadingCode = "outFragColor = vec4(baseColor + directLighting,1.0);";
	}

	QByteArray fsCode = QString(R"(#version 450
layout (binding = 0) uniform sampler2D uBaseColorTexture;
layout (binding = 1) uniform sampler2D uPositionTexture;
layout (binding = 2) uniform sampler2D uNormal_MetalnessTexture;
layout (binding = 3) uniform sampler2D uTangent_RoughnessTexture;
layout (binding = 4) uniform UniformBlock{
	vec3 eyePosition;
}UBO;
%1

layout (location = 0) in vec2 vUV;

layout (location = 0) out vec4 outFragColor;

const float PI = 3.141592;
const float Epsilon = 0.00001;
// Constant normal incidence Fresnel factor for all dielectrics.
const vec3 Fdielectric = vec3(0.04);

float ndfGGX(float cosLh, float roughness){
	float alpha   = roughness * roughness;
	float alphaSq = alpha * alpha;
	float denom = (cosLh * cosLh) * (alphaSq - 1.0) + 1.0;
	return alphaSq / (PI * denom * denom);
}

// Single term for separable Schlick-GGX below.
float gaSchlickG1(float cosTheta, float k){
	return cosTheta / (cosTheta * (1.0 - k) + k);
}

// Schlick-GGX approximation of geometric attenuation function using Smith's method.
float gaSchlickGGX(float cosLi, float cosLo, float roughness){
	float r = roughness + 1.0;
	float k = (r * r) / 8.0; // Epic suggests using this roughness remapping for analytic lights.
	return gaSchlickG1(cosLi, k) * gaSchlickG1(cosLo, k);
}

// Shlick's approximation of the Fresnel factor.
vec3 fresnelSchlick(vec3 F0, float cosTheta){
	return F0 + (vec3(1.0) - F0) * pow(1.0 - cosTheta, 5.0);
}

struct AnalyticalLight {
	vec3 direction;
	vec3 radiance;
};

void main() {
	vec3 baseColor =	texture(uBaseColorTexture, vUV).rgb;
	float metalness =	texture(uNormal_MetalnessTexture, vUV).a;
	float roughness =	texture(uTangent_RoughnessTexture, vUV).a;
	vec3 position =		texture(uPositionTexture, vUV).rgb;
	vec3 N =			normalize(texture(uNormal_MetalnessTexture, vUV).rgb);

	vec3 Lo = normalize(UBO.eyePosition - position);
	
	float cosLo = max(0.0, dot(N, Lo));
		
	vec3 Lr = 2.0 * cosLo * N - Lo;

	vec3 F0 = mix(Fdielectric, baseColor, metalness);

	vec3 directLighting = vec3(0);
	%2
	%3
}
)").arg(lightDefineCode).arg(lightingCode).arg(shadingCode).toLocal8Bit();

	QShader fs = QRenderSystem::createShaderFromCode(QShader::FragmentStage, fsCode);

	mPipeline->setShaderStages({
		{ QRhiShaderStage::Vertex, vs },
		{ QRhiShaderStage::Fragment, fs }
	});

	QRhiVertexInputLayout inputLayout;
	mPipeline->setVertexInputLayout(inputLayout);
	mPipeline->setShaderResourceBindings(mBindings.get());
	mPipeline->setRenderPassDescriptor(mRT.renderTarget->renderPassDescriptor());
	mPipeline->create();
}

void LightingRenderPass::execute(QRhiCommandBuffer* cmdBuffer) {
	QSkyBoxComponent* comp = TheEngine->world()->getCurrentSkyBox();
	if (comp) {
		if (comp->getTexture().get() != mSkyCube) {
			mSkyCube = comp->getTexture().get();
			//buildIBL(cmdBuffer);
			rebuildLight();
		}
	}

	QRhiResourceUpdateBatch* resUpdateBatch = RHI->nextResourceUpdateBatch();
	for (auto& lightItem : mLightItemList) {
		lightItem->updateResource(resUpdateBatch);
	}
	UniformBlock block;
	block.eyePosition = TheEngine->world()->getCurrentCamera()->getPosition();
	resUpdateBatch->updateDynamicBuffer(mUniformBuffer.get(), 0, sizeof(UniformBlock), &block);

	cmdBuffer->beginPass(mRT.renderTarget.get(), QColor::fromRgbF(0.0f, 0.0f, 0.0f, 0.0f), { 1.0f, 0 }, resUpdateBatch);
	cmdBuffer->setGraphicsPipeline(mPipeline.get());
	cmdBuffer->setViewport(QRhiViewport(0, 0, mRT.renderTarget->pixelSize().width(), mRT.renderTarget->pixelSize().height()));
	cmdBuffer->setShaderResources();
	cmdBuffer->draw(4);
	cmdBuffer->endPass();
}


void LightingRenderPass::buildIBL(QRhiCommandBuffer* cmdBuffer) {
//	if (mSkyCube == nullptr)
//		return;
//
//	spmapTexture.reset(RHI->newTexture(QRhiTexture::RGBA16F, mSkyCube->pixelSize(), 1,
//					   QRhiTexture::Flag::MipMapped
//					   | QRhiTexture::Flag::UsedWithGenerateMips 
//					   | QRhiTexture::Flag::CubeMap
//					   | QRhiTexture::Flag::UsedWithLoadStore));
//	spmapTexture->create();
//
//	spmapPipeline.reset(RHI->newComputePipeline());
//
//	spmapUniform.reset(RHI->newBuffer(QRhiBuffer::Type::Dynamic, QRhiBuffer::UsageFlag::UniformBuffer,sizeof(float)));
//	spmapUniform->create();
//	QVector<QRhiShaderResourceBinding> spmapBindings;
//	spmapBindings << QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::ComputeStage, mSkyCube, mSampler.get());
//	spmapBindings << QRhiShaderResourceBinding::imageStore(1, QRhiShaderResourceBinding::ComputeStage, spmapTexture.get(),0);
//	spmapBindings << QRhiShaderResourceBinding::uniformBuffer(2, QRhiShaderResourceBinding::ComputeStage, spmapUniform.get());
//	spmapShaderBindings.reset(RHI->newShaderResourceBindings());
//	spmapShaderBindings->setBindings(spmapBindings.begin(), spmapBindings.end());
//	spmapShaderBindings->create();
//	spmapPipeline->setShaderResourceBindings(spmapShaderBindings.get());
//	QShader spmapShader = QRenderSystem::createShaderFromCode(QShader::ComputeStage,R"(
//#version 450 core
//
//const float PI = 3.141592;
//const float TwoPI = 2 * PI;
//const float Epsilon = 0.00001;
//
//const uint NumSamples = 1024;
//const float InvNumSamples = 1.0 / float(NumSamples);
//
//
//const int NumMipLevels = 16;
//layout( binding = 0) uniform samplerCube inputTexture;
//layout( binding = 1, rgba16f) restrict writeonly uniform imageCube outputTexture;
//layout(std140, binding = 2) uniform SpmapParam{
//		float roughness;
//}UBO;
//
//#define PARAM_ROUGHNESS UBO.roughness
//
//float radicalInverse_VdC(uint bits)
//{
//	bits = (bits << 16u) | (bits >> 16u);
//	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
//	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
//	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
//	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
//	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
//}
//
//vec2 sampleHammersley(uint i)
//{
//	return vec2(i * InvNumSamples, radicalInverse_VdC(i));
//}
//
//vec3 sampleGGX(float u1, float u2, float roughness)
//{
//	float alpha = roughness * roughness;
//
//	float cosTheta = sqrt((1.0 - u2) / (1.0 + (alpha*alpha - 1.0) * u2));
//	float sinTheta = sqrt(1.0 - cosTheta*cosTheta); // Trig. identity
//	float phi = TwoPI * u1;
//
//	// Convert to Cartesian upon return.
//	return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
//}
//
//float ndfGGX(float cosLh, float roughness)
//{
//	float alpha   = roughness * roughness;
//	float alphaSq = alpha * alpha;
//
//	float denom = (cosLh * cosLh) * (alphaSq - 1.0) + 1.0;
//	return alphaSq / (PI * denom * denom);
//}
//
//vec3 getSamplingVector()
//{
//    vec2 st = gl_GlobalInvocationID.xy/vec2(imageSize(outputTexture));
//    vec2 uv = 2.0 * vec2(st.x, 1.0-st.y) - vec2(1.0);
//
//    vec3 ret;
//    // Sadly 'switch' doesn't seem to work, at least on NVIDIA.
//    if(gl_GlobalInvocationID.z == 0)      ret = vec3(1.0,  uv.y, -uv.x);
//    else if(gl_GlobalInvocationID.z == 1) ret = vec3(-1.0, uv.y,  uv.x);
//    else if(gl_GlobalInvocationID.z == 2) ret = vec3(uv.x, 1.0, -uv.y);
//    else if(gl_GlobalInvocationID.z == 3) ret = vec3(uv.x, -1.0, uv.y);
//    else if(gl_GlobalInvocationID.z == 4) ret = vec3(uv.x, uv.y, 1.0);
//    else if(gl_GlobalInvocationID.z == 5) ret = vec3(-uv.x, uv.y, -1.0);
//    return normalize(ret);
//}
//
//void computeBasisVectors(const vec3 N, out vec3 S, out vec3 T)
//{
//	T = cross(N, vec3(0.0, 1.0, 0.0));
//	T = mix(cross(N, vec3(1.0, 0.0, 0.0)), T, step(Epsilon, dot(T, T)));
//
//	T = normalize(T);
//	S = normalize(cross(N, T));
//}
//
//vec3 tangentToWorld(const vec3 v, const vec3 N, const vec3 S, const vec3 T)
//{
//	return S * v.x + T * v.y + N * v.z;
//}
//
//layout(local_size_x=32, local_size_y=32, local_size_z=1) in;
//void main(void)
//{
//	ivec2 outputSize = imageSize(outputTexture);
//	if(gl_GlobalInvocationID.x >= outputSize.x || gl_GlobalInvocationID.y >= outputSize.y) {
//		return;
//	}
//	
//	vec2 inputSize = vec2(textureSize(inputTexture, 0));
//	float wt = 4.0 * PI / (6 * inputSize.x * inputSize.y);
//	
//	// Approximation: Assume zero viewing angle (isotropic reflections).
//	vec3 N = getSamplingVector();
//	vec3 Lo = N;
//	
//	vec3 S, T;
//	computeBasisVectors(N, S, T);
//
//	vec3 color = vec3(0);
//	float weight = 0;
//
//	for(uint i=0; i<NumSamples; ++i) {
//		vec2 u = sampleHammersley(i);
//		vec3 Lh = tangentToWorld(sampleGGX(u.x, u.y, PARAM_ROUGHNESS), N, S, T);
//
//		vec3 Li = 2.0 * dot(Lo, Lh) * Lh - Lo;
//
//		float cosLi = dot(N, Li);
//		if(cosLi > 0.0) {
//			float cosLh = max(dot(N, Lh), 0.0);
//
//			float pdf = ndfGGX(cosLh, PARAM_ROUGHNESS) * 0.25;
//
//			float ws = 1.0 / (NumSamples * pdf);
//
//			float mipLevel = max(0.5 * log2(ws / wt) + 1.0, 0.0);
//
//			color  += textureLod(inputTexture, Li, mipLevel).rgb * cosLi;
//			weight += cosLi;
//		}
//	}
//	color /= weight;
//
//	imageStore(outputTexture, ivec3(gl_GlobalInvocationID), vec4(1,0,1, 1.0));
//}
//)");
//
//	spmapPipeline->setShaderStage({ QRhiShaderStage::Compute,spmapShader });
//	spmapPipeline->create();
//
//	int levels = RHI->mipLevelsForSize(mSkyCube->pixelSize());
//	const float deltaRoughness = 1.0f / qMax(levels - 1, 1);
//	QRhiResourceUpdateBatch* batch = RHI->nextResourceUpdateBatch();
//	batch->updateDynamicBuffer(mUniformBuffer.get(), 0, sizeof(float), &deltaRoughness);
//	cmdBuffer->resourceUpdate(batch);
//	for (int level = 0, size = mSkyCube->pixelSize().width() / 2; level < levels; ++level, size /= 2) {
//		spmapBindings[1] = QRhiShaderResourceBinding::imageStore(1, QRhiShaderResourceBinding::ComputeStage, spmapTexture.get(), level);
//		spmapShaderBindings->setBindings(spmapBindings.begin(), spmapBindings.end());
//		spmapShaderBindings->create();
//		int numGroups = qMax(1, size / 32);
//		cmdBuffer->beginComputePass();
//		cmdBuffer->setComputePipeline(spmapPipeline.get());
//		cmdBuffer->setShaderResources(spmapShaderBindings.get());
//		cmdBuffer->dispatch(numGroups, numGroups, 6);
//		cmdBuffer->endComputePass();
//	}
//
//	irmapTexture.reset(RHI->newTexture(QRhiTexture::RGBA16F, mSkyCube->pixelSize(), 1,
//					     QRhiTexture::Flag::CubeMap
//					   | QRhiTexture::Flag::UsedWithLoadStore));
//	irmapTexture->create();
//
//	irmapPipeline.reset(RHI->newComputePipeline());
//
//	QVector<QRhiShaderResourceBinding> irmapBindings;
//	irmapBindings << QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::ComputeStage, spmapTexture.get(), mSampler.get());
//	irmapBindings << QRhiShaderResourceBinding::imageStore(1, QRhiShaderResourceBinding::ComputeStage, irmapTexture.get(),0);
//	irmapShaderBindings.reset(RHI->newShaderResourceBindings());
//	irmapShaderBindings->setBindings(irmapBindings.begin(), irmapBindings.end());
//	irmapShaderBindings->create();
//	irmapPipeline->setShaderResourceBindings(irmapShaderBindings.get());
//	QShader irmapShader = QRenderSystem::createShaderFromCode(QShader::ComputeStage, R"(
//#version 450 core
//
//const float PI = 3.141592;
//const float TwoPI = 2 * PI;
//const float Epsilon = 0.00001;
//
//const uint NumSamples = 64 * 1024;
//const float InvNumSamples = 1.0 / float(NumSamples);
//
//layout(binding = 0) uniform samplerCube inputTexture;
//layout(binding = 1, rgba16f) restrict writeonly uniform imageCube outputTexture;
//
//float radicalInverse_VdC(uint bits)
//{
//	bits = (bits << 16u) | (bits >> 16u);
//	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
//	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
//	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
//	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
//	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
//}
//
//vec2 sampleHammersley(uint i)
//{
//	return vec2(i * InvNumSamples, radicalInverse_VdC(i));
//}
//
//vec3 sampleHemisphere(float u1, float u2)
//{
//	const float u1p = sqrt(max(0.0, 1.0 - u1*u1));
//	return vec3(cos(TwoPI*u2) * u1p, sin(TwoPI*u2) * u1p, u1);
//}
//
//vec3 getSamplingVector()
//{
//    vec2 st = gl_GlobalInvocationID.xy/vec2(imageSize(outputTexture));
//    vec2 uv = 2.0 * vec2(st.x, 1.0-st.y) - vec2(1.0);
//
//    vec3 ret;
//    // Sadly 'switch' doesn't seem to work, at least on NVIDIA.
//    if(gl_GlobalInvocationID.z == 0)      ret = vec3(1.0,  uv.y, -uv.x);
//    else if(gl_GlobalInvocationID.z == 1) ret = vec3(-1.0, uv.y,  uv.x);
//    else if(gl_GlobalInvocationID.z == 2) ret = vec3(uv.x, 1.0, -uv.y);
//    else if(gl_GlobalInvocationID.z == 3) ret = vec3(uv.x, -1.0, uv.y);
//    else if(gl_GlobalInvocationID.z == 4) ret = vec3(uv.x, uv.y, 1.0);
//    else if(gl_GlobalInvocationID.z == 5) ret = vec3(-uv.x, uv.y, -1.0);
//    return normalize(ret);
//}
//
//// Compute orthonormal basis for converting from tanget/shading space to world space.
//void computeBasisVectors(const vec3 N, out vec3 S, out vec3 T)
//{
//	// Branchless select non-degenerate T.
//	T = cross(N, vec3(0.0, 1.0, 0.0));
//	T = mix(cross(N, vec3(1.0, 0.0, 0.0)), T, step(Epsilon, dot(T, T)));
//
//	T = normalize(T);
//	S = normalize(cross(N, T));
//}
//
//// Convert point from tangent/shading space to world space.
//vec3 tangentToWorld(const vec3 v, const vec3 N, const vec3 S, const vec3 T)
//{
//	return S * v.x + T * v.y + N * v.z;
//}
//
//layout(local_size_x=32, local_size_y=32, local_size_z=1) in;
//void main(void)
//{
//	vec3 N = getSamplingVector();
//	
//	vec3 S, T;
//	computeBasisVectors(N, S, T);
//
//	vec3 irradiance = vec3(0);
//	for(uint i=0; i<NumSamples; ++i) {
//		vec2 u  = sampleHammersley(i);
//		vec3 Li = tangentToWorld(sampleHemisphere(u.x, u.y), N, S, T);
//		float cosTheta = max(0.0, dot(Li, N));
//
//		// PIs here cancel out because of division by pdf.
//		irradiance += 2.0 * textureLod(inputTexture, Li, 0).rgb * cosTheta;
//	}
//	irradiance /= vec3(NumSamples);
//
//	imageStore(outputTexture, ivec3(gl_GlobalInvocationID), vec4(irradiance, 1.0));
//}
//
//)");
//	irmapPipeline->setShaderStage({ QRhiShaderStage::Compute,irmapShader });
//	irmapPipeline->create();
//
//	cmdBuffer->beginComputePass();
//	cmdBuffer->setComputePipeline(irmapPipeline.get());
//	cmdBuffer->setShaderResources(irmapShaderBindings.get());
//	cmdBuffer->dispatch(irmapTexture->pixelSize().width() / 32, irmapTexture->pixelSize().height() / 32, 6);
//	cmdBuffer->endComputePass();
//
//
//	BRDFTexture.reset(RHI->newTexture(QRhiTexture::RGBA16F, mSkyCube->pixelSize(), 1, QRhiTexture::Flag::UsedWithLoadStore));
//	BRDFTexture->create();
//
//	BRDFPipeline.reset(RHI->newComputePipeline());
//
//	QVector<QRhiShaderResourceBinding> BRDFBindings;
//	BRDFBindings << QRhiShaderResourceBinding::imageStore(0, QRhiShaderResourceBinding::ComputeStage, BRDFTexture.get(), 0);
//	BRDFShaderBindings.reset(RHI->newShaderResourceBindings());
//	BRDFShaderBindings->setBindings(BRDFBindings.begin(), BRDFBindings.end());
//	BRDFShaderBindings->create();
//	BRDFPipeline->setShaderResourceBindings(BRDFShaderBindings.get());
//	QShader BRDFShader = QRenderSystem::createShaderFromCode(QShader::ComputeStage, R"(
//#version 450 core
//
//const float PI = 3.141592;
//const float TwoPI = 2 * PI;
//const float Epsilon = 0.001; // This program needs larger eps.
//
//const uint NumSamples = 1024;
//const float InvNumSamples = 1.0 / float(NumSamples);
//
//layout(set=0, binding = 0, rgba16f) restrict writeonly uniform image2D LUT;
//
//
//// Compute Van der Corput radical inverse
//// See: http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
//float radicalInverse_VdC(uint bits)
//{
//	bits = (bits << 16u) | (bits >> 16u);
//	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
//	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
//	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
//	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
//	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
//}
//
//// Sample i-th point from Hammersley point set of NumSamples points total.
//vec2 sampleHammersley(uint i)
//{
//	return vec2(i * InvNumSamples, radicalInverse_VdC(i));
//}
//
//// Importance sample GGX normal distribution function for a fixed roughness value.
//// This returns normalized half-vector between Li & Lo.
//// For derivation see: http://blog.tobias-franke.eu/2014/03/30/notes_on_importance_sampling.html
//vec3 sampleGGX(float u1, float u2, float roughness)
//{
//	float alpha = roughness * roughness;
//
//	float cosTheta = sqrt((1.0 - u2) / (1.0 + (alpha*alpha - 1.0) * u2));
//	float sinTheta = sqrt(1.0 - cosTheta*cosTheta); // Trig. identity
//	float phi = TwoPI * u1;
//
//	// Convert to Cartesian upon return.
//	return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
//}
//
//// Single term for separable Schlick-GGX below.
//float gaSchlickG1(float cosTheta, float k)
//{
//	return cosTheta / (cosTheta * (1.0 - k) + k);
//}
//
//// Schlick-GGX approximation of geometric attenuation function using Smith's method (IBL version).
//float gaSchlickGGX_IBL(float cosLi, float cosLo, float roughness)
//{
//	float r = roughness;
//	float k = (r * r) / 2.0; // Epic suggests using this roughness remapping for IBL lighting.
//	return gaSchlickG1(cosLi, k) * gaSchlickG1(cosLo, k);
//}
//
//layout(local_size_x=32, local_size_y=32, local_size_z=1) in;
//void main(void)
//{
//	// Get integration parameters.
//	float cosLo = gl_GlobalInvocationID.x / float(imageSize(LUT).x);
//	float roughness = gl_GlobalInvocationID.y / float(imageSize(LUT).y);
//
//	// Make sure viewing angle is non-zero to avoid divisions by zero (and subsequently NaNs).
//	cosLo = max(cosLo, Epsilon);
//
//	// Derive tangent-space viewing vector from angle to normal (pointing towards +Z in this reference frame).
//	vec3 Lo = vec3(sqrt(1.0 - cosLo*cosLo), 0.0, cosLo);
//
//	// We will now pre-integrate Cook-Torrance BRDF for a solid white environment and save results into a 2D LUT.
//	// DFG1 & DFG2 are terms of split-sum approximation of the reflectance integral.
//	// For derivation see: "Moving Frostbite to Physically Based Rendering 3.0", SIGGRAPH 2014, section 4.9.2.
//	float DFG1 = 0;
//	float DFG2 = 0;
//
//	for(uint i=0; i<NumSamples; ++i) {
//		vec2 u  = sampleHammersley(i);
//
//		// Sample directly in tangent/shading space since we don't care about reference frame as long as it's consistent.
//		vec3 Lh = sampleGGX(u.x, u.y, roughness);
//
//		// Compute incident direction (Li) by reflecting viewing direction (Lo) around half-vector (Lh).
//		vec3 Li = 2.0 * dot(Lo, Lh) * Lh - Lo;
//
//		float cosLi   = Li.z;
//		float cosLh   = Lh.z;
//		float cosLoLh = max(dot(Lo, Lh), 0.0);
//
//		if(cosLi > 0.0) {
//			float G  = gaSchlickGGX_IBL(cosLi, cosLo, roughness);
//			float Gv = G * cosLoLh / (cosLh * cosLo);
//			float Fc = pow(1.0 - cosLoLh, 5);
//
//			DFG1 += (1 - Fc) * Gv;
//			DFG2 += Fc * Gv;
//		}
//	}
//
//	imageStore(LUT, ivec2(gl_GlobalInvocationID), vec4(DFG1, DFG2, 0, 0) * InvNumSamples);
//}
//
//)");
//	BRDFPipeline->setShaderStage({ QRhiShaderStage::Compute,BRDFShader });
//	BRDFPipeline->create();
//
//	cmdBuffer->beginComputePass();
//	cmdBuffer->setComputePipeline(BRDFPipeline.get());
//	cmdBuffer->setShaderResources(BRDFShaderBindings.get());
//	cmdBuffer->dispatch(BRDFTexture->pixelSize().width() / 32, BRDFTexture->pixelSize().height() / 32, 6);
//	cmdBuffer->endComputePass();

}