#include "GPU_Vulkan.h"

#include "mu-core/Array.h"
#include "mu-core/Debug.h"
#include "mu-core/Paths.h"


#ifndef VC_EXTRALEAN
	#define VC_EXTRALEAN
#endif
	#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
#endif
	#ifndef NOMINMAX
	#define NOMINMAX
#endif
#include <Windows.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#pragma warning(disable : 4100)
#pragma comment(lib, "vulkan-1.lib")

#define EnsureVK(expr) \
	do { \
		VkResult res = (expr); \
		if(res == VK_SUCCESS) { break; } \
		mu::dbg::AssertImpl(false, "VK call failed with error {}: {}", res, #expr); \
	} while(false)

using VertexBufferID = GPU::VertexBufferID;
using IndexBufferID = GPU::IndexBufferID;
using VertexShaderID = GPU::VertexShaderID;
using PixelShaderID = GPU::PixelShaderID;
using ProgramID = GPU::ProgramID;
using PipelineStateID = GPU::PipelineStateID;
using ConstantBufferID = GPU::ConstantBufferID;
using RenderTargetID = GPU::RenderTargetID;
using DepthTargetID = GPU::DepthTargetID;
using TextureID = GPU::TextureID;
using ShaderResourceListID = GPU::ShaderResourceListID;
using RenderPass = GPU::RenderPass;

using namespace mu;

struct GPU_Vulkan_Frame : public GPUFrameInterface 
{
	virtual ConstantBufferID GetTemporaryConstantBuffer(PointerRange<const u8> data)
	{
		return ConstantBufferID{};
	}
	virtual VertexBufferID GetTemporaryVertexBuffer(PointerRange<const u8> data)
	{
		return VertexBufferID{};
	}
	virtual IndexBufferID GetTemporaryIndexBuffer(PointerRange<const u8> data) override
	{
		return IndexBufferID{};
	}
};

struct SwapChainSupport
{
	VkSurfaceCapabilitiesKHR Capabilities;
	Array<VkSurfaceFormatKHR> Formats;
	Array<VkPresentModeKHR> PresentModes;
};

struct PhysicalDeviceSelection
{
	VkPhysicalDevice	PhysicalDevice		= nullptr;
	u32					GraphicsQueueIndex	= 0xffffff;
	u32					PresentQueueIndex	= 0xffffff;
	u32					ComputeQueueIndex	= 0xffffff;
	SwapChainSupport	SwapChain			= {};
};


struct GPU_Vulkan : public GPUInterface {

	// Internal types


	GPU_Vulkan();
	~GPU_Vulkan();
	GPU_Vulkan(const GPU_Vulkan&) = delete;
	void operator=(const GPU_Vulkan&) = delete;
	GPU_Vulkan(GPU_Vulkan&&) = delete;
	void operator=(GPU_Vulkan&&) = delete;

	// BEGIN GPUInterface functions
	virtual void Init(void* hwnd) override;
	virtual void Shutdown() override;
	virtual void CreateSwapChain(u32 width, u32 height) override;
	virtual void ResizeSwapChain(u32 width, u32 height) override;
	virtual Vector<u32, 2> GetSwapChainDimensions() override;

	virtual GPUFrameInterface* BeginFrame(Vec4 scene_clear_color) override;
	virtual void EndFrame(GPUFrameInterface* frame) override;

	virtual void SubmitPass(const RenderPass& pass) override;

	virtual mu::String GetShaderFilename(mu::PointerRange<const char> name) override;
	virtual VertexShaderID CompileVertexShaderHLSL(const char* entry_point, mu::PointerRange<const u8> code) override;
	virtual PixelShaderID CompilePixelShaderHLSL(const char* entry_point, mu::PointerRange<const u8> code) override;
	virtual ProgramID LinkProgram(VertexShaderID vertex_shader, PixelShaderID pixel_shader) override;

	virtual GPU::PipelineStateID CreatePipelineState(const GPU::PipelineStateDesc& desc) override;
	virtual void DestroyPipelineState(GPU::PipelineStateID id) override;

	virtual ConstantBufferID CreateConstantBuffer(mu::PointerRange<const u8> data) override;
	virtual void DestroyConstantBuffer(ConstantBufferID id) override;

	virtual VertexBufferID CreateVertexBuffer(mu::PointerRange<const u8> data) override;
	virtual void DestroyVertexBuffer(GPU::VertexBufferID id) override;
	virtual IndexBufferID CreateIndexBuffer(mu::PointerRange<const u8> data) override;
	virtual void DestroyIndexBuffer(GPU::IndexBufferID id) override;

	virtual TextureID CreateTexture2D(u32 width, u32 height, GPU::TextureFormat format, mu::PointerRange<const u8> data) override;

	virtual GPU::DepthTargetID CreateDepthTarget(u32 width, u32 height) override;

	virtual ShaderResourceListID CreateShaderResourceList(const GPU::ShaderResourceListDesc& desc) override;
	virtual const char* GetName() override
	{
		return "Vulkan";
	}
	// END GPUInterface functions

	static VkBool32 VKAPI_CALL DebugReportCallback(
		VkDebugReportFlagsEXT                       flags,
		VkDebugReportObjectTypeEXT                  objectType,
		uint64_t                                    object,
		size_t                                      location,
		int32_t                                     messageCode,
		const char*                                 pLayerPrefix,
		const char*                                 pMessage,
		void*                                       pUserData)
	{
		dbg::Log("{} {}", pLayerPrefix, pMessage);
		return VK_FALSE;
	}
	
	static void* VulkanAllocCallback(
		void*                                       pUserData,
		size_t                                      size,
		size_t                                      alignment,
		VkSystemAllocationScope                     allocationScope) {
		return malloc(size);
	}
	static void* VulkanReallocCallback(
		void*                                       pUserData,
		void*                                       pOriginal,
		size_t                                      size,
		size_t                                      alignment,
		VkSystemAllocationScope                     allocationScope) {
		return realloc(pOriginal, size);
	}
	static void VulkanFreeCallback(
		void*                                       pUserData,
		void*                                       pMemory) {
		free(pMemory);
	}
	static void VulkanInternalAllocationCallback(
		void*                                       pUserData,
		size_t                                      size,
		VkInternalAllocationType                    allocationType,
		VkSystemAllocationScope                     allocationScope) {}
	static void VulkanInternalFreeCallback(
		void*                                       pUserData,
		size_t                                      size,
		VkInternalAllocationType                    allocationType,
		VkSystemAllocationScope                     allocationScope) {}

	VkAllocationCallbacks m_allocation_callbacks_impl = 
	{
		this,
		&VulkanAllocCallback,
		&VulkanReallocCallback,
		&VulkanFreeCallback,
		&VulkanInternalAllocationCallback,
		&VulkanInternalFreeCallback,
	};

	VkAllocationCallbacks* m_allocation_callbacks = nullptr;
	VkDebugReportCallbackEXT m_debug_callback = nullptr;

	PhysicalDeviceSelection m_chosen_device;
	VkPresentModeKHR m_present_mode = VK_PRESENT_MODE_FIFO_KHR;

	VkInstance	m_vkinst			= nullptr;
	VkDevice	m_device			= nullptr;
	VkQueue		m_graphics_queue	= nullptr;
	VkQueue		m_present_queue		= nullptr;
	VkQueue		m_compute_queue		= nullptr;
	
	VkSurfaceKHR	m_surface		= nullptr;
	VkSwapchainKHR	m_swap_chain	= nullptr;
	Array<VkImage>	m_swap_chain_images;
	Array<VkImageView> m_swap_chain_image_views;
	Vector<u32, 2> m_swap_chain_dimensions;

	GPU_Vulkan_Frame m_frame_data;
};

GPU_Vulkan::GPU_Vulkan() {
}

GPU_Vulkan::~GPU_Vulkan() {
}

GPUInterface* CreateGPU_Vulkan() {
	return new GPU_Vulkan;
}


SwapChainSupport GetSwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	SwapChainSupport result = {};
	EnsureVK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &result.Capabilities));

	u32 num_formats = 0;
	EnsureVK(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &num_formats, nullptr));
	result.Formats.AddZeroed(num_formats);
	EnsureVK(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &num_formats, result.Formats.Data()));

	u32 num_present_modes = 0;
	EnsureVK(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &num_present_modes, nullptr));
	result.PresentModes.AddZeroed(num_present_modes);
	EnsureVK(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &num_present_modes, result.PresentModes.Data()));

	return result;
}

