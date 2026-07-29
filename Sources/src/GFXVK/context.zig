const std = @import("std");
const raw = @import("vulkan");

extern fn vkGetInstanceProcAddr(instance: raw.Instance, p_name: [*:0]const u8) raw.PfnVoidFunction;
extern "kernel32" fn GetModuleHandleW(name: ?[*:0]const u16) callconv(.winapi) ?std.os.windows.HINSTANCE;

var g_base: raw.BaseWrapper = undefined;
var g_instance: raw.InstanceWrapper = undefined;
var g_device: raw.DeviceWrapper = undefined;

// Compatibility facade for the pre-dispatch vulkan-zig API used by the
// renderer code below. Every operation is routed through the dispatch table
// loaded for the current Vulkan instance/device.
const vk = struct {
    pub const API_VERSION_1_3 = raw.API_VERSION_1_3;
    pub const QUEUE_FAMILY_IGNORED = raw.QUEUE_FAMILY_IGNORED;
    pub const SUBPASS_EXTERNAL = raw.SUBPASS_EXTERNAL;
    pub const BaseWrapper = raw.BaseWrapper;
    pub const InstanceWrapper = raw.InstanceWrapper;
    pub const DeviceWrapper = raw.DeviceWrapper;
    pub const ApplicationInfo = raw.ApplicationInfo;
    pub const Instance = raw.Instance;
    pub const InstanceCreateInfo = raw.InstanceCreateInfo;
    pub const PhysicalDevice = raw.PhysicalDevice;
    pub const QueueFamilyProperties = raw.QueueFamilyProperties;
    pub const Device = raw.Device;
    pub const DeviceCreateInfo = raw.DeviceCreateInfo;
    pub const DeviceQueueCreateInfo = raw.DeviceQueueCreateInfo;
    pub const PhysicalDeviceFeatures = raw.PhysicalDeviceFeatures;
    pub const Queue = raw.Queue;
    pub const Win32SurfaceCreateInfoKHR = raw.Win32SurfaceCreateInfoKHR;
    pub const SurfaceKHR = raw.SurfaceKHR;
    pub const SurfaceCapabilitiesKHR = raw.SurfaceCapabilitiesKHR;
    pub const SurfaceFormatKHR = raw.SurfaceFormatKHR;
    pub const PresentModeKHR = raw.PresentModeKHR;
    pub const SwapchainKHR = raw.SwapchainKHR;
    pub const SwapchainCreateInfoKHR = raw.SwapchainCreateInfoKHR;
    pub const Image = raw.Image;
    pub const ImageView = raw.ImageView;
    pub const ImageViewCreateInfo = raw.ImageViewCreateInfo;
    pub const RenderPass = raw.RenderPass;
    pub const RenderPassBeginInfo = raw.RenderPassBeginInfo;
    pub const RenderPassCreateInfo = raw.RenderPassCreateInfo;
    pub const AttachmentDescription = raw.AttachmentDescription;
    pub const AttachmentReference = raw.AttachmentReference;
    pub const SubpassDescription = raw.SubpassDescription;
    pub const SubpassDependency = raw.SubpassDependency;
    pub const Framebuffer = raw.Framebuffer;
    pub const FramebufferCreateInfo = raw.FramebufferCreateInfo;
    pub const Extent2D = raw.Extent2D;
    pub const CommandPool = raw.CommandPool;
    pub const CommandPoolCreateInfo = raw.CommandPoolCreateInfo;
    pub const CommandBuffer = raw.CommandBuffer;
    pub const CommandBufferAllocateInfo = raw.CommandBufferAllocateInfo;
    pub const CommandBufferBeginInfo = raw.CommandBufferBeginInfo;
    pub const Semaphore = raw.Semaphore;
    pub const SemaphoreCreateInfo = raw.SemaphoreCreateInfo;
    pub const Fence = raw.Fence;
    pub const FenceCreateInfo = raw.FenceCreateInfo;
    pub const ClearColorValue = raw.ClearColorValue;
    pub const ClearValue = raw.ClearValue;
    pub const Result = raw.Result;
    pub const Pipeline = raw.Pipeline;
    pub const PipelineLayout = raw.PipelineLayout;
    pub const DescriptorSetLayout = raw.DescriptorSetLayout;
    pub const DescriptorPool = raw.DescriptorPool;
    pub const DescriptorSet = raw.DescriptorSet;
    pub const DeviceMemory = raw.DeviceMemory;
    pub const Buffer = raw.Buffer;
    pub const BufferCreateInfo = raw.BufferCreateInfo;
    pub const MemoryPropertyFlags = raw.MemoryPropertyFlags;
    pub const MemoryMapFlags = raw.MemoryMapFlags;
    pub const MemoryAllocateInfo = raw.MemoryAllocateInfo;
    pub const PhysicalDeviceMemoryProperties = raw.PhysicalDeviceMemoryProperties;
    pub const DeviceSize = raw.DeviceSize;
    pub const Sampler = raw.Sampler;
    pub const SamplerCreateInfo = raw.SamplerCreateInfo;
    pub const ShaderModule = raw.ShaderModule;
    pub const ShaderModuleCreateInfo = raw.ShaderModuleCreateInfo;
    pub const PresentInfoKHR = raw.PresentInfoKHR;
    pub const SubmitInfo = raw.SubmitInfo;
    pub const PipelineStageFlags = raw.PipelineStageFlags;
    pub const SampleCountFlags = raw.SampleCountFlags;
    pub const Viewport = raw.Viewport;
    pub const Rect2D = raw.Rect2D;
    pub const ImageCreateInfo = raw.ImageCreateInfo;
    pub const ImageMemoryBarrier = raw.ImageMemoryBarrier;
    pub const BufferImageCopy = raw.BufferImageCopy;
    pub const DescriptorSetLayoutCreateInfo = raw.DescriptorSetLayoutCreateInfo;
    pub const DescriptorSetLayoutBinding = raw.DescriptorSetLayoutBinding;
    pub const DescriptorPoolCreateInfo = raw.DescriptorPoolCreateInfo;
    pub const DescriptorPoolSize = raw.DescriptorPoolSize;
    pub const DescriptorSetAllocateInfo = raw.DescriptorSetAllocateInfo;
    pub const DescriptorImageInfo = raw.DescriptorImageInfo;
    pub const WriteDescriptorSet = raw.WriteDescriptorSet;
    pub const PipelineShaderStageCreateInfo = raw.PipelineShaderStageCreateInfo;
    pub const PipelineVertexInputStateCreateInfo = raw.PipelineVertexInputStateCreateInfo;
    pub const VertexInputBindingDescription = raw.VertexInputBindingDescription;
    pub const VertexInputAttributeDescription = raw.VertexInputAttributeDescription;
    pub const PipelineInputAssemblyStateCreateInfo = raw.PipelineInputAssemblyStateCreateInfo;
    pub const PipelineViewportStateCreateInfo = raw.PipelineViewportStateCreateInfo;
    pub const PipelineRasterizationStateCreateInfo = raw.PipelineRasterizationStateCreateInfo;
    pub const PipelineMultisampleStateCreateInfo = raw.PipelineMultisampleStateCreateInfo;
    pub const PipelineColorBlendAttachmentState = raw.PipelineColorBlendAttachmentState;
    pub const PipelineColorBlendStateCreateInfo = raw.PipelineColorBlendStateCreateInfo;
    pub const GraphicsPipelineCreateInfo = raw.GraphicsPipelineCreateInfo;
    pub const PushConstantRange = raw.PushConstantRange;
    pub const makeApiVersion = raw.makeApiVersion;

    pub fn createInstance(info: *const raw.InstanceCreateInfo, allocator: ?*const raw.AllocationCallbacks, out: *raw.Instance) !void {
        out.* = try g_base.createInstance(info, allocator);
    }
    pub fn enumeratePhysicalDevices(instance: raw.Instance, count: *u32, devices: ?[*]raw.PhysicalDevice) !void {
        _ = try g_instance.enumeratePhysicalDevices(instance, count, devices);
    }
    pub fn getPhysicalDeviceQueueFamilyProperties(pd: raw.PhysicalDevice, count: *u32, props: ?[*]raw.QueueFamilyProperties) void {
        g_instance.getPhysicalDeviceQueueFamilyProperties(pd, count, props);
    }
    pub fn createDevice(pd: raw.PhysicalDevice, info: *const raw.DeviceCreateInfo, allocator: ?*const raw.AllocationCallbacks, out: *raw.Device) !void {
        out.* = try g_instance.createDevice(pd, info, allocator);
    }
    pub fn getDeviceQueue(device: raw.Device, family: u32, index: u32, out: *raw.Queue) void {
        out.* = g_device.getDeviceQueue(device, family, index);
    }
    pub fn createWin32SurfaceKHR(instance: raw.Instance, info: *const raw.Win32SurfaceCreateInfoKHR, allocator: ?*const raw.AllocationCallbacks, out: *raw.SurfaceKHR) !void {
        out.* = try g_instance.createWin32SurfaceKHR(instance, info, allocator);
    }
    pub fn getPhysicalDeviceSurfaceCapabilitiesKHR(pd: raw.PhysicalDevice, surface: raw.SurfaceKHR, out: *raw.SurfaceCapabilitiesKHR) !void {
        out.* = try g_instance.getPhysicalDeviceSurfaceCapabilitiesKHR(pd, surface);
    }
    pub fn getPhysicalDeviceSurfaceFormatsKHR(pd: raw.PhysicalDevice, surface: raw.SurfaceKHR, count: *u32, formats: ?[*]raw.SurfaceFormatKHR) !void {
        _ = try g_instance.getPhysicalDeviceSurfaceFormatsKHR(pd, surface, count, formats);
    }
    pub fn getPhysicalDeviceSurfacePresentModesKHR(pd: raw.PhysicalDevice, surface: raw.SurfaceKHR, count: *u32, modes: ?[*]raw.PresentModeKHR) !void {
        _ = try g_instance.getPhysicalDeviceSurfacePresentModesKHR(pd, surface, count, modes);
    }
    pub fn createSwapchainKHR(device: raw.Device, info: *const raw.SwapchainCreateInfoKHR, allocator: ?*const raw.AllocationCallbacks, out: *raw.SwapchainKHR) !void {
        out.* = try g_device.createSwapchainKHR(device, info, allocator);
    }
    pub fn getSwapchainImagesKHR(device: raw.Device, swapchain: raw.SwapchainKHR, count: *u32, images: ?[*]raw.Image) !void {
        _ = try g_device.getSwapchainImagesKHR(device, swapchain, count, images);
    }
    pub fn createImageView(device: raw.Device, info: *const raw.ImageViewCreateInfo, allocator: ?*const raw.AllocationCallbacks, out: *raw.ImageView) !void {
        out.* = try g_device.createImageView(device, info, allocator);
    }
    pub fn createRenderPass(device: raw.Device, info: *const raw.RenderPassCreateInfo, allocator: ?*const raw.AllocationCallbacks, out: *raw.RenderPass) !void {
        out.* = try g_device.createRenderPass(device, info, allocator);
    }
    pub fn createFramebuffer(device: raw.Device, info: *const raw.FramebufferCreateInfo, allocator: ?*const raw.AllocationCallbacks, out: *raw.Framebuffer) !void {
        out.* = try g_device.createFramebuffer(device, info, allocator);
    }
    pub fn createCommandPool(device: raw.Device, info: *const raw.CommandPoolCreateInfo, allocator: ?*const raw.AllocationCallbacks, out: *raw.CommandPool) !void {
        out.* = try g_device.createCommandPool(device, info, allocator);
    }
    pub fn allocateCommandBuffers(device: raw.Device, info: *const raw.CommandBufferAllocateInfo, allocator: ?*const raw.AllocationCallbacks, bufs: []raw.CommandBuffer) !void {
        _ = allocator;
        try g_device.allocateCommandBuffers(device, info, bufs.ptr);
    }
    pub fn deviceWaitIdle(device: raw.Device) !void {
        try g_device.deviceWaitIdle(device);
    }
    pub fn waitForFences(device: raw.Device, count: u32, fences: *const raw.Fence, wait_all: bool, timeout: u64) !void {
        _ = try g_device.waitForFences(device, @as([*]const raw.Fence, @ptrCast(fences))[0..count], if (wait_all) .true else .false, timeout);
    }
    pub fn cmdEndRenderPass(command_buffer: raw.CommandBuffer) void {
        g_device.cmdEndRenderPass(command_buffer);
    }
    pub fn queuePresentKHR(queue: raw.Queue, info: *const raw.PresentInfoKHR) !raw.Result {
        return try g_device.queuePresentKHR(queue, info);
    }
    pub fn updateDescriptorSets(device: raw.Device, count: u32, writes: *const raw.WriteDescriptorSet, _: u32, _: ?*const anyopaque) void {
        g_device.updateDescriptorSets(device, writes[0..count], null);
    }
    pub fn cmdBindPipeline(command_buffer: raw.CommandBuffer, bind_point: raw.PipelineBindPoint, pipeline: raw.Pipeline) void {
        g_device.cmdBindPipeline(command_buffer, bind_point, pipeline);
    }
    pub fn cmdSetViewport(command_buffer: raw.CommandBuffer, first: u32, count: u32, viewports: *const raw.Viewport) void {
        g_device.cmdSetViewport(command_buffer, first, @as([*]const raw.Viewport, @ptrCast(viewports))[0..count]);
    }
    pub fn destroyInstance(instance: raw.Instance, allocator: ?*const raw.AllocationCallbacks) void { g_instance.destroyInstance(instance, allocator); }
    pub fn endCommandBuffer(command_buffer: raw.CommandBuffer) !void { try g_device.endCommandBuffer(command_buffer); }
    pub fn cmdBindDescriptorSets(command_buffer: raw.CommandBuffer, bind_point: raw.PipelineBindPoint, layout: raw.PipelineLayout, first: u32, count: u32, sets: *const raw.DescriptorSet, _: u32, _: ?*const u32) void {
        g_device.cmdBindDescriptorSets(command_buffer, bind_point, layout, first, @as([*]const raw.DescriptorSet, @ptrCast(sets))[0..count], null);
    }
    pub fn createImage(device: raw.Device, info: *const raw.ImageCreateInfo, allocator: ?*const raw.AllocationCallbacks, out: *raw.Image) !void { out.* = try g_device.createImage(device, info, allocator); }
    pub fn destroySampler(device: raw.Device, sampler: raw.Sampler, allocator: ?*const raw.AllocationCallbacks) void { g_device.destroySampler(device, sampler, allocator); }
    pub fn destroyImageView(device: raw.Device, view: raw.ImageView, allocator: ?*const raw.AllocationCallbacks) void { g_device.destroyImageView(device, view, allocator); }
    pub fn destroyImage(device: raw.Device, image: raw.Image, allocator: ?*const raw.AllocationCallbacks) void { g_device.destroyImage(device, image, allocator); }
    pub fn destroyBuffer(device: raw.Device, buffer: raw.Buffer, allocator: ?*const raw.AllocationCallbacks) void { g_device.destroyBuffer(device, buffer, allocator); }
    pub fn destroyDevice(device: raw.Device, allocator: ?*const raw.AllocationCallbacks) void { g_device.destroyDevice(device, allocator); }
    pub fn freeMemory(device: raw.Device, memory: raw.DeviceMemory, allocator: ?*const raw.AllocationCallbacks) void { g_device.freeMemory(device, memory, allocator); }
    pub fn resetFences(device: raw.Device, count: u32, fences: *const raw.Fence) !void { try g_device.resetFences(device, @as([*]const raw.Fence, @ptrCast(fences))[0..count]); }
    pub fn queueSubmit(queue: raw.Queue, count: u32, submits: *const raw.SubmitInfo, fence: raw.Fence) !void { try g_device.queueSubmit(queue, @as([*]const raw.SubmitInfo, @ptrCast(submits))[0..count], fence); }
    pub fn acquireNextImageKHR(device: raw.Device, swapchain: raw.SwapchainKHR, timeout: u64, semaphore: raw.Semaphore, fence: raw.Fence, out: *u32) !void { const result = try g_device.acquireNextImageKHR(device, swapchain, timeout, semaphore, fence); out.* = result.image_index; }
    pub fn resetCommandBuffer(command_buffer: raw.CommandBuffer, flags: raw.CommandBufferResetFlags) !void { try g_device.resetCommandBuffer(command_buffer, flags); }
    pub fn beginCommandBuffer(command_buffer: raw.CommandBuffer, info: *const raw.CommandBufferBeginInfo) !void { try g_device.beginCommandBuffer(command_buffer, info); }
    pub fn getImageMemoryRequirements(device: raw.Device, image: raw.Image) raw.MemoryRequirements { return g_device.getImageMemoryRequirements(device, image); }
    pub fn getBufferMemoryRequirements(device: raw.Device, buffer: raw.Buffer) raw.MemoryRequirements { return g_device.getBufferMemoryRequirements(device, buffer); }
    pub fn getPhysicalDeviceMemoryProperties(pd: raw.PhysicalDevice) raw.PhysicalDeviceMemoryProperties { return g_instance.getPhysicalDeviceMemoryProperties(pd); }
    pub fn destroyPipeline(device: raw.Device, pipeline: raw.Pipeline, allocator: ?*const raw.AllocationCallbacks) void { g_device.destroyPipeline(device, pipeline, allocator); }
    pub fn destroyPipelineLayout(device: raw.Device, layout: raw.PipelineLayout, allocator: ?*const raw.AllocationCallbacks) void { g_device.destroyPipelineLayout(device, layout, allocator); }
    pub fn destroySurfaceKHR(instance: raw.Instance, surface: raw.SurfaceKHR, allocator: ?*const raw.AllocationCallbacks) void { g_instance.destroySurfaceKHR(instance, surface, allocator); }
    pub fn destroyCommandPool(device: raw.Device, pool: raw.CommandPool, allocator: ?*const raw.AllocationCallbacks) void { g_device.destroyCommandPool(device, pool, allocator); }
    pub fn destroyDescriptorPool(device: raw.Device, pool: raw.DescriptorPool, allocator: ?*const raw.AllocationCallbacks) void { g_device.destroyDescriptorPool(device, pool, allocator); }
    pub fn destroyDescriptorSetLayout(device: raw.Device, layout: raw.DescriptorSetLayout, allocator: ?*const raw.AllocationCallbacks) void { g_device.destroyDescriptorSetLayout(device, layout, allocator); }
    pub fn destroyFence(device: raw.Device, fence: raw.Fence, allocator: ?*const raw.AllocationCallbacks) void { g_device.destroyFence(device, fence, allocator); }
    pub fn destroySemaphore(device: raw.Device, semaphore: raw.Semaphore, allocator: ?*const raw.AllocationCallbacks) void { g_device.destroySemaphore(device, semaphore, allocator); }
    pub fn destroyFramebuffer(device: raw.Device, framebuffer: raw.Framebuffer, allocator: ?*const raw.AllocationCallbacks) void { g_device.destroyFramebuffer(device, framebuffer, allocator); }
    pub fn destroyRenderPass(device: raw.Device, render_pass: raw.RenderPass, allocator: ?*const raw.AllocationCallbacks) void { g_device.destroyRenderPass(device, render_pass, allocator); }
    pub fn destroySwapchainKHR(device: raw.Device, swapchain: raw.SwapchainKHR, allocator: ?*const raw.AllocationCallbacks) void { g_device.destroySwapchainKHR(device, swapchain, allocator); }
    pub fn allocateMemory(device: raw.Device, info: *const raw.MemoryAllocateInfo, allocator: ?*const raw.AllocationCallbacks, out: *raw.DeviceMemory) !void { out.* = try g_device.allocateMemory(device, info, allocator); }
    pub fn bindImageMemory(device: raw.Device, image: raw.Image, memory: raw.DeviceMemory, offset: raw.DeviceSize) !void { try g_device.bindImageMemory(device, image, memory, offset); }
    pub fn bindBufferMemory(device: raw.Device, buffer: raw.Buffer, memory: raw.DeviceMemory, offset: raw.DeviceSize) !void { try g_device.bindBufferMemory(device, buffer, memory, offset); }
    pub fn mapMemory(device: raw.Device, memory: raw.DeviceMemory, offset: raw.DeviceSize, size: raw.DeviceSize, flags: raw.MemoryMapFlags, out: *[*]u8, _: ?*anyopaque) !void { out.* = @ptrCast(try g_device.mapMemory(device, memory, offset, size, flags)); }
    pub fn createSampler(device: raw.Device, info: *const raw.SamplerCreateInfo, allocator: ?*const raw.AllocationCallbacks, out: *raw.Sampler) !void { out.* = try g_device.createSampler(device, info, allocator); }
    pub fn createBuffer(device: raw.Device, info: *const raw.BufferCreateInfo, allocator: ?*const raw.AllocationCallbacks, out: *raw.Buffer) !void { out.* = try g_device.createBuffer(device, info, allocator); }
    pub fn cmdBeginRenderPass(command_buffer: raw.CommandBuffer, info: *const raw.RenderPassBeginInfo, contents: raw.SubpassContents) void { g_device.cmdBeginRenderPass(command_buffer, info, contents); }
    pub fn cmdSetScissor(command_buffer: raw.CommandBuffer, first: u32, count: u32, scissors: *const raw.Rect2D) void { g_device.cmdSetScissor(command_buffer, first, @as([*]const raw.Rect2D, @ptrCast(scissors))[0..count]); }
    pub fn cmdPushConstants(command_buffer: raw.CommandBuffer, layout: raw.PipelineLayout, stages: raw.ShaderStageFlags, offset: u32, size: u32, values: *const anyopaque) void { g_device.cmdPushConstants(command_buffer, layout, stages, offset, size, values); }
};

