#ifdef _MSC_VER
#    pragma comment(linker, "/subsystem:windows /ENTRY:mainCRTStartup")
#endif

#include "GPU_DX12.h"

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

int main(int, char**) {
	using namespace GPUCommon;

	dbg::Log("Running from: ", paths::GetExecutableDirectory());

	if (glfwInit() == GLFW_FALSE) {
		return 1;
	}

	GLFWwindow* win = glfwCreateWindow(800, 600, "muteki", nullptr, nullptr);
	if (!win) {
		return 1;
	}

	GPU::Init();
	HWND hwnd = glfwGetWin32Window(win);
	GPU::RecreateSwapChain(hwnd, 800, 600);

	//ImGui_Impl_Init(hwnd);

	String shader_filename{ GetShaderDirectory(), "basic_shader.hlsl" };
	Array<u8> shader_txt_code = LoadFileToArray(shader_filename.GetRaw(), FileReadType::Text);
	VertexShaderID vshader_id = GPU::CompileVertexShaderHLSL("vs_main", Range(shader_txt_code));
	PixelShaderID pshader_id = GPU::CompilePixelShaderHLSL("ps_main", Range(shader_txt_code));
	ProgramID program_id = GPU::LinkProgram(vshader_id, pshader_id);

	GPUCommon::StreamFormatDesc stream_format{};
	stream_format.AddSlot().Set({ ScalarType::Float, 3, InputSemantic::Position, 0 });

	StreamFormatID stream_format_id = GPU::RegisterStreamFormat(stream_format);
	// {
	//	1, // Num slots
	//	{
	//		0, // Slot index
	//		1, // Num elements
	//		{
	//			{ ScalarType::Float, 3, InputSemantic::Position, 0 },
	//		},
	//	},
	//});

	struct Vertex {
		Vec3 position;
	};
	Vertex triangle_vertices[] = {
		{ 0.0f, 0.5f, 0.0f },{ 0.5f, -0.5f, 0.0f },{ -0.5f, -0.5f, 0.0f }
	};
	VertexBufferID vbuffer_id = GPU::CreateVertexBuffer(Range((u8*)triangle_vertices, sizeof(triangle_vertices)));
	InputAssemblerConfigID ia_id = GPU::RegisterInputAssemblyConfig(stream_format_id, Range(&vbuffer_id, &vbuffer_id+1), IndexBufferID{});

	//bool show_test_window = true;
	while (glfwWindowShouldClose(win) == false) {
		glfwPollEvents();

		GPU::BeginFrame();
		//ImGui_Impl_BeginFrame();

#if 0
		GPU::RenderTestFrame();
#else
		DrawItem draw_item = {};
		// default raster state, blend state, depth stenci state
		draw_item.PipelineSetup.InputAssemblerConfig = ia_id;
		draw_item.PipelineSetup.Program = program_id;
		draw_item.Command.VertexOrIndexCount = 3;
		draw_item.Command.PrimTopology = PrimitiveTopology::TriangleList;

		RenderPass pass;
		pass.RenderTargets.Add(BackBufferID);
		pass.DrawItems = mu::Range(&draw_item, &draw_item+1);
		GPU::SubmitPass(pass);
#endif
		//ImGui::ShowTestWindow(&show_test_window);
		//ImGui::Render();

		GPU::EndFrame();
	}

	//ImGui_Impl_Shutdown();
	GPU::Shutdown();


	return 0;
}

