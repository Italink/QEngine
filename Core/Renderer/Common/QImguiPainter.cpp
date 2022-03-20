#include "QImguiPainter.h"
#include "QApplication"
#include "qevent.h"
#include "QEngine.h"
#include "QGuiApplication"
#include "QClipboard"
#include "QDateTime"

const int64_t IMGUI_BUFFER_SIZE = 10000;

const QHash<int, ImGuiKey> keyMap = {
	{ Qt::Key_Tab, ImGuiKey_Tab },
	{ Qt::Key_Left, ImGuiKey_LeftArrow },
	{ Qt::Key_Right, ImGuiKey_RightArrow },
	{ Qt::Key_Up, ImGuiKey_UpArrow },
	{ Qt::Key_Down, ImGuiKey_DownArrow },
	{ Qt::Key_PageUp, ImGuiKey_PageUp },
	{ Qt::Key_PageDown, ImGuiKey_PageDown },
	{ Qt::Key_Home, ImGuiKey_Home },
	{ Qt::Key_End, ImGuiKey_End },
	{ Qt::Key_Insert, ImGuiKey_Insert },
	{ Qt::Key_Delete, ImGuiKey_Delete },
	{ Qt::Key_Backspace, ImGuiKey_Backspace },
	{ Qt::Key_Space, ImGuiKey_Space },
	{ Qt::Key_Enter, ImGuiKey_Enter },
	{ Qt::Key_Return, ImGuiKey_Enter },
	{ Qt::Key_Escape, ImGuiKey_Escape },
	{ Qt::Key_A, ImGuiKey_A },
	{ Qt::Key_C, ImGuiKey_C },
	{ Qt::Key_V, ImGuiKey_V },
	{ Qt::Key_X, ImGuiKey_X },
	{ Qt::Key_Y, ImGuiKey_Y },
	{ Qt::Key_Z, ImGuiKey_Z },
	{ Qt::MiddleButton, ImGuiMouseButton_Middle }
};

const QHash<ImGuiMouseCursor, Qt::CursorShape> cursorMap = {
	{ ImGuiMouseCursor_Arrow,      Qt::CursorShape::ArrowCursor },
	{ ImGuiMouseCursor_TextInput,  Qt::CursorShape::IBeamCursor },
	{ ImGuiMouseCursor_ResizeAll,  Qt::CursorShape::SizeAllCursor },
	{ ImGuiMouseCursor_ResizeNS,   Qt::CursorShape::SizeVerCursor },
	{ ImGuiMouseCursor_ResizeEW,   Qt::CursorShape::SizeHorCursor },
	{ ImGuiMouseCursor_ResizeNESW, Qt::CursorShape::SizeBDiagCursor },
	{ ImGuiMouseCursor_ResizeNWSE, Qt::CursorShape::SizeFDiagCursor },
	{ ImGuiMouseCursor_Hand,       Qt::CursorShape::PointingHandCursor },
	{ ImGuiMouseCursor_NotAllowed, Qt::CursorShape::ForbiddenCursor },
};

QByteArray g_currentClipboardText;

QImguiPainter::QImguiPainter()
{
	mImGuiContext = ImGui::CreateContext();
	ImGui::SetCurrentContext(mImGuiContext);

	ImGuiIO& io = ImGui::GetIO();
	io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors; // We can honor GetMouseCursor() values (optional)
	io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;  // We can honor io.WantSetMousePos requests (optional, rarely used)
	io.BackendPlatformName = "Qt ImGUI";
	io.SetClipboardTextFn = [](void* user_data, const char* text) {
		Q_UNUSED(user_data);
		QGuiApplication::clipboard()->setText(text);
	};

	io.GetClipboardTextFn = [](void* user_data) {
		Q_UNUSED(user_data);
		g_currentClipboardText = QGuiApplication::clipboard()->text().toUtf8();
		return (const char*)g_currentClipboardText.data();
	};
}


