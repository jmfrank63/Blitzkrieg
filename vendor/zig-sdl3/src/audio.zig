const c = @import("c");
const errors = @import("errors.zig");
const io_stream = @import("io_stream.zig");
const properties = @import("properties.zig");
const std = @import("std");

/// A callback that fires when data is about to be fed to an audio device.
///
/// ## Function Parameters
/// * `user_data`: A pointer provided by the app through `audio.setPostmixCallback()`, for its own use.
/// * `spec`: The current format of audio that is to be submitted to the audio device.
/// * `buffer`: The buffer of audio samples to be submitted. The callback can inspect and/or modify this data.
/// * `buffer_len`: The size of `buffer` in bytes.
///
/// ## Remarks
/// This is useful for accessing the final mix,
/// perhaps for writing a visualizer or applying a final effect to the audio data before playback.
///
/// This callback should run as quickly as possible and not block for any significant time,
/// as this callback delays submission of data to the audio device, which can cause audio playback problems.
///
/// The postmix callback must be able to handle any audio data format specified in spec,
/// which can change between callbacks if the audio device changed.
/// However, this only covers frequency and channel count; data is always provided here in `audio.Format.floating_32_bit` format.
///
/// The postmix callback runs after logical device gain and audiostream gain have been applied,
/// which is to say you can make the output data louder at this point than the gain settings would suggest.
///
/// ## Thread Safety
/// This will run from a background thread owned by SDL.
/// The application is responsible for locking resources the callback touches that need to be protected.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub fn PostmixCallback(comptime UserData: type) type {
    return *const fn (
        user_data: ?*UserData,
        spec: Spec,
        buffer: []f32,
    ) void;
}

/// A callback that fires when data passes through a `Stream`.
///
/// ## Function Parameters
/// * `user_data`: An opaque pointer provided by the app for their personal use.
/// * `stream`: The SDL audio stream associated with this callback.
/// * `additional_amount`: The amount of data, in bytes, that is needed right now.
/// * `total_amount`: The total amount of data requested, in bytes, that is requested or available.
///
/// ## Remarks
/// Apps can (optionally) register a callback with an audio stream that is called when data is added with
/// `audio.Stream.putData()`, or requested with `audio.Stream.getData()`.
///
/// Two values are offered here: one is the amount of additional data needed to satisfy the immediate request
/// (which might be zero if the stream already has enough data queued) and the other is the total amount being requested.
/// In a Get call triggering a Put callback, these values can be different.
/// In a Put call triggering a Get callback, these values are always the same.
///
/// Byte counts might be slightly overestimated due to buffering or resampling, and may change from call to call.
///
/// This callback is not required to do anything.
/// Generally this is useful for adding/reading data on demand, and the app will often put/get data as appropriate,
/// but the system goes on with the data currently available to it if this callback does nothing.
///
/// ## Thread Safety
/// This callbacks may run from any thread, so if you need to protect shared data,
/// you should use `Stream.lock()` to serialize access; this lock will be held before your callback is called,
/// so your callback does not need to manage the lock explicitly.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub fn StreamCallback(
    comptime UserData: type,
) type {
    return *const fn (
        user_data: ?*UserData,
        stream: Stream,
        additional_amount: usize,
        total_amount: usize,
    ) void;
}

/// A callback that fires for completed `Stream.putNoData()` data.
///
/// ## Function Parameters
/// * `user_data`: An opaque pointer provided by the app for their personal use.
/// * `buf`: Buffer provided to the put no data function.
///
/// ## Remarks
/// When using this to provide data to a `Stream`, it's not safe to dispose of the data until the stream has completely consumed it.
/// Often times it's difficult to know exactly when this has happened.
///
/// This callback fires once when the stream no longer needs the buffer, allowing the app to easily free or reuse it.
///
/// ## Thread Safety
/// This callbacks may run from any thread, so if you need to protect shared data, you should use `Stream.lock()` to serialize access;
/// this lock will be held before your callback is called, so your callback does not need to manage the lock explicitly.
///
/// ## Version
/// This datatype is available since SDL 3.4.0.
pub fn StreamDataCompleteCallback(
    comptime UserData: type,
) type {
    return *const fn (
        user_data: ?*UserData,
        buf: []const u8,
    ) void;
}

/// Audio format.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const Format = enum(c.SDL_AudioFormat) {
    unsigned_8_bit = c.SDL_AUDIO_U8,
    signed_8_bit = c.SDL_AUDIO_S8,
    signed_16_bit_little_endian = c.SDL_AUDIO_S16LE,
    signed_16_bit_big_endian = c.SDL_AUDIO_S16BE,
    signed_32_bit_little_endian = c.SDL_AUDIO_S32LE,
    signed_32_bit_big_endian = c.SDL_AUDIO_S32BE,
    floating_32_bit_little_endian = c.SDL_AUDIO_F32LE,
    floating_32_bit_big_endian = c.SDL_AUDIO_F32BE,
    _,

    pub const signed_16_bit: Format = @enumFromInt(c.SDL_AUDIO_S16);
    pub const signed_32_bit: Format = @enumFromInt(c.SDL_AUDIO_S32);
    pub const floating_32_bit: Format = @enumFromInt(c.SDL_AUDIO_F32);

    /// Define an audio format.
    ///
    /// ## Function Parameters
    /// * `signed`: True for signed, false for unsigned.
    /// * `big_endian`: True for big endian, false for little endian.
    /// * `float`: True for floating point data, false for integer data.
    /// * `bit_width`: Number of bits per sample.
    ///
    /// ## Return Value
    /// Returns a format value in the style of `audio.Format`.
    ///
    /// ## Remarks
    /// SDL does not support custom audio formats, so this function is not of much use externally,
    /// but it can be illustrative as to what the various bits of an `audio.Format` mean.
    ///
    /// For example, `audio.Format.signed_32_bit_little_endian` looks like this:
    /// ```zig
    /// audio.Format.define(true, false, false, 32)
    /// ```
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn define(
        signed: bool,
        big_endian: bool,
        float: bool,
        bit_width: u8,
    ) Format {
        var ret: c_int = 0;
        if (signed)
            ret |= c.SDL_AUDIO_MASK_SIGNED;
        if (big_endian)
            ret |= c.SDL_AUDIO_MASK_BIG_ENDIAN;
        if (float)
            ret |= c.SDL_AUDIO_MASK_FLOAT;
        ret |= @as(c_int, @intCast(bit_width));
        return @enumFromInt(ret);
    }

    /// Retrieve the size in bits.
    ///
    /// ## Function Parameters
    /// * `self`: The audio format.
    ///
    /// ## Return Value
    /// Returns data size in bits.
    ///
    /// ## Remarks
    /// For example, calling this on `audio.Format.signed_16_bit` returns 16.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getBitwidth(
        self: Format,
    ) u8 {
        const ret = c.SDL_AUDIO_BITSIZE(
            @intFromEnum(self),
        );
        return @intCast(ret);
    }

    /// Get the byte size of the format.
    ///
    /// ## Function Parameters
    /// * `self`: The audio format.
    ///
    /// ## Return Value
    /// Returns data size in bytes.
    ///
    /// ## Remarks
    /// For example, calling this on `audio.Format.signed_16_bit` returns `2`.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getByteSize(
        self: Format,
    ) u8 {
        const ret = c.SDL_AUDIO_BYTESIZE(
            @intFromEnum(self),
        );
        return @intCast(ret);
    }

    /// Get the human readable name of an audio format.
    ///
    /// ## Function Parameters
    /// * `self`: The audio format to query.
    ///
    /// ## Return Value
    /// Returns the human readable name of the specified audio format or `null` if the format is not recognized.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getName(
        self: Format,
    ) ?[:0]const u8 {
        const ret: [:0]const u8 = std.mem.span(c.SDL_GetAudioFormatName(@intFromEnum(self)));
        if (std.mem.eql(u8, ret, "SDL_AUDIO_UNKNOWN"))
            return null;
        return ret;
    }

    /// Get the appropriate memset value for silencing an audio format.
    ///
    /// ## Function Parameters
    /// * `self`: The audio data format to query.
    ///
    /// ## Return Value
    /// Returns a byte value that can be passed to memset.
    ///
    /// ## Remarks
    /// The value returned by this function can be used as the second argument to `@memset()` to set an audio buffer in a specific format to silence.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getSilenceValue(
        self: Format,
    ) u8 {
        return @intCast(c.SDL_GetSilenceValueForFormat(@intFromEnum(self)));
    }

    /// If the format is big endian.
    ///
    /// ## Function Parameters
    /// * `self`: The format value.
    ///
    /// ## Return Value
    /// Returns if the format is big endian.
    ///
    /// ## Remarks
    /// For example, calling this on `audio.Format.signed_16_bit_little_endian` returns `false`.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn isBigEndian(
        self: Format,
    ) bool {
        const ret = c.SDL_AUDIO_ISBIGENDIAN(
            @intFromEnum(self),
        );
        return ret != 0;
    }

    /// If the format is floating point data.
    ///
    /// ## Function Parameters
    /// * `self`: The format value.
    ///
    /// ## Return Value
    /// Returns if the format is floating point.
    ///
    /// ## Remarks
    /// For example, calling this on `audio.Format.signed_16_bit` returns `false`.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn isFloat(
        self: Format,
    ) bool {
        const ret = c.SDL_AUDIO_ISFLOAT(
            @intFromEnum(self),
        );
        return ret != 0;
    }

    /// If the format is integer data.
    ///
    /// ## Function Parameters
    /// * `self`: The format value.
    ///
    /// ## Return Value
    /// Returns if the format is an integer.
    ///
    /// ## Remarks
    /// For example, calling this on `audio.Format.floating_32_bit` returns `false`.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn isInt(
        self: Format,
    ) bool {
        const ret = c.SDL_AUDIO_ISINT(
            @intFromEnum(self),
        );
        return ret;
    }

    /// If the format represents little-endian data.
    ///
    /// ## Function Parameters
    /// * `self`: The format value.
    ///
    /// ## Return Value
    /// Returns if the format is little-endian.
    ///
    /// ## Remarks
    /// For example, calling this on `audio.Format.signed_16_bit_big_endian` returns `false`.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn isLittleEndian(
        self: Format,
    ) bool {
        const ret = c.SDL_AUDIO_ISLITTLEENDIAN(
            @intFromEnum(self),
        );
        return ret;
    }

    /// If the format represents signed data.
    ///
    /// ## Function Parameters
    /// * `self`: The format value.
    ///
    /// ## Return Value
    /// Returns if the format is signed.
    ///
    /// ## Remarks
    /// For example, calling this on `audio.Format.unsigned_8_bit` returns `false`.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn isSigned(
        self: Format,
    ) bool {
        const ret = c.SDL_AUDIO_ISSIGNED(
            @intFromEnum(self),
        );
        return ret != 0;
    }

    /// If the format represents unsigned data.
    ///
    /// ## Function Parameters
    /// * `self`: The format value.
    ///
    /// ## Return Value
    /// Returns if the format is unsigned.
    ///
    /// ## Remarks
    /// For example, calling this on `audio.Format.signed_16_bit` returns `false`.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn isUnsigned(
        self: Format,
    ) bool {
        const ret = c.SDL_AUDIO_ISUNSIGNED(
            @intFromEnum(self),
        );
        return ret;
    }
};

