#include "GPU_Vulkan.h"

#include "mu-core/Array.h"
#include "mu-core/Debug.h"
#include "mu-core/FileReader.h"
#include "mu-core/Paths.h"
#include "mu-core/Pool.h"

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
#include <shaderc/shaderc.h>

#pragma warning(disable : 4100)
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "shaderc_combined.lib")

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
using FramebufferID = GPU::FramebufferID;
using RenderPass = GPU::RenderPass;

using namespace mu;
using std::tuple;

static const i32 MaxSwapChainImages = 8;

static const size_t MaxPersistentVertexBuffers		= 128;
static const size_t MaxPersistentIndexBuffers		= 128;
static const size_t MaxPersistentConstantBuffers	= 128;
static const size_t MaxDescriptorSets				= 128;
static const size_t MaxTextures						= 128;
static const size_t MaxShaderResourceLists			= 128;
static const size_t MaxDepthTargets					= 128;

struct VulkanVertexLayout
{
	FixedArray<VkVertexInputBindingDescription, GPU::MaxStreamSlots> m_bindings;
	FixedArray<VkVertexInputAttributeDescription, GPU::MaxStreamSlots * GPU::MaxStreamElements> m_attributes;

	static VkFormat GetStreamElementFormat(GPU::StreamElementDesc desc)
	{
		switch (desc.Type)
		{
		case GPU::ScalarType::Float:
		{
			static const VkFormat float_formats[] = {
				VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT,
			};
			return float_formats[desc.CountMinusOne];
		}
		case GPU::ScalarType::U8:
		{
			static const VkFormat byte_formats[] = {
				VK_FORMAT_R8_UINT, VK_FORMAT_R8G8_UINT, VK_FORMAT_R8G8B8A8_UINT, VK_FORMAT_R8G8B8A8_UINT,
			};
			static const VkFormat byte_formats_norm[] = {
				VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM,
			};
			return (desc.Normalized ? byte_formats_norm : byte_formats)[desc.CountMinusOne];
		}
		}

		Assert(false);
		return VK_FORMAT_UNDEFINED;
	}

	u32 GetStreamElementSize(GPU::StreamElementDesc desc)
	{
		switch (desc.Type)
		{
		case GPU::ScalarType::Float:
		{
			return sizeof(f32) * (desc.CountMinusOne + 1);
		}
		case GPU::ScalarType::U8:
		{
			return desc.CountMinusOne + 1;
		}
		}

		Assert(false);
		return 0;
	}


	VulkanVertexLayout(const GPU::StreamFormatDesc& layout)
	{
		u32 slot_index = 0;
		u32 location = 0;
		for (const GPU::StreamSlotDesc& slot : layout.Slots)
		{
			u32 current_offset = 0;
			for (const GPU::StreamElementDesc& element : slot.Elements)
			{
				// Add one attribute per element
				VkVertexInputAttributeDescription attribute = {};
				attribute.location = location;
				attribute.binding = slot_index;
				attribute.format = GetStreamElementFormat(element);
				attribute.offset = current_offset;
				m_attributes.Add(attribute);
				location += 1; // TODO: Attributes that consume multiple locations

				current_offset += GetStreamElementSize(element);
			}

			// Add one binding per slot
			VkVertexInputBindingDescription binding = {};
			binding.binding = slot_index;
			binding.stride = current_offset;
			binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			m_bindings.Add(binding);
			++slot_index;
		}
	}
};

struct VulkanSubAllocation
{
	VkBuffer		m_buffer;
	VkDeviceAddress m_offset;
	VkDeviceSize	m_size;
	void*			m_mapped;
};

struct VulkanLinearAllocator
{
	VulkanLinearAllocator(VkDevice device, VkAllocationCallbacks* allocation_callbacks, VkBufferUsageFlags usage, u32 queue_index, u32 memory_type_index)
	{
		VkBufferCreateInfo buffer_info = {};
		buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_info.pNext = nullptr;
		buffer_info.flags = 0;
		buffer_info.size = m_max_size;
		buffer_info.usage = usage;
		buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		buffer_info.queueFamilyIndexCount = 1;
		buffer_info.pQueueFamilyIndices = &queue_index;

		EnsureVK(vkCreateBuffer(device, &buffer_info, allocation_callbacks, &m_buffer));

		VkMemoryRequirements memory_requirements = {};
		vkGetBufferMemoryRequirements(device, m_buffer, &memory_requirements);

		Assert(memory_requirements.size == m_max_size);

		VkMemoryAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.pNext = nullptr;
		alloc_info.allocationSize = m_max_size;
		alloc_info.memoryTypeIndex = memory_type_index;
		EnsureVK(vkAllocateMemory(device, &alloc_info, allocation_callbacks, &m_device_memory));

		EnsureVK(vkBindBufferMemory(device, m_buffer, m_device_memory, 0));

		EnsureVK(vkMapMemory(device, m_device_memory, 0, m_max_size, 0, (void**)&m_mapped));
	}

	VulkanLinearAllocator(VulkanLinearAllocator&&) = default;
	VulkanLinearAllocator& operator=(VulkanLinearAllocator&&) = default;

	void Destroy(VkDevice device, VkAllocationCallbacks* alloc_callbacks)
	{
		vkDestroyBuffer(device, m_buffer, alloc_callbacks);
		vkFreeMemory(device, m_device_memory, alloc_callbacks);
	}

	void Reset()
	{
		m_current_offset = 0;
	}

	VulkanSubAllocation Allocate(u64 size, u64 alignment)
	{
		VkDeviceAddress offset = AlignUp(m_current_offset, alignment);
		m_current_offset = offset + size;
		Assert(m_current_offset <= m_max_size);
		return { m_buffer, offset, size, (void*)(m_mapped + offset) };
	}

	VkBuffer m_buffer = nullptr;
	VkDeviceMemory m_device_memory = nullptr;
	const u64 m_max_size = 2 * 1024 * 1024;
	u64 m_current_offset = 0;
	uptr m_mapped = 0;
};

struct GPU_Vulkan_Frame : public GPUFrameInterface 
{
	GPU_Vulkan_Frame(VulkanLinearAllocator linear_allocator, VkDescriptorPool descriptor_pool, VkPhysicalDeviceLimits limits)
		: m_linear_allocator(std::move(linear_allocator))
		, m_descriptor_pool(descriptor_pool)
		, m_uniform_alignment(limits.minUniformBufferOffsetAlignment)
	{
	}

	void Destroy(VkDevice device, VkAllocationCallbacks* alloc_callbacks)
	{
		vkDestroyFence(device, m_fence, alloc_callbacks);
		vkDestroySemaphore(device, m_backbuffer_semaphore, alloc_callbacks);
		vkDestroySemaphore(device, m_render_complete_semaphore, alloc_callbacks);

		// vkFreeCommandBuffers(device, m_command_pool, m_command_buffers.Num(), m_command_buffers.Data());
		// vkFreeCommandBuffers(device, m_command_pool, m_used_command_buffers.Num(), m_used_command_buffers.Data());

		vkDestroyCommandPool(device, m_command_pool, alloc_callbacks);
		
		vkDestroyDescriptorPool(device, m_descriptor_pool, alloc_callbacks);

		m_linear_allocator.Destroy(device, alloc_callbacks);
	}

	VkCommandBuffer GetCommandBuffer(VkDevice device)
	{
		if (m_command_buffers.Num() == 0)
		{
			m_command_buffers.AddZeroed(10);
			VkCommandBufferAllocateInfo command_buffer_info = {};
			command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			command_buffer_info.pNext = nullptr;
			command_buffer_info.commandPool = m_command_pool;
			command_buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			command_buffer_info.commandBufferCount = (u32)m_command_buffers.Num();
			EnsureVK(vkAllocateCommandBuffers(device, &command_buffer_info, m_command_buffers.Data()));
		}

		VkCommandBuffer buffer = m_command_buffers[m_command_buffers.Num() - 1];
		m_command_buffers.RemoveAt(m_command_buffers.Num() - 1);
		m_used_command_buffers.Add(buffer);
		return buffer;
	}

	void ResetFrame(VkDevice device)
	{
		m_command_buffers.Append(std::move(m_used_command_buffers));
		m_used_command_buffers.RemoveAll();

		vkResetCommandPool(device, m_command_pool, 0);
		vkResetDescriptorPool(device, m_descriptor_pool, 0);

		m_linear_allocator.Reset();
		m_temp_vbs.RemoveAll();
		m_temp_ibs.RemoveAll();
		m_temp_cbs.RemoveAll();
	}

	virtual ConstantBufferID GetTemporaryConstantBuffer(PointerRange<const u8> data)
	{
		VulkanSubAllocation alloc = m_linear_allocator.Allocate(data.Size(), m_uniform_alignment);
		size_t idx = m_temp_cbs.Add(alloc);
		memcpy(alloc.m_mapped, data.m_start, data.Size());
		return ConstantBufferID{ MaxPersistentConstantBuffers + idx };
	}
	virtual VertexBufferID GetTemporaryVertexBuffer(PointerRange<const u8> data)
	{
		VulkanSubAllocation alloc = m_linear_allocator.Allocate(data.Size(), 1);
		size_t idx = m_temp_vbs.Add(alloc);
		memcpy(alloc.m_mapped, data.m_start, data.Size());
		return VertexBufferID{ MaxPersistentVertexBuffers + idx };
	}
	virtual IndexBufferID GetTemporaryIndexBuffer(PointerRange<const u8> data) override
	{
		VulkanSubAllocation alloc = m_linear_allocator.Allocate(data.Size(), 1);
		size_t idx = m_temp_ibs.Add(alloc);
		memcpy(alloc.m_mapped, data.m_start, data.Size());
		return IndexBufferID{ MaxPersistentIndexBuffers + idx };
	}

	tuple<VkBuffer, VkDeviceAddress> LookupTempVertexBuffer(size_t index)
	{
		Assert(index < m_temp_vbs.Num());
		const VulkanSubAllocation& alloc = m_temp_vbs[index];
		return { alloc.m_buffer, alloc.m_offset };
	}

	tuple<VkBuffer, VkDeviceAddress> LookupTempIndexBuffer(size_t index)
	{
		Assert(index < m_temp_ibs.Num());
		const VulkanSubAllocation& alloc = m_temp_ibs[index];
		return { alloc.m_buffer, alloc.m_offset };
	}

	VkDescriptorBufferInfo LookupTempConstantBuffer(size_t index)
	{
		Assert(index < m_temp_cbs.Num());
		const VulkanSubAllocation& alloc = m_temp_cbs[index];
		return { alloc.m_buffer, alloc.m_offset, alloc.m_size };
	}


	u32 m_swap_chain_image_index = 0;

	VulkanLinearAllocator m_linear_allocator;

	VkFence		m_fence = nullptr;
	VkSemaphore m_backbuffer_semaphore = nullptr;
	VkSemaphore m_render_complete_semaphore = nullptr;

	VkCommandPool			m_command_pool = nullptr;
	Array<VkCommandBuffer>	m_command_buffers;
	Array<VkCommandBuffer>	m_used_command_buffers;

	Array<VulkanSubAllocation> m_temp_vbs;
	Array<VulkanSubAllocation> m_temp_ibs;
	Array<VulkanSubAllocation> m_temp_cbs;

	VkDescriptorPool m_descriptor_pool;

