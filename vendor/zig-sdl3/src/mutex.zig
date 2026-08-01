const atomic = @import("atomic.zig");
const c = @import("c");
const errors = @import("errors.zig");
const std = @import("std");
const thread = @import("thread.zig");

/// A means to block multiple threads until a condition is satisfied.
///
/// ## Remarks
/// Condition variables, paired with a `mutex.Mutex`, let an app halt multiple threads until a condition has occurred,
/// at which time the app can release one or all waiting threads.
///
/// Wikipedia has a thorough explanation of the concept:
/// https://en.wikipedia.org/wiki/Condition_variable
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Condition = struct {
    value: *c.SDL_Condition,

    /// Restart all threads that are waiting on the condition variable.
    ///
    /// ## Function Parameters
    /// * `self`: The condition variable to signal.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    ///
    /// ## Code Examples
    /// TODO!!!
    pub fn broadcast(
        self: Condition,
    ) void {
        c.SDL_BroadcastCondition(self.value);
    }

    /// Destroy a condition variable.
    ///
    /// ## Function Parameters
    /// * `self`: The condition variable to destroy.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn deinit(
        self: Condition,
    ) void {
        c.SDL_DestroyCondition(self.value);
    }

    /// Create a condition variable.
    ///
    /// ## Return Value
    /// Returns a new condition variable.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    ///
    /// ## Code Examples
    /// TODO!!!
    pub fn init() !Condition {
        return .{ .value = try errors.wrapCallNull(*c.SDL_Condition, c.SDL_CreateCondition()) };
    }

    /// Restart one of the threads that are waiting on the condition variable.
    ///
    /// ## Function Parameters
    /// * `self`: The condition variable to signal.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn signal(
        self: Condition,
    ) void {
        c.SDL_SignalCondition(self.value);
    }

    /// Wait until a condition variable is signaled.
    ///
    /// ## Function Parameters
    /// * `self`: The condition variable to wait on.
    /// * `mutex`: The mutex used to coordinate thread access.
    ///
    /// ## Remarks
    /// This function unlocks the specified mutex and waits for another thread to call `mutex.Condition.signal()` or `mutex.Condition.broadcast()` on the condition variable cond.
    /// Once the condition variable is signaled, the mutex is re-locked and the function returns.
    ///
    /// The mutex must be locked before calling this function.
    /// Locking the mutex recursively (more than once) is not supported and leads to undefined behavior.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn wait(
        self: Condition,
        mutex: Mutex,
    ) void {
        c.SDL_WaitCondition(self.value, mutex.value);
    }

    /// Wait until a condition variable is signaled or a certain time has passed.
    ///
    /// ## Function Parameters
    /// * `self`: The condition variable to wait on.
    /// * `mutex`: The mutex used to coordinate thread access.
    /// * `timeout_milliseconds`: The maximum time to wait, in milliseconds.
    ///
    /// ## Return Value
    /// Returns true if the condition variable is signaled, false if the condition is not signaled in the allotted time.
    ///
    /// ## Remarks
    /// This function unlocks the specified mutex and waits for another thread to call `mutex.Condition.signal()`
    /// or `mutex.Condition.broadcast()` on the condition variable cond, or for the specified time to elapse.
    /// Once the condition variable is signaled or the time elapsed, the mutex is re-locked and the function returns.
    ///
    /// The mutex must be locked before calling this function.
    /// Locking the mutex recursively (more than once) is not supported and leads to undefined behavior.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    ///
    /// ## Code Examples
    /// TODO!!!
    pub fn waitTimeout(
        self: Condition,
        mutex: Mutex,
        timeout_milliseconds: i31,
    ) bool {
        return c.SDL_WaitConditionTimeout(self.value, mutex.value, @intCast(timeout_milliseconds));
    }
};