/// SDL Audio Device instance.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub const Device = packed struct {
    value: c.SDL_AudioDeviceID,
    /// A value used to request a default playback audio device.
    pub const default_playback = Device{ .value = c.SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK };
    /// A value used to request a default recording audio device.
    pub const default_recording = Device{ .value = c.SDL_AUDIO_DEVICE_DEFAULT_RECORDING };

    // Test sizes.
    comptime {
        std.debug.assert(@sizeOf(c.SDL_AudioDeviceID) == @sizeOf(Device));
    }

    /// Bind a single audio stream to an audio device.
    ///
    /// ## Function Parameters
    /// * `self`: An audio device to bind a stream to.
    /// * `stream`: An audio stream to bind to a device.
    ///
    /// ## Remarks
    /// This is a convenience function, equivalent to calling `audio.Device.bindStreams(devid, &.{stream})`.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn bindStream(
        self: Device,
        stream: Stream,
    ) !void {
        const ret = c.SDL_BindAudioStream(
            self.value,
            stream.value,
        );
        return errors.wrapCallBool(ret);
    }

    /// Bind a list of audio streams to an audio device.
    ///
    /// ## Function Parameters
    /// * `self`: An audio device to bind a stream to.
    /// * `streams: A slice of audio streams to bind.
    ///
    /// ## Remarks
    /// Audio data will flow through any bound streams.
    /// For a playback device, data for all bound streams will be mixed together and fed to the device.
    /// For a recording device, a copy of recorded data will be provided to each bound stream.
    ///
    /// Audio streams can only be bound to an open device.
    /// This operation is atomic--all streams bound in the same call will start processing at the same time, so they can stay in sync.
    /// Also: either all streams will be bound or none of them will be.
    ///
    /// It is an error to bind an already-bound stream; it must be explicitly unbound first.
    ///
    /// Binding a stream to a device will set its output format for playback devices, and its input format for recording devices,
    /// so they match the device's settings.
    /// The caller is welcome to change the other end of the stream's format at any time with `audio.Stream.setFormat()`.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn bindStreams(
        self: Device,
        streams: []Stream,
    ) !void {
        const ret = c.SDL_BindAudioStreams(
            self.value,
            @ptrCast(streams.ptr),
            @intCast(streams.len),
        );
        return errors.wrapCallBool(ret);
    }

    /// Close a previously-opened audio device.
    ///
    /// ## Function Parameters
    /// * `self`: An audio device id previously returned by `audio.Device.open()`.
    ///
    /// ## Remarks
    /// The application should close open audio devices once they are no longer needed.
    ///
    /// This function may block briefly while pending audio data is played by the hardware, so that applications don't drop the last buffer of data they supplied if terminating immediately afterwards.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    ///
    /// ## Code Examples
    /// TODO!!!
    pub fn close(
        self: Device,
    ) void {
        const ret = c.SDL_CloseAudioDevice(
            self.value,
        );
        _ = ret;
    }

    /// Get the current audio format of a specific audio device.
    ///
    /// ## Function Parameters
    /// * `self`: The device instance to query.
    ///
    /// ## Return Value
    /// Returns the audio specification of the device as well as the buffer size in sample frames.
    ///
    /// ## Remarks
    /// For an opened device, this will report the format the device is currently using.
    /// If the device isn't yet opened, this will report the device's preferred format (or a reasonable default if this can't be determined).
    ///
    /// You may also specify `audio.Device.default_playback` or `audio.Device.default_recording` here,
    /// which is useful for getting a reasonable recommendation before opening the system-recommended default device.
    ///
    /// You can also use this to request the current device buffer size.
    /// This is specified in sample frames and represents the amount of data SDL will feed to the physical hardware in each chunk.
    /// This can be converted to milliseconds of audio with the following equation:
    /// `const ms = val.buffer_size_frames * 1000 / val.spec.sample_rate`.
    ///
    /// Buffer size is only important if you need low-level control over the audio playback timing.
    /// Most apps do not need this.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getFormat(
        self: Device,
    ) !struct { Spec, usize } {
        var spec: c.SDL_AudioSpec = undefined;
        var buffer_size_frames: c_int = undefined;
        const ret = c.SDL_GetAudioDeviceFormat(
            self.value,
            &spec,
            &buffer_size_frames,
        );
        try errors.wrapCallBool(ret);
        return .{ Spec.fromSdl(spec), @intCast(buffer_size_frames) };
    }

    /// Get the current channel map of an audio device.
    ///
    /// ## Function Parameters
    /// * `self`: The device instant to query.
    ///
    /// ## Return Value
    /// Returns a slice of the current channel mapping, with as many elements as the current output spec's channels, or `null` if default/no remapping.
    /// This should be freed with `free()` when it is no longer needed.
    ///
    /// ## Remarks
    /// Channel maps are optional; most things do not need them, instead passing data in the order that SDL expects.
    ///
    /// Audio devices usually have no remapping applied.
    /// This is represented by returning `null`.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getChannelMap(
        self: Device,
    ) ?[]c_int {
        var count: c_int = undefined;
        const ret = c.SDL_GetAudioDeviceChannelMap(
            self.value,
            &count,
        );
        if (ret == null)
            return null;
        return ret[0..@intCast(count)];
    }

    /// Get the gain of an audio device.
    ///
    /// ## Function Parameters
    /// * `self`: The audio device to query.
    ///
    /// ## Return Value
    /// Returns the gain of the device.
    ///
    /// ## Remarks
    /// The gain of a device is its volume; a larger gain means a louder output, with a gain of zero being silence.
    ///
    /// Audio devices default to a gain of 1 (no change in output).
    ///
    /// Physical devices may not have their gain changed, only logical devices, physical devices will always return an error.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getGain(
        self: Device,
    ) !f32 {
        const ret = c.SDL_GetAudioDeviceGain(
            self.value,
        );
        return @floatCast(try errors.wrapCall(f32, ret, -1));
    }

    /// Get the human-readable name of a specific audio device.
    ///
    /// ## Function Parameters
    /// * `self`: The instance of the device to query.
    ///
    /// ## Return Value
    /// Returns the name of the audio device.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getName(
        self: Device,
    ) ![:0]const u8 {
        const ret = c.SDL_GetAudioDeviceName(
            self.value,
        );
        return errors.wrapCallCString(ret);
    }

    /// Use this function to query if an audio device is paused.
    ///
    /// ## Function Parameters
    /// * `self`: A device opened by `audio.Device.open()`.
    ///
    /// ## Return Value
    /// Returns true if device is valid and paused, false otherwise.
    ///
    /// ## Remarks
    /// Unlike in SDL2, audio devices start in an unpaused state, since an app has to bind a stream before any audio will flow.
    ///
    /// Physical devices can not be paused or unpaused, only logical devices created through `audio.Device.open()` can be.
    /// Physical and invalid device IDs will report themselves as unpaused here.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getPaused(
        self: Device,
    ) bool {
        const ret = c.SDL_AudioDevicePaused(
            self.value,
        );
        return ret;
    }

    /// Determine if an audio device is physical (instead of logical).
    ///
    /// ## Function Parameters
    /// * `self`: The device ID to query.
    ///
    /// ## Return Value
    /// Returns true if this is a physical device, false if it is logical.
    ///
    /// ## Remarks
    /// An `audio.Device` that represents physical hardware is a physical device; there is one for each piece of hardware that SDL can see.
    /// Logical devices are created by calling `audio.Device.open()` or `audio.Device.openStream()`, and while each is associated with a physical device,
    /// there can be any number of logical devices on one physical device.
    ///
    /// For the most part, logical and physical IDs are interchangeable--if you try to open a logical device,
    /// SDL understands to assign that effort to the underlying physical device, etc.
    /// However, it might be useful to know if an arbitrary device ID is physical or logical.
    /// This function reports which.
    ///
    /// This function may return either true or false for invalid device IDs.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn isPhysical(
        self: Device,
    ) bool {
        return c.SDL_IsAudioDevicePhysical(self.value);
    }

    /// Determine if an audio device is a playback device (instead of recording).
    ///
    /// ## Function Parameters
    /// * `self`: The device ID to query.
    ///
    /// ## Return Value
    /// Returns true if this is a playback device, false if it is recording.
    ///
    /// ## Remarks
    /// This function may return either true or false for invalid device IDs.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn isPlayback(
        self: Device,
    ) bool {
        return c.SDL_IsAudioDevicePlayback(self.value);
    }

    /// Open a specific audio device.
    ///
    /// ## Function Parameters
    /// * `self`: The audio device to open.
    /// * `spec`: The audio stream's data format.
    ///
    /// ## Return Value
    /// Returns an audio device.
    ///
    /// ## Remarks
    /// You can open both playback and recording devices through this function.
    /// Playback devices will take data from bound audio streams, mix it, and send it to the hardware.
    /// Recording devices will feed any bound audio streams with a copy of any incoming data.
    ///
    /// An opened audio device starts out with no audio streams bound.
    /// To start audio playing, bind a stream and supply audio data to it.
    /// Unlike SDL2, there is no audio callback; you only bind audio streams and make sure they have data flowing into them
    /// (however, you can simulate SDL2's semantics fairly closely by using `audio.Device.openStream()` instead of this function).
    ///
    /// If you don't care about opening a specific device, pass a devid of either `audio.Device.default_playback` or `audio.Device.default_recording`.
    /// In this case, SDL will try to pick the most reasonable default, and may also switch between physical devices seamlessly later,
    /// if the most reasonable default changes during the lifetime of this opened device (user changed the default in the OS's system preferences,
    /// the default got unplugged so the system jumped to a new default, the user plugged in headphones on a mobile device, etc).
    /// Unless you have a good reason to choose a specific device, this is probably what you want.
    ///
    /// You may request a specific format for the audio device, but there is no promise the device will honor that request for several reasons.
    /// As such, it's only meant to be a hint as to what data your app will provide.
    /// Audio streams will accept data in whatever format you specify and manage conversion for you as appropriate.
    /// `audio.Device.getFormat()` can tell you the preferred format for the device before opening and the actual format the device is using after opening.
    ///
    /// It's legal to open the same device ID more than once; each successful open will generate a new logical `audio.Device` that is managed separately
    /// from others on the same physical device.
    /// This allows libraries to open a device separately from the main app and bind its own streams without conflicting.
    ///
    /// It is also legal to open a device ID returned by a previous call to this function; doing so just creates another logical device on the same physical device.
    /// This may be useful for making logical groupings of audio streams.
    ///
    /// This function returns the opened device ID on success.
    /// This is a new, unique `audio.Device` that represents a logical device.
    ///
    /// Some backends might offer arbitrary devices (for example, a networked audio protocol that can connect to an arbitrary server).
    /// For these, as a change from SDL2, you should open a default device ID and use an SDL hint to specify the target if you care,
    /// or otherwise let the backend figure out a reasonable default.
    /// Most backends don't offer anything like this, and often this would be an end user setting an environment variable for their custom need,
    /// and not something an application should specifically manage.
    ///
    /// When done with an audio device, possibly at the end of the app's life, one should call `audio.Device.close()` on the returned device id.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    ///
    /// ## Code Examples
    /// TODO!!!
    pub fn open(
        self: Device,
        spec: ?Spec,
    ) !Device {
        const spec_sdl: c.SDL_AudioSpec = if (spec) |val| val.toSdl() else undefined;
        const ret = c.SDL_OpenAudioDevice(
            self.value,
            if (spec != null) &spec_sdl else null,
        );
        return .{
            .value = try errors.wrapCall(c.SDL_AudioDeviceID, ret, 0),
        };
    }

    /// Convenience function for straightforward audio init for the common case.
    ///
    /// ## Function Parameters
    /// * `self`: The audio device to open.
    /// * `spec`: The audio stream's data format.
    /// * `UserData`: User data type.
    /// * `callback`: A callback where the app will provide new data for playback, or receive new data for recording. Can be `null`, in which case the app will need to call `audio.Stream.putData()` or `audio.Stream.getData()` as necessary.
    /// * `user_data`: App-controlled pointer passed to callback.
    ///
    /// ## Return Value
    /// Returns an audio stream that must be freed with `audio.Stream.deinit()`.
    ///
    /// ## Remarks
    /// If all your app intends to do is provide a single source of PCM audio, this function allows you to do all your audio setup in a single call.
    ///
    /// This is also intended to be a clean means to migrate apps from SDL2.
    ///
    /// This function will open an audio device, create a stream and bind it.
    /// Unlike other methods of setup, the audio device will be closed when this stream is destroyed,
    /// so the app can treat the returned `audio.Stream` as the only object needed to manage audio playback.
    ///
    /// Also unlike other functions, the audio device begins paused.
    /// This is to map more closely to SDL2-style behavior, since there is no extra step here to bind a stream to begin audio flowing.
    /// The audio device should be resumed with `audio.Device.resumeStream()`.
    ///
    /// This function works with both playback and recording devices.
    ///
    /// The spec parameter represents the app's side of the audio stream.
    /// That is, for recording audio, this will be the output format, and for playing audio, this will be the input format.
    /// If `spec` is `null`, the system will choose the format, and the app can use `audio.Stream.getFormat()` to obtain this information later.
    ///
    /// If you don't care about opening a specific audio device, you can (and probably should),
    /// use `audio.Device.default_playback` for playback and `audio.Device.default_recording` for recording.
    ///
    /// One can optionally provide a callback function; if `null`, the app is expected to queue audio data for playback (or unqueue audio data if capturing).
    /// Otherwise, the callback will begin to fire once the device is unpaused.
    ///
    /// Destroying the returned stream with `audio.Stream.deinit()` will also close the audio device associated with this stream.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn openStream(
        self: Device,
        spec: ?Spec,
        comptime UserData: type,
        comptime callback: ?StreamCallback(UserData),
        user_data: ?*UserData,
    ) !Stream {
        const Cb = struct {
            pub fn run(
                user_data_c: ?*anyopaque,
                stream_c: ?*c.SDL_AudioStream,
                additional_amount_c: c_int,
                total_amount_c: c_int,
            ) callconv(.c) void {
                callback.?(@ptrCast(@alignCast(user_data_c)), .{ .value = stream_c.? }, @intCast(additional_amount_c), @intCast(total_amount_c));
            }
        };
        const spec_sdl: c.SDL_AudioSpec = if (spec) |val| val.toSdl() else undefined;
        const ret = c.SDL_OpenAudioDeviceStream(
            self.value,
            if (spec != null) &spec_sdl else null,
            if (callback != null) Cb.run else null,
            @ptrCast(user_data),
        );
        return .{
            .value = try errors.wrapCallNull(*c.SDL_AudioStream, ret),
        };
    }

    /// Use this function to pause audio playback on a specified device.
    ///
    /// ## Function Parameters:
    /// * `self`: A device opened by `audio.Device.open()`.
    ///
    /// ## Remarks
    /// This function pauses audio processing for a given device.
    /// Any bound audio streams will not progress, and no audio will be generated.
    /// Pausing one device does not prevent other unpaused devices from running.
    ///
    /// Unlike in SDL2, audio devices start in an unpaused state, since an app has to bind a stream before any audio will flow.
    /// Pausing a paused device is a legal no-op.
    ///
    /// Pausing a device can be useful to halt all audio without unbinding all the audio streams.
    /// This might be useful while a game is paused, or a level is loading, etc.
    ///
    /// Physical devices can not be paused or unpaused, only logical devices created through `audio.Device.open()` can be.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn pausePlayback(
        self: Device,
    ) !void {
        const ret = c.SDL_PauseAudioDevice(
            self.value,
        );
        return errors.wrapCallBool(ret);
    }

    /// Use this function to unpause audio playback on a specified device.
    ///
    /// ## Function Parameters
    /// * `self`: A device opened by `audio.Device.open()`.
    ///
    /// ## Remarks
    /// This function unpauses audio processing for a given device that has previously been paused with `audio.Device.pausePlayback()`.
    /// Once unpaused, any bound audio streams will begin to progress again, and audio can be generated.
    ///
    /// Unlike in SDL2, audio devices start in an unpaused state, since an app has to bind a stream before any audio will flow.
    /// Unpausing an unpaused device is a legal no-op.
    ///
    /// Physical devices can not be paused or unpaused, only logical devices created through `audio.Device.open()` can be.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn resumePlayback(
        self: Device,
    ) !void {
        const ret = c.SDL_ResumeAudioDevice(
            self.value,
        );
        return errors.wrapCallBool(ret);
    }

    /// Change the gain of an audio device.
    ///
    /// ## Function Parameters
    /// * `self`: The audio device on which to change gain.
    /// * `gain`: The gain. `1` is no change, `0` is silence.
    ///
    /// ## Remarks
    /// The gain of a device is its volume; a larger gain means a louder output, with a gain of zero being silence.
    ///
    /// Audio devices default to a gain of `1` (no change in output).
    ///
    /// Physical devices may not have their gain changed, only logical devices, and this function will always return and error when used on physical devices.
    /// While it might seem attractive to adjust several logical devices at once in this way,
    /// it would allow an app or library to interfere with another portion of the program's otherwise-isolated devices.
    ///
    /// This is applied, along with any per-audiostream gain, during playback to the hardware, and can be continuously changed to create various effects.
    /// On recording devices, this will adjust the gain before passing the data into an audiostream;
    /// that recording audiostream can then adjust its gain further when outputting the data elsewhere,
    /// if it likes, but that second gain is not applied until the data leaves the audiostream again.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread, as it holds a stream-specific mutex while running.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setGain(
        self: Device,
        gain: f32,
    ) !void {
        const ret = c.SDL_SetAudioDeviceGain(
            self.value,
            gain,
        );
        return errors.wrapCallBool(ret);
    }

    /// Set a callback that fires when data is about to be fed to an audio device.
    ///
    /// ## Function Parameters
    /// * `self`: The ID of an opened audio device.
    /// * `UserData`: User data type.
    /// * `callback`: A callback function to be called, can be `null`.
    /// * `user_data`: App-controlled pointer passed to callback.
    ///
    /// ## Remarks
    /// This is useful for accessing the final mix, perhaps for writing a visualizer or applying a final effect to the audio data before playback.
    ///
    /// The buffer is the final mix of all bound audio streams on an opened device; this callback will fire regularly for any device that is both opened and unpaused.
    /// If there is no new data to mix, either because no streams are bound to the device or all the streams are empty,
    /// this callback will still fire with the entire buffer set to silence.
    ///
    /// This callback is allowed to make changes to the data; the contents of the buffer after this call is what is ultimately passed along to the hardware.
    ///
    /// The callback is always provided the data in float format (values from `-1` to `1`),
    /// but the number of channels or sample rate may be different than the format the app requested when opening the device;
    /// SDL might have had to manage a conversion behind the scenes, or the playback might have jumped to new physical hardware when a system default changed, etc.
    /// These details may change between calls. Accordingly, the size of the buffer might change between calls as well.
    ///
    /// This callback can run at any time, and from any thread; if you need to serialize access to your app's data,
    /// you should provide and use a mutex or other synchronization device.
    ///
    /// All of this to say: there are specific needs this callback can fulfill, but it is not the simplest interface.
    /// Apps should generally provide audio in their preferred format through an `audio.Stream` and let SDL handle the difference.
    ///
    /// This function is extremely time-sensitive; the callback should do the least amount of work possible and return as quickly as it can.
    /// The longer the callback runs, the higher the risk of audio dropouts or other problems.
    ///
    /// This function will block until the audio device is in between iterations,
    /// so any existing callback that might be running will finish before this function sets the new callback and returns.
    ///
    /// Setting a `null` callback function disables any previously-set callback.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setPostmixCallback(
        self: Device,
        comptime UserData: type,
        comptime callback: ?PostmixCallback(UserData),
        user_data: ?*UserData,
    ) !void {
        const Cb = struct {
            pub fn run(user_data_c: ?*anyopaque, spec_c: [*c]const c.SDL_AudioSpec, buffer_c: [*c]f32, buffer_len_c: c_int) callconv(.c) void {
                callback.?(@ptrCast(@alignCast(user_data_c)), Spec.fromSdl(spec_c.*), buffer_c[0..@intCast(buffer_len_c)]);
            }
        };
        return errors.wrapCallBool(c.SDL_SetAudioPostmixCallback(self.value, Cb.run orelse null, @ptrCast(user_data)));
    }
};