pub const VkContext = opaque {};

const PushConsts = extern struct {
    mvp: [16]f32,
    alpha_ref: f32,
    use_texture: i32,
    alpha_test: i32,
};

const Texture = struct {
    img: vk.Image,
    view: vk.ImageView,
    sampler: vk.Sampler,
    mem: vk.DeviceMemory,
    staging: vk.Buffer,
    staging_mem: vk.DeviceMemory,
    stage_ptr: [*]u8,
    width: u32,
    height: u32,
    format: u32, // EGFXPixelFormat
};

const RingBuffer = struct {
    buf: vk.Buffer,
    mem: vk.DeviceMemory,
    ptr: [*]u8,
    size: usize,
    cursor: usize,
    lock_base: usize,
};

const PipelineState = struct {
    pl: vk.Pipeline,
    layout: vk.PipelineLayout,
};

pub const Context = struct {
    a: std.mem.Allocator,
    h: std.os.windows.HWND,
    inst: vk.Instance,
    pd: vk.PhysicalDevice,
    dev: vk.Device,
    q: vk.Queue,
    qf: u32,
    surf: vk.SurfaceKHR,
    sf: vk.SurfaceFormatKHR,
    sc: vk.SwapchainKHR,
    si: []vk.Image,
    siv: []vk.ImageView,
    se: vk.Extent2D,
    rp: vk.RenderPass,
    cp: vk.CommandPool,
    cb: []vk.CommandBuffer,
    fb: []Framebuffer,
    ia: vk.Semaphore,
    rf: vk.Semaphore,
    f: vk.Fence,
    cc: vk.ClearColorValue = .{ .float_32 = .{ 0.1, 0.1, 0.1, 1.0 } },
    idx: u32 = 0,

    // Phase 3 — 2D resources
    dsl: vk.DescriptorSetLayout,
    dp: vk.DescriptorPool,
    ds: vk.DescriptorSet,
    upload_cp: vk.CommandPool,
    upload_cb: vk.CommandBuffer,
    pc: PushConsts = .{
        .mvp = undefined,
        .alpha_ref = 0.0,
        .use_texture = 1,
        .alpha_test = 0,
    },
    curr_tex: ?*anyopaque = null,
    textures: std.AutoHashMap(*anyopaque, Texture) = undefined,
    vb: RingBuffer = .{ .buf = .null_handle, .mem = .null_handle, .ptr = undefined, .size = 0, .cursor = 0, .lock_base = 0 },
    ib: RingBuffer = .{ .buf = .null_handle, .mem = .null_handle, .ptr = undefined, .size = 0, .cursor = 0, .lock_base = 0 },
    pipelines: std.AutoHashMap(u32, PipelineState) = undefined,
    curr_effect: u32 = 0,
    curr_pl: vk.Pipeline = .null_handle,
    curr_pl_layout: vk.PipelineLayout = .null_handle,
    bgra_view: vk.ImageView = .null_handle,
};

