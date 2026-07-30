const c = @import("c");
const errors = @import("errors.zig");
const std = @import("std");

/// A type representing an atomic integer value.
///
/// ## Remarks
/// This can be used to manage a value that is synchronized across multiple CPUs without a race condition; when an app sets a value with `atomic.Int.set()` all other threads,
/// regardless of the CPU it is running on, will see that value when retrieved with `atomic.Int.get()`, regardless of CPU caches, etc.
///
/// This is also useful for atomic compare-and-swap operations: a thread can change the value as long as its current value matches expectations.
/// When done in a loop, one can guarantee data consistency across threads without a lock (but the usual warnings apply: if you don't know what you're doing,
/// or you don't do it carefully, you can confidently cause any number of disasters with this, so in most cases, you should use a mutex instead of this!).
///
/// This is a struct so people don't accidentally use numeric operations on it directly.
/// You have to use SDL atomic functions.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Int = extern struct {
    value: c.SDL_AtomicInt,

    // Size tests.
    comptime {
        std.debug.assert(@sizeOf(c.SDL_AtomicInt) == @sizeOf(Int));
    }

    /// Add to an atomic variable.
    ///
    /// ## Function Parameters
    /// * `self`: The atomic int to be modified.
    /// * `v`: The desired value to add.
    ///
    /// ## Return Value
    /// Returns the previous value of the atomic variable.
    ///
    /// ## Remarks
    /// This function also acts as a full memory barrier.
    ///
    /// Note: If you don't know what this function is for, you shouldn't use it!
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn add(
        self: *Int,
        v: c_int,
    ) c_int {
        return c.SDL_AddAtomicInt(&self.value, v);
    }

    /// Set an atomic variable to a new value if it is currently an old value.
    ///
    /// ## Function Parameters
    /// * `self`: A pointer to a variable to be modified.
    /// * `old_val`: The old value.
    /// * `new_val`: The new value.
    ///
    /// ## Return Value
    /// Returns true if the atomic variable was set, false otherwise.
    ///
    /// ## Remarks
    /// Note: If you don't know what this function is for, you shouldn't use it!
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn compareAndSwap(
        self: *Int,
        old_val: c_int,
        new_val: c_int,
    ) bool {
        return c.SDL_CompareAndSwapAtomicInt(&self.value, old_val, new_val);
    }

    /// Decrement an atomic variable used as a reference count.
    ///
    /// ## Function Parameters
    /// * `self`: The int to decrement.
    ///
    /// ## Return Value
    /// Returns true if the variable reached zero after decrementing, false otherwise.
    ///
    /// ## Remarks
    /// Note: If you don't know what this function is for, you shouldn't use it!
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn decRef(
        self: *Int,
    ) bool {
        return c.SDL_AtomicDecRef(&self.value);
    }

    /// Get the value of an atomic variable.
    ///
    /// ## Function Parameters
    /// * `self`: A pointer to an atomic value.
    ///
    /// ## Return Value
    /// Returns the current value of an atomic variable.
    ///
    /// ## Remarks
    /// Note: If you don't know what this function is for, you shouldn't use it!
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn get(
        self: *Int,
    ) c_int {
        return c.SDL_GetAtomicInt(&self.value);
    }

    /// Increment an atomic variable used as a reference count.
    ///
    /// ## Function Parameters
    /// * `self`: The int to decrement.
    ///
    /// ## Return Value
    /// Returns the previous value of the atomic variable.
    ///
    /// ## Remarks
    /// Note: If you don't know what this function is for, you shouldn't use it!
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn incRef(
        self: *Int,
    ) c_int {
        return c.SDL_AtomicIncRef(&self.value);
    }

    /// Set an atomic variable to a value.
    ///
    /// ## Function Parameters
    /// * `self`: The variable to be modified.
    /// * `v`: The desired value.
    ///
    /// ## Return Value
    /// Returns the previous value of the atomic variable.
    ///
    /// ## Remarks
    /// This function also acts as a full memory barrier.
    ///
    /// Note: If you don't know what this function is for, you shouldn't use it!
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn set(
        self: *Int,
        v: c_int,
    ) c_int {
        return c.SDL_SetAtomicInt(&self.value, v);
    }
};