/// The opaque handle that represents an audio stream.
///
/// ## Remarks
/// This is an audio conversion interface:
/// * It can handle resampling data in chunks without generating artifacts, when it doesn't have the complete buffer available.
/// * It can handle incoming data in any variable size.
/// * It can handle input/output format changes on the fly.
/// * It can remap audio channels between inputs and outputs.
/// * You push data as you have it, and pull it when you need it
/// * It can also function as a basic audio data queue even if you just have sound that needs to pass from one place to another.
/// * You can hook callbacks up to them when more data is added or requested, to manage data on-the-fly.
///
/// Audio streams are the core of the SDL3 audio interface.
/// You create one or more of them, bind them to an opened audio device, and feed data to them (or for recording, consume data from them).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Stream = extern struct {
    value: *c.SDL_AudioStream,

    // Size tests.
    comptime {
        std.debug.assert(@sizeOf(*c.SDL_AudioStream) == @sizeOf(Stream));
    }

    /// Properties for an audio stream.
    ///
    /// ## Version
    /// This struct is provided by zig-sdl3.
    pub const Properties = struct {
        /// If true (the default), the stream be automatically cleaned up when the audio subsystem quits.
        /// If set to false, the streams will persist beyond that.
        /// This property is ignored for streams created through `Device.openStream()`, and will always be cleaned up.
        /// Streams that are not cleaned up will still be unbound from devices when the audio subsystem quits.
        /// This property was added in SDL 3.4.0.
        auto_cleanup: ?bool = null,

        /// Convert from an SDL value.
        pub fn fromSdl(value: properties.Group) Properties {
            return .{
                .auto_cleanup = if (value.get(c.SDL_PROP_AUDIOSTREAM_AUTO_CLEANUP_BOOLEAN)) |val| val.boolean else null,
            };
        }

        /// Convert to an SDL value.
        pub fn toSdl(self: Properties, group: properties.Group) !void {
            if (self.auto_cleanup) |val|
                try group.set(c.SDL_PROP_AUDIOSTREAM_AUTO_CLEANUP_BOOLEAN, .{ .boolean = val });
        }
    };

    /// Clear any pending data in the stream.
    ///
    /// ## Function Parameters
    /// * `self`: The audio stream to clear.
    ///
    /// ## Remarks
    /// This drops any queued data, so there will be nothing to read from the stream until more is added.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn clear(
        self: Stream,
    ) !void {
        return errors.wrapCallBool(c.SDL_ClearAudioStream(self.value));
    }

    /// Free an audio stream.
    ///
    /// ## Function Parameters
    /// * `self`: The audio stream to destroy.
    ///
    /// ## Remarks
    /// This will release all allocated data, including any audio that is still queued.
    /// You do not need to manually clear the stream first.
    ///
    /// If this stream was bound to an audio device, it is unbound during this call.
    /// If this stream was created with `audio.Device.open()`, the audio device that was opened alongside this stream's creation will be closed, too.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn deinit(
        self: Stream,
    ) void {
        c.SDL_DestroyAudioStream(self.value);
    }

    /// Tell the stream that you're done sending data, and anything being buffered should be converted/resampled and made available immediately.
    ///
    /// ## Function Parameters
    /// * `self`: The audio stream to flush.
    ///
    /// ## Remarks
    /// It is legal to add more data to a stream after flushing, but there may be audio gaps in the output.
    /// Generally this is intended to signal the end of input, so the complete output becomes available.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn flush(
        self: Stream,
    ) !void {
        return errors.wrapCallBool(c.SDL_FlushAudioStream(self.value));
    }

    /// Get the number of converted/resampled bytes available.
    ///
    /// ## Function Parameters
    /// * `self`: The audio stream to query.
    ///
    /// ## Return Value
    /// Returns the number of converted/resampled bytes available.
    ///
    /// ## Remarks
    /// The stream may be buffering data behind the scenes until it has enough to resample correctly, so this number might be lower than what you expect, or even be zero.
    /// Add more data or flush the stream if you need the data now.
    ///
    /// If the stream has so much data that it would overflow, the return value is clamped to a maximum value, but no queued data is lost; if there are gigabytes of data queued,
    /// the app might need to read some of it with `audio.Stream.getData()` before this function's return value is no longer clamped.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getAvailable(
        self: Stream,
    ) usize {
        return @intCast(c.SDL_GetAudioStreamAvailable(self.value));
    }

    /// Query an audio stream for its currently-bound device.
    ///
    /// ## Function Parameters
    /// * `self`: The audio stream to query.
    ///
    /// ## Return Value
    /// Returns the bound audio device if bound and valid, `null` otherwise.
    ///
    /// ## Remarks
    /// This reports the audio device that an audio stream is currently bound to.
    ///
    /// If not bound, or invalid, this returns `null`.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getDevice(
        self: Stream,
    ) ?Device {
        const ret = c.SDL_GetAudioStreamDevice(
            self.value,
        );
        if (ret == 0)
            return null;
        return .{ .value = ret };
    }

    /// Use this function to query if an audio device associated with a stream is paused.
    ///
    /// ## Function Parameters
    /// * `self`: The audio stream associated with the audio device to query.
    ///
    /// ## Return Value
    /// Returns true if device is valid and paused, false otherwise.
    ///
    /// ## Remarks
    /// Unlike in SDL2, audio devices start in an *unpaused* state, since an app has to bind a stream before any audio will flow.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getDevicePaused(
        self: Stream,
    ) bool {
        return c.SDL_AudioStreamDevicePaused(self.value);
    }

    /// Query the current format of an audio stream.
    ///
    /// ## Function Parameaters
    /// * `self`: The stream to query.
    ///
    /// ## Return Value
    /// Returns the input and output formats of the stream in that order.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread, as it holds a stream-specific mutex while running.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getFormat(
        self: Stream,
    ) !struct { Spec, Spec } {
        var input_format: c.SDL_AudioSpec = undefined;
        var output_format: c.SDL_AudioSpec = undefined;
        try errors.wrapCallBool(c.SDL_GetAudioStreamFormat(self.value, &input_format, &output_format));
        return .{
            Spec.fromSdl(input_format),
            Spec.fromSdl(output_format),
        };
    }

    /// Get the frequency ratio of an audio stream.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to query.
    ///
    /// ## Return Value
    /// Returns the frequency ratio of the stream.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread, as it holds a stream-specific mutex while running.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getFrequencyRatio(
        self: Stream,
    ) !f32 {
        return errors.wrapCall(f32, c.SDL_GetAudioStreamFrequencyRatio(self.value), 0);
    }

    /// Get the gain of an audio stream.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to query.
    ///
    /// ## Return Value
    /// Returns the gain of the stream.
    ///
    /// ## Remarks
    /// The gain of a stream is its volume; a larger gain means a louder output, with a gain of zero being silence.
    ///
    /// Audio streams default to a gain of `1` (no change in output).
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread, as it holds a stream-specific mutex while running.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getGain(
        self: Stream,
    ) !f32 {
        return errors.wrapCall(f32, c.SDL_GetAudioStreamGain(self.value), -1);
    }

    /// Get the current input channel map of an audio stream.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to query.
    ///
    /// ## Return Value
    /// Returns an array of the current channel mapping, with as many elements as the current output spec's channels, or `null` if default.
    /// This should be freed with `free()` when it is no longer needed.
    ///
    /// ## Remarks
    /// Channel maps are optional; most things do not need them, instead passing data in [the order that SDL expects](https://wiki.libsdl.org/SDL3/CategoryAudio#channel-layouts).
    ///
    /// Audio streams default to no remapping applied.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread, as it holds a stream-specific mutex while running.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getInputChannelMap(
        self: Stream,
    ) ?[]c_int {
        var count: c_int = undefined;
        const ret = c.SDL_GetAudioStreamInputChannelMap(self.value, &count);
        if (ret == null)
            return null;
        return ret[0..@intCast(count)];
    }

    /// Get the properties associated with an audio stream.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to query.
    ///
    /// ## Return Value
    /// Returns properties for the stream.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getProperties(
        self: Stream,
    ) !Properties {
        const group = properties.Group{
            .value = try errors.wrapCall(c.SDL_PropertiesID, c.SDL_GetAudioStreamProperties(self.value), 0),
        };
        return Properties.fromSdl(group);
    }

    /// Get the number of bytes currently queued.
    ///
    /// ## Function Parameters
    /// * `self`: The audio stream to query.
    ///
    /// ## Return Value
    /// Returns the number of bytes queued.
    ///
    /// ## Remarks
    /// This is the number of bytes put into a stream as input, not the number that can be retrieved as output.
    /// Because of several details, it's not possible to calculate one number directly from the other.
    /// If you need to know how much usable data can be retrieved right now, you should use `audio.Stream.getAvailable()` and not this function.
    ///
    /// Note that audio streams can change their input format at any time, even if there is still data queued in a different format,
    /// so the returned byte count will not necessarily match the number of sample frames available.
    /// Users of this API should be aware of format changes they make when feeding a stream and plan accordingly.
    ///
    /// Queued data is not converted until it is consumed by `audio.Stream.getData()`, so this value should be representative of the exact data that was put into the stream.
    ///
    /// If the stream has so much data that it would overflow an int, the return value is clamped to a maximum value, but no queued data is lost;
    /// if there are gigabytes of data queued, the app might need to read some of it with `audio.Stream.getData()` before this function's return value is no longer clamped.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getQueued(
        self: Stream,
    ) !usize {
        return @intCast(try errors.wrapCall(c_int, c.SDL_GetAudioStreamQueued(self.value), -1));
    }

    /// Get the current output channel map of an audio stream.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to query.
    ///
    /// ## Return Value
    /// Returns an array of the current channel mapping, with as many elements as the current output spec's channels, or `null` if default.
    /// This should be freed with `free()` when it is no longer needed.
    ///
    /// ## Remarks
    /// Channel maps are optional; most things do not need them, instead passing data in [the order that SDL expects](https://wiki.libsdl.org/SDL3/CategoryAudio#channel-layouts).
    ///
    /// Audio streams default to no remapping applied.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread, as it holds a stream-specific mutex while running.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getOutputChannelMap(
        self: Stream,
    ) ?[]c_int {
        var count: c_int = undefined;
        const ret = c.SDL_GetAudioStreamOutputChannelMap(self.value, &count);
        if (ret == null)
            return null;
        return ret[0..@intCast(count)];
    }

    /// Create a new audio stream.
    ///
    /// ## Function Parameters
    /// * `src_spec`: The format details of the input audio.
    /// * `dst_spec`: The format details of the output audio.
    ///
    /// ## Return Value
    /// Returns a new audio stream.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn init(
        src_spec: Spec,
        dst_spec: Spec,
    ) !Stream {
        const src_spec_sdl = src_spec.toSdl();
        const dst_spec_sdl = dst_spec.toSdl();
        return .{
            .value = try errors.wrapCallNull(*c.SDL_AudioStream, c.SDL_CreateAudioStream(&src_spec_sdl, &dst_spec_sdl)),
        };
    }

    /// Lock an audio stream for serialized access.
    ///
    /// ## Function Parameters
    /// * `self`: The audio stream to lock.
    ///
    /// ## Remarks
    /// Each `audio.Stream` has an internal mutex it uses to protect its data structures from threading conflicts.
    /// This function allows an app to lock that mutex, which could be useful if registering callbacks on this stream.
    ///
    /// One does not need to lock a stream to use in it most cases, as the stream manages this lock internally.
    /// However, this lock is held during callbacks, which may run from arbitrary threads at any time,
    /// so if an app needs to protect shared data during those callbacks, locking the stream guarantees that the callback is not running while the lock is held.
    ///
    /// As this is just a wrapper over `mutex.Lock` for an internal lock; it has all the same attributes (recursive locks are allowed, etc).
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn lock(
        self: Stream,
    ) !void {
        return errors.wrapCallBool(c.SDL_LockAudioStream(self.value));
    }

    /// Use this function to pause audio playback on the audio device associated with an audio stream.
    ///
    /// ## Function Parameters
    /// * `self`: The audio stream associated with the audio device to pause.
    ///
    /// ## Remarks
    /// This function pauses audio processing for a given device.
    /// Any bound audio streams will not progress, and no audio will be generated.
    /// Pausing one device does not prevent other unpaused devices from running.
    ///
    /// Pausing a device can be useful to halt all audio without unbinding all the audio streams.
    /// This might be useful while a game is paused, or a level is loading, etc.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn pauseDevice(
        self: Stream,
    ) !void {
        return errors.wrapCallBool(c.SDL_PauseAudioStreamDevice(self.value));
    }

    /// Get converted/resampled data from the stream.
    ///
    /// ## Function Parameters
    /// * `self`: The stream the audio is being requested from.
    /// * `buf`: The buffer to fill with audio data.
    ///
    /// ## Return Value
    /// Returns the data actually filled.
    /// Note that the returned `ret` reuses `data.ptr`, just that `ret.len <= data.len`.
    ///
    /// ## Remarks
    /// The input/output data format/channels/samplerate is specified when creating the stream, and can be changed after creation by calling `audio.Stream.setFormat()`
    ///
    /// Note that any conversion and resampling necessary is done during this call, and `audio.Stream.putData()` simply queues unconverted data for later.
    /// This is different than SDL2, where that work was done while inputting new data to the stream and requesting the output just copied the converted data.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread, but if the stream has a callback set, the caller might need to manage extra locking.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getData(
        self: Stream,
        data: []u8,
    ) ![]u8 {
        const ret = try errors.wrapCall(c_int, c.SDL_GetAudioStreamData(self.value, data.ptr, @intCast(data.len)), -1);
        return data.ptr[0..@intCast(ret)];
    }

    /// Add data to the stream.
    ///
    /// ## Function Parameters
    /// * `self`: The stream the audio data is being added to.
    /// * `data`: Buffer of audio data to read.
    ///
    /// ## Remarks
    /// This data must match the format/channels/samplerate specified in the latest call to `audio.Stream.setFormat()`,
    /// or the format specified when creating the stream if it hasn't been changed.
    ///
    /// Note that this call simply copies the unconverted data for later.
    /// This is different than SDL2, where data was converted during the Put call and the Get call would just dequeue the previously-converted data.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread, but if the stream has a callback set, the caller might need to manage extra locking.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn putData(
        self: Stream,
        data: []const u8,
    ) !void {
        return errors.wrapCallBool(c.SDL_PutAudioStreamData(
            self.value,
            data.ptr,
            @intCast(data.len),
        ));
    }

    /// Add external data to an audio stream without copying it.
    ///
    /// ## Function Parameters
    /// * `self`: The stream the audio data is being added to.
    /// * `data`: Buffer of audio data to read.
    /// * `UserData`: Type of user data for the callback.
    /// * `callback`: The callback function to call when the data is no longer needed by the stream.
    /// * `user_data`: An opaque pointer provided to the callback for its own personal use.
    ///
    /// ## Remarks
    /// Unlike `putData()`, this function does not make a copy of the provided data, instead storing the provided pointer.
    /// This means that the put operation does not need to allocate and copy the data, but the original data must remain available until the stream is done with it,
    /// either by being read from the stream in its entirety, or a call to `clear()` or `deinit()`.
    ///
    /// The data must match the format/channels/samplerate specified in the latest call to `setFormat()`,
    /// or the format specified when creating the stream if it hasn't been changed.
    ///
    /// An optional callback may be provided, which is called when the stream no longer needs the data. Once this callback fires, the stream will not access the data again.
    /// This callback will fire for any reason the data is no longer needed, including clearing or destroying the stream.
    ///
    /// Note that there is still an allocation to store tracking information, so this function is more efficient for larger blocks of data.
    /// If you're planning to put a few samples at a time, it will be more efficient to use `putData()`, which allocates and buffers in blocks.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread, but if the stream has a callback set, the caller might need to manage extra locking.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn putDataNoCopy(
        self: Stream,
        data: []const u8,
        comptime UserData: type,
        callback: ?StreamDataCompleteCallback(UserData),
        user_data: ?*UserData,
    ) !void {
        const Cb = struct {
            fn run(c_user_data: ?*anyopaque, c_buf: ?*const anyopaque, c_buf_len: c_int) void {
                callback.?(@ptrCast(@alignCast(c_user_data)), @as([*]u8, @ptrCast(@alignCast(c_buf)))[0..@intCast(c_buf_len)]);
            }
        };
        return errors.wrapCallBool(c.SDL_PutAudioStreamDataNoCopy(
            self.value,
            data.ptr,
            @intCast(data.len),
            if (callback != null) Cb.run else null,
            @ptrCast(user_data),
        ));
    }

    /// Add data to the stream with each channel in a separate array.
    ///
    /// ## Function Parameters
    /// * `self`: The stream the audio data is being added to.
    /// * `channel_buffers`: Slice of arrays for the channel buffer data.
    /// * `num_samples`: The number of samples per array to write to the stream.
    ///
    /// ## Remarks
    /// This data must match the format/channels/samplerate specified in the latest call to `setFormat()`,
    /// or the format specified when creating the stream if it hasn't been changed.
    ///
    /// The data will be interleaved and queued.
    /// Note that the stream only operates on interleaved data, so this is simply a convenience function for easily queueing data from sources that provide separate arrays.
    /// There is no equivalent function to retrieve planar data.
    ///
    /// The arrays in `channel_buffers` are ordered as they are to be interleaved; the first array will be the first sample in the interleaved data.
    /// Any individual array may be `null`; in this case, silence will be interleaved for that channel.
    ///
    /// If more channels are specified than the current input spec, they are ignored.
    /// If less channels are specified, the missing arrays are treated as if they are `null` (silence is written to those channels).
    ///
    /// Note that `num_samples` is the number of samples per array.
    /// This can also be thought of as the number of sample frames to be queued.
    /// A value of 1 with stereo arrays will queue two samples to the stream.
    /// This is different than `putData()`, which wants the size of a single array in bytes.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread, but if the stream has a callback set, the caller might need to manage extra locking.
    ///
    /// ## Version
    /// This function is available since SDL 3.4.0.
    pub fn putPlanarData(
        self: Stream,
        channel_buffers: []const [*c]const u8,
        num_samples: usize,
    ) !void {
        return errors.wrapCallBool(c.SDL_PutAudioStreamPlanarData(self.value, @ptrCast(channel_buffers), @intCast(channel_buffers.len), @intCast(num_samples)));
    }

    /// Use this function to unpause audio playback on the audio device associated with an audio stream.
    ///
    /// ## Function Parameters
    /// * `self`: The audio stream associated with the audio device to resume.
    ///
    /// ## Remarks
    /// This function unpauses audio processing for a given device that has previously been paused.
    /// Once unpaused, any bound audio streams will begin to progress again, and audio can be generated.
    ///
    /// `audio.Device.openStream()` opens audio devices in a paused state, so this function call is required for audio playback to begin on such devices.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn resumeDevice(
        self: Stream,
    ) !void {
        return errors.wrapCallBool(c.SDL_ResumeAudioStreamDevice(self.value));
    }

    /// Change the input and output formats of an audio stream.
    ///
    /// ## Function Parameters
    /// * `self`: The stream the format is being changed.
    /// * `src_spec`: The new format of the audio input; if `null`, it is not changed.
    /// * `dst_spec`: The new format of the audio output; if `null`, it is not changed.
    ///
    /// ## Remarks
    /// Future calls to and `audio.Stream.getAvailable()` and `audio.Stream.getData()` will reflect the new format,
    /// and future calls to `audio.Stream.putData()` must provide data in the new input formats.
    ///
    /// Data that was previously queued in the stream will still be operated on in the format that was current when it was added,
    /// which is to say you can put the end of a sound file in one format to a stream, change formats for the next sound file,
    /// and start putting that new data while the previous sound file is still queued, and everything will still play back correctly.
    ///
    /// If a stream is bound to a device, then the format of the side of the stream bound to a device cannot be changed
    /// (`src_spec` for recording devices, `dst_spec` for playback devices).
    /// Attempts to make a change to this side will be ignored, but this will not report an error.
    /// The other side's format can be changed.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread, as it holds a stream-specific mutex while running.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setFormat(
        self: Stream,
        src_spec: ?Spec,
        dst_spec: ?Spec,
    ) !void {
        const src_spec_sdl: c.SDL_AudioSpec = if (src_spec) |val| val.toSdl() else undefined;
        const dst_spec_sdl: c.SDL_AudioSpec = if (dst_spec) |val| val.toSdl() else undefined;
        return errors.wrapCallBool(c.SDL_SetAudioStreamFormat(
            self.value,
            if (src_spec != null) &src_spec_sdl else null,
            if (dst_spec != null) &dst_spec_sdl else null,
        ));
    }

    /// Change the frequency ratio of an audio stream.
    ///
    /// ## Function Parameters
    /// * `self`: The stream on which the gain is being changed.
    /// * `frequency_ratio`: The frequency ratio. `1` is normal speed. Must be between `0.01` and `100`.
    ///
    /// ## Remarks
    /// The frequency ratio is used to adjust the rate at which input data is consumed.
    /// Changing this effectively modifies the speed and pitch of the audio.
    /// A value greater than `1` will play the audio faster, and at a higher pitch.
    /// A value less than `1` will play the audio slower, and at a lower pitch.
    ///
    /// This is applied during `audio.Stream.getData()`, and can be continuously changed to create various effects.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread, as it holds a stream-specific mutex while running.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setFrequencyRatio(
        self: Stream,
        frequency_ratio: f32,
    ) !void {
        return errors.wrapCallBool(c.SDL_SetAudioStreamFrequencyRatio(self.value, frequency_ratio));
    }

    /// Change the gain of an audio stream.
    ///
    /// ## Function Parameters
    /// * `self`: The stream on which the gain is being changed.
    /// * `gain`: The gain. `1` is no change, `0` is silence.
    ///
    /// ## Remarks
    /// The gain of a stream is its volume; a larger gain means a louder output, with a gain of zero being silence.
    ///
    /// Audio streams default to a gain of `1` (no change in output).
    ///
    /// This is applied during `audio.Stream.getData()`, and can be continuously changed to create various effects.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread, as it holds a stream-specific mutex while running.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setGain(
        self: Stream,
        gain: f32,
    ) !void {
        return errors.wrapCallBool(c.SDL_SetAudioStreamGain(self.value, gain));
    }

    /// Set a callback that runs when data is requested from an audio stream.
    ///
    /// ## Function Parameters
    /// * `self`: The audio stream to set the new callback on.
    /// * `UserData`: Type of user data.
    /// * `callback`: The new callback function to call when data is requested from the stream.
    /// * `user_data`: An opaque pointer provided to the callback for its own personal use.
    ///
    /// ## Remarks
    /// This callback is called before data is obtained from the stream, giving the callback the chance to add more on-demand.
    ///
    /// The callback can (optionally) call `audio.Stream.putData()` to add more audio to the stream during this call; if needed,
    /// the request that triggered this callback will obtain the new data immediately.
    ///
    /// The callback's additional_amount argument is roughly how many bytes of unconverted data (in the stream's input format) is needed by the caller,
    /// although this may overestimate a little for safety.
    /// This takes into account how much is already in the stream and only asks for any extra necessary to resolve the request,
    /// which means the callback may be asked for zero bytes, and a different amount on each call.
    ///
    /// The callback is not required to supply exact amounts; it is allowed to supply too much or too little or none at all.
    /// The caller will get what's available, up to the amount they requested, regardless of this callback's outcome.
    ///
    /// Clearing or flushing an audio stream does not call this callback.
    ///
    /// This function obtains the stream's lock, which means any existing callback (get or put) in progress will finish running before setting the new callback.
    ///
    /// Setting a `null` function turns off the callback.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setGetCallback(
        self: Stream,
        comptime UserData: type,
        comptime callback: ?StreamCallback(UserData),
        user_data: ?*UserData,
    ) void {
        const Cb = struct {
            pub fn run(
                user_data_c: ?*anyopaque,
                stream_c: ?*c.SDL_AudioStream,
                additional_amount_c: c_int,
                total_amount_c: c_int,
            ) callconv(.c) void {
                callback.?(@ptrCast(@alignCast(user_data_c)), .{ .value = stream_c.? }, @intCast(additional_amount_c), @intCast(total_amount_c));
            }
        };
        _ = c.SDL_SetAudioStreamGetCallback(
            self.value,
            if (callback != null) Cb.run else null,
            @ptrCast(user_data),
        );
    }

    /// Set the current output channel map of an audio stream.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to change.
    /// * `channel_map`: Set the current input channel map of an audio stream.
    ///
    /// ## Remarks
    /// Channel maps are optional; most things do not need them, instead passing data in the order that SDL expects.
    ///
    /// The input channel map reorders data that is added to a stream via `audio.Stream.putData()`.
    /// Future calls to `audio.Stream.putData()` must provide data in the new channel order.
    ///
    /// Each item in the slice represents an input channel, and its value is the channel that it should be remapped to.
    /// To reverse a stereo signal's left and right values, you'd have a slice of `&.{ 1, 0 }`.
    /// It is legal to remap multiple channels to the same thing, so `&.{ 1, 1 }` would duplicate the right channel to both channels of a stereo signal.
    /// An element in the channel map set to `-1` instead of a valid channel will mute that channel, setting it to a silence value.
    ///
    /// You cannot change the number of channels through a channel map, just reorder/mute them.
    ///
    /// The output channel map can be changed at any time, as output remapping is applied during `audio.Stream.getData()`.
    ///
    /// Audio streams default to no remapping applied.
    /// Passing a `null` channel map is legal, and turns off remapping.
    ///
    /// SDL will copy the channel map; the caller does not have to save this array after this call.
    ///
    /// Unlike attempting to change the stream's format, the output channel map on a stream bound to a recording device is permitted to change at any time;
    /// any data added to the stream after this call will have the new mapping, but previously-added data will still have the prior mapping.
    /// When the channel map doesn't match the hardware's channel layout, SDL will convert the data before feeding it to the device for playback.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread, as it holds a stream-specific mutex while running.
    /// Don't change the stream's format to have a different number of channels from a a different thread at the same time, though!
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setInputChannelMap(
        self: Stream,
        channel_map: ?[]const c_int,
    ) !void {
        return errors.wrapCallBool(
            c.SDL_SetAudioStreamInputChannelMap(self.value, if (channel_map) |val| val.ptr else null, @intCast(if (channel_map) |val| val.len else 0)),
        );
    }

    /// Set the current output channel map of an audio stream.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to change.
    /// * `channel_map`: The new channel map, or `null` to reset to default.
    ///
    /// ## Remarks
    /// Channel maps are optional; most things do not need them, instead passing data in the order that SDL expects.
    ///
    /// The output channel map reorders data that leaving a stream via `audio.Stream.getData()`.
    ///
    /// Each item in the slice represents an input channel, and its value is the channel that it should be remapped to.
    /// To reverse a stereo signal's left and right values, you'd have a slice of `&.{ 1, 0 }`.
    /// It is legal to remap multiple channels to the same thing, so `&.{ 1, 1 }` would duplicate the right channel to both channels of a stereo signal.
    /// An element in the channel map set to `-1` instead of a valid channel will mute that channel, setting it to a silence value.
    ///
    /// You cannot change the number of channels through a channel map, just reorder/mute them.
    ///
    /// The output channel map can be changed at any time, as output remapping is applied during `audio.Stream.getData()`.
    ///
    /// Audio streams default to no remapping applied.
    /// Passing a `null` channel map is legal, and turns off remapping.
    ///
    /// SDL will copy the channel map; the caller does not have to save this array after this call.
    ///
    /// Unlike attempting to change the stream's format, the output channel map on a stream bound to a recording device is permitted to change at any time;
    /// any data added to the stream after this call will have the new mapping, but previously-added data will still have the prior mapping.
    /// When the channel map doesn't match the hardware's channel layout, SDL will convert the data before feeding it to the device for playback.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread, as it holds a stream-specific mutex while running.
    /// Don't change the stream's format to have a different number of channels from a a different thread at the same time, though!
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setOutputChannelMap(
        self: Stream,
        channel_map: ?[]const c_int,
    ) !void {
        return errors.wrapCallBool(
            c.SDL_SetAudioStreamOutputChannelMap(self.value, if (channel_map) |val| val.ptr else null, @intCast(if (channel_map) |val| val.len else 0)),
        );
    }

    /// Set the properties associated with an audio stream.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to set properties for.
    /// * `props`: The properties to set.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setProperties(
        self: Stream,
        props: Properties,
    ) !void {
        try props.toSdl(.{
            .value = try errors.wrapCall(c.SDL_PropertiesID, c.SDL_GetAudioStreamProperties(self.value), 0),
        });
    }

    /// Set a callback that runs when data is added to an audio stream.
    ///
    /// ## Function Parameters
    /// * `self`: The audio stream to set the new callback on.
    /// * `UserData`: Type of user data.
    /// * `callback`: The new callback function to call when data is added to the stream.
    /// * `user_data`: An opaque pointer provided to the callback for its own personal use.
    ///
    /// ## Remarks
    /// This callback is called after the data is added to the stream, giving the callback the chance to obtain it immediately.
    ///
    /// The callback can (optionally) call `audio.Stream.getData()` to obtain audio from the stream during this call.
    ///
    /// The callback's additional_amount argument is how many bytes of converted data (in the stream's output format) was provided by the caller,
    /// although this may underestimate a little for safety.
    /// This value might be less than what is currently available in the stream, if data was already there,
    /// and might be less than the caller provided if the stream needs to keep a buffer to aid in resampling.
    /// Which means the callback may be provided with zero bytes, and a different amount on each call.
    ///
    /// The callback may call `audio.Stream.getAvailable()` to see the total amount currently available to read from the stream,
    /// instead of the total provided by the current call.
    ///
    /// The callback is not required to obtain all data.
    /// It is allowed to read less or none at all.
    /// Anything not read now simply remains in the stream for later access.
    ///
    /// Clearing or flushing an audio stream does not call this callback.
    ///
    /// This function obtains the stream's lock, which means any existing callback (get or put) in progress will finish running before setting the new callback.
    ///
    /// Setting a `null` function turns off the callback.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setPutCallback(
        self: Stream,
        comptime UserData: type,
        comptime callback: ?StreamCallback(UserData),
        user_data: ?*UserData,
    ) void {
        const Cb = struct {
            pub fn run(
                user_data_c: ?*anyopaque,
                stream_c: ?*c.SDL_AudioStream,
                additional_amount_c: c_int,
                total_amount_c: c_int,
            ) callconv(.c) void {
                callback.?(@ptrCast(@alignCast(user_data_c)), .{ .value = stream_c.? }, @intCast(additional_amount_c), @intCast(total_amount_c));
            }
        };
        _ = c.SDL_SetAudioStreamPutCallback(
            self.value,
            if (callback != null) Cb.run else null,
            @ptrCast(user_data),
        );
    }

    /// Unbind a single audio stream from its audio device.
    ///
    /// ## Function Parameters
    /// * `self`: An audio stream to unbind from a device.
    ///
    /// ## Remarks
    /// This is a convenience function, equivalent to calling `audio.unbindStreams(&.{stream})`.
    ///
    /// ## Thread
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn unbind(
        self: Stream,
    ) void {
        c.SDL_UnbindAudioStream(self.value);
    }

    /// Unlock an audio stream for serialized access.
    ///
    /// ## Function Parameters
    /// * `self`: The audio stream to unlock.
    ///
    /// ## Remarks
    /// This unlocks an audio stream after a call to `audio.Stream.lock()`.
    ///
    /// ## Thread Safety
    /// You should only call this from the same thread that previously called `audio.Stream.lock()`.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn unlock(
        self: Stream,
    ) !void {
        return errors.wrapCallBool(c.SDL_UnlockAudioStream(self.value));
    }
};

