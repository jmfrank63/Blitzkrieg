const c = @import("c");
const errors = @import("errors.zig");
const std = @import("std");

/// The normalized factor used to multiply pixel components.
///
/// ## Remarks
/// The blend factors are multiplied with the pixels from a drawing operation (source),
/// and the pixels from the render target (destination) before the blend operation.
/// The comma-separated factors listed above are always applied in the component order red, green, blue, and alpha.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const Factor = enum(c.SDL_BlendFactor) {
    /// (0, 0, 0, 0)
    zero = c.SDL_BLENDFACTOR_ZERO,
    /// (1, 1, 1, 1)
    one = c.SDL_BLENDFACTOR_ONE,
    /// (r, g, b, a)
    source_color = c.SDL_BLENDFACTOR_SRC_COLOR,
    /// (1-r, 1-g, 1-b, 1-a)
    one_minus_source_color = c.SDL_BLENDFACTOR_ONE_MINUS_SRC_COLOR,
    /// (a, a, a, a)
    soure_alpha = c.SDL_BLENDFACTOR_SRC_ALPHA,
    /// (1-a, 1-a, 1-a, 1-a)
    one_minus_source_alpha = c.SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
    /// (r, g, b, a)
    destination_color = c.SDL_BLENDFACTOR_DST_COLOR,
    /// (1-r, 1-g, 1-b, 1-a)
    one_minus_destination_color = c.SDL_BLENDFACTOR_ONE_MINUS_DST_COLOR,
    /// (a, a, a, a)
    destination_alpha = c.SDL_BLENDFACTOR_DST_ALPHA,
    /// (1-a, 1-a, 1-a, 1-a)
    one_minus_destination_alpha = c.SDL_BLENDFACTOR_ONE_MINUS_DST_ALPHA,
};

/// The blend operation used when combining source and destination pixel components.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const Operation = enum(c.SDL_BlendOperation) {
    /// Destination + Source. Supported by all renderers.
    add = c.SDL_BLENDOPERATION_ADD,
    /// Source - Destination. Supported by D3D, OpenGL, OpenGLES, and Vulkan.
    sub = c.SDL_BLENDOPERATION_SUBTRACT,
    /// Destination - Source. Supported by D3D, OpenGL, OpenGLES, and Vulkan.
    rev_sub = c.SDL_BLENDOPERATION_REV_SUBTRACT,
    /// Min(Destination, Source). Supported by D3D, OpenGL, OpenGLES, and Vulkan.
    min = c.SDL_BLENDOPERATION_MINIMUM,
    /// Max(Destination, Source). Supported by D3D, OpenGL, OpenGLES, and Vulkan.
    max = c.SDL_BLENDOPERATION_MAXIMUM,
};

