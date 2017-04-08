#ifdef _MSC_VER
#    pragma comment(linker, "/subsystem:windows /ENTRY:mainCRTStartup")
#endif

#include "GPU_DX12.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"

#include "mu-core/Vectors.h"

#include "imgui.h"

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

int main(int, char**)
{
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

	StreamFormatID stream_format_id = GPU::RegisterStreamFormat({
		1, // Num slots
		{
			0, // Slot index
			1, // Num elements
			{
				{ ScalarType::Float, 3, InputSemantic::Position, 0 },
			},
		},
	});

	struct Vertex {
		Vec3 position;
	};
	Vertex triangle_vertices[] = {
		{ 0.0f, 0.5f, 0.0f },{ 0.5f, -0.5f, 0.0f },{ -0.5f, -0.5f, 0.0f }
	};


	//bool show_test_window = true;
	while (glfwWindowShouldClose(win) == false) {
		glfwPollEvents();

		GPU::BeginFrame();
		//ImGui_Impl_BeginFrame();

		GPU::RenderTestFrame();

		DrawItem draw_item = {};

		RenderPass pass;
		pass.NumRenderTargets = 1;
		pass.RenderTargets[0] = BackBufferID;
		pass.DrawItemCount = 1;
		pass.DrawItems = &draw_item;
		GPU::SubmitPass(pass);

		//ImGui::ShowTestWindow(&show_test_window);
		//ImGui::Render();

		GPU::EndFrame();
	}

	//ImGui_Impl_Shutdown();
	GPU::Shutdown();


	return 0;
}