/// Format specifier for audio data.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Spec = struct {
    /// Audio data format.
    format: Format,
    /// Number of channels.
    num_channels: usize,
    /// Sample frames per second.
    sample_rate: usize,

    /// Convert from an SDL value.
    pub fn fromSdl(data: c.SDL_AudioSpec) Spec {
        return .{
            .format = @enumFromInt(data.format),
            .num_channels = @intCast(data.channels),
            .sample_rate = @intCast(data.freq),
        };
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: Spec) c.SDL_AudioSpec {
        return .{
            .format = @intFromEnum(self.format),
            .channels = @intCast(self.num_channels),
            .freq = @intCast(self.sample_rate),
        };
    }

    /// Convert some audio data of one format to another format.
    ///
    /// ## Function Parameters
    /// * `self`: The format details of the input audio.
    /// * `src_data`: The audio data to be converted.
    /// * `dst_spec`: The format details of the output audio.
    ///
    /// ## Return Value
    /// Returns the converted audio samples.
    /// This should be freed with `free()`.
    ///
    /// ## Remarks
    /// Please note that this function is for convenience, but should not be used to resample audio in blocks,
    /// as it will introduce audio artifacts on the boundaries.
    /// You should only use this function if you are converting audio data in its entirety in one call.
    /// If you want to convert audio in smaller chunks, use an `audio.Stream`, which is designed for this situation.
    ///
    /// Internally, this function creates and destroys an `audio.Stream` on each use, so it's also less efficient than using one directly,
    /// if you need to convert multiple times.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn convertSamples(
        self: Spec,
        src_data: []const u8,
        dst_spec: Spec,
    ) ![]u8 {
        const src_spec_sdl = self.toSdl();
        const dst_spec_sdl = dst_spec.toSdl();
        var dst_data: [*c]u8 = undefined;
        var dst_len: c_int = undefined;
        try errors.wrapCallBool(c.SDL_ConvertAudioSamples(
            &src_spec_sdl,
            src_data.ptr,
            @intCast(src_data.len),
            &dst_spec_sdl,
            &dst_data,
            &dst_len,
        ));
        return dst_data[0..@intCast(dst_len)];
    }

    /// Calculate the size of each audio frame (in bytes) from an audio spec.
    ///
    /// ## Function Parameters
    /// * `self`: The audio spec to query.
    ///
    /// ## Return Value
    /// Returns the number of bytes used per sample frame.
    ///
    /// ## Remarks
    /// This reports on the size of an audio sample frame: stereo signed 16-bit data (2 channels of 2 bytes each) would be 4 bytes per frame, for example.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getFrameSize(
        self: Spec,
    ) usize {
        return @as(usize, @intCast(self.format.getByteSize())) * self.num_channels;
    }
};