/// A structure used for thread-safe initialization and shutdown.
///
/// ## Remarks
/// Note that this doesn't protect any resources created during initialization, or guarantee that nobody is using those resources during cleanup.
/// You should use other mechanisms to protect those, if that's a concern for your code.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
///
/// ## Code Example
/// TODO!!!
pub const InitState = struct {
    value: c.SDL_InitState,

    /// Get the status of the init state.
    ///
    /// ## Function Parameters
    /// * `self`: The init state.
    ///
    /// ## Return Value
    /// The status of the init state.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn getStatus(
        self: *InitState,
    ) InitStatus {
        return @enumFromInt(c.SDL_GetAtomicInt(&self.value.status));
    }

    /// Get the thread of the init state.
    ///
    /// ## Function Parameters
    /// * `self`: The init state.
    ///
    /// ## Return Value
    /// The thread of the init state.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn getThread(
        self: *InitState,
    ) thread.Id {
        return .{ .value = self.value.thread };
    }

    /// Finish an initialization state transition.
    ///
    /// ## Function Parameters
    /// * `self`: The initialization state to check.
    /// * `initialized`: The new initialization state.
    ///
    /// ## Remarks
    /// This function sets the status of the passed in state to `mutex.InitStatus.initialized` or `mutex.InitStatus.uninitialized` and allows any threads waiting for the status to proceed.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setInitialized(
        self: *InitState,
        initialized: bool,
    ) void {
        c.SDL_SetInitialized(&self.value, initialized);
    }

    /// Return whether initialization should be done.
    ///
    /// ## Function Parameters
    /// * `self`: The initialization state to check.
    ///
    /// ## Return Value
    /// Returns true if initialization needs to be done, false otherwise.
    ///
    /// ## Remarks
    /// This function checks the passed in state and if initialization should be done, sets the status to `mutex.InitStatus.initializing` and returns true.
    /// If another thread is already modifying this state, it will wait until that's done before returning.
    ///
    /// If this function returns true, the calling code must call `mutex.InitState.setInitialized()` to complete the initialization.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn shouldInit(
        self: *InitState,
    ) bool {
        return c.SDL_ShouldInit(&self.value);
    }

    /// Return whether cleanup should be done.
    ///
    /// ## Function Parameters
    /// * `self`: The initialization state to check.
    ///
    /// ## Return Value
    /// Returns true if cleanup needs to be done, false otherwise.
    ///
    /// ## Remarks
    /// This function checks the passed in state and if cleanup should be done, sets the status to `mutex.InitStatus.uninitializing` and returns true.
    ///
    /// If this function returns true, the calling code must call `mutex.InitState.setInitialized()` to complete the cleanup.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn shouldQuit(
        self: *InitState,
    ) bool {
        return c.SDL_ShouldQuit(&self.value);
    }
};

/// The current status of an `mutex.InitState` structure.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const InitStatus = enum(c.SDL_InitStatus) {
    uninitialized = c.SDL_INIT_STATUS_UNINITIALIZED,
    initializing = c.SDL_INIT_STATUS_INITIALIZING,
    initialized = c.SDL_INIT_STATUS_INITIALIZED,
    uninitializing = c.SDL_INIT_STATUS_UNINITIALIZING,
};

/// A means to serialize access to a resource between threads.
///
/// ## Remarks
/// Mutexes (short for "mutual exclusion") are a synchronization primitive that allows exactly one thread to proceed at a time.
///
/// Wikipedia has a thorough explanation of the concept:
/// https://en.wikipedia.org/wiki/Mutex
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Mutex = struct {
    value: *c.SDL_Mutex,

    /// Destroy a mutex variable.
    ///
    /// ## Function Parameters
    /// * `self`: The mutex to destroy.
    ///
    /// ## Remarks
    /// This function must be called on any mutex that is no longer needed.
    /// Failure to destroy a mutex will result in a system memory or resource leak.
    /// While it is safe to destroy a mutex that is unlocked, it is not safe to attempt to destroy a locked mutex, and may result in undefined behavior depending on the platform.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn deinit(
        self: Mutex,
    ) void {
        c.SDL_DestroyMutex(self.value);
    }

    /// Create a new mutex.
    ///
    /// ## Return Value
    /// Returns the initialized and unlocked mutex.
    ///
    /// ## Remarks
    /// All newly-created mutexes begin in the unlocked state.
    ///
    /// Calls to `mutex.Mutex.lock()` will not return while the mutex is locked by another thread.
    /// See `mutex.Mutex.tryLock()` to attempt to lock without blocking.
    ///
    /// SDL mutexes are reentrant.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    ///
    /// ## Code Examples
    /// TODO!!!
    pub fn init() !Mutex {
        return .{ .value = try errors.wrapCallNull(*c.SDL_Mutex, c.SDL_CreateMutex()) };
    }

    /// Lock the mutex.
    ///
    /// ## Function Parameters
    /// * `self`: The mutex to lock.
    ///
    /// ## Remarks
    /// This will block until the mutex is available, which is to say it is in the unlocked state and the OS has chosen the caller as the next thread to lock it.
    /// Of all threads waiting to lock the mutex, only one may do so at a time.
    ///
    /// It is legal for the owning thread to lock an already-locked mutex.
    /// It must unlock it the same number of times before it is actually made available for other threads in the system (this is known as a "recursive mutex").
    ///
    /// If the mutex is valid, this function will always block until it can lock the mutex, and return with it locked.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn lock(
        self: Mutex,
    ) void {
        c.SDL_LockMutex(self.value);
    }

    /// Try to lock a mutex without blocking.
    ///
    /// ## Function Parameters
    /// * `self`: The mutex to try to lock.
    ///
    /// ## Return Value
    /// Returns true on success, false if the mutex would block.
    ///
    /// ## Remarks
    /// This works just like `mutex.Mutex.lock()`, but if the mutex is not available, this function returns false immediately.
    ///
    /// This technique is useful if you need exclusive access to a resource but don't want to wait for it, and will return to it to try again later.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn tryLock(
        self: Mutex,
    ) bool {
        return c.SDL_TryLockMutex(self.value);
    }

    /// Unlock the mutex.
    ///
    /// ## Function Parameters
    /// * `self`: The mutex to unlock.
    ///
    /// ## Remarks
    /// It is legal for the owning thread to lock an already-locked mutex.
    /// It must unlock it the same number of times before it is actually made available for other threads in the system (this is known as a "recursive mutex").
    ///
    /// It is illegal to unlock a mutex that has not been locked by the current thread, and doing so results in undefined behavior.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn unlock(
        self: Mutex,
    ) void {
        c.SDL_UnlockMutex(self.value);
    }
};

