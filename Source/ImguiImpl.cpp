#include "ImguiImpl.h"
#include "TransformMatrices.h"

#include "mu-core/PointerRange.h"
#include "mu-core/WindowsWrapped.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"

#include "imgui.h"

using namespace mu;

struct ImGuiImpl : public ImguiImplInterface {
	ImGuiContext* m_context = nullptr;
	void* m_hwnd = nullptr;
	GPUInterface* m_gpu = nullptr;
	ShaderManager* m_shader_manager = nullptr;
	GPU::PipelineStateID m_pipeline_state = {};
	GPU::TextureID m_fonts_tex = {};
	GPU::ShaderResourceListID m_fonts_srl = {};
	GPU::FramebufferID m_framebuffer = {};

	void Init(void* hwnd, GPUInterface* gpu, ShaderManager* shader_manager) {
		m_context = ImGui::CreateContext(nullptr);
		ImGui::SetCurrentContext(m_context);
		m_hwnd = hwnd;
		m_gpu = gpu;
		m_shader_manager = shader_manager;
		ImGuiIO& io = ImGui::GetIO();
		io.ImeWindowHandle = hwnd;
		io.IniFilename = nullptr;
		io.LogFilename = nullptr;

		io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;                         // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array.
		io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
		io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
		io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
		io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
		io.KeyMap[ImGuiKey_PageUp] = GLFW_KEY_PAGE_UP;
		io.KeyMap[ImGuiKey_PageDown] = GLFW_KEY_PAGE_DOWN;
		io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
		io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
		io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
		io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
		io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
		io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
		io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
		io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
		io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
		io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
		io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
		io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;

		RECT rect;
		GetClientRect((HWND)m_hwnd, &rect); // TODO: Avoid windows API
		io.DisplaySize = ImVec2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));

		GPU::PipelineStateDesc pipeline_state_desc = {};

		GPU::ShaderID vshader_id = shader_manager->CompileShader(GPU::ShaderType::Vertex, "imgui");
		GPU::ShaderID pshader_id = shader_manager->CompileShader(GPU::ShaderType::Pixel, "imgui");
		pipeline_state_desc.Program = gpu->LinkProgram({ vshader_id, pshader_id });

		pipeline_state_desc.StreamFormat.AddSlot({
			{ GPU::ScalarType::Float,	2, 0 },
			{ GPU::ScalarType::Float,	2, 1 },
			{ GPU::ScalarType::U8,		4, 2, true }
			});

		pipeline_state_desc.BlendState.BlendEnable = true;
		pipeline_state_desc.BlendState.ColorBlend = { GPU::BlendValue::SourceAlpha, GPU::BlendOp::Add, GPU::BlendValue::InverseSourceAlpha };
		pipeline_state_desc.BlendState.AlphaBlend = { GPU::BlendValue::InverseSourceAlpha, GPU::BlendOp::Add, GPU::BlendValue::Zero };

		pipeline_state_desc.RasterState.CullMode = GPU::CullMode::None;
		pipeline_state_desc.RasterState.ScissorEnable = true;
		pipeline_state_desc.DepthStencilState.DepthEnable = false;

		m_pipeline_state = gpu->CreatePipelineState(pipeline_state_desc); // TODO: Correct blend state etc

		io.Fonts->AddFontDefault();
		u8* pixels = nullptr;
		i32 width = 0, height = 0;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
		m_fonts_tex = gpu->CreateTexture2D(width, height, GPU::TextureFormat::RGBA8, Range(pixels, pixels + width * 4 * height));

		GPU::ShaderResourceListDesc srl_desc;
		srl_desc.StartSlot = 0;
		srl_desc.Textures.Add(m_fonts_tex);
		m_fonts_srl = gpu->CreateShaderResourceList(srl_desc);

		io.Fonts->SetTexID((void*)(uptr)m_fonts_srl);

		GPU::FramebufferDesc framebuffer_desc = {};
		framebuffer_desc.RenderTargets.Add(GPU::BackBufferID);
		m_framebuffer = gpu->CreateFramebuffer(framebuffer_desc);
	}

	void Shutdown() {
		ImGui::DestroyContext();
	}

	void BeginFrame(Vector<double, 2> mouse_pos, Vector<bool, 3> mouse_buttons, Vec2 scroll) {
		ImGuiIO& io = ImGui::GetIO();

		Vector<u32, 2> current = m_gpu->GetSwapChainDimensions();
		io.DisplaySize = ImVec2((float)current[0], (float)current[1]);

		io.MousePos = ImVec2{ (float)mouse_pos.X, (float)mouse_pos.Y };
		io.MouseWheelH = scroll.X;
		io.MouseWheel = scroll.Y;
		io.MouseDown[0] = mouse_buttons[0];
		io.MouseDown[1] = mouse_buttons[1];
		io.MouseDown[2] = mouse_buttons[2];

		static double last_time = glfwGetTime();
		io.DeltaTime = (float)(glfwGetTime() - last_time);
		last_time = glfwGetTime();

		ImGui::NewFrame();
	}

	void Render(GPUFrameInterface* gpu_frame) {
		ImGui::Render();
		ImDrawData* draw_data = ImGui::GetDrawData();

		// TODO: Update constant buffer 
		float L = 0.0f;
		float R = ImGui::GetIO().DisplaySize.x;
		float B = ImGui::GetIO().DisplaySize.y;
		float T = 0.0f;
		Mat4x4 orth_proj = CreateOrthographicProjection(L, R, T, B, -1, 1);
		auto cbuffer_id = gpu_frame->GetTemporaryConstantBuffer(ByteRange(orth_proj));

		for (const ImDrawList* draw_list : Range(draw_data->CmdLists, draw_data->CmdListsCount)) {
			Vec4 last_clip_rect{ 0,0,0,0 };
			GPU::RenderPass* current_pass = nullptr;
			Array<GPU::RenderPass> passes;
			Array<GPU::DrawItem> gpu_cmds;
			gpu_cmds.AddZeroed(draw_list->CmdBuffer.Size);
			PointerRange<GPU::DrawItem> gpu_cmd_cursor{ Range(gpu_cmds) };

			GPU::VertexBufferID vb_id = gpu_frame->GetTemporaryVertexBuffer(ByteRange(draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size), 12); // TODO: Determine alignment properly from stream format
			GPU::IndexBufferID ib_id = gpu_frame->GetTemporaryIndexBuffer(ByteRange(draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size));
			GPU::StreamSetup stream_setup;
			stream_setup.VertexBuffers.Add(vb_id);
			stream_setup.IndexBuffer = ib_id;

			u32 index_offset = 0;
			for (const ImDrawCmd& draw_cmd : draw_list->CmdBuffer) {
				Assert(!draw_cmd.UserCallback);
				Vec4 clip_rect = draw_cmd.ClipRect;
				if (!current_pass || clip_rect != last_clip_rect) {
					// Start new pass to change clip rect
					// TODO: Reconsider where clip rect fits into api
					last_clip_rect = clip_rect;
					current_pass = &passes.AddDefaulted(1).Front();
					// left top right bottom
					current_pass->ClipRect = { (u32)last_clip_rect.X, (u32)last_clip_rect.Y, (u32)last_clip_rect.Z, (u32)last_clip_rect.W };
					current_pass->DrawItems = { gpu_cmd_cursor.m_start, 0 };
					current_pass->Framebuffer = m_framebuffer;
					current_pass->Name = "IMGUI";
				}

				GPU::DrawItem* new_item = gpu_cmd_cursor.m_start;
				new_item->BoundResources.ResourceLists.Add(GPU::ShaderResourceListID{ (uptr)draw_cmd.TextureId });
				new_item->BoundResources.ConstantBuffers.Add(cbuffer_id);
				new_item->PipelineState = m_pipeline_state;
				new_item->Command.PrimTopology = GPU::PrimitiveTopology::TriangleList;
				new_item->Command.VertexOrIndexCount = draw_cmd.ElemCount;
				new_item->Command.IndexOffset = index_offset;
				new_item->StreamSetup = stream_setup;
				gpu_cmd_cursor.Advance();
				current_pass->DrawItems.m_end = gpu_cmd_cursor.m_start;
				index_offset += draw_cmd.ElemCount;
			}

			for (const GPU::RenderPass& pass : passes) {
				m_gpu->SubmitPass(pass);
			}
		}
	}
};


std::unique_ptr<ImguiImplInterface> CreateImguiImpl() {
	return std::make_unique< ImGuiImpl>();
}