/// Get the name of the current audio driver.
///
/// ## Return Value
/// Returns the name of the current audio driver or `null` if no driver has been initialized.
///
/// ## Remarks
/// The names of drivers are all simple, low-ASCII identifiers, like "alsa", "coreaudio" or "wasapi".
/// These never have Unicode characters, and are not meant to be proper names.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getCurrentDriverName() ?[:0]const u8 {
    const ret = c.SDL_GetCurrentAudioDriver();
    if (ret == null)
        return null;
    return std.mem.span(ret);
}

/// Use this function to get the name of a built in audio driver.
///
/// ## Function Parameters
/// * `index`: The index of the audio driver; the value ranges from 0 to `audio.getNumDrivers() - 1`.
///
/// ## Return Value
/// Returns the name of the audio driver at the requested index, or `null` if an invalid index was specified.
///
/// ## Remarks
/// The list of audio drivers is given in the order that they are normally initialized by default;
/// the drivers that seem more reasonable to choose first (as far as the SDL developers believe) are earlier in the list.
///
/// The names of drivers are all simple, low-ASCII identifiers, like "alsa", "coreaudio" or "wasapi".
/// These never have Unicode characters, and are not meant to be proper names.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getDriverName(
    index: usize,
) ?[:0]const u8 {
    const ret = c.SDL_GetAudioDriver(
        @intCast(index),
    );
    if (ret == null)
        return null;
    return std.mem.span(ret);
}