static PhysicalDeviceSelection ChooseDevice(
	VkInstance instance,
	const Array<VkPhysicalDevice>& devices,
	VkSurfaceKHR surface
	)
{
	for (VkPhysicalDevice device : devices) {
		VkPhysicalDeviceProperties device_props;
		vkGetPhysicalDeviceProperties(device, &device_props);
		dbg::Log("Considering physical device {}", device_props.deviceName);

		u32 queue_family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
		Array<VkQueueFamilyProperties> queue_families;
		queue_families.AddZeroed(queue_family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.Data());

		for (i32 i = 0; i < queue_families.Num(); ++i)
		{
			const VkQueueFamilyProperties& props = queue_families[i];
			dbg::Log(" Queue family {}: {} queues", i, props.queueCount);
			if (props.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				dbg::Log("  Graphics");
			}
			if (props.queueFlags & VK_QUEUE_COMPUTE_BIT)
			{
				dbg::Log("  Compute");
			}
			if (props.queueFlags & VK_QUEUE_TRANSFER_BIT)
			{
				dbg::Log("  Transfer");
			}
			if (props.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT)
			{
				dbg::Log("  Sparse Binding");
			}
			if (props.queueFlags & VK_QUEUE_PROTECTED_BIT)
			{
				dbg::Log("  Protected");
			}
		}
		
		u32 graphics_queue_index = 0;
		u32 compute_queue_index = 0;
		for (; graphics_queue_index < queue_families.Num(); ++graphics_queue_index) {
			const VkQueueFamilyProperties& props = queue_families[graphics_queue_index];
			if (props.queueCount > 0 && props.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				if (props.queueFlags & VK_QUEUE_COMPUTE_BIT)
				{
					compute_queue_index = graphics_queue_index;
				}
				break;
			}
		}
		if (graphics_queue_index >= queue_families.Num()) {
			dbg::Log("No graphics queue");
			continue;
		}

		dbg::Log("  Graphics queue {}", graphics_queue_index);
		for (u32 i = 0; i< queue_families.Num(); ++i) {
			const VkQueueFamilyProperties& props = queue_families[i];
			if (i == compute_queue_index) // we've already seen a graphics queue family that supports compute
			{
				continue;
			}

			if (props.queueCount > 0 && props.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				compute_queue_index = i; // here's a graphics-unique queue family
				break;
			}
		}
		dbg::Log("  Compute queue {}", compute_queue_index);

		u32 present_queue_index = 0;
		for (; present_queue_index < queue_families.Num(); ++present_queue_index)
		{
			const VkQueueFamilyProperties& props = queue_families[present_queue_index];
			if (props.queueCount == 0) {
				continue;
			}

			VkBool32 supported = VK_FALSE;
			EnsureVK(vkGetPhysicalDeviceSurfaceSupportKHR(device, present_queue_index, surface, &supported));
			if (supported) {
				break;
			}
		}
		if (present_queue_index >= queue_families.Num()) {
			dbg::Log("No present queue");
			continue;
		}

		dbg::Log("  Present queue {}", present_queue_index);

		SwapChainSupport swap_chain_support = GetSwapChainSupport(device, surface);
		if (swap_chain_support.Formats.Num() == 0)
		{
			dbg::Log("No swap chain formats");
			continue;
		}
		if (swap_chain_support.PresentModes.Num() == 0)
		{
			dbg::Log("No present modes");
			continue;
		}

		return { device, graphics_queue_index, present_queue_index, compute_queue_index, swap_chain_support };
	}

	return {  };
}

