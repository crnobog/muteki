#ifdef _MSC_VER
#    pragma comment(linker, "/subsystem:windows /ENTRY:mainCRTStartup")
#endif

#include "GPU_DX12/GPU_DX12.h"
#include "GPU_DX11/GPU_DX11.h"
#include "Vectors.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"

#include "mu-core/Paths.h"
#include "mu-core/Debug.h"
#include "mu-core/String.h"
#include "mu-core/FileReader.h"

#include "imgui.h"

using namespace mu;

PointerRange<const char> GetShaderDirectory() {
	struct Initializer {
		String path;
		Initializer() {
			auto exe_dir = paths::GetExecutableDirectory();
			path = String{ exe_dir, Range("../Shaders/") };
		}
	};
	static Initializer init;
	return Range(init.path);
}

struct ImGuiImpl {
	HWND m_hwnd;
	GPUInterface* m_gpu = nullptr;
	GPU::PipelineStateID m_pipeline_state = {};
	GPU::TextureID m_fonts_tex = {};
	GPU::ShaderResourceListID m_fonts_srl = {};

	void Init(HWND hwnd, GPUInterface* gpu) {
		m_hwnd = hwnd;
		m_gpu = gpu;
		ImGuiIO& io = ImGui::GetIO();
		io.ImeWindowHandle = hwnd;

		GPU::PipelineStateDesc pipeline_state_desc = {};

		String shader_filename{ GetShaderDirectory(), "imgui.hlsl" };
		Array<u8> shader_txt_code = LoadFileToArray(shader_filename.GetRaw(), FileReadType::Text);
		GPU::VertexShaderID vshader_id = gpu->CompileVertexShaderHLSL("vs_main", Range(shader_txt_code));
		GPU::PixelShaderID pshader_id = gpu->CompilePixelShaderHLSL("ps_main", Range(shader_txt_code));
		pipeline_state_desc.Program = gpu->LinkProgram(vshader_id, pshader_id);

		pipeline_state_desc.StreamFormat.AddSlot()
			.Add({ GPU::ScalarType::Float, 2, GPU::InputSemantic::Position, 0 })
			.Add({ GPU::ScalarType::Float, 2, GPU::InputSemantic::Texcoord, 0 })
			.Add({ GPU::ScalarType::U8,	   4, GPU::InputSemantic::Color, 0, true });

		pipeline_state_desc.BlendState.BlendEnable = true;
		pipeline_state_desc.BlendState.ColorBlend = { GPU::BlendValue::SourceAlpha, GPU::BlendOp::Add, GPU::BlendValue::InverseSourceAlpha };
		pipeline_state_desc.BlendState.AlphaBlend = { GPU::BlendValue::InverseSourceAlpha, GPU::BlendOp::Add, GPU::BlendValue::SourceAlpha };

		m_pipeline_state = gpu->CreatePipelineState(pipeline_state_desc); // TODO: Correct blend state etc
		
		//io.Fonts->AddFontDefault();
		u8* pixels = nullptr;
		i32 width=0, height=0;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
		m_fonts_tex = gpu->CreateTexture2D(width, height, GPU::TextureFormat::RGBA8, Range(pixels, pixels + width * 4 * height)); 

		GPU::ShaderResourceListDesc srl_desc;
		srl_desc.StartSlot = 0;
		srl_desc.Textures.Add(m_fonts_tex);
		m_fonts_srl = gpu->CreateShaderResourceList(srl_desc);

		io.Fonts->SetTexID((void*)(uptr)m_fonts_srl);
	}

	void Shutdown() {
		ImGui::Shutdown();
	}

