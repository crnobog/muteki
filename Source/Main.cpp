#ifdef _MSC_VER
#    pragma comment(linker, "/subsystem:windows /ENTRY:mainCRTStartup")
#endif

#include "GPU_DX12.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"

#define MU_CORE_IMPL
#include "mu-core/mu-core.h"


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

	while (glfwWindowShouldClose(win) == false) {
		glfwPollEvents();
		
		GPU::BeginFrame();
		GPU::EndFrame();
	}

	GPU::Shutdown();

	return 0;
}