/// A mutex that allows read-only threads to run in parallel.
///
/// ## Remarks
/// A rwlock is roughly the same concept as mutex, but allows threads that request read-only access to all hold the lock at the same time.
/// If a thread requests write access, it will block until all read-only threads have released the lock, and no one else can hold the thread (for reading or writing)
/// at the same time as the writing thread.
///
/// This can be more efficient in cases where several threads need to access data frequently, but changes to that data are rare.
///
/// There are other rules that apply to rwlocks that don't apply to mutexes, about how threads are scheduled and when they can be recursively locked.
/// These are documented in the other rwlock functions.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const RwLock = struct {
    value: *c.SDL_RWLock,

    /// Destroy a read/write lock.
    ///
    /// ## Function Parameters
    /// * `self`: The rwlock to destroy.
    ///
    /// ## Remarks
    /// This function must be called on any read/write lock that is no longer needed.
    /// Failure to destroy a rwlock will result in a system memory or resource leak.
    /// While it is safe to destroy a rwlock that is unlocked, it is not safe to attempt to destroy a locked rwlock, and may result in undefined behavior depending on the platform.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn deinit(
        self: RwLock,
    ) void {
        c.SDL_DestroyRWLock(self.value);
    }

    /// Create a new read/write lock.
    ///
    /// ## Return Value
    /// Returns the initialized and unlocked read/write lock.
    ///
    /// ## Remarks
    /// A read/write lock is useful for situations where you have multiple threads trying to access a resource that is rarely updated.
    /// All threads requesting a read-only lock will be allowed to run in parallel; if a thread requests a write lock, it will be provided exclusive access.
    /// This makes it safe for multiple threads to use a resource at the same time if they promise not to change it, and when it has to be changed,
    /// the rwlock will serve as a gateway to make sure those changes can be made safely.
    ///
    /// In the right situation, a rwlock can be more efficient than a mutex, which only lets a single thread proceed at a time, even if it won't be modifying the data.
    ///
    /// All newly-created read/write locks begin in the unlocked state.
    ///
    /// Calls to `mutex.RwLock.lockForReading()` and `mutex.RwLock.lockForWriting()` will not return while the rwlock is locked for writing by another thread.
    /// See `mutex.RwLock.tryLockForReading()` and `mutex.RwLock.tryLockForWriting()` to attempt to lock without blocking.
    ///
    /// SDL read/write locks are only recursive for read-only locks!
    /// They are not guaranteed to be fair, or provide access in a FIFO manner!
    /// They are not guaranteed to favor writers.
    /// You may not lock a rwlock for both read-only and write access at the same time from the same thread
    /// (so you can't promote your read-only lock to a write lock without unlocking first).
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn init() !Mutex {
        return .{ .value = try errors.wrapCallNull(*c.SDL_Mutex, c.SDL_CreateMutex()) };
    }

    /// Lock the read/write lock for read only operations.
    ///
    /// ## Function Parameters
    /// * `self`: The read/write lock to lock.
    ///
    /// ## Remarks
    /// This will block until the rwlock is available, which is to say it is not locked for writing by any other thread.
    /// Of all threads waiting to lock the rwlock, all may do so at the same time as long as they are requesting read-only access; if a thread wants to lock for writing,
    /// only one may do so at a time, and no other threads, read-only or not, may hold the lock at the same time.
    ///
    /// It is legal for the owning thread to lock an already-locked rwlock for reading.
    /// It must unlock it the same number of times before it is actually made available for other threads in the system (this is known as a "recursive rwlock").
    ///
    /// Note that locking for writing is not recursive (this is only available to read-only locks).
    ///
    /// It is illegal to request a read-only lock from a thread that already holds the write lock.
    /// Doing so results in undefined behavior.
    /// Unlock the write lock before requesting a read-only lock.
    /// (But, of course, if you have the write lock, you don't need further locks to read in any case).
    ///
    /// If the rwlock is valid, this function will always block until it can lock the mutex, and return with it locked.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn lockForReading(
        self: RwLock,
    ) void {
        c.SDL_LockRWLockForReading(self.value);
    }

    /// Lock the read/write lock for write operations.
    ///
    /// ## Function Parameters
    /// * `self`: The read/write lock to lock.
    ///
    /// ## Remarks
    /// This will block until the rwlock is available, which is to say it is not locked for reading or writing by any other thread.
    /// Only one thread may hold the lock when it requests write access; all other threads, whether they also want to write or only want read-only access,
    /// must wait until the writer thread has released the lock.
    ///
    /// It is illegal for the owning thread to lock an already-locked rwlock for writing (read-only may be locked recursively, writing can not).
    /// Doing so results in undefined behavior.
    ///
    /// It is illegal to request a write lock from a thread that already holds a read-only lock.
    /// Doing so results in undefined behavior.
    /// Unlock the read-only lock before requesting a write lock.
    ///
    /// If the rwlock is valid, this function will always block until it can lock the mutex, and return with it locked.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn lockForWriting(
        self: RwLock,
    ) void {
        c.SDL_LockRWLockForWriting(self.value);
    }

    /// Try to lock a read/write lock for reading without blocking.
    ///
    /// ## Function Parameters
    /// * `self`: The rwlock to try to lock.
    ///
    /// ## Return Value
    /// Returns true on success, false if the lock would block.
    ///
    /// ## Remarks
    /// This works just like `mutex.RwLock.lockForReading()`, but if the rwlock is not available, then this function returns false immediately.
    ///
    /// This technique is useful if you need access to a resource but don't want to wait for it, and will return to it to try again later.
    ///
    /// Trying to lock for read-only access can succeed if other threads are holding read-only locks, as this won't prevent access.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn tryLockForReading(
        self: RwLock,
    ) bool {
        return c.SDL_TryLockRWLockForReading(self.value);
    }

    /// Try to lock a read/write lock for writing without blocking.
    ///
    /// ## Function Parameters
    /// * `self`: The rwlock to try to lock.
    ///
    /// ## Return Value
    /// Returns true on success, false if the lock would block.
    ///
    /// ## Remarks
    /// This works just like `mutex.RwLock.lockForWrotomg()`, but if the rwlock is not available, then this function returns false immediately.
    ///
    /// This technique is useful if you need exclusive access to a resource but don't want to wait for it, and will return to it to try again later.
    ///
    /// It is illegal for the owning thread to lock an already-locked rwlock for writing (read-only may be locked recursively, writing can not).
    /// Doing so results in undefined behavior.
    ///
    /// It is illegal to request a write lock from a thread that already holds a read-only lock. Doing so results in undefined behavior.
    /// Unlock the read-only lock before requesting a write lock.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn tryLockForWriting(
        self: RwLock,
    ) bool {
        return c.SDL_TryLockRWLockForWriting(self.value);
    }

    /// Unlock the read/write lock.
    ///
    /// ## Function Parameters
    /// * `self`: The rwlock to unlock.
    ///
    /// ## Remarks
    /// Use this function to unlock the rwlock, whether it was locked for read-only or write operations.
    ///
    /// It is legal for the owning thread to lock an already-locked read-only lock.
    /// It must unlock it the same number of times before it is actually made available for other threads in the system (this is known as a "recursive rwlock").
    ///
    /// It is illegal to unlock a rwlock that has not been locked by the current thread, and doing so results in undefined behavior.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn unlock(
        self: Mutex,
    ) void {
        c.SDL_UnlockMutex(self.value);
    }
};