fn fmt(avail: []const vk.SurfaceFormatKHR) vk.SurfaceFormatKHR {
    for (avail) |f| if (f.format == .b8g8r8a8_unorm and f.color_space == .srgb_nonlinear_khr) return f;
    return avail[0];
}
fn pmode(avail: []const vk.PresentModeKHR) vk.PresentModeKHR {
    for (avail) |m| if (m == .mailbox_khr) return m;
    return .fifo_khr;
}
fn ext(caps: vk.SurfaceCapabilitiesKHR) vk.Extent2D {
    if (caps.current_extent.width != std.math.maxInt(u32)) return caps.current_extent;
    var e = vk.Extent2D{ .width = 800, .height = 600 };
    e.width = std.math.clamp(e.width, caps.min_image_extent.width, caps.max_image_extent.width);
    e.height = std.math.clamp(e.height, caps.min_image_extent.height, caps.max_image_extent.height);
    return e;
}

fn find_mem_type(pd: vk.PhysicalDevice, flags: vk.MemoryPropertyFlags) !u32 {
    const mp = vk.getPhysicalDeviceMemoryProperties(pd);
    var i: u32 = 0;
    while (i < mp.memory_type_count) : (i += 1) {
        if ((mp.memory_types[i].property_flags.raw == flags.raw) or (mp.memory_types[i].property_flags.contains(flags))) return i;
    }
    return error.NoSuitableMemoryType;
}