	void BeginFrame() {
		ImGuiIO& io = ImGui::GetIO();

		RECT rect;
		GetClientRect(m_hwnd, &rect);
		io.DisplaySize = ImVec2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));

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
		Mat4x4 orth_proj{
			2.0f / (R - L),   0.0f,           0.0f,       0.0f, 
			0.0f,         2.0f / (T - B),     0.0f,       0.0f ,
			0.0f,         0.0f,           0.5f,       0.0f, 
			(R + L) / (L - R),  (T + B) / (B - T),    0.5f,       1.0f,
		};
		auto cbuffer_id = gpu_frame->GetTemporaryConstantBuffer(ByteRange(orth_proj));

		for (const ImDrawList* draw_list : Range(draw_data->CmdLists, draw_data->CmdListsCount)) {
			Vec4 last_clip_rect{ 0,0,0,0 };
			GPU::RenderPass* current_pass = nullptr;
			Array<GPU::RenderPass> passes;
			Array<GPU::DrawItem> gpu_cmds;
			gpu_cmds.AddZeroed(draw_list->CmdBuffer.Size);
			PointerRange<GPU::DrawItem> gpu_cmd_cursor{ Range(gpu_cmds) };

			GPU::VertexBufferID vb_id = gpu_frame->GetTemporaryVertexBuffer(mu::ByteRange(draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size));
			GPU::IndexBufferID ib_id = gpu_frame->GetTemporaryIndexBuffer(mu::ByteRange(draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size));
			GPU::StreamSetup stream_setup;
			stream_setup.VertexBuffers.Add(vb_id);
			stream_setup.IndexBuffer = ib_id;

			u32 index_offset = 0;
			for (const ImDrawCmd& draw_cmd : draw_list->CmdBuffer) {
				CHECK(!draw_cmd.UserCallback);
				if (!current_pass || Vec4(draw_cmd.ClipRect) != last_clip_rect) {
					// Start new pass to change clip rect
					// TODO: Reconsider where clip rect fits into api
					last_clip_rect = draw_cmd.ClipRect;
					current_pass = &passes.AddZeroed();
					current_pass->ClipRect = { last_clip_rect.X, last_clip_rect.Y, last_clip_rect.Z, last_clip_rect.W };
					current_pass->DrawItems = { gpu_cmd_cursor.m_start, 0 };
					current_pass->RenderTargets.Add(GPU::RenderTargetID{});
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

using Color4 = Vector<u8, 4>;

int main(int, char**) {
	dbg::Log("Running from: ", paths::GetExecutableDirectory());

	if (glfwInit() == GLFW_FALSE) {
		return 1;
	}

	GLFWwindow* win = glfwCreateWindow(800, 600, "muteki", nullptr, nullptr);
	if (!win) {
		return 1;
	}

	GPUInterface* gpu = CreateGPU_DX11();
	gpu->Init();
	HWND hwnd = glfwGetWin32Window(win);
	gpu->RecreateSwapChain(hwnd, 800, 600);

	ImGuiImpl imgui;
	imgui.Init(hwnd, gpu);

	String shader_filename{ GetShaderDirectory(), "basic_shader.hlsl" };
	Array<u8> shader_txt_code = LoadFileToArray(shader_filename.GetRaw(), FileReadType::Text);
	GPU::VertexShaderID vshader_id = gpu->CompileVertexShaderHLSL("vs_main", Range(shader_txt_code));
	GPU::PixelShaderID pshader_id = gpu->CompilePixelShaderHLSL("ps_main", Range(shader_txt_code));
	GPU::ProgramID program_id = gpu->LinkProgram(vshader_id, pshader_id);

	GPU::PipelineStateDesc pipeline_state_desc{};
	pipeline_state_desc.Program = program_id;
	pipeline_state_desc.StreamFormat.AddSlot().Set({ GPU::ScalarType::Float, 3, GPU::InputSemantic::Position, 0 });
	pipeline_state_desc.BlendState.BlendEnable = false;
	GPU::PipelineStateID pipeline_state = gpu->CreatePipelineState(pipeline_state_desc);

	struct Vertex {
		Vec3 position;
	};
	Array<Vertex> triangle_vertices;
	triangle_vertices.Emplace({ { 0.0f, 0.5f, 0.0f } });
	triangle_vertices.Emplace({ { 0.5f, -0.5f, 0.0f } });
	triangle_vertices.Emplace({ { -0.5f, -0.5f, 0.0f } });
	GPU::VertexBufferID vbuffer_id = gpu->CreateVertexBuffer(Range(triangle_vertices.Bytes(), triangle_vertices.NumBytes()));

	struct CBuffer_Color {
		Vec4 color;
	};
	CBuffer_Color cbuffer = { { 1.0f, 0.5f, 0.0f, 1.0f } };
	GPU::ConstantBufferID cbuffer_id = gpu->CreateConstantBuffer(Range((u8*)&cbuffer, sizeof(cbuffer)));

	Color4 pixels[] = {
		{ 255, 0, 0, 255 }, { 0, 255, 0, 255 },
		{ 0, 255, 0, 255 }, { 255, 0, 0, 255 },
	};
	GPU::TextureID texture_id = gpu->CreateTexture2D(2, 2, GPU::TextureFormat::RGBA8, Range((u8*)pixels, sizeof(pixels)));
	GPU::ShaderResourceListDesc resource_list_desc = {};
	resource_list_desc.StartSlot = 0;
	resource_list_desc.Textures.Add(texture_id);
	GPU::ShaderResourceListID resource_list_id = gpu->CreateShaderResourceList(resource_list_desc);

	bool show_test_window = true;
	while (glfwWindowShouldClose(win) == false) {
		glfwPollEvents();

		auto* gpu_frame = gpu->BeginFrame();
		imgui.BeginFrame();

		GPU::DrawItem draw_item = {};
		// default raster state, blend state, depth stenci state
		draw_item.PipelineState = pipeline_state;
		draw_item.BoundResources.ConstantBuffers.Add(cbuffer_id);
		draw_item.BoundResources.ResourceLists.Add(resource_list_id);
		draw_item.Command.VertexOrIndexCount = 3;
		draw_item.Command.PrimTopology = GPU::PrimitiveTopology::TriangleList;
		draw_item.StreamSetup.VertexBuffers.Add(vbuffer_id);

		GPU::RenderPass pass;
		pass.RenderTargets.Add(GPU::BackBufferID);
		pass.DrawItems = mu::Range(&draw_item, &draw_item+1);
		gpu->SubmitPass(pass);

		if (show_test_window) {
			ImGui::SetNextWindowPos(ImVec2(400, 20), ImGuiSetCond_FirstUseEver);
			ImGui::ShowTestWindow(&show_test_window);
		}
		imgui.Render(gpu_frame);

		gpu->EndFrame(gpu_frame);
	}

	imgui.Shutdown();
	gpu->Shutdown();
	delete gpu;


	return 0;
}