	VkDeviceSize m_uniform_alignment;
};

struct SwapChainSupport
{
	VkSurfaceCapabilitiesKHR Capabilities;
	Array<VkSurfaceFormatKHR> Formats;
	Array<VkPresentModeKHR> PresentModes;
};

struct PhysicalDeviceSelection
{
	VkPhysicalDevice			PhysicalDevice				= nullptr;
	VkPhysicalDeviceProperties	DeviceProperties			= {};
	u32							GraphicsQueueIndex			= 0xffffff;
	u32							PresentQueueIndex			= 0xffffff;
	u32							ComputeQueueIndex			= 0xffffff;
	SwapChainSupport			SwapChain					= {};
	u32							m_upload_memory_type_index = 0xffffffff;
	u32							m_device_memory_type_index = 0xffffffff;
};

VkPrimitiveTopology CommonToVK(GPU::PrimitiveTopology topo)
{
	i32 i = (i32)topo;

	VkPrimitiveTopology topo_list[] = {
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
		VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
	};

	Assert(i >= 0 && i < ArraySize(topo_list));
	return topo_list[i];
}

VkCompareOp CommonToVK(GPU::ComparisonFunc func)
{
	i32 i = (i32)func;

	VkCompareOp ops[] = {
		VK_COMPARE_OP_NEVER,			//Never,
		VK_COMPARE_OP_LESS,				//Less,
		VK_COMPARE_OP_EQUAL,			//Equal,
		VK_COMPARE_OP_LESS_OR_EQUAL,	//LessThanOrEqual,
		VK_COMPARE_OP_GREATER,			//Greater,
		VK_COMPARE_OP_NOT_EQUAL,		//NotEqual,
		VK_COMPARE_OP_GREATER_OR_EQUAL, //GreaterThanOrEqual,
		VK_COMPARE_OP_ALWAYS,			//Always
	};
	Assert(i >= 0 && i < ArraySize(ops));
	return ops[i];
}

VkPolygonMode CommonToVK(GPU::FillMode mode)
{
	i32 i = (i32)mode;

	VkPolygonMode modes[] = {
		VK_POLYGON_MODE_FILL, // Solid,
		VK_POLYGON_MODE_LINE, // Wireframe,
	};
	Assert(i >= 0 && i < ArraySize(modes));
	return modes[i];
}

VkCullModeFlags CommonToVK(GPU::CullMode mode)
{
	i32 i = (i32)mode;

	VkCullModeFlags modes[] = {
		VK_CULL_MODE_NONE,		// None,
		VK_CULL_MODE_FRONT_BIT, // Front,
		VK_CULL_MODE_BACK_BIT,	// Back
	};
	Assert(i >= 0 && i < ArraySize(modes));
	return modes[i];
}

VkFrontFace CommonToVK(GPU::FrontFace mode)
{
	i32 i = (i32)mode;

	VkFrontFace modes[] = {
		VK_FRONT_FACE_CLOCKWISE,		// Clockwise,
		VK_FRONT_FACE_COUNTER_CLOCKWISE // CounterClockwise,
	};
	Assert(i >= 0 && i < ArraySize(modes));
	return modes[i];
}

VkFormat CommonToVK(GPU::TextureFormat format)
{
	i32 i = (i32)format;

	VkFormat formats[] = {
		VK_FORMAT_R8G8B8_UNORM,
		VK_FORMAT_R8G8B8A8_UNORM,
	};
	Assert(i >= 0 && i < ArraySize(formats));
	return formats[i];
}

VkBlendFactor CommonToVK(GPU::BlendValue value)
{
	i32 i = (i32)value;

	VkBlendFactor factors[] = {
		VK_BLEND_FACTOR_ZERO,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_SRC_COLOR,
		VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
		VK_BLEND_FACTOR_SRC_ALPHA,
		VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		VK_BLEND_FACTOR_DST_ALPHA,
		VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
		VK_BLEND_FACTOR_DST_COLOR,
		VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
	};
	Assert(i >= 0 && i < ArraySize(factors));
	return factors[i];
}

VkBlendOp CommonToVK(GPU::BlendOp op)
{
	i32 i = (i32)op;

	VkBlendOp ops[] = {
		VK_BLEND_OP_ADD,
		VK_BLEND_OP_SUBTRACT,
		VK_BLEND_OP_REVERSE_SUBTRACT,
		VK_BLEND_OP_MIN,
		VK_BLEND_OP_MAX,
	};
	Assert(i >= 0 && i < ArraySize(ops));
	return ops[i];
}

struct GPU_Vulkan : public GPUInterface {

	// Internal types
	struct LinkedProgram {
		VertexShaderID VertexShader;
		PixelShaderID PixelShader;

		VkPipelineShaderStageCreateInfo ShaderStages[2];
	};

	struct PipelineState {
		VkPipeline				m_pipeline = nullptr;
		GPU::PipelineStateDesc	m_desc;
	};

	struct VertexBuffer {
		VkBuffer		m_buffer = nullptr;
		VkDeviceMemory	m_device_memory = nullptr;
	};

	struct IndexBuffer {
		VkBuffer		m_buffer = nullptr;
		VkDeviceMemory	m_device_memory = nullptr;
	};

	struct ConstantBuffer {
		VkBuffer		m_buffer = nullptr;
		VkDeviceMemory	m_device_memory = nullptr;
	};

	struct Texture2D {
		VkImage			m_image = nullptr;
		VkImageView		m_image_view = nullptr;
		VkDeviceMemory	m_device_memory = nullptr;
	};

	struct ShaderResourceList {
		GPU::ShaderResourceListDesc Desc;
	};

	struct Framebuffer {
		GPU::FramebufferDesc Desc;

		VkFramebuffer m_framebuffer;
		FixedArray<VkFramebuffer, MaxSwapChainImages> m_frame_framebuffers;

		VkFramebuffer GetForFrame(u32 frame_index) {
			if (m_framebuffer) {
				return m_framebuffer;
			}
			Assert(frame_index < m_frame_framebuffers.Num());
			return m_frame_framebuffers[frame_index];
		}
	};

	struct DepthTarget {
		VkImage			m_image = nullptr;
		VkDeviceMemory	m_device_memory = nullptr;
		VkImageView		m_image_view = nullptr;
	};

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

	virtual GPU::VertexShaderID CompileVertexShaderHLSL(PointerRange<const char> name) override;
	virtual PixelShaderID CompilePixelShaderHLSL(PointerRange<const char> name) override;
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

	virtual FramebufferID CreateFramebuffer(const GPU::FramebufferDesc& desc) override;
	virtual void DestroyFramebuffer(FramebufferID) override;

	virtual const char* GetName() override
	{
		return "Vulkan";
	}
	// END GPUInterface functions
	
	static VkBool32 VKAPI_CALL DebugMessengerCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
		const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
		void*                                            pUserData)
	{
		if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		{
			dbg::AssertImpl(false, "{} {} : {}",
				pCallbackData->pMessageIdName, pCallbackData->messageIdNumber,
				pCallbackData->pMessage
			);
		}
		else 
		{
			dbg::Log("{} {} : {}", 
				pCallbackData->pMessageIdName, pCallbackData->messageIdNumber,
				pCallbackData->pMessage
			);
		}

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

	PhysicalDeviceSelection m_chosen_device;
	VkPresentModeKHR m_present_mode = VK_PRESENT_MODE_FIFO_KHR;

	VkInstance	m_vkinst			= nullptr;
	VkDevice	m_device			= nullptr;
	VkQueue		m_graphics_queue	= nullptr;
	VkQueue		m_present_queue		= nullptr;
	VkQueue		m_compute_queue		= nullptr;

	VkCommandPool	m_transfer_command_pool		= nullptr;
	VkCommandBuffer m_transfer_command_buffer	= nullptr;
	VkFence			m_transfer_complete_fence	= nullptr;

	VkSurfaceKHR		m_surface		= nullptr;
	VkSwapchainKHR		m_swap_chain	= nullptr;
	Vector<u32, 2>		m_swap_chain_dimensions = { 0, 0 };
	Array<VkImage>		m_swap_chain_images;
	Array<VkImageView>	m_swap_chain_image_views;
	VkViewport			m_viewport;

	Pool<VkShaderModule, VertexShaderID>			m_registered_vertex_shaders{ 128 };
	Pool<VkShaderModule, PixelShaderID>				m_registered_pixel_shaders{ 128 };
	Pool<LinkedProgram, ProgramID>					m_linked_programs{ 128 };
	Pool<PipelineState, PipelineStateID>			m_pipeline_states{ 128 };
	Pool<VertexBuffer, VertexBufferID>				m_vertex_buffers{ MaxPersistentVertexBuffers };
	Pool<IndexBuffer, IndexBufferID>				m_index_buffers{ MaxPersistentIndexBuffers };
	Pool<ConstantBuffer, ConstantBufferID>			m_constant_buffers{ MaxPersistentConstantBuffers };
	Pool<Texture2D, TextureID>						m_textures{ MaxTextures };
	Pool<ShaderResourceList, ShaderResourceListID>	m_shader_resource_lists{ MaxShaderResourceLists };
	Pool<DepthTarget, DepthTargetID>				m_depth_targets{ MaxDepthTargets };
	Pool<Framebuffer, FramebufferID>				m_framebuffers{ 128 };

	// Temporary descriptor set layouts
	// TODO: Expose descriptor set layouts to higher level? Design descriptor set layouts to allow some guarantees about update frequency? Hide it all and use push constants to index large buffers?
	// Uniform buffers
	VkDescriptorSetLayout m_descriptor_set_layout_0 = nullptr;
	// Shader resource views/sampled image
	VkDescriptorSetLayout m_descriptor_set_layout_1 = nullptr;
	// Samplers
	VkDescriptorSetLayout m_descriptor_set_layout_2 = nullptr;

	VkSampler m_sampler = nullptr;

	VkPipelineLayout m_pipeline_layout				= nullptr;

	// TODO: Expose render passes for pipeline state creation somehow
	VkRenderPass m_render_pass						= nullptr;
	VkRenderPass m_render_pass_depth				= nullptr;

	GPU_Vulkan_Frame* m_frame_data;
	u64 m_frame_count = 0;

	shaderc_compiler_t m_shader_compiler = shaderc_compiler_initialize();

	tuple<VkBuffer, VkDeviceAddress> GetVertexBufferForBinding(VertexBufferID id, GPU_Vulkan_Frame& frame);
	tuple<VkBuffer, VkDeviceAddress> GetIndexBufferForBinding(IndexBufferID id, GPU_Vulkan_Frame& frame);
	VkDescriptorBufferInfo GetConstantBufferForBinding(ConstantBufferID id, GPU_Vulkan_Frame& frame);

	struct {
		PFN_vkSetDebugUtilsObjectNameEXT VkSetDebugUtilsObjectNameEXT = nullptr;
		PFN_vkSetDebugUtilsObjectTagEXT VkSetDebugUtilsObjectTagEXT = nullptr;
		PFN_vkQueueBeginDebugUtilsLabelEXT VkQueueBeginDebugUtilsLabelEXT = nullptr;
		PFN_vkQueueEndDebugUtilsLabelEXT VkQueueEndDebugUtilsLabelEXT = nullptr;
		PFN_vkQueueInsertDebugUtilsLabelEXT VkQueueInsertDebugUtilsLabelEXT = nullptr;
		PFN_vkCmdBeginDebugUtilsLabelEXT VkCmdBeginDebugUtilsLabelEXT = nullptr;
		PFN_vkCmdEndDebugUtilsLabelEXT VkCmdEndDebugUtilsLabelEXT = nullptr;
		PFN_vkCmdInsertDebugUtilsLabelEXT VkCmdInsertDebugUtilsLabelEXT = nullptr;
		PFN_vkCreateDebugUtilsMessengerEXT VkCreateDebugUtilsMessengerEXT = nullptr;
		PFN_vkDestroyDebugUtilsMessengerEXT VkDestroyDebugUtilsMessengerEXT = nullptr;
		PFN_vkSubmitDebugUtilsMessageEXT VkSubmitDebugUtilsMessageEXT = nullptr;

		VkDebugUtilsMessengerEXT m_messenger;
	} m_debug_utils;

	void CmdDebugBegin(VkCommandBuffer command_buffer, const char* name) {
		VkDebugUtilsLabelEXT label = {};
		label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		label.pNext = nullptr;
		label.pLabelName = name;
		label.color[4];
		m_debug_utils.VkCmdBeginDebugUtilsLabelEXT(command_buffer, &label);
	}
	void CmdDebugEnd(VkCommandBuffer command_buffer) {
		m_debug_utils.VkCmdEndDebugUtilsLabelEXT(command_buffer);
	}
};