void QImguiPainter::updatePrePass(QRhiResourceUpdateBatch* batch, QRhiRenderTarget* outputTarget)
{
	if (!mPipeline) {
		initRhiResource(batch, outputTarget);
	}
	ImGui::SetCurrentContext(mImGuiContext);
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2(outputTarget->pixelSize().width(), outputTarget->pixelSize().height());
	io.DisplayFramebufferScale = ImVec2(1,1);
	double current_time = QDateTime::currentMSecsSinceEpoch() / double(1000);
	io.DeltaTime = mTime > 0.0 ? (float)(current_time - mTime) : (float)(1.0f / 60.0f);
	mTime = current_time;
	if (io.WantSetMousePos) {
		const QPoint global_pos = mWindow->mapToGlobal(QPoint{ (int)io.MousePos.x, (int)io.MousePos.y });
		QCursor cursor = mWindow->cursor();
		cursor.setPos(global_pos);
		mWindow->setCursor(cursor);
	}
	if (mWindow->isActive()) {
		const QPoint pos = mWindow->mapFromGlobal(QCursor::pos());
		io.MousePos = ImVec2(pos.x() * mWindow->devicePixelRatio(), pos.y() * mWindow->devicePixelRatio());  // Mouse position in screen coordinates (set to -1,-1 if no mouse / on another screen, etc.)
	}
	else {
		io.MousePos = ImVec2(-1, -1);
	}
	for (int i = 0; i < 3; i++) {
		io.MouseDown[i] = mMousePressed[i];
	}
	io.MouseWheelH = mMouseWheelH;
	io.MouseWheel = mMouseWheel;
	mMouseWheelH = 0;
	mMouseWheel = 0;
	if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)
		return;
	const ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
	if (io.MouseDrawCursor || (imgui_cursor == ImGuiMouseCursor_None)) {
		mWindow->setCursor(Qt::CursorShape::BlankCursor);
	}
	else {
		const auto cursor_it = cursorMap.constFind(imgui_cursor);
		if (cursor_it != cursorMap.constEnd()) {
			const Qt::CursorShape qt_cursor_shape = *(cursor_it);
			mWindow->setCursor(qt_cursor_shape);
		}
		else {
			mWindow->setCursor(Qt::CursorShape::ArrowCursor);
		}
	}
	ImGui::SetCurrentContext(mImGuiContext);
	ImGui::NewFrame();
	paint();
	ImGui::Render();
	ImDrawData* draw_data = ImGui::GetDrawData();
	int64_t vertexBufferOffset = 0;
	int64_t indexBufferOffset = 0;
	for (int i = 0; i < draw_data->CmdListsCount; i++) {
		const ImDrawList* cmd_list = draw_data->CmdLists[i];
		batch->updateDynamicBuffer(mVertexBuffer.get(), vertexBufferOffset, cmd_list->VtxBuffer.size_in_bytes(), cmd_list->VtxBuffer.Data);
		batch->updateDynamicBuffer(mIndexBuffer.get(), indexBufferOffset, cmd_list->IdxBuffer.size_in_bytes(), cmd_list->IdxBuffer.Data);
		vertexBufferOffset += cmd_list->VtxBuffer.size_in_bytes();
		indexBufferOffset += cmd_list->IdxBuffer.size_in_bytes();

		QMatrix4x4 MVP = RHI->clipSpaceCorrMatrix();
		QRect rect(0, 0, outputTarget->pixelSize().width(), outputTarget->pixelSize().height());
		MVP.ortho(rect);
		batch->updateDynamicBuffer(mUniformBuffer.get(), 0, sizeof(QMatrix4x4), MVP.constData());
	}
}


