/// Number of samplers.
samplers: u32,
/// Number of storage textures.
storage_textures: u32,
/// Number of storage buffers.
storage_buffers: u32,
/// Number of uniform buffers.
uniform_buffers: u32,
/// Input variables.
inputs: []const Var,
/// Output variables.
outputs: []const Var,

/// Variable.
pub const Var = struct {
    /// Name of the variable.
    name: []const u8,
    /// Type of the variable.
    type: VarType,
    /// Location of the variable (binding location).
    location: u32,
};

/// Variable type.
pub const VarType = enum {
    byte,
    byte2,
    byte3,
    byte4,
    ubyte,
    ubyte2,
    ubyte3,
    ubyte4,
    short,
    short2,
    short3,
    short4,
    ushort,
    ushort2,
    ushort3,
    ushort4,
    int,
    int2,
    int3,
    int4,
    uint,
    uint2,
    uint3,
    uint4,
    long,
    long2,
    long3,
    long4,
    ulong,
    ulong2,
    ulong3,
    ulong4,
    half,
    half2,
    half3,
    half4,
    float,
    float2,
    float3,
    float4,
    double,
    double2,
    double3,
    double4,

    /// How many elements make up the type.
    ///
    /// ## Function Parameters
    /// * `self`: The variable type.
    ///
    /// ## Return Value
    /// Returns the length of the metadata.
    pub fn length(
        self: VarType,
    ) usize {
        return switch (self) {
            .byte, .ubyte, .short, .ushort, .int, .uint, .long, .ulong, .half, .float, .double => 1,
            .byte2, .ubyte2, .short2, .ushort2, .int2, .uint2, .long2, .ulong2, .half2, .float2, .double2 => 2,
            .byte3, .ubyte3, .short3, .ushort3, .int3, .uint3, .long3, .ulong3, .half3, .float3, .double3 => 3,
            .byte4, .ubyte4, .short4, .ushort4, .int4, .uint4, .long4, .ulong4, .half4, .float4, .double4 => 4,
        };
    }

    /// Get the zig type of the variable type.
    ///
    /// ## Function Parameters
    /// * `self`: The variable type.
    ///
    /// ## Return Value
    /// Returns the zig type representing the variable type.
    pub fn zigType(
        comptime self: VarType,
    ) type {
        return switch (self) {
            .byte => i8,
            .byte2, .byte3, .byte4 => [self.length()]i8,
            .ubyte => u8,
            .ubyte2, .ubyte3, .ubyte4 => [self.length()]u8,
            .short => i16,
            .short2, .short3, .short4 => [self.length()]i16,
            .ushort => u16,
            .ushort2, .ushort3, .ushort4 => [self.length()]u16,
            .int => i32,
            .int2, .int3, .int4 => [self.length()]i32,
            .uint => u32,
            .uint2, .uint3, .uint4 => [self.length()]u32,
            .long => i64,
            .long2, .long3, .long4 => [self.length()]i64,
            .ulong => u64,
            .ulong2, .ulong3, .ulong4 => [self.length()]u64,
            .half => f16,
            .half2, .half3, .half4 => [self.length()]f16,
            .float => f32,
            .float2, .float3, .float4 => [self.length()]f32,
            .double => f64,
            .double2, .double3, .double4 => [self.length()]f64,
        };
    }
};
