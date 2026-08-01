const c = @import("c");
const errors = @import("errors.zig");
const std = @import("std");

/// A guess for the cacheline size used for padding.
///
/// ## Remarks
/// Most x86 processors have a 64 byte cache line.
/// The 64-bit PowerPC processors have a 128 byte cache line.
/// We use the larger value to be generally safe.
///
/// ## Version
/// This macro is available since SDL 3.2.0.
pub const cacheline_size = c.SDL_CACHELINE_SIZE;

/// Determine the L1 cache line size of the CPU.
///
/// ## Return Value
/// Returns the L1 cache line size of the CPU, in bytes.
///
/// ## Remarks
/// This is useful for determining multi-threaded structure padding or SIMD prefetch sizes.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getCacheLineSize() usize {
    return @intCast(c.SDL_GetCPUCacheLineSize());
}

/// Get the number of logical CPU cores available.
///
/// ## Return Value
/// Returns the total number of logical CPU cores.
/// On CPUs that include technologies such as hyperthreading, the number of logical cores may be more than the number of physical cores.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getNumLogicalCores() usize {
    return @intCast(c.SDL_GetNumLogicalCPUCores());
}

/// Report the alignment this system needs for SIMD allocations.
///
/// ## Return Value
/// Returns the alignment in bytes needed for available, known SIMD instructions.
///
/// ## Remarks
/// This will return the minimum number of bytes to which a pointer must be aligned to be compatible with SIMD instructions on the current machine.
/// For example, if the machine supports SSE only, it will return 16, but if it supports AVX-512F, it'll return 64 (etc).
/// This only reports values for instruction sets SDL knows about, so if your SDL build doesn't have `cpu_info.hasAvx512F()`,
/// then it might return 16 for the SSE support it sees and not 64 for the AVX-512 instructions that exist but SDL doesn't know about.
/// Plan accordingly.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getSimdAlignment() usize {
    return c.SDL_GetSIMDAlignment();
}

/// Report the size of a page of memory.
///
/// ## Return Value
/// Returns the size of a single page of memory, in bytes, or `null` if SDL can't determine this information.
///
/// ## Remarks
/// Different platforms might have different memory page sizes.
/// In current times, 4 kilobytes is not unusual, but newer systems are moving to larger page sizes, and esoteric platforms might have any unexpected size.
///
/// If `null` is returned, defaulting to `4096` is often a reasonable option.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.4.0.
pub fn getSystemPageSize() ?usize {
    const ret = c.SDL_GetSystemPageSize();
    if (ret == 0)
        return null;
    return @intCast(ret);
}

/// Get the amount of RAM configured in the system.
///
/// ## Return Value
/// Returns the amount of RAM configured in the system in MiB.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getSystemRam() usize {
    return @intCast(c.SDL_GetSystemRAM());
}

/// Determine whether the CPU has AltiVec features.
///
/// ## Return Value
/// Returns true if the CPU has AltiVec features or false if not.
///
/// ## Remarks
/// This always returns false on CPUs that aren't using PowerPC instruction sets.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn hasAltiVec() bool {
    return c.SDL_HasAltiVec();
}

/// Determine whether the CPU has ARM SIMD (ARMv6) features.
///
/// ## Return Value
/// Returns true if the CPU has ARM SIMD features or false if not.
///
/// ## Remarks
/// This is different from ARM NEON, which is a different instruction set.
///
/// This always returns false on CPUs that aren't using ARM instruction sets.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn hasArmSimd() bool {
    return c.SDL_HasARMSIMD();
}

/// Determine whether the CPU has AVX features.
///
/// ## Return Value
/// Returns true if the CPU has AVX features or false if not.
///
/// ## Remarks
/// This always returns false on CPUs that aren't using Intel instruction sets.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn hasAvx() bool {
    return c.SDL_HasAVX();
}

/// Determine whether the CPU has AVX2 features.
///
/// ## Return Value
/// Returns true if the CPU has AVX2 features or false if not.
///
/// ## Remarks
/// This always returns false on CPUs that aren't using Intel instruction sets.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn hasAvx2() bool {
    return c.SDL_HasAVX2();
}