GPU_Vulkan::GPU_Vulkan() {
}

GPU_Vulkan::~GPU_Vulkan() {
	shaderc_compiler_release(m_shader_compiler);
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

		dbg::Log(" Graphics queue {}", graphics_queue_index);
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
		dbg::Log(" Compute queue {}", compute_queue_index);

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

		dbg::Log(" Present queue {}", present_queue_index);

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


		VkPhysicalDeviceMemoryProperties memory_properties = {};
		vkGetPhysicalDeviceMemoryProperties(device, &memory_properties);
		dbg::Log(" {} memory heaps", memory_properties.memoryHeapCount);
		for (u32 i = 0; i < memory_properties.memoryHeapCount; ++i) {
			const VkMemoryHeap& heap = memory_properties.memoryHeaps[i];
			dbg::Log("  Heap {} - {} bytes", i, heap.size);

			if( heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ) {
				dbg::Log("   Device local");
			}
			if (heap.flags & VK_MEMORY_HEAP_MULTI_INSTANCE_BIT) {
				dbg::Log("   Multi-instance");
			}
		}

		dbg::Log(" {} memory types", memory_properties.memoryTypeCount);
		for (u32 i = 0; i < memory_properties.memoryTypeCount; ++i) {
			const VkMemoryType& type = memory_properties.memoryTypes[i];
			dbg::Log("  Type {} on heap {} flags {}", i, type.heapIndex, type.propertyFlags);

			if (type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
				dbg::Log("   Device-local");
			}
			if (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
				dbg::Log("   Host-visible");
			}
			if (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) {
				dbg::Log("   Host-coherent");
			}
			if (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) {
				dbg::Log("   Host-cached");
			}
			if (type.propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) {
				dbg::Log("   Lazily-allocated");
			}
			if (type.propertyFlags & VK_MEMORY_PROPERTY_PROTECTED_BIT) {
				dbg::Log("   Protected");
			}
		}

		u32 upload_memory_type = memory_properties.memoryTypeCount + 1;
		for (u32 i = 0; i < memory_properties.memoryTypeCount; ++i) {
			const VkMemoryType& type = memory_properties.memoryTypes[i];
			VkMemoryPropertyFlags required_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			if (type.propertyFlags & required_flags ){
				upload_memory_type = i;
				break;
			}
		}

		if (upload_memory_type >= memory_properties.memoryTypeCount) {
			dbg::Log("No memory type for uploading");
			continue;
		}

		u32 device_memory_type = upload_memory_type;
		for (u32 i = 0; i < memory_properties.memoryTypeCount; ++i) {
			const VkMemoryType& type = memory_properties.memoryTypes[i];
			VkMemoryPropertyFlags required_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			if (type.propertyFlags & required_flags) {
				device_memory_type = i;
				break;
			}
		}
		if (device_memory_type >= memory_properties.memoryTypeCount) {
			dbg::Log("No memory type for device-local resources");
			continue;
		}

		return PhysicalDeviceSelection
		{ 
			device, 
			device_props,
			graphics_queue_index, 
			present_queue_index, 
			compute_queue_index, 
			swap_chain_support,
			upload_memory_type,
			device_memory_type
		};
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
	//instance_extensions.Add(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	instance_extensions.Add(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

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

	m_debug_utils.VkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(m_vkinst, "vkSetDebugUtilsObjectNameEXT");
	m_debug_utils.VkSetDebugUtilsObjectTagEXT = (PFN_vkSetDebugUtilsObjectTagEXT)vkGetInstanceProcAddr(m_vkinst, "vkSetDebugUtilsObjectTagEXT");
	m_debug_utils.VkQueueBeginDebugUtilsLabelEXT = nullptr;
	m_debug_utils.VkQueueEndDebugUtilsLabelEXT = nullptr;
	m_debug_utils.VkQueueInsertDebugUtilsLabelEXT = nullptr;
	m_debug_utils.VkCmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(m_vkinst, "vkCmdBeginDebugUtilsLabelEXT");
	m_debug_utils.VkCmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(m_vkinst, "vkCmdEndDebugUtilsLabelEXT");;
	m_debug_utils.VkCmdInsertDebugUtilsLabelEXT = nullptr;
	m_debug_utils.VkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_vkinst, "vkCreateDebugUtilsMessengerEXT");
	m_debug_utils.VkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_vkinst, "vkDestroyDebugUtilsMessengerEXT");;
	m_debug_utils.VkSubmitDebugUtilsMessageEXT = nullptr;

	VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info = {};
	debug_messenger_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debug_messenger_info.pNext = nullptr;
	debug_messenger_info.flags = 0;
	debug_messenger_info.messageSeverity = 0
		//| VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
		//| VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
		;
	debug_messenger_info.messageType = 0 
		| VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
		;
	debug_messenger_info.pfnUserCallback = DebugMessengerCallback;
	debug_messenger_info.pUserData = this;
	m_debug_utils.VkCreateDebugUtilsMessengerEXT(m_vkinst, &debug_messenger_info, m_allocation_callbacks, &m_debug_utils.m_messenger);

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
	
	{
		VkPhysicalDeviceProperties device_props;
		vkGetPhysicalDeviceProperties(m_chosen_device.PhysicalDevice, &device_props);
		dbg::Log("Chose physical device {}", device_props.deviceName);
	}

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
	device_features.fillModeNonSolid = VK_TRUE;
	
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

	VkDescriptorSetLayoutBinding descriptor_set_0_bindings[] = {
		{
			0,									/* binding */
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,	/* descriptorType */
			1,									/* descriptorCount */
			VK_SHADER_STAGE_ALL_GRAPHICS,		/* stageFlags */
			nullptr,							/* pImmutableSamplers */
		},
		{
			1,									/* binding */
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,	/* descriptorType */
			1,									/* descriptorCount */
			VK_SHADER_STAGE_ALL_GRAPHICS,		/* stageFlags */
			nullptr,							/* pImmutableSamplers */
		},
	};

	VkDescriptorSetLayoutCreateInfo descriptor_set_0_info = {};
	descriptor_set_0_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptor_set_0_info.pNext = nullptr;
	descriptor_set_0_info.flags = 0;
	descriptor_set_0_info.bindingCount = (u32)ArraySize(descriptor_set_0_bindings);
	descriptor_set_0_info.pBindings = descriptor_set_0_bindings;	
	EnsureVK(vkCreateDescriptorSetLayout(m_device, &descriptor_set_0_info, m_allocation_callbacks, &m_descriptor_set_layout_0));
	
	VkDescriptorSetLayoutBinding descriptor_set_1_bindings[] = {
		{
			0,									/* binding */
			VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,	/* descriptorType */
			1,									/* descriptorCount */
			VK_SHADER_STAGE_ALL_GRAPHICS,		/* stageFlags */
			nullptr,							/* pImmutableSamplers */
		},
	};
	VkDescriptorSetLayoutCreateInfo descriptor_set_1_info = {};
	descriptor_set_1_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptor_set_1_info.pNext = nullptr;
	descriptor_set_1_info.flags = 0;
	descriptor_set_1_info.bindingCount = (u32)ArraySize(descriptor_set_1_bindings);
	descriptor_set_1_info.pBindings = descriptor_set_1_bindings;
	EnsureVK(vkCreateDescriptorSetLayout(m_device, &descriptor_set_1_info, m_allocation_callbacks, &m_descriptor_set_layout_1));

	VkSamplerCreateInfo sampler_create_info = {};
	sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_create_info.pNext = nullptr;
	sampler_create_info.flags = 0;
	sampler_create_info.magFilter = VK_FILTER_NEAREST;
	sampler_create_info.minFilter = VK_FILTER_NEAREST;
	sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_create_info.mipLodBias = 0;
	sampler_create_info.anisotropyEnable = VK_FALSE;
	sampler_create_info.maxAnisotropy = 0.0f;
	sampler_create_info.compareEnable = VK_FALSE;
	sampler_create_info.compareOp = VK_COMPARE_OP_NEVER;
	sampler_create_info.minLod = 0.0f;
	sampler_create_info.maxLod = m_chosen_device.DeviceProperties.limits.maxSamplerLodBias; // correct?
	sampler_create_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	sampler_create_info.unnormalizedCoordinates = VK_FALSE;
	EnsureVK(vkCreateSampler(m_device, &sampler_create_info, m_allocation_callbacks, &m_sampler));

	VkSampler descriptor_set_2_immutable_samplers[] = {
		m_sampler
	};

	VkDescriptorSetLayoutBinding descriptor_set_2_bindings[] = {
		{
			0,									/* binding */
			VK_DESCRIPTOR_TYPE_SAMPLER,			/* descriptorType */
			1,									/* descriptorCount */
			VK_SHADER_STAGE_ALL_GRAPHICS,		/* stageFlags */
			descriptor_set_2_immutable_samplers,/* pImmutableSamplers */
		},
	};
	VkDescriptorSetLayoutCreateInfo descriptor_set_2_info = {};
	descriptor_set_2_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptor_set_2_info.pNext = nullptr;
	descriptor_set_2_info.flags = 0;
	descriptor_set_2_info.bindingCount = (u32)ArraySize(descriptor_set_2_bindings);
	descriptor_set_2_info.pBindings = descriptor_set_2_bindings;
	EnsureVK(vkCreateDescriptorSetLayout(m_device, &descriptor_set_2_info, m_allocation_callbacks, &m_descriptor_set_layout_2));

	VkDescriptorSetLayout set_layouts[] = {
		m_descriptor_set_layout_0,
		m_descriptor_set_layout_1,
		m_descriptor_set_layout_2
	};

	VkPipelineLayoutCreateInfo pipeline_layout_info = { };
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.pNext = nullptr;
	pipeline_layout_info.flags = 0;
	pipeline_layout_info.setLayoutCount = (u32)ArraySize(set_layouts);
	pipeline_layout_info.pSetLayouts = set_layouts;
	pipeline_layout_info.pushConstantRangeCount = 0;
	pipeline_layout_info.pPushConstantRanges = nullptr;
	EnsureVK(vkCreatePipelineLayout(m_device, &pipeline_layout_info, m_allocation_callbacks, &m_pipeline_layout));

	{
		VkAttachmentDescription color_attachment = {};
		color_attachment.flags = 0;
		color_attachment.format = VK_FORMAT_B8G8R8A8_UNORM;
		color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color_attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription depth_attachment = {};
		depth_attachment.flags = 0;
		depth_attachment.format = VK_FORMAT_D32_SFLOAT;
		depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth_attachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription attachments[] = {
			color_attachment,
			depth_attachment,
		};

		{
			VkAttachmentReference color_attachment_ref = {};
			color_attachment_ref.attachment = 0;
			color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			// TODO: promote to API so it can be used for PSO creation
			VkSubpassDescription subpass_info = {};
			subpass_info.flags = 0;
			subpass_info.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass_info.inputAttachmentCount;
			subpass_info.pInputAttachments;
			subpass_info.colorAttachmentCount = 1;
			subpass_info.pColorAttachments = &color_attachment_ref;
			subpass_info.pResolveAttachments;
			subpass_info.pDepthStencilAttachment = nullptr;
			subpass_info.preserveAttachmentCount;
			subpass_info.pPreserveAttachments;

			VkRenderPassCreateInfo render_pass_info = {};
			render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			render_pass_info.pNext = nullptr;
			render_pass_info.flags = 0;
			render_pass_info.attachmentCount = 1;
			render_pass_info.pAttachments = attachments;
			render_pass_info.subpassCount = 1;
			render_pass_info.pSubpasses = &subpass_info;
			render_pass_info.dependencyCount = 0;
			render_pass_info.pDependencies = nullptr;
			vkCreateRenderPass(m_device, &render_pass_info, m_allocation_callbacks, &m_render_pass);
		}

		{
			VkAttachmentReference color_attachment_ref = {};
			color_attachment_ref.attachment = 0;
			color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkAttachmentReference depth_attachment_ref = {};
			depth_attachment_ref.attachment = 1;
			depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			// TODO: promote to API so it can be used for PSO creation
			VkSubpassDescription subpass_info = {};
			subpass_info.flags = 0;
			subpass_info.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass_info.inputAttachmentCount;
			subpass_info.pInputAttachments;
			subpass_info.colorAttachmentCount = 1;
			subpass_info.pColorAttachments = &color_attachment_ref;
			subpass_info.pResolveAttachments;
			subpass_info.pDepthStencilAttachment = &depth_attachment_ref;
			subpass_info.preserveAttachmentCount;
			subpass_info.pPreserveAttachments;

			VkRenderPassCreateInfo render_pass_info = {};
			render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			render_pass_info.pNext = nullptr;
			render_pass_info.flags = 0;
			render_pass_info.attachmentCount = (u32)ArraySize(attachments);
			render_pass_info.pAttachments = attachments;
			render_pass_info.subpassCount = 1;
			render_pass_info.pSubpasses = &subpass_info;
			render_pass_info.dependencyCount = 0;
			render_pass_info.pDependencies = nullptr;
			vkCreateRenderPass(m_device, &render_pass_info, m_allocation_callbacks, &m_render_pass_depth);
		}
	}
	for (i32 i = 0; i < 1; ++i)
	{
		VulkanLinearAllocator linear_allocator{
			m_device, m_allocation_callbacks, 
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			m_chosen_device.GraphicsQueueIndex,
			m_chosen_device.m_upload_memory_type_index
		};

		VkDescriptorPoolSize pool_sizes[] = {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MaxDescriptorSets * 2 }		
		};

		VkDescriptorPoolCreateInfo descriptor_pool_0_create_info = {};
		descriptor_pool_0_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptor_pool_0_create_info.pNext = nullptr;
		descriptor_pool_0_create_info.flags = 0;
		descriptor_pool_0_create_info.maxSets = MaxDescriptorSets;
		descriptor_pool_0_create_info.poolSizeCount = (u32)ArraySize(pool_sizes);
		descriptor_pool_0_create_info.pPoolSizes = pool_sizes;

		VkDescriptorPool descriptor_pool_0;
		EnsureVK(vkCreateDescriptorPool(m_device, &descriptor_pool_0_create_info, m_allocation_callbacks, &descriptor_pool_0));
		m_frame_data = new GPU_Vulkan_Frame(std::move(linear_allocator), descriptor_pool_0, m_chosen_device.DeviceProperties.limits);

		auto& frame = *m_frame_data;
		VkSemaphoreCreateInfo semaphore_info = {};
		semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		semaphore_info.flags = 0;
		semaphore_info.pNext = nullptr;
		EnsureVK(vkCreateSemaphore(m_device, &semaphore_info, m_allocation_callbacks, &frame.m_backbuffer_semaphore));
		EnsureVK(vkCreateSemaphore(m_device, &semaphore_info, m_allocation_callbacks, &frame.m_render_complete_semaphore));

		VkFenceCreateInfo fence_info = {};
		fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		fence_info.pNext = nullptr;
		fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		EnsureVK(vkCreateFence(m_device, &fence_info, m_allocation_callbacks, &frame.m_fence));

		VkCommandPoolCreateInfo command_pool_info = {};
		command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		command_pool_info.pNext = nullptr;
		command_pool_info.flags = 0;
		command_pool_info.queueFamilyIndex = m_chosen_device.GraphicsQueueIndex;
		EnsureVK(vkCreateCommandPool(m_device, &command_pool_info, m_allocation_callbacks, &frame.m_command_pool)); // TODO: Per frame?

		frame.m_command_buffers.AddZeroed(10);

		VkCommandBufferAllocateInfo command_buffer_info = {};
		command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		command_buffer_info.pNext = nullptr;
		command_buffer_info.commandPool = frame.m_command_pool;
		command_buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		command_buffer_info.commandBufferCount = (u32)frame.m_command_buffers.Num();
		EnsureVK(vkAllocateCommandBuffers(m_device, &command_buffer_info, frame.m_command_buffers.Data()));
	}

	// TODO: Pipeline layout? Analogous to root signature?

	// Upload commands
	{
		VkFenceCreateInfo fence_info = {};
		fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_info.pNext = nullptr;
		fence_info.flags = 0;
		vkCreateFence(m_device, &fence_info, m_allocation_callbacks, &m_transfer_complete_fence);

		VkCommandPoolCreateInfo command_pool_info = {};
		command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		command_pool_info.pNext = nullptr;
		command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		command_pool_info.queueFamilyIndex = m_chosen_device.GraphicsQueueIndex;  // TODO: Transfer queue?
		EnsureVK(vkCreateCommandPool(m_device, &command_pool_info, m_allocation_callbacks, &m_transfer_command_pool)); // TODO: Per frame?

		VkCommandBufferAllocateInfo command_buffer_info = {};
		command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		command_buffer_info.pNext = nullptr;
		command_buffer_info.commandPool = m_transfer_command_pool;
		command_buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		command_buffer_info.commandBufferCount = 1;
		EnsureVK(vkAllocateCommandBuffers(m_device, &command_buffer_info, &m_transfer_command_buffer));
	}
}