/// A means to manage access to a resource, by count, between threads.
///
/// ## Remarks
/// Semaphores (specifically, "counting semaphores"), let X number of threads request access at the same time, each thread granted access decrementing a counter.
/// When the counter reaches zero, future requests block until a prior thread releases their request, incrementing the counter again.
///
/// Wikipedia has a thorough explanation of the concept:
/// https://en.wikipedia.org/wiki/Semaphore_(programming)
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Semaphore = struct {
    value: *c.SDL_Semaphore,

    /// Destroy a semaphore.
    ///
    /// ## Function Parameters
    /// * `self`: The semaphore to destroy.
    ///
    /// ## Remarks
    /// It is not safe to destroy a semaphore if there are threads currently waiting on it.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn deinit(
        self: Semaphore,
    ) void {
        c.SDL_DestroySemaphore(self.value);
    }

    /// Get the current value of a semaphore.
    ///
    /// ## Function Parameters
    /// * `self`: The semaphore to query.
    ///
    /// ## Return Value
    /// Returns the current value of the semaphore.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getValue(
        self: Semaphore,
    ) u32 {
        return c.SDL_GetSemaphoreValue(self.value);
    }

    /// Create a semaphore.
    ///
    /// ## Function Parameters
    /// * `initial_value`: The starting value of the semaphore.
    ///
    /// ## Return Value
    /// Returns a new semaphore.
    ///
    /// ## Remarks
    /// This function creates a new semaphore and initializes it with the value `initial_value`.
    /// Each wait operation on the semaphore will atomically decrement the semaphore value and potentially block if the semaphore value is `0`.
    /// Each post operation will atomically increment the semaphore value and wake waiting threads and allow them to retry the wait operation.
    ///
    /// SDL mutexes are reentrant.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    ///
    /// ## Code Examples
    /// TODO!!!
    pub fn init(
        initial_value: u32,
    ) !Semaphore {
        return .{ .value = try errors.wrapCallNull(*c.SDL_Semaphore, c.SDL_CreateSemaphore(initial_value)) };
    }

    /// Atomically increment a semaphore's value and wake waiting threads.
    ///
    /// ## Function Parameters
    /// * `self`: The semaphore to increment.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn signal(
        self: Semaphore,
    ) void {
        c.SDL_SignalSemaphore(self.value);
    }

    /// See if a semaphore has a positive value and decrement it if it does.
    ///
    /// ## Function Parameters
    /// * `self`: The semaphore to wait on.
    ///
    /// ## Return Value
    /// Returns true if the wait succeeds, false if the wait would block.
    ///
    /// ## Remarks
    /// This function checks to see if the semaphore pointed to by sem has a positive value and atomically decrements the semaphore value if it does.
    /// If the semaphore doesn't have a positive value, the function immediately returns false.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn tryWait(
        self: Semaphore,
    ) bool {
        return c.SDL_TryWaitSemaphore(self.value);
    }

    /// Wait until a semaphore has a positive value and then decrements it.
    ///
    /// ## Function Parameters
    /// * `self`: The semaphore wait on.
    ///
    /// ## Remarks
    /// This function suspends the calling thread until the semaphore pointed to by sem has a positive value, and then atomically decrement the semaphore value.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn wait(
        self: Semaphore,
    ) void {
        c.SDL_WaitSemaphore(self.value);
    }

    /// Wait until a semaphore has a positive value and then decrements it.
    ///
    /// ## Function Parameters
    /// * `self`: The semaphore to wait on.
    /// * `timeout_milliseconds`: The maximum time to wait, in milliseconds.
    ///
    /// ## Return Value
    /// Returns true if the wait succeeds or false if the wait times out.
    ///
    /// ## Remarks
    /// This function suspends the calling thread until either the semaphore pointed to by sem has a positive value or the specified time has elapsed.
    /// If the call is successful it will atomically decrement the semaphore value.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    ///
    /// ## Code Examples
    /// TODO!!!
    pub fn waitTimeout(
        self: Semaphore,
        timeout_milliseconds: i31,
    ) bool {
        return c.SDL_WaitSemaphoreTimeout(self.value, timeout_milliseconds);
    }
};

// Mutex testing.
test "Mutex" {
    errors.refAllDeclsRecursive(@This());
}