/// An atomic spinlock.
///
/// The atomic locks are efficient spinlocks using CPU instructions, but are vulnerable to starvation and can spin forever if a thread holding a lock has been terminated.
/// For this reason you should minimize the code executed inside an atomic lock and never do expensive things like API or system calls while holding them.
///
/// They are also vulnerable to starvation if the thread holding the lock is lower priority than other threads and doesn't get scheduled.
/// In general you should use mutexes instead, since they have better performance and contention behavior.
///
/// The atomic locks are not safe to lock recursively.
///
/// Porting Note: The spin lock functions and type are required and can not be emulated because they are used in the atomic emulation code.
pub const Spinlock = extern struct {
    value: c.SDL_SpinLock = 0,

    // Size tests.
    comptime {
        std.debug.assert(@sizeOf(c.SDL_SpinLock) == @sizeOf(Spinlock));
    }

    /// Lock a spin lock by setting it to a non-zero value.
    ///
    /// ## Function Parameters
    /// * `self`: A pointer to a lock variable.
    ///
    /// ## Remarks
    /// Please note that spinlocks are dangerous if you don't know what you're doing.
    /// Please be careful using any sort of spinlock!
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn lock(
        self: *Spinlock,
    ) void {
        c.SDL_LockSpinlock(&self.value);
    }

    /// Try to lock a spin lock by setting it to a non-zero value.
    ///
    /// ## Function Parameters
    /// * `self`: A pointer to a lock variable.
    ///
    /// ## Return Value
    /// Returns true if the lock succeeded, false if the lock is already held.
    ///
    /// ## Remarks
    /// Please note that spinlocks are dangerous if you don't know what you're doing.
    /// Please be careful using any sort of spinlock!
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn tryLock(
        self: *Spinlock,
    ) bool {
        return c.SDL_TryLockSpinlock(&self.value);
    }

    /// Unlock a spin lock by setting it to 0.
    ///
    /// ## Function Parameters
    /// * `self`: A pointer to a lock variable.
    ///
    /// ## Remarks
    /// Always returns immediately.
    ///
    /// Please note that spinlocks are dangerous if you don't know what you're doing.
    /// Please be careful using any sort of spinlock!
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn unlock(
        self: *Spinlock,
    ) void {
        c.SDL_UnlockSpinlock(&self.value);
    }
};

/// A type representing an atomic unsigned 32-bit value.
///
/// ## Remarks
/// This can be used to manage a value that is synchronized across multiple CPUs without a race condition; when an app sets a value with `atomic.U32.set()` all other threads,
/// regardless of the CPU it is running on, will see that value when retrieved with `atomic.U32.get()`, regardless of CPU caches, etc.
///
/// This is also useful for atomic compare-and-swap operations: a thread can change the value as long as its current value matches expectations.
/// When done in a loop, one can guarantee data consistency across threads without a lock (but the usual warnings apply: if you don't know what you're doing,
/// or you don't do it carefully, you can confidently cause any number of disasters with this, so in most cases, you should use a mutex instead of this!).
///
/// This is a struct so people don't accidentally use numeric operations on it directly.
/// You have to use SDL atomic functions.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const U32 = struct {
    value: c.SDL_AtomicU32,

    /// Add to an atomic variable.
    ///
    /// ## Function Parameters
    /// * `self`: The atomic variable to be modified.
    /// * `v`: The desired value to add or subtract.
    ///
    /// ## Return Value
    /// Returns the previous value of the atomic variable.
    ///
    /// ## Remarks
    /// This function also acts as a full memory barrier.
    ///
    /// Note: If you don't know what this function is for, you shouldn't use it!
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn add(
        self: *U32,
        v: c_int,
    ) u32 {
        return c.SDL_AddAtomicU32(&self.value, v);
    }

    /// Set an atomic variable to a new value if it is currently an old value.
    ///
    /// ## Function Parameters
    /// * `self`: A pointer to a variable to be modified.
    /// * `old_val`: The old value.
    /// * `new_val`: The new value.
    ///
    /// ## Return Value
    /// Returns true if the atomic variable was set, false otherwise.
    ///
    /// ## Remarks
    /// Note: If you don't know what this function is for, you shouldn't use it!
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn compareAndSwap(
        self: *U32,
        old_val: u32,
        new_val: u32,
    ) bool {
        return c.SDL_CompareAndSwapAtomicU32(&self.value, old_val, new_val);
    }

    /// Get the value of an atomic variable.
    ///
    /// ## Function Parameters
    /// * `self`: A pointer to an atomic value.
    ///
    /// ## Return Value
    /// Returns the current value of an atomic variable.
    ///
    /// ## Remarks
    /// Note: If you don't know what this function is for, you shouldn't use it!
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn get(
        self: *U32,
    ) u32 {
        return c.SDL_GetAtomicU32(&self.value);
    }

    /// Set an atomic variable to a value.
    ///
    /// ## Function Parameters
    /// * `self`: The variable to be modified.
    /// * `v`: The desired value.
    ///
    /// ## Return Value
    /// Returns the previous value of the atomic variable.
    ///
    /// ## Remarks
    /// This function also acts as a full memory barrier.
    ///
    /// Note: If you don't know what this function is for, you shouldn't use it!
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn set(
        self: *U32,
        v: u32,
    ) u32 {
        return c.SDL_SetAtomicU32(&self.value, v);
    }
};