/// Use this function to get the number of built-in audio drivers.
///
/// ## Return Value
/// Returns the number of built-in audio drivers.
///
/// ## Remarks
/// This function returns a hardcoded number.
/// If there are no drivers compiled into this build of SDL, this function returns zero.
/// The presence of a driver in this list does not mean it will function, it just means SDL is capable of interacting with that interface.
/// For example, a build of SDL might have esound support, but if there's no esound server available, SDL's esound driver would fail if used.
///
/// By default, SDL tries all drivers, in its preferred order, until one is found to be usable.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getNumDrivers() usize {
    const ret = c.SDL_GetNumAudioDrivers();
    return @intCast(ret);
}

/// Get a list of currently-connected audio playback devices.
///
/// ## Return Value
/// Returns a slice of device instances.
/// This should be freed with `free()`.
///
/// ## Remarks
/// This returns of list of available devices that play sound, perhaps to speakers or headphones ("playback" devices).
/// If you want devices that record audio, like a microphone ("recording" devices), use `audio.getRecordingDevices()` instead.
///
/// This only returns a list of physical devices; it will not have any device IDs returned by `audio.Device.open()`.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getPlaybackDevices() ![]Device {
    var count: c_int = undefined;
    const ret = @as([*]Device, @ptrCast(try errors.wrapCallCPtr(c.SDL_AudioDeviceID, c.SDL_GetAudioPlaybackDevices(&count))));
    return ret[0..@intCast(count)];
}