fn find_mem_type_filter(pd: vk.PhysicalDevice, filter: u32, flags: vk.MemoryPropertyFlags) !u32 {
    const mp = vk.getPhysicalDeviceMemoryProperties(pd);
    var i: u32 = 0;
    while (i < mp.memory_type_count) : (i += 1) {
        if ((filter & (@as(u32, 1) << @as(u5, @intCast(i)))) != 0 and mp.memory_types[i].property_flags.contains(flags)) return i;
    }
    return error.NoSuitableMemoryType;
}

fn init_inst() !vk.Instance {
    g_base = vk.BaseWrapper.load(vkGetInstanceProcAddr);
    const app = vk.ApplicationInfo{ .s_type = .application_info, .p_application_name = "Blitzkrieg", .application_version = vk.makeApiVersion(0, 2, 0, 0).toU32(), .p_engine_name = "Blitzkrieg", .engine_version = vk.makeApiVersion(0, 2, 0, 0).toU32(), .api_version = vk.API_VERSION_1_3.toU32() };
    var inst: vk.Instance = undefined;
    try vk.createInstance(&vk.InstanceCreateInfo{ .s_type = .instance_create_info, .p_application_info = &app }, null, &inst);
    g_instance = vk.InstanceWrapper.load(inst, g_base.dispatch.vkGetInstanceProcAddr.?);
    return inst;
}
fn pick_dev(inst: vk.Instance, al: std.mem.Allocator) !vk.PhysicalDevice {
    var n: u32 = undefined; try vk.enumeratePhysicalDevices(inst, &n, null);
    var devs: []vk.PhysicalDevice = undefined;
    if (n > 0) { devs = try al.alloc(vk.PhysicalDevice, @intCast(n)); try vk.enumeratePhysicalDevices(inst, &n, devs.ptr); }
    return devs[0];
}
fn mk_dev(pd: vk.PhysicalDevice, al: std.mem.Allocator) !struct { vk.Device, u32, vk.Queue } {
    var n: u32 = undefined; vk.getPhysicalDeviceQueueFamilyProperties(pd, &n, null);
    var fams: []vk.QueueFamilyProperties = undefined;
    if (n > 0) { fams = try al.alloc(vk.QueueFamilyProperties, @intCast(n)); vk.getPhysicalDeviceQueueFamilyProperties(pd, &n, fams.ptr); }
    const qf: u32 = 0;
    var q: vk.Queue = undefined;
    var dev: vk.Device = undefined;
    try vk.createDevice(pd, &vk.DeviceCreateInfo{ .s_type = .device_create_info, .queue_create_info_count = 1, .p_queue_create_infos = &[_]vk.DeviceQueueCreateInfo{.{ .s_type = .device_queue_create_info, .queue_family_index = qf, .queue_count = 1, .p_queue_priorities = &[1]f32{1.0} }}, .enabled_extension_count = 1, .pp_enabled_extension_names = &[_][*:0]const u8{"VK_KHR_swapchain"}, .p_enabled_features = &vk.PhysicalDeviceFeatures{ .sampler_anisotropy = .true } }, null, &dev);
    g_device = vk.DeviceWrapper.load(dev, g_instance.dispatch.vkGetDeviceProcAddr.?);
    vk.getDeviceQueue(dev, qf, 0, &q);
    return .{ dev, qf, q };
}
fn mk_surf(inst: vk.Instance, h: std.os.windows.HWND) !vk.SurfaceKHR {
    var s: vk.SurfaceKHR = undefined;
    const hinstance = GetModuleHandleW(null) orelse return error.GetModuleHandleFailed;
    try vk.createWin32SurfaceKHR(inst, &vk.Win32SurfaceCreateInfoKHR{ .s_type = .win32_surface_create_info_khr, .hinstance = hinstance, .hwnd = h }, null, &s);
    return s;
}
fn mk_sc(al: std.mem.Allocator, d: vk.Device, pd: vk.PhysicalDevice, s: vk.SurfaceKHR, qf: u32) !struct { vk.SwapchainKHR, []vk.Image, []vk.ImageView, vk.SurfaceFormatKHR, vk.Extent2D } {
    _ = qf;
    var caps: vk.SurfaceCapabilitiesKHR = undefined; try vk.getPhysicalDeviceSurfaceCapabilitiesKHR(pd, s, &caps);
    var fc: u32 = undefined; try vk.getPhysicalDeviceSurfaceFormatsKHR(pd, s, &fc, null);
    var fmts: []vk.SurfaceFormatKHR = undefined;
    if (fc > 0) { fmts = try al.alloc(vk.SurfaceFormatKHR, @intCast(fc)); try vk.getPhysicalDeviceSurfaceFormatsKHR(pd, s, &fc, fmts.ptr); }
    var mc: u32 = undefined; try vk.getPhysicalDeviceSurfacePresentModesKHR(pd, s, &mc, null);
    var modes: []vk.PresentModeKHR = undefined;
    if (mc > 0) { modes = try al.alloc(vk.PresentModeKHR, @intCast(mc)); try vk.getPhysicalDeviceSurfacePresentModesKHR(pd, s, &mc, modes.ptr); }
    const f = fmt(fmts); const m = pmode(modes); const e = ext(caps);
    var ic = caps.min_image_count + 1; if (caps.max_image_count > 0 and ic > caps.max_image_count) ic = caps.max_image_count;
    var sc: vk.SwapchainKHR = undefined;
    try vk.createSwapchainKHR(d, &vk.SwapchainCreateInfoKHR{ .s_type = .swapchain_create_info_khr, .surface = s, .min_image_count = ic, .image_format = f.format, .image_color_space = f.color_space, .image_extent = e, .image_array_layers = 1, .image_usage = .{ .color_attachment_bit = true }, .image_sharing_mode = .exclusive, .pre_transform = caps.current_transform, .composite_alpha = .{ .opaque_bit_khr = true }, .present_mode = m, .clipped = .true, .old_swapchain = .null_handle }, null, &sc);
    var cnt: u32 = undefined; try vk.getSwapchainImagesKHR(d, sc, &cnt, null);
    const imgs = try al.alloc(vk.Image, @intCast(cnt));
    try vk.getSwapchainImagesKHR(d, sc, &cnt, imgs.ptr);
    const ivs = try al.alloc(vk.ImageView, @intCast(cnt));
    for (imgs, 0..) |_, i| {
        try vk.createImageView(d, &vk.ImageViewCreateInfo{ .s_type = .image_view_create_info, .image = imgs[i], .view_type = .@"2d", .format = f.format, .components = .{ .r = .identity, .g = .identity, .b = .identity, .a = .identity }, .subresource_range = .{ .aspect_mask = .{ .color_bit = true }, .base_mip_level = 0, .level_count = 1, .base_array_layer = 0, .layer_count = 1 } }, null, &ivs[i]);
    }
    return .{ sc, imgs, ivs, f, e };
}

