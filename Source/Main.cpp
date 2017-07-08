#ifdef _MSC_VER
#    pragma comment(linker, "/subsystem:windows /ENTRY:mainCRTStartup")
#endif

#include "GPU_DX12/GPU_DX12.h"
#include "GPU_DX11/GPU_DX11.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"

#include "mu-core/Vectors.h"
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

void ImGui_Impl_RenderDrawLists(ImDrawData* ) {}
void ImGui_Impl_Init(HWND hwnd) {
	ImGuiIO& io = ImGui::GetIO();
	io.RenderDrawListsFn = ImGui_Impl_RenderDrawLists;  // TODO: Alternatively you can set this to NULL and call ImGui::GetDrawData() after ImGui::Render() to get the same ImDrawData pointer.
	io.ImeWindowHandle = hwnd;
}
void ImGui_Impl_Shutdown() {}
void ImGui_Impl_BeginFrame() {
	ImGui::NewFrame();
}
void ImGui_Impl_DrawFrame() {}

struct Color4 {
	u8 R, G, B, A;
};

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

	//ImGui_Impl_Init(hwnd);

	String shader_filename{ GetShaderDirectory(), "basic_shader.hlsl" };
	Array<u8> shader_txt_code = LoadFileToArray(shader_filename.GetRaw(), FileReadType::Text);
	GPU::VertexShaderID vshader_id = gpu->CompileVertexShaderHLSL("vs_main", Range(shader_txt_code));
	GPU::PixelShaderID pshader_id = gpu->CompilePixelShaderHLSL("ps_main", Range(shader_txt_code));
	GPU::ProgramID program_id = gpu->LinkProgram(vshader_id, pshader_id);

	GPU::StreamFormatDesc stream_format{};
	stream_format.AddSlot().Set({ GPU::ScalarType::Float, 3, GPU::InputSemantic::Position, 0 });

	GPU::StreamFormatID stream_format_id = gpu->RegisterStreamFormat(stream_format);

	struct Vertex {
		Vec3 position;
	};
	Vertex triangle_vertices[] = {
		{ 0.0f, 0.5f, 0.0f },{ 0.5f, -0.5f, 0.0f },{ -0.5f, -0.5f, 0.0f }
	};
	GPU::VertexBufferID vbuffer_id = gpu->CreateVertexBuffer(Range((u8*)triangle_vertices, sizeof(triangle_vertices)));
	GPU::InputAssemblerConfigID ia_id = gpu->RegisterInputAssemblyConfig(stream_format_id, Range(&vbuffer_id, &vbuffer_id+1), GPU::IndexBufferID{});

	struct CBuffer_Color {
		Vec4 color;
	};
	CBuffer_Color cbuffer = { { 1.0f, 1.0f, 0.0f, 1.0f } };
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

	//bool show_test_window = true;
	while (glfwWindowShouldClose(win) == false) {
		glfwPollEvents();

		gpu->BeginFrame();
		//ImGui_Impl_BeginFrame();

		GPU::DrawItem draw_item = {};
		// default raster state, blend state, depth stenci state
		draw_item.PipelineSetup.InputAssemblerConfig = ia_id;
		draw_item.PipelineSetup.Program = program_id;
		draw_item.BoundResources.ConstantBuffers.Add(cbuffer_id);
		draw_item.BoundResources.ResourceLists.Add(resource_list_id);
		draw_item.Command.VertexOrIndexCount = 3;
		draw_item.Command.PrimTopology = GPU::PrimitiveTopology::TriangleList;

		GPU::RenderPass pass;
		pass.RenderTargets.Add(GPU::BackBufferID);
		pass.DrawItems = mu::Range(&draw_item, &draw_item+1);
		gpu->SubmitPass(pass);

		//ImGui::ShowTestWindow(&show_test_window);
		//ImGui::Render();

		gpu->EndFrame();
	}

	//ImGui_Impl_Shutdown();
	gpu->Shutdown();
	delete gpu;


	return 0;
}