void GPU_Vulkan::Shutdown()
{
	vkDeviceWaitIdle(m_device);

	m_debug_utils.VkDestroyDebugUtilsMessengerEXT(m_vkinst, m_debug_utils.m_messenger, m_allocation_callbacks);

	for (i32 i = 0; i < 1; ++i)	{
		auto& frame = *m_frame_data;
		frame.Destroy(m_device, m_allocation_callbacks);
	}

	for (auto[i, texture] : m_textures.Range()) {
		vkDestroyImageView(m_device, texture.m_image_view, m_allocation_callbacks);
		vkDestroyImage(m_device, texture.m_image, m_allocation_callbacks);
		vkFreeMemory(m_device, texture.m_device_memory, m_allocation_callbacks);
	}

	for (auto[i, vb] : m_vertex_buffers.Range()) {
		vkDestroyBuffer(m_device, vb.m_buffer, m_allocation_callbacks);
		vkFreeMemory(m_device, vb.m_device_memory, m_allocation_callbacks);
	}

	for (auto[i, ib] : m_index_buffers.Range()) {
		vkDestroyBuffer(m_device, ib.m_buffer, m_allocation_callbacks);
		vkFreeMemory(m_device, ib.m_device_memory, m_allocation_callbacks);
	}

	for (auto[i, pipeline] : m_pipeline_states.Range()) {
		vkDestroyPipeline(m_device, pipeline.m_pipeline, m_allocation_callbacks);
	}

	for (auto [i, shader] : m_registered_vertex_shaders.Range()) {
		vkDestroyShaderModule(m_device, shader, m_allocation_callbacks);
	}
	m_registered_vertex_shaders.Empty();
	for (auto[i, shader] : m_registered_pixel_shaders.Range()) {
		vkDestroyShaderModule(m_device, shader, m_allocation_callbacks);
	}
	m_registered_pixel_shaders.Empty();

	for (auto[i, framebuffer] : m_framebuffers.Range()) {
	//	vkDestroyFramebuffer(m_device, framebuffer, m_allocation_callbacks);
	}

	for (VkImageView view : m_swap_chain_image_views)
	{
		vkDestroyImageView(m_device, view, m_allocation_callbacks);
	}
	m_swap_chain_image_views.RemoveAll();

	vkFreeCommandBuffers(m_device, m_transfer_command_pool, 1, &m_transfer_command_buffer);
	vkDestroyCommandPool(m_device, m_transfer_command_pool, m_allocation_callbacks);
	vkDestroyFence(m_device, m_transfer_complete_fence, m_allocation_callbacks);

	vkDestroySampler(m_device, m_sampler, m_allocation_callbacks);

	vkDestroyDescriptorSetLayout(m_device, m_descriptor_set_layout_0, m_allocation_callbacks);
	vkDestroyDescriptorSetLayout(m_device, m_descriptor_set_layout_1, m_allocation_callbacks);
	vkDestroyDescriptorSetLayout(m_device, m_descriptor_set_layout_2, m_allocation_callbacks);

	vkDestroyPipelineLayout(m_device, m_pipeline_layout, m_allocation_callbacks);
	vkDestroyRenderPass(m_device, m_render_pass, m_allocation_callbacks);

	vkDestroySwapchainKHR(m_device, m_swap_chain, m_allocation_callbacks);
	vkDestroySurfaceKHR(m_vkinst, m_surface, m_allocation_callbacks);

	vkDestroyDevice(m_device, m_allocation_callbacks);
	vkDestroyInstance(m_vkinst, m_allocation_callbacks);
}