fn mk_rp(d: vk.Device, f: vk.SurfaceFormatKHR) !vk.RenderPass {
    var rp: vk.RenderPass = undefined;
    try vk.createRenderPass(d, &vk.RenderPassCreateInfo{
        .s_type = .render_pass_create_info,
        .attachment_count = 1,
        .p_attachments = &[_]vk.AttachmentDescription{.{
            .format = f.format,
            .samples = .{ .@"1_bit" = true },
            .load_op = .clear,
            .store_op = .store,
            .stencil_load_op = .dont_care,
            .stencil_store_op = .dont_care,
            .initial_layout = .undefined,
            .final_layout = .present_src_khr,
        }},
        .subpass_count = 1,
        .p_subpasses = &[_]vk.SubpassDescription{.{
            .pipeline_bind_point = .graphics,
            .color_attachment_count = 1,
            .p_color_attachments = &[_]vk.AttachmentReference{.{
                .attachment = 0,
                .layout = .color_attachment_optimal,
            }},
        }},
        .dependency_count = 1,
        .p_dependencies = &[_]vk.SubpassDependency{.{
            .src_subpass = vk.SUBPASS_EXTERNAL,
            .dst_subpass = 0,
            .src_stage_mask = .{ .color_attachment_output_bit = true },
            .dst_stage_mask = .{ .color_attachment_output_bit = true },
            .src_access_mask = .{},
            .dst_access_mask = .{ .color_attachment_write_bit = true },
        }},
    }, null, &rp);
    return rp;
}

fn mk_fbs(al: std.mem.Allocator, d: vk.Device, rp: vk.RenderPass, ivs: []const vk.ImageView, e: vk.Extent2D) ![]Framebuffer {
    var fbs = try al.alloc(Framebuffer, ivs.len);
    for (ivs, 0..) |iv, i| {
        var fb: vk.Framebuffer = undefined;
        try vk.createFramebuffer(d, &vk.FramebufferCreateInfo{
            .s_type = .framebuffer_create_info,
            .render_pass = rp,
            .attachment_count = 1,
            .p_attachments = @as([*]const vk.ImageView, @ptrCast(&iv)),
            .width = e.width,
            .height = e.height,
            .layers = 1,
        }, null, &fb);
        fbs[i] = .{ .fb = fb, .iv = iv };
    }
    return fbs;
}

fn mk_cmd(al: std.mem.Allocator, d: vk.Device, qf: u32) !struct { vk.CommandPool, []vk.CommandBuffer } {
    var cp: vk.CommandPool = undefined;
    try vk.createCommandPool(d, &vk.CommandPoolCreateInfo{
        .s_type = .command_pool_create_info,
        .queue_family_index = qf,
        .flags = .{ .reset_command_buffer_bit = true },
    }, null, &cp);
    const n_bufs: u32 = 4;
    const bufs = try al.alloc(vk.CommandBuffer, @intCast(n_bufs));
    try vk.allocateCommandBuffers(d, &vk.CommandBufferAllocateInfo{
        .s_type = .command_buffer_allocate_info,
        .command_pool = cp,
        .level = .primary,
        .command_buffer_count = n_bufs,
    }, null, bufs);
    return .{ cp, bufs };
}

const Framebuffer = struct { fb: vk.Framebuffer, iv: vk.ImageView };