/// Get a list of currently-connected audio recording devices.
///
/// ## Return Value
/// Returns a slice of device instances.
/// This should be freed with `free()`.
///
/// ## Remarks
/// This returns of list of available devices that record audio, like a microphone ("recording" devices).
/// If you want devices that play sound, perhaps to speakers or headphones ("playback" devices), use `audio.Device.getPlaybackDevices()` instead.
///
/// This only returns a list of physical devices; it will not have any device IDs returned by `audio.Device.open()`.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getRecordingDevices() ![]Device {
    var count: c_int = undefined;
    const ret = @as([*]Device, @ptrCast(try errors.wrapCallCPtr(c.SDL_AudioDeviceID, c.SDL_GetAudioRecordingDevices(&count))));
    return ret[0..@intCast(count)];
}

/// Loads a WAV from a file path.
///
/// ## Function Parameters
/// * `path`: The file path for the WAV to open.
///
/// ## Return Value
/// Returns the audio spec of the WAV along with its data in that order.
/// The `data` must be freed with `free()`.
///
/// ## Remarks
/// This is a convenience function that is effectively the same as:
/// TODO!!!
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn loadWav(
    path: [:0]const u8,
) !struct { Spec, []u8 } {
    var data: [*c]u8 = undefined;
    var len: u32 = undefined;
    var spec: c.SDL_AudioSpec = undefined;
    try errors.wrapCallBool(c.SDL_LoadWAV(
        path.ptr,
        &spec,
        &data,
        &len,
    ));
    return .{
        Spec.fromSdl(spec),
        data[0..@intCast(len)],
    };
}