/// Determine whether the CPU has AVX-512F (foundation) features.
///
/// ## Return Value
/// Returns true if the CPU has AVX-512F features or false if not.
///
/// ## Remarks
/// This always returns false on CPUs that aren't using Intel instruction sets.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn hasAvx512F() bool {
    return c.SDL_HasAVX512F();
}

/// Determine whether the CPU has LASX (LOONGARCH SIMD) features.
///
/// ## Return Value
/// Returns true if the CPU has LOONGARCH LASX features or false if not.
///
/// ## Remarks
/// This always returns false on CPUs that aren't using LOONGARCH instruction sets.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn hasLasx() bool {
    return c.SDL_HasLASX();
}

/// Determine whether the CPU has LSX (LOONGARCH SIMD) features.
///
/// ## Return Value
/// Returns true if the CPU has LOONGARCH LSX features or false if not.
///
/// ## Remarks
/// This always returns false on CPUs that aren't using LOONGARCH instruction sets.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn hasLsx() bool {
    return c.SDL_HasLSX();
}

/// Determine whether the CPU has MMX features.
///
/// ## Return Value
/// Returns true if the CPU has MMX features or false if not.
///
/// ## Remarks
/// This always returns false on CPUs that aren't using Intel instruction sets.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn hasMmx() bool {
    return c.SDL_HasMMX();
}

/// Determine whether the CPU has NEON (ARM SIMD) features.
///
/// ## Return Value
/// Returns true if the CPU has ARM NEON features or false if not.
///
/// ## Remarks
/// This always returns false on CPUs that aren't using ARM instruction sets.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn hasNeon() bool {
    return c.SDL_HasNEON();
}

/// Determine whether the CPU has SSE features.
///
/// ## Return Value
/// Returns true if the CPU has SSE features or false if not.
///
/// ## Remarks
/// This always returns false on CPUs that aren't using Intel instruction sets.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn hasSse() bool {
    return c.SDL_HasSSE();
}

/// Determine whether the CPU has SSE2 features.
///
/// ## Return Value
/// Returns true if the CPU has SSE2 features or false if not.
///
/// ## Remarks
/// This always returns false on CPUs that aren't using Intel instruction sets.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn hasSse2() bool {
    return c.SDL_HasSSE2();
}

/// Determine whether the CPU has SSE3 features.
///
/// ## Return Value
/// Returns true if the CPU has SSE3 features or false if not.
///
/// ## Remarks
/// This always returns false on CPUs that aren't using Intel instruction sets.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn hasSse3() bool {
    return c.SDL_HasSSE3();
}

/// Determine whether the CPU has SSE4.1 features.
///
/// ## Return Value
/// Returns true if the CPU has SSE4.1 features or false if not.
///
/// ## Remarks
/// This always returns false on CPUs that aren't using Intel instruction sets.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn hasSse41() bool {
    return c.SDL_HasSSE41();
}

/// Determine whether the CPU has SSE4.2 features.
///
/// ## Return Value
/// Returns true if the CPU has SSE4.2 features or false if not.
///
/// ## Remarks
/// This always returns false on CPUs that aren't using Intel instruction sets.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn hasSse42() bool {
    return c.SDL_HasSSE42();
}

// CPU testing.
test "CPU Info" {
    errors.refAllDeclsRecursive(@This());

    _ = cacheline_size;
    _ = getCacheLineSize();
    _ = getNumLogicalCores();
    _ = getSimdAlignment();
    _ = getSystemRam();
    _ = hasAltiVec();
    _ = hasArmSimd();
    _ = hasAvx();
    _ = hasAvx2();
    _ = hasAvx512F();
    _ = hasLasx();
    _ = hasLsx();
    _ = hasMmx();
    _ = hasNeon();
    _ = hasSse();
    _ = hasSse2();
    _ = hasSse3();
    _ = hasSse41();
    _ = hasSse42();
}