pub fn init(al: std.mem.Allocator, h: std.os.windows.HWND) !Context {
    const inst = try init_inst();
    errdefer vk.destroyInstance(inst, null);
    const pd = try pick_dev(inst, al);
    const dq = try mk_dev(pd, al);
    const device = dq[0];
    const queue_family = dq[1];
    const queue = dq[2];
    errdefer vk.destroyDevice(device, null);
    const surf = try mk_surf(inst, h);
    errdefer vk.destroySurfaceKHR(inst, surf, null);
    const sc = try mk_sc(al, device, pd, surf, queue_family);
    const rp = try mk_rp(device, sc[3]);
    const fbs = try mk_fbs(al, device, rp, sc[2], sc[4]);
    const cmdp = try mk_cmd(al, device, queue_family);

    var ctx = Context{
        .a = al,
        .h = h,
        .inst = inst,
        .pd = pd,
        .dev = device,
        .q = queue,
        .qf = queue_family,
        .surf = surf,
        .sf = sc[3],
        .sc = sc[0],
        .si = sc[1],
        .siv = sc[2],
        .se = sc[4],
        .rp = rp,
        .cp = cmdp[0],
        .cb = cmdp[1],
        .fb = fbs,
    };

    var ia: vk.Semaphore = undefined;
    try vk.createSemaphore(ctx.dev, &vk.SemaphoreCreateInfo{ .s_type = .semaphore_create_info }, null, &ia);
    ctx.ia = ia;
    var rf: vk.Semaphore = undefined;
    try vk.createSemaphore(ctx.dev, &vk.SemaphoreCreateInfo{ .s_type = .semaphore_create_info }, null, &rf);
    ctx.rf = rf;
    var f: vk.Fence = undefined;
    try vk.createFence(ctx.dev, &vk.FenceCreateInfo{ .s_type = .fence_create_info, .flags = .{ .signaled_bit = true } }, null, &f);
    ctx.f = f;

    // Phase 3 — pipeline layout, descriptor set, ring buffers
    const dsl = blk: {
        var d: vk.DescriptorSetLayout = undefined;
        try vk.createDescriptorSetLayout(ctx.dev, &vk.DescriptorSetLayoutCreateInfo{
            .s_type = .descriptor_set_layout_create_info,
            .binding_count = 1,
            .p_bindings = &vk.DescriptorSetLayoutBinding{
                .binding = 0,
                .descriptor_type = .combined_image_sampler,
                .descriptor_count = 1,
                .stage_flags = .{ .fragment_bit = true },
            },
        }, null, &d);
        break :blk d;
    };
    ctx.dsl = dsl;

    const dp = blk: {
        var d: vk.DescriptorPool = undefined;
        try vk.createDescriptorPool(ctx.dev, &vk.DescriptorPoolCreateInfo{
            .s_type = .descriptor_pool_create_info,
            .max_sets = 1,
            .pool_size_count = 1,
            .p_pool_sizes = &vk.DescriptorPoolSize{
                .type = .combined_image_sampler,
                .descriptor_count = 1,
            },
        }, null, &d);
        break :blk d;
    };
    ctx.dp = dp;

    const ds = blk: {
        var d: vk.DescriptorSet = undefined;
        try vk.allocateDescriptorSets(ctx.dev, &vk.DescriptorSetAllocateInfo{
            .s_type = .descriptor_set_allocate_info,
            .descriptor_pool = dp,
            .descriptor_set_count = 1,
            .p_set_layouts = &dsl,
        }, &d);
        break :blk d;
    };
    ctx.ds = ds;

    const upload_cmds = try create_upload_cmds(ctx.dev, ctx.qf);
    ctx.upload_cp = upload_cmds.cp;
    ctx.upload_cb = upload_cmds.cb;

    ctx.textures = std.AutoHashMap(*anyopaque, Texture).init(al);
    ctx.pipelines = std.AutoHashMap(u32, PipelineState).init(al);

    // 256 KB vertex ring, 128 KB index ring
    ctx.vb = try create_ring(al, ctx.dev, ctx.pd, 256 * 1024);
    ctx.ib = try create_ring(al, ctx.dev, ctx.pd, 128 * 1024);

    // Ortho projection (x:0→w -1→1, y:0→h 1→-1, z:0→1 0→1)
    const w = @as(f32, @floatFromInt(ctx.se.width));
    const height = @as(f32, @floatFromInt(ctx.se.height));
    ctx.pc.mvp = .{
        2.0 / w, 0.0,      0.0, 0.0,
        0.0,    -2.0 / height,  0.0, 0.0,
        0.0,    0.0,      1.0, 0.0,
       -1.0,    1.0,      0.0, 1.0,
    };

    return ctx;
}

pub fn deinit(ctx: *Context) void {
    vk.deviceWaitIdle(ctx.dev) catch {};

    var texture_it = ctx.textures.iterator();
    while (texture_it.next()) |entry| {
        const tex = entry.value_ptr.*;
        vk.destroySampler(ctx.dev, tex.sampler, null);
        vk.destroyImageView(ctx.dev, tex.view, null);
        vk.destroyImage(ctx.dev, tex.img, null);
        vk.destroyBuffer(ctx.dev, tex.staging, null);
        vk.freeMemory(ctx.dev, tex.staging_mem, null);
        vk.freeMemory(ctx.dev, tex.mem, null);
    }
    ctx.textures.deinit();

    var pipeline_it = ctx.pipelines.iterator();
    while (pipeline_it.next()) |entry| {
        const ps = entry.value_ptr.*;
        vk.destroyPipeline(ctx.dev, ps.pl, null);
        vk.destroyPipelineLayout(ctx.dev, ps.layout, null);
    }
    ctx.pipelines.deinit();

    if (ctx.vb.buf != .null_handle) {
        vk.destroyBuffer(ctx.dev, ctx.vb.buf, null);
        vk.freeMemory(ctx.dev, ctx.vb.mem, null);
    }
    if (ctx.ib.buf != .null_handle) {
        vk.destroyBuffer(ctx.dev, ctx.ib.buf, null);
        vk.freeMemory(ctx.dev, ctx.ib.mem, null);
    }

    vk.destroyCommandPool(ctx.dev, ctx.upload_cp, null);
    vk.destroyDescriptorPool(ctx.dev, ctx.dp, null);
    vk.destroyDescriptorSetLayout(ctx.dev, ctx.dsl, null);

    vk.destroyFence(ctx.dev, ctx.f, null);
    vk.destroySemaphore(ctx.dev, ctx.rf, null);
    vk.destroySemaphore(ctx.dev, ctx.ia, null);
    for (ctx.fb) |fb| vk.destroyFramebuffer(ctx.dev, fb.fb, null);
    vk.destroyRenderPass(ctx.dev, ctx.rp, null);
    vk.destroyCommandPool(ctx.dev, ctx.cp, null);
    ctx.a.free(ctx.fb);
    ctx.a.free(ctx.cb);
    for (ctx.siv) |iv| vk.destroyImageView(ctx.dev, iv, null);
    ctx.a.free(ctx.siv);
    ctx.a.free(ctx.si);
    vk.destroySwapchainKHR(ctx.dev, ctx.sc, null);
    vk.destroySurfaceKHR(ctx.inst, ctx.surf, null);
    vk.destroyDevice(ctx.dev, null);
    vk.destroyInstance(ctx.inst, null);
}

pub fn begin_scene(ctx: *Context) !bool {
    try vk.waitForFences(ctx.dev, 1, &ctx.f, true, std.math.maxInt(u64));
    try vk.resetFences(ctx.dev, 1, &ctx.f);
    var idx: u32 = undefined;
    try vk.acquireNextImageKHR(ctx.dev, ctx.sc, std.math.maxInt(u64), ctx.ia, .null_handle, &idx);
    ctx.idx = idx % @as(u32, @intCast(ctx.fb.len));

    try vk.resetCommandBuffer(ctx.cb[ctx.idx], .{});
    try vk.beginCommandBuffer(ctx.cb[ctx.idx], &vk.CommandBufferBeginInfo{
        .s_type = .command_buffer_begin_info,
        .flags = .{ .one_time_submit_bit = true },
    });

    const rpba = [_]vk.ClearValue{.{ .color = ctx.cc }};
    vk.cmdBeginRenderPass(ctx.cb[ctx.idx], &vk.RenderPassBeginInfo{
        .s_type = .render_pass_begin_info,
        .render_pass = ctx.rp,
        .framebuffer = ctx.fb[ctx.idx].fb,
        .render_area = .{ .offset = .{ .x = 0, .y = 0 }, .extent = ctx.se },
        .clear_value_count = rpba.len,
        .p_clear_values = &rpba,
    }, .@"inline");
    vk.cmdSetViewport(ctx.cb[ctx.idx], 0, 1, &vk.Viewport{
        .x = 0,
        .y = 0,
        .width = @floatFromInt(ctx.se.width),
        .height = @floatFromInt(ctx.se.height),
        .min_depth = 0,
        .max_depth = 1,
    });
    vk.cmdSetScissor(ctx.cb[ctx.idx], 0, 1, &vk.Rect2D{
        .offset = .{ .x = 0, .y = 0 },
        .extent = ctx.se,
    });

    // Reset ring buffers each frame
    ctx.vb.cursor = 0;
    ctx.ib.cursor = 0;
    return true;
}

pub fn end_scene(ctx: *Context) void {
    vk.cmdEndRenderPass(ctx.cb[ctx.idx]);
    vk.endCommandBuffer(ctx.cb[ctx.idx]) catch {};
    vk.queueSubmit(ctx.q, 1, &vk.SubmitInfo{
        .s_type = .submit_info,
        .wait_semaphore_count = 1,
        .p_wait_semaphores = @ptrCast(&ctx.ia),
        .p_wait_dst_stage_mask = @ptrCast(&[_]vk.PipelineStageFlags{.{ .color_attachment_output_bit = true }}),
        .command_buffer_count = 1,
        .p_command_buffers = @ptrCast(&ctx.cb[ctx.idx]),
        .signal_semaphore_count = 1,
        .p_signal_semaphores = @ptrCast(&ctx.rf),
    }, ctx.f) catch {};
}

