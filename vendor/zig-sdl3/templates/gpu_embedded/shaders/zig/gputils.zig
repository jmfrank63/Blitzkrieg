/// Create a 2d sampler.
///
/// ## Function Parameters
/// * `set`: The descriptor set.
/// * `bind`: The binding slot.
///
/// ## Return Value
/// The 2d sampler object.
pub fn Sampler2d(
    comptime set: u32,
    comptime bind: u32,
) type {
    return struct {
        /// Get the texture size of a 2d sampler.
        ///
        /// ## Function Parameters
        /// * `lod`: The LOD to sample at.
        ///
        /// ## Return Value
        /// Returns the sampler texture size.
        pub fn size(
            lod: i32,
        ) @Vector(2, i32) {
            return asm volatile (
                \\                  OpCapability ImageQuery
                \\%float          = OpTypeFloat 32
                \\%int            = OpTypeInt 32 1
                \\%v2int          = OpTypeVector %int 2
                \\%img_type       = OpTypeImage %float 2D 0 0 0 1 Unknown
                \\%sampler_type   = OpTypeSampledImage %img_type
                \\%sampler_ptr    = OpTypePointer UniformConstant %sampler_type
                \\%tex            = OpVariable %sampler_ptr UniformConstant
                \\                  OpDecorate %tex DescriptorSet $set
                \\                  OpDecorate %tex Binding $bind
                \\%loaded_sampler = OpLoad %sampler_type %tex
                \\%loaded_image   = OpImage %img_type %loaded_sampler
                \\%ret            = OpImageQuerySizeLod %v2int %loaded_image %lod
                : [ret] "" (-> @Vector(2, i32)),
                : [set] "c" (set),
                  [bind] "c" (bind),
                  [lod] "" (lod),
            );
        }

        /// Sample the 2d sampler at a given UV.
        ///
        /// ## Function Parameters
        /// * `uv`: The UV to sample at.
        ///
        /// ## Return Value
        /// Returns the sampled color value.
        pub fn texture(
            uv: @Vector(2, f32),
        ) @Vector(4, f32) {
            return asm volatile (
                \\%float          = OpTypeFloat 32
                \\%v4float        = OpTypeVector %float 4
                \\%img_type       = OpTypeImage %float 2D 0 0 0 1 Unknown
                \\%sampler_type   = OpTypeSampledImage %img_type
                \\%sampler_ptr    = OpTypePointer UniformConstant %sampler_type
                \\%tex            = OpVariable %sampler_ptr UniformConstant
                \\                  OpDecorate %tex DescriptorSet $set
                \\                  OpDecorate %tex Binding $bind
                \\%loaded_sampler = OpLoad %sampler_type %tex
                \\%ret            = OpImageSampleImplicitLod %v4float %loaded_sampler %uv
                : [ret] "" (-> @Vector(4, f32)),
                : [uv] "" (uv),
                  [set] "c" (set),
                  [bind] "c" (bind),
            );
        }

        /// Sample a 2d sampler at a given UV.
        ///
        /// ## Function Parameters
        /// * `uv`: The UV to sample at.
        /// * `lod`: The LOD to sample with.
        ///
        /// ## Return Value
        /// Returns the sampled color value.
        pub fn textureLod(
            uv: @Vector(2, f32),
            lod: f32,
        ) @Vector(4, f32) {
            return asm volatile (
                \\%float          = OpTypeFloat 32
                \\%v4float        = OpTypeVector %float 4
                \\%img_type       = OpTypeImage %float 2D 0 0 0 1 Unknown
                \\%sampler_type   = OpTypeSampledImage %img_type
                \\%sampler_ptr    = OpTypePointer UniformConstant %sampler_type
                \\%tex            = OpVariable %sampler_ptr UniformConstant
                \\                  OpDecorate %tex DescriptorSet $set
                \\                  OpDecorate %tex Binding $bind
                \\%loaded_sampler = OpLoad %sampler_type %tex
                \\%ret            = OpImageSampleExplicitLod %v4float %loaded_sampler %uv Lod %lod
                : [ret] "" (-> @Vector(4, f32)),
                : [uv] "" (uv),
                  [lod] "" (lod),
                  [set] "c" (set),
                  [bind] "c" (bind),
            );
        }
    };
}

/// Create a 2d texture in RGBA8 format.
///
/// ## Function Parameters
/// * `set`: The descriptor set.
/// * `bind`: The binding slot.
///
/// ## Return Value
/// The 2d RGBA8 texture object.
pub fn Texture2dRgba8(
    comptime set: u32,
    comptime bind: u32,
) type {
    return struct {
        /// Get the texture size of a 2d RGBA8 texture.
        ///
        /// ## Return Value
        /// Returns the texture size.
        pub fn size() @Vector(2, i32) {
            return asm volatile (
                \\                  OpCapability ImageQuery
                \\%float          = OpTypeFloat 32
                \\%int            = OpTypeInt 32 1
                \\%v2int          = OpTypeVector %int 2
                \\%img_type       = OpTypeImage %float 2D 0 0 0 2 Rgba8
                \\%img_ptr        = OpTypePointer UniformConstant %img_type
                \\%img            = OpVariable %img_ptr UniformConstant
                \\                  OpDecorate %img DescriptorSet $set
                \\                  OpDecorate %img Binding $bind
                \\%loaded_image   = OpLoad %img_type %img
                \\%ret            = OpImageQuerySize %v2int %loaded_image
                : [ret] "" (-> @Vector(2, i32)),
                : [set] "c" (set),
                  [bind] "c" (bind),
            );
        }

        /// Store to a 2d RGBA8 texture.
        ///
        /// ## Function Parameters
        /// * `uv`: The UV to store to.
        /// * `pixel`: The pixel data to store.
        pub fn store(
            uv: @Vector(2, u32),
            pixel: @Vector(4, f32),
        ) void {
            asm volatile (
                \\%float          = OpTypeFloat 32
                \\%v4float        = OpTypeVector %float 4
                \\%img_type       = OpTypeImage %float 2D 0 0 0 2 Rgba8
                \\%img_ptr        = OpTypePointer UniformConstant %img_type
                \\%img            = OpVariable %img_ptr UniformConstant
                \\                  OpDecorate %img DescriptorSet $set
                \\                  OpDecorate %img Binding $bind
                \\%loaded_image   = OpLoad %img_type %img
                \\                  OpImageWrite %loaded_image %uv %pixel
                :
                : [uv] "" (uv),
                  [pixel] "" (pixel),
                  [set] "c" (set),
                  [bind] "c" (bind),
            );
        }
    };
}