void GPU_Vulkan::Init(void* hwnd) {
	{
		u32 num_extension_properties = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &num_extension_properties, nullptr);
		Array<VkExtensionProperties> extension_properties;
		extension_properties.AddZeroed(num_extension_properties);
		vkEnumerateInstanceExtensionProperties(nullptr, &num_extension_properties, extension_properties.Data());
		dbg::Log("Supported instance extensions");
		for (const VkExtensionProperties& props : extension_properties)
		{
			dbg::Log("  {} version {}", props.extensionName, props.specVersion);
		}
	}

	VkApplicationInfo app_info = {};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pNext = nullptr;
	app_info.pApplicationName = "muteki";
	app_info.applicationVersion = 1;
	app_info.pEngineName = "muteki";
	app_info.engineVersion = 1;
	app_info.apiVersion = VK_API_VERSION_1_1; // TODO: What's new in 1.1?

	// TODO: Check if these layers/extensions exist first
	Array<const char*> instance_layers;
	instance_layers.Add("VK_LAYER_LUNARG_standard_validation");
	Array<const char*> instance_extensions;
	instance_extensions.Add(VK_KHR_SURFACE_EXTENSION_NAME);
	instance_extensions.Add("VK_KHR_win32_surface"); //VK_KHR_WIN32_SURFACE_EXTENSION_NAME); // TODO: Multiplat
	instance_extensions.Add(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

	VkInstanceCreateInfo inst_info = {};
	inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	inst_info.pNext = nullptr;
	inst_info.flags = 0;
	inst_info.pApplicationInfo = &app_info;
	inst_info.enabledExtensionCount = (u32)instance_extensions.Num();
	inst_info.ppEnabledExtensionNames = instance_extensions.Data();
	inst_info.enabledLayerCount = (u32)instance_layers.Num();
	inst_info.ppEnabledLayerNames = instance_layers.Data(); // TODO: Enable debug layer?

	EnsureVK(vkCreateInstance(&inst_info, m_allocation_callbacks, &m_vkinst));

	PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(m_vkinst, "vkCreateDebugReportCallbackEXT");
	VkDebugReportCallbackCreateInfoEXT callback_info = {};
	callback_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	callback_info.pNext = nullptr;
	callback_info.flags = 0
		| VK_DEBUG_REPORT_DEBUG_BIT_EXT 
		| VK_DEBUG_REPORT_INFORMATION_BIT_EXT 
		| VK_DEBUG_REPORT_WARNING_BIT_EXT 
		| VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT 
		| VK_DEBUG_REPORT_ERROR_BIT_EXT
		;
	callback_info.pfnCallback = DebugReportCallback;
	callback_info.pUserData = this;
	vkCreateDebugReportCallbackEXT(m_vkinst, &callback_info, m_allocation_callbacks, &m_debug_callback);

	// TODO: Ideally we need to create window surface here so we can validate the device has
	// the ability to present to it

	auto* vkCreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(m_vkinst, "vkCreateWin32SurfaceKHR");
	Assertf(vkCreateWin32SurfaceKHR, "Failed to get function ptr for vkCreateWin32SurfaceKHR");
	VkWin32SurfaceCreateInfoKHR surface_create_info = {};
	surface_create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surface_create_info.pNext = nullptr;
	surface_create_info.flags = 0;
	surface_create_info.hinstance = nullptr;
	surface_create_info.hwnd = (HWND)hwnd;
	vkCreateWin32SurfaceKHR(m_vkinst, &surface_create_info, m_allocation_callbacks, &m_surface);

	u32 gpu_count = 0;
	EnsureVK(vkEnumeratePhysicalDevices(m_vkinst, &gpu_count, nullptr));
	Assert(gpu_count > 0);

	Array<VkPhysicalDevice> devices;
	devices.AddZeroed(gpu_count);
	EnsureVK(vkEnumeratePhysicalDevices(m_vkinst, &gpu_count, devices.Data()));

	dbg::Log("{} vulkan device(s) available", devices.Num());
	for (u32 i = 0; i < devices.Num(); ++i)
	{
		VkPhysicalDevice device = devices[i];
		VkPhysicalDeviceProperties properties = {};
		vkGetPhysicalDeviceProperties(device, &properties);

		dbg::Log("  {}) {}", i, properties.deviceName);
	}

	m_chosen_device = ChooseDevice(m_vkinst, devices, m_surface);
	
	Array<VkDeviceQueueCreateInfo> queues;
	float queue_priority = 1.0f;
	{
		VkDeviceQueueCreateInfo queue_info = {};
		queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_info.pNext = nullptr;
		queue_info.flags = 0;
		queue_info.queueFamilyIndex = m_chosen_device.GraphicsQueueIndex;
		queue_info.queueCount = 1;
		queue_info.pQueuePriorities = &queue_priority;
		queues.Add(queue_info);
	}
	if (m_chosen_device.GraphicsQueueIndex != m_chosen_device.PresentQueueIndex)
	{
		VkDeviceQueueCreateInfo queue_info = {};
		queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_info.pNext = nullptr;
		queue_info.flags = 0;
		queue_info.queueFamilyIndex = m_chosen_device.PresentQueueIndex;
		queue_info.queueCount = 1;
		queue_info.pQueuePriorities = &queue_priority;
		queues.Add(queue_info);
	}
	if (m_chosen_device.ComputeQueueIndex != m_chosen_device.PresentQueueIndex && m_chosen_device.ComputeQueueIndex != m_chosen_device.GraphicsQueueIndex)
	{
		VkDeviceQueueCreateInfo queue_info = {};
		queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_info.pNext = nullptr;
		queue_info.flags = 0;
		queue_info.queueFamilyIndex = m_chosen_device.ComputeQueueIndex;
		queue_info.queueCount = 1;
		queue_info.pQueuePriorities = &queue_priority;
		queues.Add(queue_info);
	}

	// TODO: Check if extensions/layers are supported
	Array<const char*> device_extensions;
	device_extensions.Add(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	Array<const char*> device_layers;
	device_layers.Add("VK_LAYER_LUNARG_standard_validation");

	VkPhysicalDeviceFeatures device_features = {};
	
	VkDeviceCreateInfo device_info = {};
	device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	device_info.pNext = nullptr;
	device_info.flags = 0;
	device_info.queueCreateInfoCount = (u32)queues.Num();
	device_info.pQueueCreateInfos = queues.Data();
	device_info.enabledExtensionCount = (u32)device_extensions.Num();
	device_info.ppEnabledExtensionNames = device_extensions.Data();
	device_info.enabledLayerCount = (u32)device_layers.Num();
	device_info.ppEnabledLayerNames = device_layers.Data();
	device_info.pEnabledFeatures = &device_features;

	EnsureVK(vkCreateDevice(m_chosen_device.PhysicalDevice, &device_info, m_allocation_callbacks, &m_device));

	vkGetDeviceQueue(m_device, m_chosen_device.GraphicsQueueIndex, 0, &m_graphics_queue);
	vkGetDeviceQueue(m_device, m_chosen_device.PresentQueueIndex, 0, &m_present_queue);
	vkGetDeviceQueue(m_device, m_chosen_device.ComputeQueueIndex, 0, &m_compute_queue);
}

void GPU_Vulkan::Shutdown()
{
	for (VkImageView view : m_swap_chain_image_views)
	{
		vkDestroyImageView(m_device, view, m_allocation_callbacks);
	}
	m_swap_chain_image_views.RemoveAll();

	vkDestroySwapchainKHR(m_device, m_swap_chain, m_allocation_callbacks);
	vkDestroySurfaceKHR(m_vkinst, m_surface, m_allocation_callbacks);
	auto vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)(vkGetInstanceProcAddr(m_vkinst, "vkDestroyDebugReportCallbackEXT"));
	vkDestroyDebugReportCallbackEXT(m_vkinst, m_debug_callback, m_allocation_callbacks);
	vkDestroyDevice(m_device, m_allocation_callbacks);
	vkDestroyInstance(m_vkinst, m_allocation_callbacks);
}