pub fn flip(ctx: *Context) !bool {
    var r: vk.Result = undefined;
    r = vk.queuePresentKHR(ctx.q, &vk.PresentInfoKHR{
        .s_type = .present_info_khr,
        .wait_semaphore_count = 1,
        .p_wait_semaphores = @ptrCast(&ctx.rf),
        .swapchain_count = 1,
        .p_swapchains = @ptrCast(&ctx.sc),
        .p_image_indices = @ptrCast(&ctx.idx),
        .p_results = null,
    }) catch return false;
    if (r == .error_out_of_date_khr or r == .suboptimal_khr) return false;
    if (r != .success) return error.VkPresentFailed;
    return true;
}

pub fn set_clear_color(ctx: *Context, color: vk.ClearColorValue) void {
    ctx.cc = color;
}

// ── Phase 3 public API ──────────────────────────────────────────

pub fn set_effect(ctx: *Context, effect_id: u32) void {
    _ = ctx;
    _ = effect_id;
}

fn create_pipeline_for_effect(d: vk.Device, dsl: vk.DescriptorSetLayout, rp: vk.RenderPass, effect_id: u32) !PipelineState {
    // Load SPIR-V shaders at runtime from the shaders directory.
    // The build step compiles GLSL → SPIR-V and installs them
    // alongside the executable.
    const al = std.heap.page_allocator;
    var vert_file = std.fs.cwd().openFile("shaders/ff.vert.spv", .{}) catch return error.ShaderNotFound;
    defer vert_file.close();
    const vert_code = try vert_file.readToEndAlloc(al, 1024 * 1024);
    defer al.free(vert_code);

    var frag_file = std.fs.cwd().openFile("shaders/ff.frag.spv", .{}) catch return error.ShaderNotFound;
    defer frag_file.close();
    const frag_code = try frag_file.readToEndAlloc(al, 1024 * 1024);
    defer al.free(frag_code);

    var vert_mod: vk.ShaderModule = undefined;
    try vk.createShaderModule(d, &vk.ShaderModuleCreateInfo{
        .s_type = .shader_module_create_info,
        .code_size = vert_code.len,
        .p_code = @ptrCast(@alignCast(vert_code.ptr)),
    }, null, &vert_mod);
    defer vk.destroyShaderModule(d, vert_mod, null);

    var frag_mod: vk.ShaderModule = undefined;
    try vk.createShaderModule(d, &vk.ShaderModuleCreateInfo{
        .s_type = .shader_module_create_info,
        .code_size = frag_code.len,
        .p_code = @ptrCast(@alignCast(frag_code.ptr)),
    }, null, &frag_mod);
    defer vk.destroyShaderModule(d, frag_mod, null);

    var pl: vk.Pipeline = undefined;
    const layout = try create_pipeline_layout(d, dsl);

    var blend = vk.PipelineColorBlendAttachmentState{
        .blend_enable = true,
        .src_color_blend_factor = .src_alpha,
        .dst_color_blend_factor = .one_minus_src_alpha,
        .color_blend_op = .add,
        .src_alpha_blend_factor = .one,
        .dst_alpha_blend_factor = .one_minus_src_alpha,
        .alpha_blend_op = .add,
        .color_write_mask = .{ .r_bit = true, .g_bit = true, .b_bit = true, .a_bit = true },
    };

    switch (effect_id) {
        2 => blend.blend_enable = false,
        4, 8 => {},  // alpha blend + alpha test (same as default for now)
        16 => {
            blend.src_color_blend_factor = .src_alpha;
            blend.dst_color_blend_factor = .one;
        },
        22 => {
            blend.src_color_blend_factor = .src_alpha;
            blend.dst_color_blend_factor = .one;
        },
        else => {},
    }

    try vk.createGraphicsPipelines(d, .null_handle, 1, &vk.GraphicsPipelineCreateInfo{
        .s_type = .graphics_pipeline_create_info,
        .stage_count = 2,
        .p_stages = &[_]vk.PipelineShaderStageCreateInfo{
            .{ .s_type = .pipeline_shader_stage_create_info, .stage = .vertex_bit, .module = vert_mod, .p_name = "main" },
            .{ .s_type = .pipeline_shader_stage_create_info, .stage = .fragment_bit, .module = frag_mod, .p_name = "main" },
        },
        .p_vertex_input_state = &vk.PipelineVertexInputStateCreateInfo{
            .s_type = .pipeline_vertex_input_state_create_info,
            .vertex_attribute_description_count = 4,
            .p_vertex_attribute_descriptions = &[_]vk.VertexInputAttributeDescription{
                .{ .location = 0, .binding = 0, .format = .r32g32b32_sfloat, .offset = 0 },
                .{ .location = 1, .binding = 0, .format = .r8g8b8a8_unorm, .offset = 12 },
                .{ .location = 2, .binding = 0, .format = .r8g8b8a8_unorm, .offset = 16 },
                .{ .location = 3, .binding = 0, .format = .r32g32_sfloat, .offset = 24 },
            },
            .vertex_binding_description_count = 1,
            .p_vertex_binding_descriptions = &vk.VertexInputBindingDescription{
                .binding = 0,
                .stride = 32,
                .input_rate = .vertex,
            },
        },
        .p_input_assembly_state = &vk.PipelineInputAssemblyStateCreateInfo{
            .s_type = .pipeline_input_assembly_state_create_info,
            .topology = .triangle_list,
        },
        .p_viewport_state = &vk.PipelineViewportStateCreateInfo{
            .s_type = .pipeline_viewport_state_create_info,
            .viewport_count = 1,
            .p_viewports = &vk.Viewport{ .x = 0, .y = 0, .width = 800, .height = 600, .min_depth = 0, .max_depth = 1 },
            .scissor_count = 1,
            .p_scissors = &vk.Rect2D{ .offset = .{ .x = 0, .y = 0 }, .extent = .{ .width = 800, .height = 600 } },
        },
        .p_rasterization_state = &vk.PipelineRasterizationStateCreateInfo{
            .s_type = .pipeline_rasterization_state_create_info,
            .polygon_mode = .fill,
            .cull_mode = .{},
            .front_face = .clockwise,
            .line_width = 1.0,
        },
        .p_multisample_state = &vk.PipelineMultisampleStateCreateInfo{
            .s_type = .pipeline_multisample_state_create_info,
            .rasterization_samples = .@"1_bit",
        },
        .p_color_blend_state = &vk.PipelineColorBlendStateCreateInfo{
            .s_type = .pipeline_color_blend_state_create_info,
            .logic_op_enable = false,
            .attachment_count = 1,
            .p_attachments = &blend,
        },
        .layout = layout,
        .render_pass = rp,
        .subpass = 0,
    }, null, &pl);

    return .{ .pl = pl, .layout = layout };
}

pub fn set_texture(ctx: *Context, tex_ptr: ?*anyopaque) void {
    _ = ctx;
    _ = tex_ptr;
}

pub fn create_texture(ctx: *Context, tex_ptr: ?*anyopaque, width: u32, height: u32, format: u32, mips: u32, data: ?[*]const u8, data_size: usize) bool {
    // Texture upload is a Phase 3 operation. Keep the Phase 2 renderer
    // surface-only and avoid exposing the unfinished upload path here.
    _ = ctx; _ = tex_ptr; _ = width; _ = height; _ = format; _ = mips; _ = data; _ = data_size;
    return false;
}

pub fn lock_vb(ctx: *Context, num_vertices: usize) ?[*]u8 {
    if (ctx.vb.cursor + num_vertices * 32 > ctx.vb.size) return null;
    const ptr = ctx.vb.ptr + ctx.vb.cursor;
    ctx.vb.cursor += num_vertices * 32;
    return ptr;
}
pub fn unlock_vb(ctx: *Context) void { _ = ctx; }