/// A set of blend modes used in drawing operations.
///
/// ## Remarks
/// These predefined blend modes are supported everywhere.
///
/// Additional values may be obtained from `blend_mode.Mode.custom()`.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub const Mode = enum(c.SDL_BlendMode) {
    /// Destination = Source.
    none = c.SDL_BLENDMODE_NONE,
    /// DestinationRGB = (SourceRGB * SourceA) + (DestinationRGB * (1-SourceA)), DestinationA = SourceA + (DestinationA * (1-SourceA)).
    blend = c.SDL_BLENDMODE_BLEND,
    /// DestinationRGBA = SourceRGBA + (DestinationRGBA * (1-SourceA)).
    blend_premultiplied = c.SDL_BLENDMODE_BLEND_PREMULTIPLIED,
    /// DestinationRGB = (SourceRGB * SourceA) + DestinationRGB, DestinationA = DestinationA.
    add = c.SDL_BLENDMODE_ADD,
    /// DestinationRGB = SourceRGB + DestinationRGB, DestinationA = DestinationA.
    add_premultiplied = c.SDL_BLENDMODE_ADD_PREMULTIPLIED,
    /// DestinationRGB = SourceRGB * DestinationRGB, DestinationA = DestinationA.
    mod = c.SDL_BLENDMODE_MOD,
    /// DestinationRGB = (SourceRGB * DestinationRGB) + (DestinationRGB * (1-SourceA)), DestinationA = DestinationA.
    mul = c.SDL_BLENDMODE_MUL,
    _,

    /// Convert from SDL. Returns null if invalid.
    pub fn fromSdl(val: c.SDL_BlendMode) ?Mode {
        if (val == c.SDL_BLENDMODE_INVALID)
            return null;
        return @enumFromInt(val);
    }

    /// Convert to SDL.
    pub fn toSdl(val: ?Mode) c.SDL_BlendMode {
        if (val) |id| {
            return @intFromEnum(id);
        }
        return c.SDL_BLENDMODE_INVALID;
    }

    /// Compose a custom blend mode for renderers.
    ///
    /// ## Function Parameters
    /// * `srcRgb`: The `blend.Factor` applied to the red, green, and blue components of the source pixels.
    /// * `dstRgb`: The `blend.Factor` applied to the red, green, and blue components of the destination pixels.
    /// * `rgbOp`: The `blend.Operation` used to combine the red, green, and blue components of the source and destination pixels.
    /// * `srcAlpha`: The `blend.Factor` applied to the alpha component of the source pixels.
    /// * `dstAlpha`: The `blend.Factor` applied to the alpha component of the destination pixels.
    /// * `alphaOp`: The `blend.Operation` used to combine the alpha component of the source and destination pixels.
    ///
    /// ## Return Value
    /// Returns a `blend_mode.Mode` that represents the chosen factors and operations.
    ///
    /// ## Remarks
    /// The functions `render.Renderer.setDrawBlendMode()` and `render.Texture.setBlendMode()`
    /// accept the `blend_mode.Mode` returned by this function if the renderer supports it.
    ///
    /// A blend mode controls how the pixels from a drawing operation (source) get combined with the pixels from the render target (destination).
    /// First, the components of the source and destination pixels get multiplied with their blend factors.
    /// Then, the blend operation takes the two products and calculates the result that will get stored in the render target.
    ///
    /// Expressed in pseudocode, it would look like this:
    /// * `dstRGB = colorOperation(srcRGB * srcColorFactor, dstRGB * dstColorFactor);`
    /// * `dstA = alphaOperation(srcA * srcAlphaFactor, dstA * dstAlphaFactor);`
    ///
    /// Where the functions `colorOperation(src, dst)` and `alphaOperation(src, dst)` can return one of the following:
    /// * `src + dst`
    /// * `src - dst`
    /// * `dst - src`
    /// * `min(src, dst)`
    /// * `max(src, dst)`
    ///
    /// The red, green, and blue components are always multiplied with the first, second, and third components of the `blend_mode.Factor`, respectively.
    /// The fourth component is not used.
    ///
    /// The alpha component is always multiplied with the fourth component of the `blend_mode.Factor`.
    /// The other components are not used in the alpha calculation.
    ///
    /// Support for these blend modes varies for each renderer.
    /// To check if a specific `blend_mode.Mode` is supported, create a renderer and pass it to either
    /// `render.Renderer.setDrawBlendMode()` or `render.Texture.setBlendMode()`.
    /// They will return with an error if the blend mode is not supported.
    ///
    /// This list describes the support of custom blend modes for each renderer.
    /// All renderers support the four blend modes listed in the `blend_mode.Mode` enumeration.
    /// * direct3d: Supports all operations with all factors. However, some factors produce unexpected results with `blend_mode.Operation.min` and `blend_mode.Operation.max`.
    /// * direct3d11: Same as Direct3D 9.
    /// * opengl: Supports the `blend_mode.Operation.add` operation with all factors. OpenGL versions 1.1, 1.2, and 1.3 do not work correctly here.
    /// * opengles2: Supports the `blend_mode.Operation.add`, `blend_mode.Operation.sub`, `blend_mode.Operation.rev_sub` operations with all factors.
    /// * psp: No custom blend mode support.
    /// * software: No custom blend mode support.
    ///
    /// Some renderers do not provide an alpha component for the default render target.
    /// The `blend_mode.Factor.destination_alpha` and `blend_mode.Factor.one_minus_destination_alpha` factors do not have an effect in this case.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn custom(
        srcRgb: Factor,
        dstRgb: Factor,
        rgbOp: Operation,
        srcAlpha: Factor,
        dstAlpha: Factor,
        alphaOp: Operation,
    ) ?Mode {
        const ret = c.SDL_ComposeCustomBlendMode(
            @intFromEnum(srcRgb),
            @intFromEnum(dstRgb),
            @intFromEnum(rgbOp),
            @intFromEnum(srcAlpha),
            @intFromEnum(dstAlpha),
            @intFromEnum(alphaOp),
        );
        return fromSdl(ret);
    }
};

// Test blend mode creation.
test "Blend Mode" {
    errors.refAllDeclsRecursive(@This());

    try std.testing.expect(Mode.custom(
        .source_color,
        .destination_color,
        .add,
        .soure_alpha,
        .destination_alpha,
        .add,
    ) != null);
}