void QImguiPainter::drawInPass(QRhiCommandBuffer* cmdBuffer, QRhiRenderTarget* outputTarget)
{
	ImDrawData* draw_data = ImGui::GetDrawData();
	int64_t vertexBufferOffset = 0;
	int64_t indexBufferOffset = 0;
	for (int i = 0; i < draw_data->CmdListsCount; i++) {
		const ImDrawList* cmd_list = draw_data->CmdLists[i];
		cmdBuffer->setGraphicsPipeline(mPipeline.get());
		cmdBuffer->setViewport(QRhiViewport(0, 0, outputTarget->pixelSize().width(), outputTarget->pixelSize().height()));
		cmdBuffer->setShaderResources();
		const QRhiCommandBuffer::VertexInput VertexInput(mVertexBuffer.get(), vertexBufferOffset);
		cmdBuffer->setVertexInput(0, 1, &VertexInput, mIndexBuffer.get(), indexBufferOffset, sizeof(ImDrawIdx) == 2? QRhiCommandBuffer::IndexUInt16: QRhiCommandBuffer::IndexUInt32);
		vertexBufferOffset += cmd_list->VtxBuffer.size_in_bytes();
		indexBufferOffset += cmd_list->IdxBuffer.size_in_bytes();
		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			if (pcmd->UserCallback) {
				pcmd->UserCallback(cmd_list, pcmd);
			}
			else {
				QRhiScissor scissor;
				scissor.setScissor(pcmd->ClipRect.x, pcmd->ClipRect.y, pcmd->ClipRect.z , pcmd->ClipRect.w );
				cmdBuffer->setScissor(scissor);
				cmdBuffer->drawIndexed(pcmd->ElemCount, 1, pcmd->IdxOffset, pcmd->VtxOffset, 0);
			}
		}
	}
}

void QImguiPainter::setupWindow(QRhiWindow* window)
{
	mWindow = window;
	mWindow->installEventFilter(this);
}

void QImguiPainter::initRhiResource(QRhiResourceUpdateBatch* batch, QRhiRenderTarget* outputTarget)
{
	mVertexBuffer.reset(RHI->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, sizeof(ImDrawVert) * IMGUI_BUFFER_SIZE));
	Q_ASSERT(mVertexBuffer->create());

	mIndexBuffer.reset(RHI->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::IndexBuffer, sizeof(ImDrawIdx) * IMGUI_BUFFER_SIZE));
	Q_ASSERT(mIndexBuffer->create());

	mUniformBuffer.reset(RHI->newBuffer(QRhiBuffer::Type::Dynamic, QRhiBuffer::UniformBuffer, sizeof(QMatrix4x4)));
	Q_ASSERT(mUniformBuffer->create());

	mSampler.reset(RHI->newSampler(QRhiSampler::Linear,
				   QRhiSampler::Linear,
				   QRhiSampler::None,
				   QRhiSampler::ClampToEdge,
				   QRhiSampler::ClampToEdge));
	mSampler->create();

	unsigned char* pixels;
	int width, height;
	ImGui::SetCurrentContext(mImGuiContext);
	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	QImage fontImage(pixels, width, height, QImage::Format::Format_RGBA8888);
	mFontTexture.reset(RHI->newTexture(QRhiTexture::RGBA8, QSize(width, height)));
	mFontTexture->create();
	batch->uploadTexture(mFontTexture.get(), fontImage);

	mPipeline.reset(RHI->newGraphicsPipeline());
	QRhiGraphicsPipeline::TargetBlend blendState;
	blendState.enable = true;
	mPipeline->setDepthTest(true);
	mPipeline->setTargetBlends({ blendState,blendState });
	mPipeline->setFlags(QRhiGraphicsPipeline::UsesScissor);
	mPipeline->setSampleCount(outputTarget->sampleCount());
	QShader vs = QSceneRenderer::createShaderFromCode(QShader::VertexStage, R"(#version 440
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;

layout(location = 0)out vec2 vUV;
layout(location = 1)out vec4 vColor;
layout(std140,binding = 0) uniform buf{
	mat4 mvp;
}ubuf;

out gl_PerVertex { vec4 gl_Position; };

void main(){
	vUV = inUV;
	vColor = inColor;
	gl_Position = ubuf.mvp*vec4(inPosition,0,1);
}
)");

	QShader fs = QSceneRenderer::createShaderFromCode(QShader::FragmentStage, R"(#version 440
layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;
layout(binding = 1) uniform sampler2D uTexture;
layout(location = 0) out vec4 OutColor;
layout(location = 1) out vec4 OutId;
void main(){
	OutColor = vec4((vColor * texture(uTexture,vUV)).rgb,1.0);
	OutId = vec4(1);
}
)");

	mPipeline->setShaderStages({
		{ QRhiShaderStage::Vertex, vs },
		{ QRhiShaderStage::Fragment, fs }
							   });

	mBindings.reset(RHI->newShaderResourceBindings());
	mBindings->setBindings({
		QRhiShaderResourceBinding::uniformBuffer(0,QRhiShaderResourceBinding::VertexStage,mUniformBuffer.get()),
		QRhiShaderResourceBinding::sampledTexture(1,QRhiShaderResourceBinding::FragmentStage,mFontTexture.get(),mSampler.get())
						   });
	mBindings->create();

	QRhiVertexInputLayout inputLayout;
	inputLayout.setBindings({ QRhiVertexInputBinding(sizeof(ImDrawVert)) });
	inputLayout.setAttributes({
		QRhiVertexInputAttribute{ 0, 0, QRhiVertexInputAttribute::Float2, offsetof(ImDrawVert, pos) },
		QRhiVertexInputAttribute{ 0, 1, QRhiVertexInputAttribute::Float2, offsetof(ImDrawVert, uv) },
		QRhiVertexInputAttribute{ 0, 2, QRhiVertexInputAttribute::UNormByte4, offsetof(ImDrawVert, col) },
							  });
	mPipeline->setVertexInputLayout(inputLayout);
	mPipeline->setShaderResourceBindings(mBindings.get());
	mPipeline->setRenderPassDescriptor(outputTarget->renderPassDescriptor());
	mPipeline->create();

}