void GPU_Vulkan::CreateSwapChain(u32 width, u32 height)
{
	VkExtent2D swap_extent = { width, height };
	VkSurfaceFormatKHR format = { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
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
	swap_chain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
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

	Assert(num_images <= MaxSwapChainImages);

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
	image_view_create_info.subresourceRange.layerCount = 1;

	for (u32 i = 0; i < num_images; ++i)
	{
		image_view_create_info.image = m_swap_chain_images[i];
		EnsureVK(vkCreateImageView(m_device, &image_view_create_info, m_allocation_callbacks, &m_swap_chain_image_views[i]));
	}

	m_viewport.x = 0;
	m_viewport.y = (f32)height;
	m_viewport.width = (f32)width;
	m_viewport.height = -(f32)height;
	m_viewport.minDepth = 0.0f;
	m_viewport.maxDepth = 1.0f;
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
	// TODO: Wait for fence
	auto& frame = *m_frame_data; // TODO: Index

	vkWaitForFences(m_device, 1, &frame.m_fence, VK_TRUE, UINT64_MAX);
	Assert(vkGetFenceStatus(m_device, frame.m_fence) == VK_SUCCESS);
	frame.ResetFrame(m_device);

	vkAcquireNextImageKHR(m_device, m_swap_chain, u64_max, frame.m_backbuffer_semaphore, nullptr, &frame.m_swap_chain_image_index); // TODO: semaphore, fence?

	VkImage image = m_swap_chain_images[frame.m_swap_chain_image_index];

	VkCommandBuffer command_buffer = frame.GetCommandBuffer(m_device);
	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.pNext = nullptr;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begin_info.pInheritanceInfo = nullptr;
	vkBeginCommandBuffer(command_buffer, &begin_info);
	CmdDebugBegin(command_buffer, "BeginFrame");
	{
		VkImageMemoryBarrier image_barrier = {};
		image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		image_barrier.pNext = nullptr;
		image_barrier.srcAccessMask;
		image_barrier.dstAccessMask;
		image_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		image_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		image_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		image_barrier.image = image;
		image_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		image_barrier.subresourceRange.baseMipLevel = 0;
		image_barrier.subresourceRange.levelCount = 1;
		image_barrier.subresourceRange.baseArrayLayer = 0;
		image_barrier.subresourceRange.layerCount = 1;

		// TODO: Less restrictive barrier?
		vkCmdPipelineBarrier(command_buffer,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &image_barrier
		);

		VkClearColorValue clear_color;
		clear_color.float32[0] = scene_clear_color[0];
		clear_color.float32[1] = scene_clear_color[1];
		clear_color.float32[2] = scene_clear_color[2];
		clear_color.float32[3] = scene_clear_color[3];
		vkCmdClearColorImage(command_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1, &image_barrier.subresourceRange);

		// TODO: Less restrictive barrier?
		image_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		image_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		vkCmdPipelineBarrier(command_buffer,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &image_barrier
		);
	}
	CmdDebugEnd(command_buffer);
	vkEndCommandBuffer(command_buffer);

	VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	// Reset image available semaphore and make sure all other submissions also happen after the semaphore is signalled
	// TODO: This is only necessary when actually writing to the backbuffer - we could start doing other submissions earlier e.g. in deferred rendering
	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = nullptr;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = &frame.m_backbuffer_semaphore;
	submit_info.pWaitDstStageMask = &wait_stage;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;
	submit_info.signalSemaphoreCount = 0; // TODO: Signal render completion
	submit_info.pSignalSemaphores = nullptr;
	vkQueueSubmit(m_graphics_queue, 1, &submit_info, false);

	// TODO: Transition back buffer to some writeable state?
	// TODO: Clear back buffer

	return &frame; // TODO: Index
}

void GPU_Vulkan::EndFrame(GPUFrameInterface* generic_frame)
{
	auto& frame = *(GPU_Vulkan_Frame*)generic_frame;

	VkCommandBuffer command_buffer = frame.GetCommandBuffer(m_device);
	{
		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.pNext = nullptr;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		begin_info.pInheritanceInfo = nullptr;
		vkBeginCommandBuffer(command_buffer, &begin_info);
		CmdDebugBegin(command_buffer, "EndFrame");
		{
			VkImage image = m_swap_chain_images[frame.m_swap_chain_image_index];

			VkImageMemoryBarrier image_barrier = {};
			image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			image_barrier.pNext = nullptr;
			image_barrier.srcAccessMask;
			image_barrier.dstAccessMask;
			image_barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			image_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			image_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			image_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			image_barrier.image = image;
			image_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			image_barrier.subresourceRange.baseMipLevel = 0;
			image_barrier.subresourceRange.levelCount = 1;
			image_barrier.subresourceRange.baseArrayLayer = 0;
			image_barrier.subresourceRange.layerCount = 1;

			// TODO: Less restrictive barrier?
			vkCmdPipelineBarrier(command_buffer,
				VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &image_barrier
			);
		}
		CmdDebugEnd(command_buffer);
		vkEndCommandBuffer(command_buffer);
	}

	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = nullptr;
	submit_info.waitSemaphoreCount = 0;
	submit_info.pWaitSemaphores = nullptr; // TODO: Wait for image available
	submit_info.pWaitDstStageMask = nullptr;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;
	submit_info.signalSemaphoreCount = 1; // TODO: Signal render completion
	submit_info.pSignalSemaphores = &frame.m_render_complete_semaphore;
	vkQueueSubmit(m_graphics_queue, 1, &submit_info, false);

	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pNext = nullptr;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &frame.m_render_complete_semaphore; // TODO: wait for last render before presenting
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &m_swap_chain;
	present_info.pImageIndices = &frame.m_swap_chain_image_index;
	present_info.pResults = nullptr;

	vkQueuePresentKHR(m_present_queue, &present_info);

	vkResetFences(m_device, 1, &frame.m_fence);
	//VkResult fence_status = vkGetFenceStatus(m_device, frame.m_fence);
	//Assert(fence_status == VK_NOT_READY);
	vkQueueSubmit(m_graphics_queue, 0, nullptr, frame.m_fence);

	++m_frame_count;

	// TODO: Advance frame data
}

void GPU_Vulkan::SubmitPass(const RenderPass& pass)
{
	auto& frame = *m_frame_data; // TODO: Index

	VkCommandBuffer command_buffer = frame.GetCommandBuffer(m_device);

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.pNext = nullptr;
	begin_info.flags = 0;
	begin_info.pInheritanceInfo = nullptr;

	Framebuffer& framebuffer = m_framebuffers[pass.Framebuffer];

	DepthTarget* depth_target = nullptr;
	if (framebuffer.Desc.DepthBuffer != DepthTargetID{}) {
		depth_target = &m_depth_targets[framebuffer.Desc.DepthBuffer];
	}
	
	VkRenderPassBeginInfo pass_begin_info = {};
	pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	pass_begin_info.pNext = nullptr;
	pass_begin_info.renderPass = depth_target ? m_render_pass_depth : m_render_pass;
	// TODO: Bind RTs for pass
	// TODO: Bind depth target here?
	pass_begin_info.framebuffer = framebuffer.GetForFrame(frame.m_swap_chain_image_index); // m_swap_chain_framebuffers[frame.m_swap_chain_image_index];
	pass_begin_info.renderArea.offset = { 0, 0 };
	pass_begin_info.renderArea.extent = { m_swap_chain_dimensions[0], m_swap_chain_dimensions[1] };
	pass_begin_info.clearValueCount = 0;
	pass_begin_info.pClearValues = nullptr;

	vkBeginCommandBuffer(command_buffer, &begin_info);
	CmdDebugBegin(command_buffer, pass.Name ? pass.Name : "Unnamed pass");
	{
		// Pre render pass
		if (depth_target && pass.DepthClearValue) {
			// Clear depth buffer
			// TODO: Promote this to render pass?

			VkImageSubresourceRange range = {};
			range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			range.baseMipLevel = 0;
			range.levelCount = 1;
			range.baseArrayLayer = 0;
			range.layerCount = 1;
			VkImageMemoryBarrier image_barrier = {};
			image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			image_barrier.pNext = nullptr;
			image_barrier.srcAccessMask = 0;
			image_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			image_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			image_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			image_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			image_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			image_barrier.image = depth_target->m_image;
			image_barrier.subresourceRange = range;

			vkCmdPipelineBarrier(command_buffer,
				VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &image_barrier);

			VkClearDepthStencilValue clear_value = {};
			clear_value.depth = *pass.DepthClearValue;
			clear_value.stencil = 0;
			vkCmdClearDepthStencilImage(command_buffer, depth_target->m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_value, 1, &range);

			image_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			image_barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			image_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			image_barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

			vkCmdPipelineBarrier(command_buffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &image_barrier);
		}
	}
	vkCmdBeginRenderPass(command_buffer, &pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
	{

		// TODO: Set viewports
		vkCmdSetViewport(command_buffer, 0, 1, &m_viewport);

		VkRect2D scissor_rect;
		scissor_rect.offset = { (i32)pass.ClipRect.Left, (i32)pass.ClipRect.Top };
		scissor_rect.extent = { pass.ClipRect.Right - pass.ClipRect.Left, pass.ClipRect.Bottom - pass.ClipRect.Top };

		VkRect2D no_scissor;
		no_scissor.offset = { 0, 0 };
		no_scissor.extent = { (u32)m_viewport.width, (u32)m_viewport.height };

		for (const GPU::DrawItem& item : pass.DrawItems) {
			Assert(item.PipelineState != PipelineStateID{});
			
			if( item.Name) {
				CmdDebugBegin(command_buffer, item.Name);
			}

			PipelineState pipeline = m_pipeline_states[item.PipelineState];
			vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.m_pipeline);

			if (pipeline.m_desc.RasterState.ScissorEnable) {
				vkCmdSetScissor(command_buffer, 0, 1, &scissor_rect);
			}
			else {
				vkCmdSetScissor(command_buffer, 0, 1, &no_scissor);
			}

			// TODO: Set pipeline state
			// TODO: Set primitive topology? 
			// TODO: Set scissor based on raster state?
			// TODO: Bind uniforms
			// TODO: Bind SRVs

			{
				// Max 2 uniform buffers + 1 image view 
				FixedArray<VkWriteDescriptorSet, 3> writes;

				Assert(item.BoundResources.ConstantBuffers.Num() <= 2);

				VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {};
				descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
				descriptor_set_allocate_info.pNext = nullptr;
				descriptor_set_allocate_info.descriptorPool = frame.m_descriptor_pool;
				descriptor_set_allocate_info.descriptorSetCount = 1;
				descriptor_set_allocate_info.pSetLayouts = &m_descriptor_set_layout_0;
				VkDescriptorSet descriptor_set_0;
				EnsureVK(vkAllocateDescriptorSets(m_device, &descriptor_set_allocate_info, &descriptor_set_0));

				descriptor_set_allocate_info.descriptorSetCount = 1;
				descriptor_set_allocate_info.pSetLayouts = &m_descriptor_set_layout_1;
				VkDescriptorSet descriptor_set_1;
				EnsureVK(vkAllocateDescriptorSets(m_device, &descriptor_set_allocate_info, &descriptor_set_1));

				descriptor_set_allocate_info.descriptorSetCount = 1;
				descriptor_set_allocate_info.pSetLayouts = &m_descriptor_set_layout_2;
				VkDescriptorSet descriptor_set_2;
				EnsureVK(vkAllocateDescriptorSets(m_device, &descriptor_set_allocate_info, &descriptor_set_2));

				FixedArray<VkDescriptorBufferInfo, 2> uniform_buffers;

				u32 start_binding = 0;
				do 
				{
					// Skip unbound uniforms
					while (start_binding < item.BoundResources.ConstantBuffers.Num() && item.BoundResources.ConstantBuffers[start_binding] == ConstantBufferID{}) {
						++start_binding;
					}

					if (start_binding >= item.BoundResources.ConstantBuffers.Num()) {
						break;
					}

					u32 end_binding = start_binding + 1;
					while (end_binding < item.BoundResources.ConstantBuffers.Num() && item.BoundResources.ConstantBuffers[end_binding] != ConstantBufferID{}) {
						++end_binding;
					}

					VkWriteDescriptorSet& write = writes.AddDefaulted(1)[0];
					write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					write.pNext = nullptr;
					write.dstSet = descriptor_set_0;
					write.dstBinding = start_binding;
					write.dstArrayElement = 0;
					write.descriptorCount = end_binding - start_binding;
					write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
					write.pImageInfo = nullptr;
					write.pBufferInfo = uniform_buffers.Data() + uniform_buffers.Num(); // Address is stable because it's a fixed-size array
					write.pTexelBufferView = nullptr;

					for (u32 i = start_binding; i < end_binding; ++i) {
						ConstantBufferID cb_id = item.BoundResources.ConstantBuffers[i];
						VkDescriptorBufferInfo buffer_info = GetConstantBufferForBinding(cb_id, frame);
						uniform_buffers.Add(buffer_info);
					}

					start_binding = end_binding;
				} while (true);

				FixedArray<TextureID, GPU::MaxBoundShaderResources> bound_resources;
				bound_resources.AddMany(GPU::MaxBoundShaderResources, TextureID{ u32_max });
				for (ShaderResourceListID id : item.BoundResources.ResourceLists) {
					const ShaderResourceList& list = m_shader_resource_lists[id];
					for (u32 i = list.Desc.StartSlot; i < list.Desc.Textures.Num() && (i + list.Desc.StartSlot < GPU::MaxBoundShaderResources); ++i) {
						bound_resources[i] = list.Desc.Textures[i - list.Desc.StartSlot];
					}
				}

				VkDescriptorImageInfo image_info = {};
				if (bound_resources[0] != TextureID{ }) {
					image_info.imageView = m_textures[bound_resources[0]].m_image_view;
					image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

					VkWriteDescriptorSet& write = writes.AddDefaulted(1)[0];
					write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					write.pNext = nullptr;
					write.dstSet = descriptor_set_1;
					write.dstBinding = 0;
					write.dstArrayElement = 0;
					write.descriptorCount = 1;
					write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
					write.pImageInfo = &image_info;
					write.pBufferInfo = nullptr;
					write.pTexelBufferView = nullptr;
				}

				vkUpdateDescriptorSets(m_device, (u32)writes.Num(), writes.Data(), 0, nullptr);

				// TODO: m_pipeline_layout? when do we need to change layout
				// TODO: Dynamic offset?
				VkDescriptorSet sets[] = {
					descriptor_set_0, 
					descriptor_set_1,
					descriptor_set_2,
				};
				vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout, 0, (u32)ArraySize(sets), sets, 0, nullptr);
			}

			const GPU::StreamSetup& stream_setup = item.StreamSetup;

			// TODO: Set vertex buffers
			FixedArray<VkBuffer, GPU::MaxStreamSlots> vbs;
			FixedArray<VkDeviceSize, GPU::MaxStreamSlots> vb_offsets;

			//for (auto[vb_id, stride] : Zip(item.StreamSetup.VertexBuffers, pipeline_state.VBStrides)) {
			for( VertexBufferID vb_id : item.StreamSetup.VertexBuffers ) {
				//vbs.Add(GetVertexBufferView(frame, vb_id, stride));
 				Assert(vb_id != VertexBufferID{});

				auto[buffer, offset] = GetVertexBufferForBinding(vb_id, frame);

				//VertexBuffer& vertex_buffer = m_vertex_buffers[vb_id];
				vbs.Add(buffer);
				vb_offsets.Add(offset);
			}

			if (vbs.Num() > 0) {
				vkCmdBindVertexBuffers(command_buffer, 0, (u32)vbs.Num(), vbs.Data(), vb_offsets.Data());
			}

			if (stream_setup.IndexBuffer != IndexBufferID{}) {
				auto[buffer, offset] = GetIndexBufferForBinding(stream_setup.IndexBuffer, frame);

				vkCmdBindIndexBuffer(command_buffer, buffer, offset, VK_INDEX_TYPE_UINT16);
				vkCmdDrawIndexed(command_buffer, item.Command.VertexOrIndexCount, 1, item.Command.IndexOffset, 0, 0);
			}
			else {
				vkCmdDraw(command_buffer, item.Command.VertexOrIndexCount, 1, 0, 0);
			}

			if (item.Name) {
				CmdDebugEnd(command_buffer);
			}
		}
	}
	vkCmdEndRenderPass(command_buffer);
	CmdDebugEnd(command_buffer);
	vkEndCommandBuffer(command_buffer);

	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = nullptr;
	submit_info.waitSemaphoreCount = 0;
	submit_info.pWaitSemaphores = nullptr; // TODO: Wait for image available
	submit_info.pWaitDstStageMask= nullptr;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;
	submit_info.signalSemaphoreCount = 0; // TODO: Signal render completion
	submit_info.pSignalSemaphores = nullptr;
	vkQueueSubmit(m_graphics_queue, 1, &submit_info, false);
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

static String GetShaderFilename(mu::PointerRange<const char> name, GPU::ShaderType type)
{
	PointerRange<const char> suffix;
	switch (type)
	{
	case GPU::ShaderType::Pixel: suffix = ".frag"; break;
	case GPU::ShaderType::Vertex: suffix = ".vert"; break;
	}
	return String::FromRanges(GetShaderDirectory(), name, suffix);
}

GPU::VertexShaderID GPU_Vulkan::CompileVertexShaderHLSL(PointerRange<const char> name)
{
	String shader_filename = GetShaderFilename(name, GPU::ShaderType::Vertex);
	String shader_txt_code = LoadFileToString(shader_filename.GetRaw());
	PointerRange<const u8> code = shader_txt_code.Bytes();
	Assert(code.Size() > 0);

	shaderc_compilation_result_t result = shaderc_compile_into_spv(
		m_shader_compiler, 
		(const char*)code.m_start, 
		code.Size() - 1, 
		shaderc_vertex_shader, 
		"", 
		"main", 
		nullptr
	);
	Assert(result);
	shaderc_compilation_status status = shaderc_result_get_compilation_status(result);
	if (status != shaderc_compilation_status_success)
	{

		if (size_t num_errors = shaderc_result_get_num_errors(result); num_errors != 0)
		{
			const char* err = shaderc_result_get_error_message(result);
			Assertf(false, "{} errors:\n {}", num_errors, err);
		}
		return VertexShaderID{};
	}

	if (size_t num_warnings = shaderc_result_get_num_warnings(result); num_warnings != 0)
	{
		const char* warnings = shaderc_result_get_error_message(result); // ?
		dbg::Log("{} warnings:\n {}", num_warnings, warnings);
	}

	size_t result_size = shaderc_result_get_length(result);
	const char* result_bytes = shaderc_result_get_bytes(result);

	VertexShaderID id = m_registered_vertex_shaders.AddDefaulted();

	VkShaderModuleCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.pNext = nullptr;
	create_info.flags = 0;
	create_info.codeSize = result_size;
	create_info.pCode = (const u32*)result_bytes;

	VkShaderModule& shader_module = m_registered_vertex_shaders[id];
	EnsureVK(vkCreateShaderModule(m_device, &create_info, m_allocation_callbacks, &shader_module));
	
	return id;
}

PixelShaderID GPU_Vulkan::CompilePixelShaderHLSL(PointerRange<const char> name)
{
	String shader_filename = GetShaderFilename(name, GPU::ShaderType::Pixel);
	String shader_txt_code = LoadFileToString(shader_filename.GetRaw());
	PointerRange<const u8> code = shader_txt_code.Bytes();
	Assert(code.Size() > 0);

	shaderc_compilation_result_t result = shaderc_compile_into_spv(
		m_shader_compiler,
		(const char*)code.m_start,
		code.Size() - 1,
		shaderc_fragment_shader,
		"",
		"main",
		nullptr
	);
	Assert(result);
	shaderc_compilation_status status = shaderc_result_get_compilation_status(result);
	if (status != shaderc_compilation_status_success)
	{

		if (size_t num_errors = shaderc_result_get_num_errors(result); num_errors != 0)
		{
			const char* err = shaderc_result_get_error_message(result);
			Assertf(false, "{} errors:\n {}", num_errors, err);
		}
		return {};
	}

	if (size_t num_warnings = shaderc_result_get_num_warnings(result); num_warnings != 0)
	{
		const char* warnings = shaderc_result_get_error_message(result); // ?
		dbg::Log("{} warnings:\n {}", num_warnings, warnings);
	}

	size_t result_size = shaderc_result_get_length(result);
	const char* result_bytes = shaderc_result_get_bytes(result);

	PixelShaderID id = m_registered_pixel_shaders.AddDefaulted();

	VkShaderModuleCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.pNext = nullptr;
	create_info.flags = 0;
	create_info.codeSize = result_size;
	create_info.pCode = (const u32*)result_bytes;

	VkShaderModule& shader_module = m_registered_pixel_shaders[id];
	EnsureVK(vkCreateShaderModule(m_device, &create_info, m_allocation_callbacks, &shader_module));

	return id;
}

ProgramID GPU_Vulkan::LinkProgram(VertexShaderID vertex_shader, PixelShaderID pixel_shader)
{
	ProgramID id = m_linked_programs.AddDefaulted();
	LinkedProgram& program = m_linked_programs[id];
	program.VertexShader = vertex_shader;
	program.PixelShader = pixel_shader;

	program.ShaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	program.ShaderStages[0].pNext = nullptr;
	program.ShaderStages[0].flags = 0;
	program.ShaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	program.ShaderStages[0].module = m_registered_vertex_shaders[vertex_shader];
	program.ShaderStages[0].pName = "main";
	program.ShaderStages[0].pSpecializationInfo = nullptr;

	program.ShaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	program.ShaderStages[1].pNext = nullptr;
	program.ShaderStages[1].flags = 0;
	program.ShaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	program.ShaderStages[1].module = m_registered_pixel_shaders[pixel_shader];
	program.ShaderStages[1].pName = "main";
	program.ShaderStages[1].pSpecializationInfo = nullptr;

	return id;
}

PipelineStateID GPU_Vulkan::CreatePipelineState(const GPU::PipelineStateDesc& desc)
{
	const LinkedProgram& program = m_linked_programs[desc.Program];

	VkDynamicState dynamic_states[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,

	};
	VkPipelineDynamicStateCreateInfo dynamic_state = {};
	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.pNext = nullptr;
	dynamic_state.flags = 0;
	dynamic_state.dynamicStateCount = (u32)ArraySize(dynamic_states);
	dynamic_state.pDynamicStates = dynamic_states;

	VulkanVertexLayout vertex_layout{ desc.StreamFormat };

	VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
	vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_info.pNext = nullptr;
	vertex_input_info.flags = 0;
	vertex_input_info.vertexBindingDescriptionCount = (u32)vertex_layout.m_bindings.Num();
	vertex_input_info.pVertexBindingDescriptions = vertex_layout.m_bindings.Data();
	vertex_input_info.vertexAttributeDescriptionCount = (u32)vertex_layout.m_attributes.Num();
	vertex_input_info.pVertexAttributeDescriptions = vertex_layout.m_attributes.Data();

	VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {};
	input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_info.pNext = nullptr;
	input_assembly_info.flags = 0;
	input_assembly_info.topology = CommonToVK(desc.PrimitiveTopology);
	input_assembly_info.primitiveRestartEnable = VK_FALSE;
	
	VkRect2D scissor;
	VkPipelineViewportStateCreateInfo viewport_info = {};
	viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_info.pNext = nullptr; // &swizzle;
	viewport_info.flags = 0;
	viewport_info.viewportCount = 1;
	viewport_info.pViewports = &m_viewport;
	viewport_info.scissorCount = 1;
	viewport_info.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo raster_info = {};
	raster_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	raster_info.pNext = nullptr;
	raster_info.flags = 0;
	raster_info.depthClampEnable;
	raster_info.rasterizerDiscardEnable;
	raster_info.polygonMode = CommonToVK(desc.RasterState.FillMode);
	raster_info.cullMode = CommonToVK(desc.RasterState.CullMode);
	raster_info.frontFace = CommonToVK(desc.RasterState.FrontFace);
	raster_info.depthBiasEnable = desc.RasterState.DepthBias != 0;
	raster_info.depthBiasConstantFactor = (f32)desc.RasterState.DepthBias;
	raster_info.depthBiasClamp = desc.RasterState.DepthBiasClamp;
	raster_info.depthBiasSlopeFactor = desc.RasterState.SlopeScaledDepthBias;
	raster_info.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisample_info = {};
	multisample_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_info.pNext = nullptr;
	multisample_info.flags = 0;
	multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisample_info.sampleShadingEnable = VK_FALSE;
	multisample_info.minSampleShading = 1.0f;
	multisample_info.pSampleMask = nullptr;
	multisample_info.alphaToCoverageEnable = VK_FALSE;
	multisample_info.alphaToOneEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depth_stencil_info = {};
	depth_stencil_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_info.pNext = nullptr;
	depth_stencil_info.flags = 0;
	depth_stencil_info.depthTestEnable = desc.DepthStencilState.DepthEnable;
	depth_stencil_info.depthWriteEnable = desc.DepthStencilState.DepthWriteEnable;
	depth_stencil_info.depthCompareOp = CommonToVK(desc.DepthStencilState.DepthComparisonFunc);
	depth_stencil_info.depthBoundsTestEnable;
	depth_stencil_info.stencilTestEnable = desc.DepthStencilState.StencilEnable;
	depth_stencil_info.front;
	depth_stencil_info.back;
	depth_stencil_info.minDepthBounds;
	depth_stencil_info.maxDepthBounds;

	VkPipelineColorBlendAttachmentState blend_attachment;
	blend_attachment.blendEnable = desc.BlendState.BlendEnable;
	blend_attachment.srcColorBlendFactor = CommonToVK(desc.BlendState.ColorBlend.Source); // VK_BLEND_FACTOR_ZERO;
	blend_attachment.dstColorBlendFactor = CommonToVK(desc.BlendState.ColorBlend.Dest); // VK_BLEND_FACTOR_ZERO;
	blend_attachment.colorBlendOp = CommonToVK(desc.BlendState.ColorBlend.Op);; // VK_BLEND_OP_ADD;
	blend_attachment.srcAlphaBlendFactor = CommonToVK(desc.BlendState.AlphaBlend.Source); // VK_BLEND_FACTOR_ZERO;
	blend_attachment.dstAlphaBlendFactor = CommonToVK(desc.BlendState.AlphaBlend.Dest); // VK_BLEND_FACTOR_ZERO;
	blend_attachment.alphaBlendOp = CommonToVK(desc.BlendState.AlphaBlend.Op); // VK_BLEND_OP_ADD;
	blend_attachment.colorWriteMask = 0xF;

	VkPipelineColorBlendStateCreateInfo blend_info = {};
	blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_info.pNext;
	blend_info.flags;
	blend_info.logicOpEnable;
	blend_info.logicOp;
	blend_info.attachmentCount = 1; // TODO: match render target count
	blend_info.pAttachments = &blend_attachment;
	//blend_info.blendConstants[4];

	VkGraphicsPipelineCreateInfo pipeline_state_info = {};
	pipeline_state_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_state_info.pNext = nullptr;
	pipeline_state_info.flags = 0;
	pipeline_state_info.stageCount = 2;
	pipeline_state_info.pStages = program.ShaderStages;
	pipeline_state_info.pVertexInputState = &vertex_input_info;
	pipeline_state_info.pInputAssemblyState = &input_assembly_info;
	pipeline_state_info.pTessellationState = nullptr;
	pipeline_state_info.pViewportState = &viewport_info;
	pipeline_state_info.pRasterizationState = &raster_info;
	pipeline_state_info.pMultisampleState = &multisample_info;
	pipeline_state_info.pDepthStencilState = &depth_stencil_info;
	pipeline_state_info.pColorBlendState = &blend_info;
	pipeline_state_info.pDynamicState = &dynamic_state;
	pipeline_state_info.layout = m_pipeline_layout;
	pipeline_state_info.renderPass = desc.DepthStencilState.DepthEnable ? m_render_pass_depth : m_render_pass;
	pipeline_state_info.subpass = 0;
	pipeline_state_info.basePipelineHandle = nullptr;
	pipeline_state_info.basePipelineIndex = -1;

	PipelineStateID id = m_pipeline_states.AddDefaulted();
	PipelineState& pipeline = m_pipeline_states[id];
	pipeline.m_desc = desc;
	EnsureVK(vkCreateGraphicsPipelines(m_device, nullptr, 1, &pipeline_state_info, m_allocation_callbacks, &pipeline.m_pipeline));
	return id;
}

void GPU_Vulkan::DestroyPipelineState(GPU::PipelineStateID id)
{
	Assert(false);
}

ConstantBufferID GPU_Vulkan::CreateConstantBuffer(mu::PointerRange<const u8> data)
{
	Assert(false);
	return ConstantBufferID{};
}

void GPU_Vulkan::DestroyConstantBuffer(ConstantBufferID id)
{
	Assert(false);
}

VertexBufferID GPU_Vulkan::CreateVertexBuffer(mu::PointerRange<const u8> data)
{
	VertexBufferID id = m_vertex_buffers.AddDefaulted();
	VertexBuffer& buffer = m_vertex_buffers[id];

	u32 queue_index = m_chosen_device.GraphicsQueueIndex;

	VkBufferCreateInfo buffer_info = {};
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.pNext = nullptr;
	buffer_info.flags = 0;
	buffer_info.size = data.Size();
	buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buffer_info.queueFamilyIndexCount = 1;
	buffer_info.pQueueFamilyIndices = & queue_index;
	EnsureVK(vkCreateBuffer(m_device, &buffer_info, m_allocation_callbacks, &buffer.m_buffer));

	// allocate and bind memory
	VkMemoryRequirements memory_requirements = {};
	vkGetBufferMemoryRequirements(m_device, buffer.m_buffer, &memory_requirements);

	VkMemoryAllocateInfo allocate_info = {};
	allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocate_info.pNext = nullptr;
	allocate_info.allocationSize = memory_requirements.size;
	allocate_info.memoryTypeIndex = m_chosen_device.m_upload_memory_type_index;
	EnsureVK(vkAllocateMemory(m_device, &allocate_info, m_allocation_callbacks, &buffer.m_device_memory));

	void* mapped_data = nullptr;
	EnsureVK(vkMapMemory(m_device, buffer.m_device_memory, 0, data.Size(), 0, &mapped_data));
	memcpy(mapped_data, data.m_start, data.Size());
	vkUnmapMemory(m_device, buffer.m_device_memory);

	EnsureVK(vkBindBufferMemory(m_device, buffer.m_buffer, buffer.m_device_memory, 0));

	return id;
}

void GPU_Vulkan::DestroyVertexBuffer(GPU::VertexBufferID id) 
{
	Assert(false);
}

IndexBufferID GPU_Vulkan::CreateIndexBuffer(mu::PointerRange<const u8> data)
{
	IndexBufferID id = m_index_buffers.AddDefaulted();
	IndexBuffer& buffer = m_index_buffers[id];
	
	u32 queue_index = m_chosen_device.GraphicsQueueIndex;

	VkBufferCreateInfo buffer_info = {};
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.pNext = nullptr;
	buffer_info.flags = 0;
	buffer_info.size = data.Size();
	buffer_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buffer_info.queueFamilyIndexCount = 1;
	buffer_info.pQueueFamilyIndices = &queue_index;
	EnsureVK(vkCreateBuffer(m_device, &buffer_info, m_allocation_callbacks, &buffer.m_buffer));

	// allocate and bind memory
	VkMemoryRequirements memory_requirements = {};
	vkGetBufferMemoryRequirements(m_device, buffer.m_buffer, &memory_requirements);

	VkMemoryAllocateInfo allocate_info = {};
	allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocate_info.pNext = nullptr;
	allocate_info.allocationSize = memory_requirements.size;
	allocate_info.memoryTypeIndex = m_chosen_device.m_upload_memory_type_index;
	EnsureVK(vkAllocateMemory(m_device, &allocate_info, m_allocation_callbacks, &buffer.m_device_memory));

	void* mapped_data = nullptr;
	EnsureVK(vkMapMemory(m_device, buffer.m_device_memory, 0, data.Size(), 0, &mapped_data));
	memcpy(mapped_data, data.m_start, data.Size());
	vkUnmapMemory(m_device, buffer.m_device_memory);

	EnsureVK(vkBindBufferMemory(m_device, buffer.m_buffer, buffer.m_device_memory, 0));

	return id;
}

void GPU_Vulkan::DestroyIndexBuffer(GPU::IndexBufferID id)
{
	Assert(false);
}

TextureID GPU_Vulkan::CreateTexture2D(u32 width, u32 height, GPU::TextureFormat format_common, mu::PointerRange<const u8> data)
{
	TextureID id = m_textures.AddDefaulted();
	Texture2D& texture = m_textures[id];

	VkFormat format = CommonToVK(format_common);
	//VkFormatProperties format_props;
	//vkGetPhysicalDeviceFormatProperties(m_chosen_device.PhysicalDevice, format, &format_props);

	VkImageCreateInfo image_info = {};
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.pNext = nullptr;
	image_info.flags = 0;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.format = format;
	image_info.extent = { width, height, 1 }; // depth 1? 0?
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_info.queueFamilyIndexCount = 1;
	image_info.pQueueFamilyIndices = &m_chosen_device.GraphicsQueueIndex;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	EnsureVK(vkCreateImage(m_device, &image_info, m_allocation_callbacks, &texture.m_image));

	VkMemoryRequirements image_mem_req = {};
	vkGetImageMemoryRequirements(m_device, texture.m_image, &image_mem_req);

	VkMemoryAllocateInfo image_alloc_info = {};
	image_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	image_alloc_info.pNext = nullptr;
	image_alloc_info.allocationSize = image_mem_req.size;
	image_alloc_info.memoryTypeIndex = m_chosen_device.m_device_memory_type_index;
	EnsureVK(vkAllocateMemory(m_device, &image_alloc_info, m_allocation_callbacks, &texture.m_device_memory));

	EnsureVK(vkBindImageMemory(m_device, texture.m_image, texture.m_device_memory, 0));

	VkImageViewCreateInfo image_view_info = {};
	image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	image_view_info.pNext = nullptr;
	image_view_info.flags = 0;
	image_view_info.image = texture.m_image;
	image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	image_view_info.format = format;
	image_view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_view_info.subresourceRange.baseMipLevel = 0;
	image_view_info.subresourceRange.levelCount = 1;
	image_view_info.subresourceRange.baseArrayLayer = 0;
	image_view_info.subresourceRange.layerCount = 1;
	EnsureVK(vkCreateImageView(m_device, &image_view_info, m_allocation_callbacks, &texture.m_image_view));

	// Allocate host-accessible memory for the image
	VkBuffer tmp_buffer = nullptr;
	VkDeviceMemory tmp_memory = nullptr;
	{
		VkBufferCreateInfo buffer_info = {};
		buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_info.pNext = nullptr;
		buffer_info.flags;
		buffer_info.size = data.Size();
		buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		buffer_info.queueFamilyIndexCount;
		buffer_info.pQueueFamilyIndices;
		EnsureVK(vkCreateBuffer(m_device, &buffer_info, m_allocation_callbacks, &tmp_buffer));

		VkMemoryRequirements mem_req = {};
		vkGetBufferMemoryRequirements(m_device, tmp_buffer, &mem_req);

		VkMemoryAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.pNext = nullptr;
		alloc_info.allocationSize = mem_req.size;
		alloc_info.memoryTypeIndex = m_chosen_device.m_upload_memory_type_index; // TODO: mem_req.memoryTypeBits?
		EnsureVK(vkAllocateMemory(m_device, &alloc_info, m_allocation_callbacks, &tmp_memory));

		EnsureVK(vkBindBufferMemory(m_device, tmp_buffer, tmp_memory, 0));
	}

	{
		void* mapped = nullptr;
		EnsureVK(vkMapMemory(m_device, tmp_memory, 0, VK_WHOLE_SIZE, 0, &mapped));
		memcpy(mapped, data.m_start, data.Size());
		vkUnmapMemory(m_device, tmp_memory); // TODO: Does unmap memory guarantee flushing writes to device? Is any synchronization needed at all to flush? VkQueueSubmit takes care of it?
	}

	EnsureVK(vkResetCommandBuffer(m_transfer_command_buffer, 0));

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.pNext = nullptr;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begin_info.pInheritanceInfo = nullptr;
	EnsureVK(vkBeginCommandBuffer(m_transfer_command_buffer, &begin_info));
	{
		// Transition image to transfer dest
		VkImageMemoryBarrier barrier_to_transfer_dest = {};
		barrier_to_transfer_dest.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier_to_transfer_dest.pNext = nullptr;
		barrier_to_transfer_dest.srcAccessMask = 0;
		barrier_to_transfer_dest.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier_to_transfer_dest.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier_to_transfer_dest.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier_to_transfer_dest.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier_to_transfer_dest.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier_to_transfer_dest.image = texture.m_image;
		barrier_to_transfer_dest.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier_to_transfer_dest.subresourceRange.baseMipLevel = 0;
		barrier_to_transfer_dest.subresourceRange.levelCount = 1;
		barrier_to_transfer_dest.subresourceRange.baseArrayLayer = 0;
		barrier_to_transfer_dest.subresourceRange.layerCount = 1;
		vkCmdPipelineBarrier(m_transfer_command_buffer,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier_to_transfer_dest);

		// Copy from host-accessible memory to device-only memory
		VkBufferImageCopy copy_region = {};
		copy_region.bufferOffset = 0;
		copy_region.bufferRowLength = width;
		copy_region.bufferImageHeight = height;
		copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copy_region.imageSubresource.mipLevel = 0;
		copy_region.imageSubresource.baseArrayLayer = 0;
		copy_region.imageSubresource.layerCount = 1;
		copy_region.imageOffset = { 0, 0, 0 };
		copy_region.imageExtent = { width, height, 1 };
		vkCmdCopyBufferToImage(m_transfer_command_buffer, tmp_buffer, texture.m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

		// Transition image to image read
		VkImageMemoryBarrier barrier_to_shader_read = barrier_to_transfer_dest;
		barrier_to_shader_read.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier_to_shader_read.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier_to_shader_read.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier_to_shader_read.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vkCmdPipelineBarrier(m_transfer_command_buffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier_to_shader_read);
	}
	EnsureVK(vkEndCommandBuffer(m_transfer_command_buffer));

	EnsureVK(vkResetFences(m_device, 1, &m_transfer_complete_fence));

	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = nullptr;
	submit_info.waitSemaphoreCount;
	submit_info.pWaitSemaphores;
	submit_info.pWaitDstStageMask;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &m_transfer_command_buffer;
	submit_info.signalSemaphoreCount;
	submit_info.pSignalSemaphores;
	EnsureVK(vkQueueSubmit(m_graphics_queue, 1, &submit_info, m_transfer_complete_fence));

	// Wait for command completion
	for(VkResult res = VK_TIMEOUT; res == VK_TIMEOUT; )
	{
		res = vkWaitForFences(m_device, 1, &m_transfer_complete_fence, VK_TRUE, u64_max);
		Assert(res == VK_TIMEOUT || res == VK_SUCCESS);
	}

	// Release temp resources
	vkDestroyBuffer(m_device, tmp_buffer, m_allocation_callbacks);
	vkFreeMemory(m_device, tmp_memory, m_allocation_callbacks);

	return id;
}

DepthTargetID GPU_Vulkan::CreateDepthTarget(u32 width, u32 height)
{
	DepthTargetID id = m_depth_targets.AddDefaulted();
	DepthTarget& target_info = m_depth_targets[id];

	VkImageCreateInfo image_info = {};
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.pNext = nullptr;
	image_info.flags = 0;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.format = VK_FORMAT_D32_SFLOAT;
	image_info.extent = { width, height, 1 };
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_info.queueFamilyIndexCount = 0;
	image_info.pQueueFamilyIndices = nullptr;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	EnsureVK(vkCreateImage(m_device, &image_info, m_allocation_callbacks, &target_info.m_image));

	VkMemoryRequirements mem_reqs = {};
	vkGetImageMemoryRequirements(m_device, target_info.m_image, &mem_reqs);

	VkMemoryAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = nullptr;
	alloc_info.allocationSize = mem_reqs.size;
	alloc_info.memoryTypeIndex = m_chosen_device.m_device_memory_type_index;
	EnsureVK(vkAllocateMemory(m_device, &alloc_info, m_allocation_callbacks, &target_info.m_device_memory));
	EnsureVK(vkBindImageMemory(m_device, target_info.m_image, target_info.m_device_memory, 0));

	VkImageViewCreateInfo view_info = {};
	view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_info.pNext = nullptr;
	view_info.flags = 0;
	view_info.image = target_info.m_image;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_info.format = image_info.format;
	view_info.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
	view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	view_info.subresourceRange.baseMipLevel = 0;
	view_info.subresourceRange.levelCount = 1;
	view_info.subresourceRange.baseArrayLayer = 0;
	view_info.subresourceRange.layerCount = 1;
	EnsureVK(vkCreateImageView(m_device, &view_info, m_allocation_callbacks, &target_info.m_image_view));

	return id;
}

ShaderResourceListID GPU_Vulkan::CreateShaderResourceList(const GPU::ShaderResourceListDesc& desc)
{
	ShaderResourceListID id = m_shader_resource_lists.AddDefaulted();
	ShaderResourceList& srl = m_shader_resource_lists[id];
	srl.Desc = desc;

	return id;
}

FramebufferID GPU_Vulkan::CreateFramebuffer(const GPU::FramebufferDesc& desc) {
	FramebufferID id = m_framebuffers.AddDefaulted();
	Framebuffer& fb = m_framebuffers[id];
	fb.Desc = desc;

	Assert(desc.RenderTargets.Num() <= 1); // TODO

	bool uses_backbuffer = Contains(desc.RenderTargets, [](RenderTargetID id) { return id == GPU::BackBufferID; });

	if (uses_backbuffer) {
		FixedArray<VkImageView, GPU::MaxRenderTargets + 1> attachments;

		for (RenderTargetID rt_id : desc.RenderTargets) {
			if (rt_id == GPU::BackBufferID) {
				attachments.Add(nullptr);
			}
			else {
				Assert(false);
			}
		}

		if (desc.DepthBuffer != DepthTargetID{}) {
			VkImageView depth_view = m_depth_targets[desc.DepthBuffer].m_image_view;
			attachments.Add(depth_view);
		}

		// We need one framebuffer per swap chain image
		fb.m_frame_framebuffers.AddDefaulted(m_swap_chain_image_views.Num());
		for ( auto[view, framebuffer] : Zip(m_swap_chain_image_views.Range(), fb.m_frame_framebuffers.Range()) ) {
			for (i32 i = 0; i < desc.RenderTargets.Num(); ++i) {
				RenderTargetID rt_id = desc.RenderTargets[i];
				if (rt_id == GPU::BackBufferID) {
					attachments[i] = view;
				}
			}

			VkFramebufferCreateInfo fb_info = {};
			fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			fb_info.pNext = nullptr;
			fb_info.flags = 0;
			fb_info.renderPass = desc.DepthBuffer == DepthTargetID{} ? m_render_pass : m_render_pass_depth; // TODO: Render pass prototypes
			fb_info.attachmentCount = (u32)attachments.Num();
			fb_info.pAttachments = attachments.Data();
			fb_info.width = m_swap_chain_dimensions[0]; // TODO: generic
			fb_info.height = m_swap_chain_dimensions[1];
			fb_info.layers = 1;
			EnsureVK(vkCreateFramebuffer(m_device, &fb_info, m_allocation_callbacks, &framebuffer));
		}		
	}
	else {
		// We only need one framebuffer
		Assert(false); // Not yet implemented
		// vkCreateFramebuffer(m_device, &fb_info, m_allocation_callbacks, &framebuffer);
	}

	return id;
}

void GPU_Vulkan::DestroyFramebuffer(FramebufferID) {
	Assert(false);
}

tuple<VkBuffer, VkDeviceAddress> GPU_Vulkan::GetVertexBufferForBinding(VertexBufferID id, GPU_Vulkan_Frame& frame)
{
	size_t idx = (size_t)id;
	if (idx < MaxPersistentVertexBuffers) {
		VertexBuffer buffer = m_vertex_buffers[id];
		return { buffer.m_buffer, 0 };
	}
	else {
		idx -= MaxPersistentVertexBuffers;
		return frame.LookupTempVertexBuffer(idx);
	}
}

tuple<VkBuffer, VkDeviceAddress> GPU_Vulkan::GetIndexBufferForBinding(IndexBufferID id, GPU_Vulkan_Frame& frame)
{
	size_t idx = (size_t)id;
	if (idx < MaxPersistentIndexBuffers) {
		IndexBuffer buffer = m_index_buffers[id];
		return { buffer.m_buffer, 0 };
	}
	else {
		idx -= MaxPersistentIndexBuffers;
		return frame.LookupTempIndexBuffer(idx);
	}
}

VkDescriptorBufferInfo GPU_Vulkan::GetConstantBufferForBinding(ConstantBufferID id, GPU_Vulkan_Frame& frame)
{
	size_t idx = (size_t)id;
	if (idx < MaxPersistentConstantBuffers) {
		ConstantBuffer buffer = m_constant_buffers[id];
		return { buffer.m_buffer, 0, VK_WHOLE_SIZE /* TODO SIZE */ };
	}
	else {
		idx -= MaxPersistentConstantBuffers;
		return frame.LookupTempConstantBuffer(idx);
	}
}
