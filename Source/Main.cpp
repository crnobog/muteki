#ifdef _MSC_VER
#    pragma comment(linker, "/subsystem:windows /ENTRY:mainCRTStartup")
#endif

#include "GPU_DX12/GPU_DX12.h"
#include "GPU_DX11/GPU_DX11.h"
#include "Vectors.h"
#include "TransformMatrices.h"
#include "CoreMath.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"

#include "mu-core/Paths.h"
#include "mu-core/Debug.h"
#include "mu-core/String.h"
#include "mu-core/FileReader.h"

#include "imgui.h"

#include <memory>

using namespace mu;
using std::unique_ptr;

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
		GetClientRect(m_hwnd, &rect);
		io.DisplaySize = ImVec2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));

		GPU::PipelineStateDesc pipeline_state_desc = {};

		String shader_filename{ GetShaderDirectory(), "imgui.hlsl" };
		Array<u8> shader_txt_code = LoadFileToArray(shader_filename.GetRaw(), FileReadType::Text); // TODO: Handle unicode BOM/other encodings
		GPU::VertexShaderID vshader_id = gpu->CompileVertexShaderHLSL("vs_main", Range(shader_txt_code));
		GPU::PixelShaderID pshader_id = gpu->CompilePixelShaderHLSL("ps_main", Range(shader_txt_code));
		pipeline_state_desc.Program = gpu->LinkProgram(vshader_id, pshader_id);

		pipeline_state_desc.StreamFormat.AddSlot({
			{ GPU::ScalarType::Float,	2, GPU::InputSemantic::Position,	0 },
			{ GPU::ScalarType::Float,	2, GPU::InputSemantic::Texcoord,	0 },
			{ GPU::ScalarType::U8,		4, GPU::InputSemantic::Color,		0, true }
		});

		pipeline_state_desc.BlendState.BlendEnable = true;
		pipeline_state_desc.BlendState.ColorBlend = { GPU::BlendValue::SourceAlpha, GPU::BlendOp::Add, GPU::BlendValue::InverseSourceAlpha };
		pipeline_state_desc.BlendState.AlphaBlend = { GPU::BlendValue::InverseSourceAlpha, GPU::BlendOp::Add, GPU::BlendValue::Zero };

		pipeline_state_desc.RasterState.CullMode = GPU::CullMode::None;
		pipeline_state_desc.RasterState.ScissorEnable = true;

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
	}

	void Shutdown() {
		ImGui::Shutdown();
	}

	void BeginFrame(Vector<double, 2> mouse_pos, Vector<bool, 3> mouse_buttons) {
		ImGuiIO& io = ImGui::GetIO();

		Vector<u32, 2> current = m_gpu->GetSwapChainDimensions();
		io.DisplaySize = ImVec2((float)current[0], (float)current[1]);

		io.MousePos = ImVec2{ (float)mouse_pos.X, (float)mouse_pos.Y };
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

			GPU::VertexBufferID vb_id = gpu_frame->GetTemporaryVertexBuffer(mu::ByteRange(draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size));
			GPU::IndexBufferID ib_id = gpu_frame->GetTemporaryIndexBuffer(mu::ByteRange(draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size));
			GPU::StreamSetup stream_setup;
			stream_setup.VertexBuffers.Add(vb_id);
			stream_setup.IndexBuffer = ib_id;

			u32 index_offset = 0;
			for (const ImDrawCmd& draw_cmd : draw_list->CmdBuffer) {
				Assert(!draw_cmd.UserCallback);
				if (!current_pass || Vec4(draw_cmd.ClipRect) != last_clip_rect) {
					// Start new pass to change clip rect
					// TODO: Reconsider where clip rect fits into api
					last_clip_rect = draw_cmd.ClipRect;
					current_pass = &passes.AddZeroed();
					// left top right bottom
					current_pass->ClipRect = { (u32)last_clip_rect.X, (u32)last_clip_rect.Y, (u32)last_clip_rect.Z, (u32)last_clip_rect.W };
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

bool mouse_pressed[3] = { false, };
void OnMouseButton(GLFWwindow*, i32 button, i32 action, i32) {
	if (action == GLFW_PRESS && button >= 0 && button < 3) {
		mouse_pressed[button] = true;
	}
}

void OnChar(GLFWwindow*, u32 c) {
	ImGuiIO& io = ImGui::GetIO();
	if (c > 0 && c < 0x10000)
		io.AddInputCharacter((u16)c);
}

void OnKey(GLFWwindow*, i32 key, i32, i32 action, i32) {
	ImGuiIO& io = ImGui::GetIO();
	if (action == GLFW_PRESS)
		io.KeysDown[key] = true;
	if (action == GLFW_RELEASE)
		io.KeysDown[key] = false;

	io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
	io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
	io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
	io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];
}

int main(int, char**) {
	dbg::Log("Running from: {}", paths::GetExecutableDirectory());

	if (glfwInit() == GLFW_FALSE) {
		return 1;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* win = glfwCreateWindow(1600, 900, "muteki", nullptr, nullptr);
	if (!win) {
		return 1;
	}

	std::unique_ptr<GPUInterface> gpu{ CreateGPU_DX12() };
	gpu->Init();
	HWND hwnd = glfwGetWin32Window(win);
	{
		i32 fb_width, fb_height;
		glfwGetFramebufferSize(win, &fb_width, &fb_height);
		gpu->CreateSwapChain(hwnd, fb_width, fb_height);
	}

	ImGuiImpl imgui;
	imgui.Init(hwnd, gpu.get());

	glfwSetMouseButtonCallback(win, OnMouseButton);
	glfwSetCharCallback(win, OnChar);
	glfwSetKeyCallback(win, OnKey);

	String shader_filename{ GetShaderDirectory(), "basic_shader.hlsl" };
	Array<u8> shader_txt_code = LoadFileToArray(shader_filename.GetRaw(), FileReadType::Text); // TODO: Handle unicode BOM/other encodings
	GPU::VertexShaderID vshader_id = gpu->CompileVertexShaderHLSL("vs_main", Range(shader_txt_code));
	GPU::PixelShaderID pshader_id = gpu->CompilePixelShaderHLSL("ps_main", Range(shader_txt_code));
	GPU::ProgramID program_id = gpu->LinkProgram(vshader_id, pshader_id);

	GPU::PipelineStateDesc pipeline_state_desc{};
	pipeline_state_desc.Program = program_id;
	pipeline_state_desc.StreamFormat
		.AddSlot({ GPU::ScalarType::Float, 3, GPU::InputSemantic::Position, 0 })
		.AddSlot({ GPU::ScalarType::Float, 3, GPU::InputSemantic::Normal, 0 })
		.AddSlot({ GPU::ScalarType::Float, 2, GPU::InputSemantic::Texcoord, 0 })
		;
	pipeline_state_desc.BlendState.BlendEnable = false;
	pipeline_state_desc.RasterState.ScissorEnable = true;
	pipeline_state_desc.RasterState.CullMode = GPU::CullMode::Back;
	pipeline_state_desc.RasterState.FrontFace = GPU::FrontFace::Clockwise;
	GPU::PipelineStateID pipeline_state = gpu->CreatePipelineState(pipeline_state_desc);

	// Unit cube centered around the origin
	Vec3 cube_positions[] = {
		// Top face
		{ -0.5, +0.5, -0.5 }, // Back left		0
		{ +0.5, +0.5, -0.5 }, // Back right		1
		{ -0.5, +0.5, +0.5 }, // Front left		2
		{ +0.5, +0.5, +0.5 }, // Front right	3

		// Bottom face
		{ -0.5, -0.5, +0.5 }, // Front left		4
		{ +0.5, -0.5, +0.5 }, // Front right	5
		{ -0.5, -0.5, -0.5 }, // Back left		6
		{ +0.5, -0.5, -0.5 }, // Back right		7

		// Front face
		{ -0.5, +0.5, +0.5 }, // Top left		8
		{ +0.5, +0.5, +0.5 }, // Top right		9
		{ -0.5, -0.5, +0.5 }, // Bottom left	10	
		{ +0.5, -0.5, +0.5 }, // Bottom right	11

		// Back face
		{ +0.5, +0.5, -0.5 }, // Top right		12
		{ -0.5, +0.5, -0.5 }, // Top left		13
		{ +0.5, -0.5, -0.5 }, // Bottom right	14
		{ -0.5, -0.5, -0.5 }, // Bottom left	15

		// Right face
		{ +0.5, +0.5, +0.5 }, // Top front		16
		{ +0.5, +0.5, -0.5 }, // Top back		17
		{ +0.5, -0.5, +0.5 }, // Bottom front	18
		{ +0.5, -0.5, -0.5 }, // Bottom back	19

		// Left face
		{ -0.5, +0.5, -0.5 }, // Top back		20
		{ -0.5, +0.5, +0.5 }, // Top front		21
		{ -0.5, -0.5, -0.5 }, // Bottom back	22
		{ -0.5, -0.5, +0.5 }, // Bottom front	23
	};
	Vec3 cube_normals[] = {
		// Top face
		Vec3::UnitY,
		Vec3::UnitY,
		Vec3::UnitY,
		Vec3::UnitY,

		// Bottom face
		-Vec3::UnitY,
		-Vec3::UnitY,
		-Vec3::UnitY,
		-Vec3::UnitY,

		// Front face
		Vec3::UnitZ,
		Vec3::UnitZ,
		Vec3::UnitZ,
		Vec3::UnitZ,

		// Back face
		-Vec3::UnitZ,
		-Vec3::UnitZ,
		-Vec3::UnitZ,
		-Vec3::UnitZ,

		// Right face
		Vec3::UnitX,
		Vec3::UnitX,
		Vec3::UnitX,
		Vec3::UnitX,

		// Left face
		-Vec3::UnitX,
		-Vec3::UnitX,
		-Vec3::UnitX,
		-Vec3::UnitX
	};
	Vec2 cube_texcoords[] = {
		// Top face
		{0, 0},
		{1, 0},
		{0, 1},
		{1, 1},

		// Bottom face
		{ 0, 0 },
		{ 1, 0 },
		{ 0, 1 },
		{ 1, 1 },

		// Front face
		{ 0, 0 },
		{ 1, 0 },
		{ 0, 1 },
		{ 1, 1 },

		// Back face
		{ 0, 0 },
		{ 1, 0 },
		{ 0, 1 },
		{ 1, 1 },

		// Right face
		{ 0, 0 },
		{ 1, 0 },
		{ 0, 1 },
		{ 1, 1 },

		// Left face
		{ 0, 0 },
		{ 1, 0 },
		{ 0, 1 },
		{ 1, 1 },
	};
	GPU::VertexBufferID vbuffer_id_pos = gpu->CreateVertexBuffer(ByteRange(cube_positions));
	GPU::VertexBufferID vbuffer_id_normals = gpu->CreateVertexBuffer(ByteRange(cube_normals));
	GPU::VertexBufferID vbuffer_id_texcoord = gpu->CreateVertexBuffer(ByteRange(cube_texcoords));

	u16 cube_indices[] = {
		// Top face
		//0, 1, 3,
		//0, 3, 2,
		2, 1, 0,
		2, 3, 1,

		// Bottom face
		//4, 5, 7,
		//4, 7, 6,
		6, 5, 4,
		5, 6, 7,

		// Front face
		//8, 9, 11,
		//8, 11, 10,
		9, 8, 10,
		9, 10, 11,

		// Back face
		//12, 13, 15,
		//12, 15, 14,
		13, 12, 14,
		13, 14, 15,

		// Right face
		//16, 17, 19,
		//16, 19, 18,
		17, 16, 18,
		17, 18, 19,

		// Left face
		//20, 21, 23,
		//20, 23, 22,
		21, 20, 22,
		21, 22, 23
	};
	GPU::IndexBufferID ibuffer_id = gpu->CreateIndexBuffer(ByteRange(cube_indices));

	Color4 pixels[] = {
		{ 128, 128, 128, 255 }, { 255, 255, 255, 255 },
		{ 255, 255, 255, 255 }, { 128, 128, 128, 255 },
	};
	GPU::TextureID texture_id = gpu->CreateTexture2D(2, 2, GPU::TextureFormat::RGBA8, ByteRange(pixels));
	GPU::ShaderResourceListDesc resource_list_desc = {};
	resource_list_desc.StartSlot = 0;
	resource_list_desc.Textures.Add(texture_id);
	GPU::ShaderResourceListID resource_list_id = gpu->CreateShaderResourceList(resource_list_desc);

	i64 frame_num = 0;
	bool show_test_window = false;
	bool pause_anim = false;
	float proj_params[] = { 0.3f, 0.2f };
	while (glfwWindowShouldClose(win) == false) {
		glfwPollEvents();

		Vector<u32, 2> current_size = gpu->GetSwapChainDimensions();
		i32 fb_width, fb_height;
		glfwGetFramebufferSize(win, &fb_width, &fb_height);
		if ((u32)fb_width != current_size[0] || (u32)fb_height != current_size[1]) {
			gpu->ResizeSwapChain(hwnd, fb_width, fb_height);
		}

		auto* gpu_frame = gpu->BeginFrame();
		Vector<double, 2> mouse_pos;
		Vector<bool, 3> mouse_buttons;
		glfwGetCursorPos(win, &mouse_pos[0], &mouse_pos[1]);
		for (i32 i = 0; i < 3; ++i) {
			mouse_buttons[i] = mouse_pressed[i] || glfwGetMouseButton(win, i) != 0;
			mouse_pressed[i] = false;
		}
		imgui.BeginFrame(mouse_pos, mouse_buttons);

		if (ImGui::Begin("muteki", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::InputFloat2("Projection Dimensions", proj_params);
			ImGui::Checkbox("Pause", &pause_anim);
			ImGui::End();
		}

		struct ViewData {
			Mat4x4 world_to_view = Mat4x4::Identity();
			Mat4x4 view_to_clip = Mat4x4::Identity(); // TODO: Nomenclature?
			Mat4x4 rotation = Mat4x4::Identity();
		};
		ViewData view_data;
		view_data.rotation = CreateTranslation({ 0, Sin(frame_num / 1600.0f), 0.95f }) * CreateRotationY(frame_num / 1600.0);
		view_data.view_to_clip = CreatePerspectiveProjection(proj_params[0], proj_params[1], 0.1f, 10);
		GPU::ConstantBufferID cbuffer_id_viewdata = gpu_frame->GetTemporaryConstantBuffer(ByteRange(view_data));


		GPU::DrawItem draw_item = {};
		// default raster state, blend state, depth stencil state
		draw_item.PipelineState = pipeline_state;
		draw_item.BoundResources.ConstantBuffers.Add(cbuffer_id_viewdata);
		draw_item.BoundResources.ResourceLists.Add(resource_list_id);
		draw_item.Command.VertexOrIndexCount = (u32)ArraySize(cube_indices);
		draw_item.Command.PrimTopology = GPU::PrimitiveTopology::TriangleList;
		draw_item.StreamSetup.VertexBuffers.Add({ vbuffer_id_pos, vbuffer_id_normals, vbuffer_id_texcoord });
		draw_item.StreamSetup.IndexBuffer = ibuffer_id;

		GPU::RenderPass pass;
		pass.RenderTargets.Add(GPU::BackBufferID);
		pass.DrawItems = mu::Range(&draw_item, &draw_item + 1);
		pass.ClipRect = { 0, 0, current_size[0], current_size[1] };
		gpu->SubmitPass(pass);

		if (show_test_window) {
			ImGui::SetNextWindowPos(ImVec2(400, 100), ImGuiSetCond_FirstUseEver);
			ImGui::ShowTestWindow(&show_test_window);
		}
		imgui.Render(gpu_frame);

		gpu->EndFrame(gpu_frame);

		if (!pause_anim) {
			++frame_num;
		}
	}

	imgui.Shutdown();
	gpu->Shutdown();

	return 0;
}
