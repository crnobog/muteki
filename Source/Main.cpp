#ifdef _MSC_VER
#    pragma comment(linker, "/subsystem:windows /ENTRY:mainCRTStartup")
#endif

#include "GPU_DX12/GPU_DX12.h"
#include "GPU_DX11/GPU_DX11.h"
#include "Vectors.h"
#include "TransformMatrices.h"
#include "Quaternion.h"
#include "CoreMath.h"
#include "CubePrimitive.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"

#include "mu-core/Debug.h"
#include "mu-core/FileReader.h"
#include "mu-core/Paths.h"
#include "mu-core/String.h"
#include "mu-core/Timer.h"

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
		String shader_txt_code = LoadFileToString(shader_filename.GetRaw());
		GPU::VertexShaderID vshader_id = gpu->CompileVertexShaderHLSL("vs_main", shader_txt_code.Bytes());
		GPU::PixelShaderID pshader_id = gpu->CompilePixelShaderHLSL("ps_main", shader_txt_code.Bytes());
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
					current_pass = &passes.AddDefaulted(1).Front();
					// left top right bottom
					current_pass->ClipRect = { (u32)last_clip_rect.X, (u32)last_clip_rect.Y, (u32)last_clip_rect.Z, (u32)last_clip_rect.W };
					current_pass->DrawItems = { gpu_cmd_cursor.m_start, 0 };
					current_pass->RenderTargets.Add(GPU::RenderTargetID{});
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
	GPU::DepthTargetID depthbuffer;
	{
		i32 fb_width, fb_height;
		glfwGetFramebufferSize(win, &fb_width, &fb_height);
		gpu->CreateSwapChain(hwnd, fb_width, fb_height);
		depthbuffer = gpu->CreateDepthTarget(fb_width, fb_height);
	}

	ImGuiImpl imgui;
	imgui.Init(hwnd, gpu.get());

	glfwSetMouseButtonCallback(win, OnMouseButton);
	glfwSetCharCallback(win, OnChar);
	glfwSetKeyCallback(win, OnKey);

	GPU::DepthStencilStateDesc depth_state = {};
	depth_state.DepthEnable = true;
	depth_state.DepthWriteEnable = true;
	depth_state.DepthComparisonFunc = GPU::ComparisonFunc::Less;

	GPU::PipelineStateID cube_pipeline_state;
	{
		Timer t;
		String shader_filename{ GetShaderDirectory(), "basic_shader.hlsl" };
		String shader_txt_code = LoadFileToString(shader_filename.GetRaw()); // TODO: Handle unicode BOM/other encodings
		GPU::VertexShaderID vshader_id = gpu->CompileVertexShaderHLSL("vs_main", shader_txt_code.Bytes());
		GPU::PixelShaderID pshader_id = gpu->CompilePixelShaderHLSL("ps_main", shader_txt_code.Bytes());
		GPU::ProgramID program_id = gpu->LinkProgram(vshader_id, pshader_id);
		dbg::Log("Loaded and compiled basic shader in {} ms", t.GetElapsedTimeMilliseconds());

		GPU::PipelineStateDesc pipeline_state_desc{};
		pipeline_state_desc.Program = program_id;
		pipeline_state_desc.StreamFormat
			.AddSlot({ GPU::ScalarType::Float, 3, GPU::InputSemantic::Position, 0 })
			.AddSlot({ GPU::ScalarType::Float, 3, GPU::InputSemantic::Normal, 0 })
			.AddSlot({ GPU::ScalarType::Float, 2, GPU::InputSemantic::Texcoord, 0 })
			;
		pipeline_state_desc.BlendState.BlendEnable = false;
		pipeline_state_desc.DepthStencilState = depth_state;
		pipeline_state_desc.RasterState.ScissorEnable = true;
		pipeline_state_desc.RasterState.CullMode = GPU::CullMode::Back;
		pipeline_state_desc.RasterState.FrontFace = GPU::FrontFace::Clockwise;
		cube_pipeline_state = gpu->CreatePipelineState(pipeline_state_desc);
	}
	GPU::PipelineStateID grid_pipeline_state;
	{
		Timer t;
		String shader_filename{ GetShaderDirectory(), "grid_shader.hlsl" };
		String shader_txt_code = LoadFileToString(shader_filename.GetRaw());
		GPU::VertexShaderID vshader_id = gpu->CompileVertexShaderHLSL("vs_main", shader_txt_code.Bytes());
		GPU::PixelShaderID pshader_id = gpu->CompilePixelShaderHLSL("ps_main", shader_txt_code.Bytes());
		GPU::ProgramID program_id = gpu->LinkProgram(vshader_id, pshader_id);
		dbg::Log("Loaded and compiled grid shader in {} ms", t.GetElapsedTimeMilliseconds());

		GPU::PipelineStateDesc pipeline_state_desc{};
		pipeline_state_desc.Program = program_id;
		pipeline_state_desc.BlendState.BlendEnable = false;
		pipeline_state_desc.DepthStencilState = depth_state;
		pipeline_state_desc.RasterState.ScissorEnable = true;
		pipeline_state_desc.RasterState.FillMode = GPU::FillMode::Wireframe;
		pipeline_state_desc.RasterState.CullMode = GPU::CullMode::None;
		pipeline_state_desc.PrimitiveType = GPU::PrimitiveType::Line;
		grid_pipeline_state = gpu->CreatePipelineState(pipeline_state_desc);
	}

	GPU::VertexBufferID cube_vbuffer_id_pos = gpu->CreateVertexBuffer(ByteRange(cube_positions));
	GPU::VertexBufferID cube_vbuffer_id_normals = gpu->CreateVertexBuffer(ByteRange(cube_normals));
	GPU::VertexBufferID cube_vbuffer_id_texcoord = gpu->CreateVertexBuffer(ByteRange(cube_texcoords));

	GPU::IndexBufferID cube_ibuffer_id = gpu->CreateIndexBuffer(ByteRange(cube_indices));

	Color4 pixels[] = {
		{ 128, 128, 128, 255 }, { 255, 255, 255, 255 },
		{ 255, 255, 255, 255 }, { 128, 128, 128, 255 },
	};
	GPU::TextureID texture_id = gpu->CreateTexture2D(2, 2, GPU::TextureFormat::RGBA8, ByteRange(pixels));
	GPU::ShaderResourceListDesc resource_list_desc = {};
	resource_list_desc.StartSlot = 0;
	resource_list_desc.Textures.Add(texture_id);
	GPU::ShaderResourceListID cube_resource_list_id = gpu->CreateShaderResourceList(resource_list_desc);

	i64 frame_num = 0;
	bool show_test_window = false;
	bool pause_anim = false;
	float vfov = 0.5f;
	float near_plane = 0.1f;
	float far_plane = 20.0f;
	Vec3 view_pos = { 0, 1, -3 }, view_target = { 0,0,0 }, view_up = { 0, 1, 0 };
	Vec3 rot_axis = { 0, 1, 0 };
	float rot_deg = 0;
	bool bUseQuat = false;
	float rot_deg_x = 0, rot_deg_y = 0, rot_deg_z = 0;
	Vec3 location = { 0, 0, 0 };
	Quat view_quat = Quat::Identity();
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


		const float aspect_ratio = (float)fb_width / (float)fb_height;

		Vec3 view_movement{ 0,0,0 };
		float view_yaw = 0, view_pitch = 0;
		if (ImGui::Begin("muteki", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Checkbox("Pause", &pause_anim);
			ImGui::Checkbox("Use Quat", &bUseQuat);
			ImGui::Separator();

			ImGui::InputFloat3("Location", location.Data);
			if (bUseQuat) {
				ImGui::InputFloat3("Rot Axis", rot_axis.Data);
				ImGui::DragFloat("Rot Angle", &rot_deg, 1.0f, 0, 360);
			}
			else {
				ImGui::InputFloat("Angle X", &rot_deg_x);
				ImGui::InputFloat("Angle Y", &rot_deg_y);
				ImGui::InputFloat("Angle Z", &rot_deg_z);
			}
			ImGui::Separator();
			ImGui::Text("Move Camera");
			if (ImGui::Button("Up")) {
				view_movement.Y += 1.0;
			}
			ImGui::SameLine();
			if (ImGui::Button("Down")) {
				view_movement.Y -= 1.0;
			}
			ImGui::SameLine();
			if (ImGui::Button("Left")) {
				view_movement.X -= 1.0;
			}
			ImGui::SameLine();
			if (ImGui::Button("Right")) {
				view_movement.X += 1.0;
			}
			ImGui::SameLine();
			if (ImGui::Button("Forward")) {
				view_movement.Z += 1.0;
			}
			ImGui::SameLine();
			if (ImGui::Button("Backward")) {
				view_movement.Z -= 1.0;
			}

			view_pos += view_quat.Rotate(view_movement);

			ImGui::Separator();
			ImGui::Text("Rotate Camera");
			if (ImGui::Button("Pitch Up")) {
				view_pitch -= 10.0f;
			}
			ImGui::SameLine();
			if (ImGui::Button("Pitch Down")) {
				view_pitch += 10.0f;
			}
			ImGui::SameLine();
			if (ImGui::Button("Yaw Left")) {
				view_yaw -= 10.0f;
			}
			ImGui::SameLine();
			if (ImGui::Button("Yaw Right")) {
				view_yaw += 10.0f;
			}
			ImGui::Separator();
			Quat delta_pitch = Quat::FromAxisAngle(view_quat.Rotate({ 1, 0, 0 }), DegreesToRadians(view_pitch));
			Quat delta_yaw = Quat::FromAxisAngle({ 0, 1, 0 }, DegreesToRadians(view_yaw));
			Quat delta_quat = delta_pitch * delta_yaw;
			view_quat = delta_quat * view_quat;

			ImGui::LabelText("View Pos", "%.2f, %.2f, %.2f", view_pos.X, view_pos.Y, view_pos.Z);
			ImGui::LabelText("View Quat", "%.2f, %.2f, %.2f, %.2f", view_quat.X, view_quat.Y, view_quat.Z, view_quat.W);
			ImGui::InputFloat3("View Target", view_target.Data);
			ImGui::InputFloat3("View Up", view_up.Data);
			ImGui::SliderFloat("VFOV", &vfov, 0.0f, 3.0f);
			ImGui::SliderFloat("Near Plane", &near_plane, 0.001f, 1.0f);
			ImGui::SliderFloat("Far Plane", &far_plane, near_plane, 100.0f);

			ImGui::End();
		}


		Quat q = Quat::FromAxisAngle(Normalize(rot_axis), DegreesToRadians(rot_deg));

		struct ViewData {
			Mat4x4 world_to_view = Mat4x4::Identity();
			Mat4x4 view_to_clip = Mat4x4::Identity(); // TODO: Nomenclature?
		};
		ViewData view_data;
		//view_data.world_to_view = CreateLookAt(view_pos, view_target, view_up);
		view_data.world_to_view = view_quat.Conjugate().ToMatrix4x4() * CreateTranslation(-view_pos);
		view_data.view_to_clip = CreatePerspectiveProjection(vfov, aspect_ratio, near_plane, far_plane);
		GPU::ConstantBufferID cbuffer_id_viewdata = gpu_frame->GetTemporaryConstantBuffer(ByteRange(view_data));

		FixedArray<GPU::DrawItem, 16> draw_items;
		{
			// Draw cube
			Mat4x4 transform;
			if (bUseQuat) {
				transform = q.ToMatrix4x4();
			}
			else {
				transform = CreateRotationZ(DegreesToRadians(rot_deg_z)) * CreateRotationY(DegreesToRadians(rot_deg_y)) * CreateRotationX(DegreesToRadians(rot_deg_x));
			}
			transform = CreateTranslation(location) * transform;
			GPU::ConstantBufferID cbuffer_id_transform = gpu_frame->GetTemporaryConstantBuffer(ByteRange(transform));
			GPU::DrawItem& draw_item = draw_items.AddDefaulted(1).Front();
			// default raster state, blend state, depth stencil state
			draw_item.PipelineState = cube_pipeline_state;
			draw_item.BoundResources.ConstantBuffers.Add(cbuffer_id_viewdata);
			draw_item.BoundResources.ConstantBuffers.Add(cbuffer_id_transform);
			draw_item.BoundResources.ResourceLists.Add(cube_resource_list_id);
			draw_item.Command.VertexOrIndexCount = (u32)ArraySize(cube_indices);
			draw_item.Command.PrimTopology = GPU::PrimitiveTopology::TriangleList;
			draw_item.StreamSetup.VertexBuffers.Add({ cube_vbuffer_id_pos, cube_vbuffer_id_normals, cube_vbuffer_id_texcoord });
			draw_item.StreamSetup.IndexBuffer = cube_ibuffer_id;
			draw_item.Name = "Cube";
		}

		{
			// Draw grid
			struct {
				Vec4 axis1 = { Vec3::UnitX, 1.0f };
				Vec4 axis1_color = { 1.0f, 0.0f, 0.0f, 1.0f };
				Vec4 axis2 = { Vec3::UnitZ, 1.0f };
				Vec4 axis2_color = { 0.0f, 0.0f, 1.0f, 1.0f };
				Vec4 base = { 0.0f, 0.0f, 0.0f, 1.0f };
				Vec4 plain_color = { 1.0f, 1.0f, 1.0f, 1.0f };
				u32 num_gridlines = 10;
				float spacing = 1.0f;
			} grid_data;
			GPU::ConstantBufferID cbuffer_id_griddata = gpu_frame->GetTemporaryConstantBuffer(ByteRange(grid_data));

			GPU::DrawItem& draw_item = draw_items.AddDefaulted(1).Front();
			draw_item.PipelineState = grid_pipeline_state;
			draw_item.BoundResources.ConstantBuffers.Add(cbuffer_id_viewdata);
			draw_item.BoundResources.ConstantBuffers.Add(cbuffer_id_griddata);
			draw_item.Command.VertexOrIndexCount = grid_data.num_gridlines * 8;
			draw_item.Command.PrimTopology = GPU::PrimitiveTopology::LineList;
			draw_item.Name = "Grid";
		}

		float DepthClear = 1.0f;
		GPU::RenderPass pass;
		pass.RenderTargets.Add(GPU::BackBufferID);
		pass.DepthBuffer = depthbuffer;
		pass.DepthClearValue = &DepthClear;
		pass.DrawItems = mu::Range(draw_items);
		pass.ClipRect = { 0, 0, current_size[0], current_size[1] };
		pass.Name = "Scene";
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