bool QImguiPainter::eventFilter(QObject* watched, QEvent* event)
{
	if (watched != nullptr) {
		switch (event->type()) {
		case QEvent::MouseButtonPress:
		case QEvent::MouseButtonRelease: {
			QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
			mMousePressed[0] = mouseEvent->buttons() & Qt::LeftButton;
			mMousePressed[1] = mouseEvent->buttons() & Qt::RightButton;
			mMousePressed[2] = mouseEvent->buttons() & Qt::MiddleButton;
			break;
		}
		case QEvent::Wheel: {
			QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);
			ImGui::SetCurrentContext(mImGuiContext);
			if (wheelEvent->pixelDelta().x() != 0) {
				mMouseWheelH += wheelEvent->pixelDelta().x() / (ImGui::GetTextLineHeight());
			}
			else {
				mMouseWheelH += wheelEvent->angleDelta().x() / 120;
			}
			if (wheelEvent->pixelDelta().y() != 0) {
				mMouseWheel += wheelEvent->pixelDelta().y() / (5.0 * ImGui::GetTextLineHeight());
			}
			else {
				mMouseWheel += wheelEvent->angleDelta().y() / 120;
			}
			break;
		}
		case QEvent::KeyPress:
		case QEvent::KeyRelease: {
			QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
			ImGui::SetCurrentContext(mImGuiContext);
			ImGuiIO& io = ImGui::GetIO();
			const bool key_pressed = (event->type() == QEvent::KeyPress);
			const auto key_it = keyMap.constFind(keyEvent->key());
			if (key_it != keyMap.constEnd()) {
				const int imgui_key = *(key_it);
				io.AddKeyEvent(imgui_key, key_pressed);
			}

			if (key_pressed) {
				const QString text = keyEvent->text();
				if (text.size() == 1) {
					io.AddInputCharacter(text.at(0).unicode());
				}
			}
		#ifdef Q_OS_MAC
			io.KeyCtrl = keyEvent->modifiers() & Qt::MetaModifier;
			io.KeyShift = keyEvent->modifiers() & Qt::ShiftModifier;
			io.KeyAlt = keyEvent->modifiers() & Qt::AltModifier;
			io.KeySuper = keyEvent->modifiers() & Qt::ControlModifier;
		#else
			io.KeyCtrl = keyEvent->modifiers() & Qt::ControlModifier;
			io.KeyShift = keyEvent->modifiers() & Qt::ShiftModifier;
			io.KeyAlt = keyEvent->modifiers() & Qt::AltModifier;
			io.KeySuper = keyEvent->modifiers() & Qt::MetaModifier;
		#endif
			break;
		}
		default:
			break;
		}
	}
	return QObject::eventFilter(watched, event);
}