/// Load the audio data of a WAV file into memory.
///
/// ## Function Parameters
/// * `src`: The data source for the WAV data.
/// * `close_io`: Will close the `src` before returning, even on error.
///
/// ## Return Value
/// Returns the audio spec of the WAV along with its data in that order.
/// The `data` must be freed with `free()`.
///
/// ## Remarks
/// The entire data portion of the file is then loaded into memory and decoded if necessary.
///
/// Supported formats are RIFF WAVE files with the formats PCM (8, 16, 24, and 32 bits), IEEE Float (32 bits), Microsoft ADPCM and IMA ADPCM (4 bits),
/// and A-law and mu-law (8 bits). Other formats are currently unsupported and cause an error.
///
/// If this function succeeds, the return value is zero and the pointer to the audio data allocated by the function is written to audio_buf and its length in bytes to audio_len.
/// The SDL_AudioSpec members freq, channels, and format are set to the values of the audio data in the buffer.
///
/// It's necessary to use SDL_free() to free the audio data returned in audio_buf when it is no longer used.
///
/// Because of the underspecification of the .WAV format, there are many problematic files in the wild that cause issues with strict decoders.
/// To provide compatibility with these files, this decoder is lenient in regards to the truncation of the file, the fact chunk, and the size of the RIFF chunk.
/// The hints `hints.Type.wave_riff_chunk_size`, `hints.Type.wave_truncation`, and `hints.Type.wave_fact_chunk` can be used to tune the behavior of the loading process.
///
/// Any file that is invalid (due to truncation, corruption, or wrong values in the headers), too big, or unsupported causes an error.
/// Additionally, any critical I/O error from the data source will terminate the loading process with an error.
///
/// It is required that the data source supports seeking.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
///
/// ## Code Examples
/// TODO!!!
pub fn loadWavIo(
    src: io_stream.Stream,
    close_io: bool,
) !struct { Spec, []u8 } {
    var data: [*c]u8 = undefined;
    var len: u32 = undefined;
    var spec: c.SDL_AudioSpec = undefined;
    try errors.wrapCallBool(c.SDL_LoadWAV_IO(
        src.value,
        close_io,
        &spec,
        &data,
        &len,
    ));
    return .{
        Spec.fromSdl(spec),
        data[0..@intCast(len)],
    };
}

/// Mix audio data in a specified format.
///
/// ## Function Parameters
/// * `dst`: The destination for the mixed audio. This should be the same length as `src`.
/// * `src`: The source audio buffer to be mixed. This should be the same length as `dst`.
/// * `format`: The structure representing the desired audio format.
/// * `volume`: Ranges from `0` to `1`, and should be set to `1` for full audio volume.
///
/// ## Remarks
/// This takes an audio buffer `src` of `len` bytes of format data and mixes it into dst, performing addition, volume adjustment, and overflow clipping.
/// The buffer pointed to by `dst` must also be `len` bytes of format data.
///
/// This is provided for convenience -- you can mix your own audio data.
///
/// Do not use this function for mixing together more than two streams of sample data.
/// The output from repeated application of this function may be distorted by clipping,
/// because there is no accumulator with greater range than the input (not to mention this being an inefficient way of doing it).
///
/// It is a common misconception that this function is required to write audio data to an output stream in an audio callback.
/// While you can do that, `audio.mix()` is really only needed when you're mixing a single audio stream with a volume adjustment.
///
/// Thread Safety
/// It is safe to call this function from any thread.
///
/// Version
/// This function is available since SDL 3.2.0.
pub fn mix(
    dst: []u8,
    src: []const u8,
    format: Format,
    volume: f32,
) !void {
    if (src.len != dst.len)
        return errors.set("Source and destination audio length for mix do not match");
    return errors.wrapCallBool(c.SDL_MixAudio(
        dst.ptr,
        src.ptr,
        @intFromEnum(format),
        @intCast(src.len),
        volume,
    ));
}

/// Unbind a list of audio streams from their audio devices.
///
/// ## Function Parameters
/// `streams`: Slice of audio streams to unbind.
///
/// ## Remarks
/// The streams being unbound do not all have to be on the same device.
/// All streams on the same device will be unbound atomically (data will stop flowing through all unbound streams on the same device at the same time).
///
/// Unbinding a stream that isn't bound to a device is a legal no-op.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn unbindStreams(
    streams: []const Stream,
) void {
    c.SDL_UnbindAudioStreams(
        @ptrCast(streams.ptr),
        @intCast(streams.len),
    );
}

// Audio related tests.
test "Audio" {
    errors.refAllDeclsRecursive(@This());
}