void GPU_Vulkan::CreateSwapChain(u32 width, u32 height)
{
	VkExtent2D swap_extent = { width, height };
	VkSurfaceFormatKHR format = { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	VkPresentModeKHR present_mode = m_present_mode;

	Array<u32> queue_families;
	if (m_chosen_device.GraphicsQueueIndex != m_chosen_device.PresentQueueIndex)
	{
		queue_families.Add(m_chosen_device.GraphicsQueueIndex);
		queue_families.Add(m_chosen_device.PresentQueueIndex);
	}

	VkSwapchainCreateInfoKHR swap_chain_create_info = {};
	swap_chain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swap_chain_create_info.pNext = nullptr;
	swap_chain_create_info.flags = 0;
	swap_chain_create_info.surface = m_surface;
	swap_chain_create_info.minImageCount = Max(m_chosen_device.SwapChain.Capabilities.minImageCount + 1, m_chosen_device.SwapChain.Capabilities.maxImageCount);
	swap_chain_create_info.imageFormat = format.format;
	swap_chain_create_info.imageColorSpace = format.colorSpace;
	swap_chain_create_info.imageExtent = swap_extent;
	swap_chain_create_info.imageArrayLayers = 1;
	swap_chain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swap_chain_create_info.imageSharingMode = queue_families.Num() == 0 ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT;
	swap_chain_create_info.queueFamilyIndexCount = (u32)queue_families.Num();
	swap_chain_create_info.pQueueFamilyIndices = queue_families.Data();
	swap_chain_create_info.preTransform = m_chosen_device.SwapChain.Capabilities.currentTransform;
	swap_chain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swap_chain_create_info.presentMode = present_mode;
	swap_chain_create_info.clipped = VK_TRUE;
	swap_chain_create_info.oldSwapchain = nullptr;

	EnsureVK(vkCreateSwapchainKHR(m_device, &swap_chain_create_info, m_allocation_callbacks, &m_swap_chain));

	m_swap_chain_dimensions[0] = swap_extent.width;
	m_swap_chain_dimensions[1] = swap_extent.height;

	u32 num_images = 0;
	EnsureVK(vkGetSwapchainImagesKHR(m_device, m_swap_chain, &num_images, nullptr));
	m_swap_chain_images.RemoveAll();
	m_swap_chain_images.AddZeroed(num_images);
	EnsureVK(vkGetSwapchainImagesKHR(m_device, m_swap_chain, &num_images, m_swap_chain_images.Data()));

	m_swap_chain_image_views.AddZeroed(num_images);
	
	VkImageViewCreateInfo image_view_create_info = {};
	image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	image_view_create_info.pNext = nullptr;
	image_view_create_info.flags = 0;
	image_view_create_info.image = nullptr;
	image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	image_view_create_info.format = format.format;
	image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_view_create_info.subresourceRange.baseMipLevel = 0;
	image_view_create_info.subresourceRange.levelCount = 1;
	image_view_create_info.subresourceRange.baseArrayLayer = 0;
	image_view_create_info.subresourceRange.layerCount = 0;

	for (u32 i = 0; i < num_images; ++i)
	{
		image_view_create_info.image = m_swap_chain_images[i];
		EnsureVK(vkCreateImageView(m_device, &image_view_create_info, m_allocation_callbacks, &m_swap_chain_image_views[i]));
	}
}

void GPU_Vulkan::ResizeSwapChain(u32 width, u32 height)
{
	Assert(false);
}

Vector<u32, 2> GPU_Vulkan::GetSwapChainDimensions()
{
	return m_swap_chain_dimensions;
}

GPUFrameInterface* GPU_Vulkan::BeginFrame(Vec4 scene_clear_color)
{
	return &m_frame_data;
}

void GPU_Vulkan::EndFrame(GPUFrameInterface* frame)
{
	Assert(frame == &m_frame_data);
}

void GPU_Vulkan::SubmitPass(const RenderPass& pass)
{
}


static PointerRange<const char> GetShaderDirectory() {
	struct Initializer {
		String path;
		Initializer() {
			auto exe_dir = mu::paths::GetExecutableDirectory();
			path = String::FromRanges(exe_dir, mu::Range("../Shaders/Vulkan/"));
		}
	};
	static Initializer init;
	return Range(init.path);
}

String GPU_Vulkan::GetShaderFilename(mu::PointerRange<const char> name)
{
	return String::FromRanges(GetShaderDirectory(), name, ".glsl");
}

VertexShaderID GPU_Vulkan::CompileVertexShaderHLSL(const char* entry_point, mu::PointerRange<const u8> code)
{
	return VertexShaderID{};
}

PixelShaderID GPU_Vulkan::CompilePixelShaderHLSL(const char* entry_point, mu::PointerRange<const u8> code)
{
	return PixelShaderID{};
}

ProgramID GPU_Vulkan::LinkProgram(VertexShaderID vertex_shader, PixelShaderID pixel_shader)
{
	return ProgramID{};
}

PipelineStateID GPU_Vulkan::CreatePipelineState(const GPU::PipelineStateDesc& desc)
{
	return PipelineStateID{};
}

void GPU_Vulkan::DestroyPipelineState(GPU::PipelineStateID id)
{
}

ConstantBufferID GPU_Vulkan::CreateConstantBuffer(mu::PointerRange<const u8> data)
{
	return ConstantBufferID{};
}

void GPU_Vulkan::DestroyConstantBuffer(ConstantBufferID id)
{
}

VertexBufferID GPU_Vulkan::CreateVertexBuffer(mu::PointerRange<const u8> data)
{
	return VertexBufferID{};
}

void GPU_Vulkan::DestroyVertexBuffer(GPU::VertexBufferID id) 
{
}

IndexBufferID GPU_Vulkan::CreateIndexBuffer(mu::PointerRange<const u8> data)
{
	return IndexBufferID{};
}

void GPU_Vulkan::DestroyIndexBuffer(GPU::IndexBufferID id)
{
}

TextureID GPU_Vulkan::CreateTexture2D(u32 width, u32 height, GPU::TextureFormat format, mu::PointerRange<const u8> data)
{
	return TextureID{};
}

DepthTargetID GPU_Vulkan::CreateDepthTarget(u32 width, u32 height)
{
	return DepthTargetID{};
}

ShaderResourceListID GPU_Vulkan::CreateShaderResourceList(const GPU::ShaderResourceListDesc& desc)
{
	return ShaderResourceListID{};
}