pub fn lock_ib(ctx: *Context, num_indices: usize) ?[*]u8 {
    if (ctx.ib.cursor + num_indices * 2 > ctx.ib.size) return null;
    const ptr = ctx.ib.ptr + ctx.ib.cursor;
    ctx.ib.cursor += num_indices * 2;
    return ptr;
}
pub fn unlock_ib(ctx: *Context) void { _ = ctx; }

pub fn draw_indexed(ctx: *Context, index_count: usize, vertex_base: usize, index_base: usize) void {
    _ = ctx; _ = index_count; _ = vertex_base; _ = index_base;
}

pub fn set_viewport(ctx: *Context, x: i32, y: i32, width: i32, height: i32) void {
    _ = ctx; _ = x; _ = y; _ = width; _ = height;
}

// ── Phase 3 internal helpers ──────────────────────────────────

fn create_ring(al: std.mem.Allocator, d: vk.Device, pd: vk.PhysicalDevice, size: usize) !RingBuffer {
    _ = al;
    var rb: RingBuffer = .{ .buf = .null_handle, .mem = .null_handle, .ptr = undefined, .size = size, .cursor = 0, .lock_base = 0 };
    try vk.createBuffer(d, &vk.BufferCreateInfo{
        .s_type = .buffer_create_info,
        .size = @intCast(size),
        .usage = .{ .vertex_buffer_bit = true, .index_buffer_bit = true, .transfer_src_bit = true },
        .sharing_mode = .exclusive,
    }, null, &rb.buf);
    const mem_reqs = vk.getBufferMemoryRequirements(d, rb.buf);
    const mem_type_idx = try find_mem_type_filter(pd, mem_reqs.memory_type_bits, .{ .host_visible_bit = true, .host_coherent_bit = true });
    try vk.allocateMemory(d, &vk.MemoryAllocateInfo{
        .s_type = .memory_allocate_info,
        .allocation_size = mem_reqs.size,
        .memory_type_index = mem_type_idx,
    }, null, &rb.mem);
    try vk.bindBufferMemory(d, rb.buf, rb.mem, 0);
    try vk.mapMemory(d, rb.mem, 0, mem_reqs.size, .{}, &rb.ptr, null);
    return rb;
}

fn create_upload_cmds(d: vk.Device, qf: u32) !struct { vk.CommandPool, vk.CommandBuffer } {
    var cp: vk.CommandPool = undefined;
    try vk.createCommandPool(d, &vk.CommandPoolCreateInfo{
        .s_type = .command_pool_create_info,
        .queue_family_index = qf,
        .flags = .{ .transient_bit = true },
    }, null, &cp);
    var cb: vk.CommandBuffer = undefined;
    try vk.allocateCommandBuffers(d, &vk.CommandBufferAllocateInfo{
        .s_type = .command_buffer_allocate_info,
        .command_pool = cp,
        .command_buffer_count = 1,
        .p_command_buffers = &cb,
    }, null);
    return .{ cp, cb };
}

fn create_texture_img(d: vk.Device, pd: vk.PhysicalDevice, width: u32, height: u32, _format: u32) !struct { vk.Image, vk.DeviceMemory, vk.Buffer, vk.DeviceMemory, [*]u8, vk.ImageView, vk.Sampler } {
    _ = _format;
    var img: vk.Image = undefined;
    try vk.createImage(d, &vk.ImageCreateInfo{
        .s_type = .image_create_info,
        .image_type = .@"2d",
        .format = .b8g8r8a8_unorm,
        .extent = .{ .width = width, .height = height, .depth = 1 },
        .mip_levels = 1,
        .array_layers = 1,
        .initial_layout = .undefined,
        .samples = .{ .@"1_bit" = true },
        .tiling = .optimal,
        .usage = .{ .transfer_dst_bit = true, .sampled_bit = true },
        .sharing_mode = .exclusive,
    }, null, &img);
    const mem_reqs = vk.getImageMemoryRequirements(d, img);
    const mem_type_idx = try find_mem_type_filter(pd, mem_reqs.memory_type_bits, .{ .device_local_bit = true });
    var mem: vk.DeviceMemory = undefined;
    try vk.allocateMemory(d, &vk.MemoryAllocateInfo{
        .s_type = .memory_allocate_info,
        .allocation_size = mem_reqs.size,
        .memory_type_index = mem_type_idx,
    }, null, &mem);
    try vk.bindImageMemory(d, img, mem, 0);

    var view: vk.ImageView = undefined;
    try vk.createImageView(d, &vk.ImageViewCreateInfo{
        .s_type = .image_view_create_info,
        .image = img,
        .view_type = .@"2d",
        .components = .{ .r = .identity, .g = .identity, .b = .identity, .a = .identity },
        .format = .b8g8r8a8_unorm,
        .subresource_range = .{
            .aspect_mask = .{ .color_bit = true },
            .base_mip_level = 0,
            .level_count = 1,
            .base_array_layer = 0,
            .layer_count = 1,
        },
    }, null, &view);

    var sampler: vk.Sampler = undefined;
    try vk.createSampler(d, &vk.SamplerCreateInfo{
        .s_type = .sampler_create_info,
        .mag_filter = .nearest,
        .min_filter = .nearest,
        .mipmap_mode = .nearest,
        .mip_lod_bias = 0,
        .address_mode_u = .repeat,
        .address_mode_v = .repeat,
        .address_mode_w = .repeat,
        .anisotropy_enable = .false,
        .max_anisotropy = 1,
        .compare_enable = .false,
        .compare_op = .never,
        .min_lod = 0,
        .max_lod = 0,
        .border_color = .float_transparent_black,
        .unnormalized_coordinates = .false,
    }, null, &sampler);

    const buf_size = @as(u64, width) * @as(u64, height) * 4;
    var staging: vk.Buffer = undefined;
    try vk.createBuffer(d, &vk.BufferCreateInfo{
        .s_type = .buffer_create_info,
        .size = buf_size,
        .usage = .{ .transfer_src_bit = true },
        .sharing_mode = .exclusive,
    }, null, &staging);
    const st_mem_reqs = vk.getBufferMemoryRequirements(d, staging);
    const st_mem_type = try find_mem_type_filter(pd, st_mem_reqs.memory_type_bits, .{ .host_visible_bit = true, .host_coherent_bit = true });
    var st_mem: vk.DeviceMemory = undefined;
    try vk.allocateMemory(d, &vk.MemoryAllocateInfo{
        .s_type = .memory_allocate_info,
        .allocation_size = st_mem_reqs.size,
        .memory_type_index = st_mem_type,
    }, null, &st_mem);
    try vk.bindBufferMemory(d, staging, st_mem, 0);
    var stage_ptr: [*]u8 = undefined;
    try vk.mapMemory(d, st_mem, 0, st_mem_reqs.size, .{}, &stage_ptr, null);

    return .{ img, mem, staging, st_mem, stage_ptr, view, sampler };
}

fn create_pipeline_layout(d: vk.Device, dsl: vk.DescriptorSetLayout) !vk.PipelineLayout {
    var pl: vk.PipelineLayout = undefined;
    try vk.createPipelineLayout(d, &vk.PipelineLayoutCreateInfo{
        .s_type = .pipeline_layout_create_info,
        .set_layout_count = 1,
        .p_set_layouts = &dsl,
        .push_constant_range_count = 1,
        .p_push_constant_ranges = &vk.PushConstantRange{
            .s_type = .push_constant_range,
            .stage_flags = .{ .vertex_bit = true, .fragment_bit = true },
            .offset = 0,
            .size = @sizeOf(PushConsts),
        },
    }, null, &pl);
    return pl;
}