/// Set a pointer to a new value if it is currently an old value.
///
/// ## Function Parameters
/// * `self`: A pointer to a pointer.
/// * `old_val`: The old pointer.
/// * `new_val`: The new pointer.
///
/// ## Return Value
/// Returns true if the pointer was set, false otherwise.
///
/// ## Remarks
/// Note: If you don't know what this function is for, you shouldn't use it!
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn compareAndSwap(
    ptr: *?*anyopaque,
    old_val: ?*anyopaque,
    new_val: ?*anyopaque,
) bool {
    return c.SDL_CompareAndSwapAtomicPointer(ptr, old_val, new_val);
}

// /// Mark a compiler barrier.
// ///
// /// ## Remarks
// /// A compiler barrier prevents the compiler from reordering reads and writes to globally visible variables across the call.
// ///
// /// This function only prevents the compiler from reordering reads and writes, it does not prevent the CPU from reordering reads and writes.
// /// However, all of the atomic operations that modify memory are full memory barriers.
// ///
// /// ## Thread Safety
// /// Obviously this function is safe to use from any thread at any time, but if you find yourself needing this, you are probably dealing with some very sensitive code; be careful!
// ///
// /// ## Version
// /// This function is available since SDL 3.2.0.
// pub fn compilerBarrier() void {
//     c.SDL_CompilerBarrier();
// }

/// Get the value of a pointer atomically.
///
/// ## Function Parameters
/// * `ptr`: A pointer to a pointer.
///
/// ## Return Value
/// Returns the current value of a pointer.
///
/// ## Remarks
/// Note: If you don't know what this function is for, you shouldn't use it!
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getPointer(
    ptr: *?*anyopaque,
) ?*anyopaque {
    return c.SDL_GetAtomicPointer(ptr);
}

/// Insert a memory acquire barrier (function version).
///
/// ## Remarks
/// Please see `atomic.memoryBarrierRelease()` for the details on what memory barriers are and when to use them.
///
/// ## Thread Safety
/// Obviously this function is safe to use from any thread at any time, but if you find yourself needing this,
/// you are probably dealing with some very sensitive code; be careful!
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn memoryBarrierAcquire() void {
    c.SDL_MemoryBarrierAcquireFunction();
}

/// Insert a memory release barrier (function version).
///
/// ## Remarks
/// Memory barriers are designed to prevent reads and writes from being reordered by the compiler and being seen out of order on multi-core CPUs.
///
/// A typical pattern would be for thread A to write some data and a flag, and for thread B to read the flag and get the data.
/// In this case you would insert a release barrier between writing the data and the flag, guaranteeing that the data write completes no later than the flag is written,
/// and you would insert an acquire barrier between reading the flag and reading the data, to ensure that all the reads associated with the flag have completed.
///
/// In this pattern you should always see a release barrier paired with an acquire barrier and you should gate the data reads/writes with a single flag variable.
///
/// For more information on these semantics, take a look at the blog post: http://preshing.com/20120913/acquire-and-release-semantics
///
/// ## Thread Safety
/// Obviously this function is safe to use from any thread at any time, but if you find yourself needing this,
/// you are probably dealing with some very sensitive code; be careful!
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn memoryBarrierRelease() void {
    c.SDL_MemoryBarrierReleaseFunction();
}

// /// A macro to insert a CPU-specific "pause" instruction into the program.
// ///
// /// ## Remarks
// /// This can be useful in busy-wait loops, as it serves as a hint to the CPU as to the program's intent; some CPUs can use this to do more efficient processing.
// /// On some platforms, this doesn't do anything, so using this macro might just be a harmless no-op.
// ///
// /// Note that if you are busy-waiting, there are often more-efficient approaches with other synchronization primitives: mutexes, semaphores, condition variables, etc.
// ///
// /// ## Thread Safety
// /// This macro is safe to use from any thread.
// ///
// /// ## Version
// /// This macro is available since SDL 3.2.0.
// pub fn pause() void {
//     c.SDL_CPUPauseInstruction();
// }

/// Set a pointer to a value atomically.
///
/// ## Function Parameters
/// * `ptr`: The pointer to a pointer.
/// * `v`: The desired pointer value.
///
/// ## Return Value
/// Returns the previous value of the pointer.
///
/// ## Remarks
/// Note: If you don't know what this function is for, you shouldn't use it!
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn setPointer(
    ptr: *?*anyopaque,
    v: ?*anyopaque,
) ?*anyopaque {
    return c.SDL_SetAtomicPointer(ptr, v);
}

// Atomic testing.
test "Atomic" {
    errors.refAllDeclsRecursive(@This());
}
