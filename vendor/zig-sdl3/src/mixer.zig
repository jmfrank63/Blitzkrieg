const audio = @import("audio.zig");
const c = @import("c");
const errors = @import("errors.zig");
const io_stream = @import("io_stream.zig");
const properties = @import("properties.zig");
const std = @import("std");

comptime {
    if (!@hasDecl(c, "MIX_Init"))
        @compileError("The ext_mixer feature is either not enabled or not working!");
}

/// A callback that fires when a `Group` has completed mixing.
///
/// ## Function Parameters
/// * `user_data`: User data for personal use.
/// * `group`: The group that is being mixed.
/// * `spec`: The format of the data in `pcm`.
/// * `pcm`: The raw PCM data in `f32` format.
///
/// ## Remarks
/// This callback is fired when a mixing group has finished mixing: all tracks in the group have mixed into a single buffer and are prepared to be mixed
/// into all other groups for the final mix output.
///
/// The audio data passed through here is not const data; the app is permitted to change it in any way it likes, and those changes will propagate through the mixing pipeline.
///
/// An audio spec is provided.
/// Different groups might be in different formats, and an app needs to be able to handle that,
/// but SDL mixer always does its mixing work in 32-bit float samples, even if the inputs or final output are not floating point.
/// As such, `spec.format` will always be `floating_32_bit` and pcm hardcoded to be a float pointer.
///
/// The `samples` is the number of float values pointed to by pcm: samples, not sample frames!
/// There are no promises how many samples will be provided per-callback, and this number can vary wildly from call to call, depending on many factors.
///
/// ## Version
/// This datatype is available since SDL_mixer 3.0.0.
pub fn GroupMixCallback(comptime UserData: type) type {
    return *const fn (
        user_data: ?*UserData,
        group: Group,
        spec: audio.Spec,
        pcm: []f32,
    ) void;
}

/// A callback that fires when all mixing has completed.
///
/// ## Function Parameters
/// * `user_data`: User data for personal use.
/// * `mixer`: The mixer that is generating audio.
/// * `spec`: The format of the data in `pcm`.
/// * `pcm`: The raw PCM data in `f32` format.
///
/// ## Remarks
/// This callback is fired when the mixer has completed all its work.
/// If this mixer was created with `Mixer.initDevice`, the data provided by this callback is what is being sent to the audio hardware,
/// minus last conversions for format requirements.
/// If this mixer was created with `Mixer.init`, this is what is being output from `Mixer.generate`, after final conversions.
///
/// The audio data passed through here is not const data; the app is permitted to change it in any way it likes, and those changes will replace the final mixer pipeline output.
///
/// An audiospec is provided.
/// Different groups might be in different formats, and an app needs to be able to handle that,
/// but SDL mixer always does its mixing work in 32-bit float samples, even if the inputs or final output are not floating point.
/// As such, `spec.format` will always be `floating_32_bit` and pcm hardcoded to be a float pointer.
///
/// The `samples` is the number of float values pointed to by pcm: samples, not sample frames!
/// There are no promises how many samples will be provided per-callback, and this number can vary wildly from call to call, depending on many factors.
///
/// ## Version
/// This datatype is available since SDL_mixer 3.0.0.
pub fn PostMixCallback(comptime UserData: type) type {
    return *const fn (
        user_data: ?*UserData,
        mixer: Mixer,
        spec: audio.Spec,
        pcm: []f32,
    ) void;
}

/// A callback that fires when a track is mixing at various stages.
///
/// ## Function Parameters
/// * `user_data`: User data for personal use.
/// * `track`: The track that is being mixed.
/// * `spec`: The format of the data in `pcm`.
/// * `pcm`: The raw PCM data in `f32` format.
///
/// ## Remarks
/// This callback is fired for different parts of the mixing pipeline, and gives the app visbility into the audio data that is being generated at various stages.
///
/// The audio data passed through here is not const data; the app is permitted to change it in any way it likes, and those changes will propagate through the mixing pipeline.
///
/// An audiospec is provided.
/// Different groups might be in different formats, and an app needs to be able to handle that,
/// but SDL mixer always does its mixing work in 32-bit float samples, even if the inputs or final output are not floating point.
/// As such, `spec.format` will always be `floating_32_bit` and pcm hardcoded to be a float pointer.
///
/// The `samples` is the number of float values pointed to by pcm: samples, not sample frames!
/// There are no promises how many samples will be provided per-callback, and this number can vary wildly from call to call, depending on many factors.
///
/// Making changes to the track during this callback is undefined behavior.
/// Change the data in pcm but not the track itself.
///
/// ## Version
/// This datatype is available since SDL_mixer 3.0.0.
pub fn TrackMixCallback(comptime UserData: type) type {
    return *const fn (
        user_data: ?*UserData,
        track: Track,
        spec: audio.Spec,
        pcm: []f32,
    ) void;
}

/// A callback that fires when a MIX Track is stopped.
///
/// ## Function Parameters
/// * `user_data`: User data for personal use.
/// * `track`: The track that has stopped.
///
/// ## Remarks
/// This callback is fired when a track completes playback, either because it ran out of data to mix (and all loops were completed as well), or it was explicitly stopped by the app.
/// Pausing a track will not fire this callback.
///
/// It is legal to adjust the track, including changing its input and restarting it.
/// If this is done because it ran out of data in the middle of mixing, the mixer will start mixing the new track state in its current run without any gap in the audio.
///
/// This callback will not fire when a playing track is destroyed.
///
/// ## Version
/// This datatype is available since SDL_mixer 3.0.0.
pub fn TrackStoppedCallback(comptime UserData: type) type {
    return *const fn (
        user_data: ?*UserData,
        track: Track,
    ) void;
}

/// An opaque object that represents audio data.
///
/// ## Remarks
/// Generally you load audio data (in whatever file format) into SDL_mixer with `Audio.load` or one of its several variants, producing an `Audio` object.
///
/// An `Audio` represents static audio data; it could be background music, or maybe a laser gun sound effect.
/// It is loaded into RAM and can be played multiple times, possibly on different tracks at the same time.
///
/// Unlike most other objects, `Audio` objects can be shared between mixers.
///
/// ## Version
/// This datatype is available since SDL_mixer 3.0.0.
pub const Audio = extern struct {
    value: *c.MIX_Audio,

    /// Audio initialization properties.
    ///
    /// ## Version
    /// This struct is provided by zig-sdl3.
    pub const InitProperties = struct {
        /// Used to load audio data.
        /// This stream must be able to seek!
        stream: io_stream.Stream,
        /// True if SDL mixer should close the `stream` before returning (success or failure).
        close_io: bool,
        /// True if SDL mixer should fully decode and decompress the data before returning.
        /// Otherwise it will be stored in its original state and decompressed on demand.
        pre_decode: bool,
        /// In case steps can be made to match its format when decoding.
        preferred_mixer: ?Mixer = null,
        /// True to skip parsing metadata tags, like ID3 and APE tags.
        /// This can be used to speed up loading if the data definitely doesn't have these tags.
        /// Some decoders will fail if these tags are present when this property is `true`.
        skip_metadata_tags: ?bool = null,
        /// True to ignore metadata in the audio data specifying loop points.
        /// This will make a file decode from start to finish without looping, even if the file specified it should have.
        /// This audio can still be looped at playback time via `Track` loop settings, regardless of this setting.
        /// Default false.
        ignore_loops: ?bool = null,
        /// he name of the decoder to use for this data.
        /// If not specified, SDL mixer will examine the data and choose the best decoder.
        /// These names are the same returned from `getAudioDecoder`.
        audio_decoder: ?[:0]const u8 = null,

        /// Convert to SDL.
        pub fn toSdl(
            self: InitProperties,
        ) !properties.Group {
            const props = try properties.Group.init();
            errdefer props.deinit();

            try props.set(c.MIX_PROP_AUDIO_LOAD_IOSTREAM_POINTER, .{ .pointer = self.stream.value });
            try props.set(c.MIX_PROP_AUDIO_LOAD_CLOSEIO_BOOLEAN, .{ .boolean = self.close_io });
            try props.set(c.MIX_PROP_AUDIO_LOAD_PREDECODE_BOOLEAN, .{ .boolean = self.pre_decode });
            if (self.preferred_mixer) |preferred_mixer|
                try props.set(c.MIX_PROP_AUDIO_LOAD_PREFERRED_MIXER_POINTER, .{ .pointer = preferred_mixer.value });
            if (self.skip_metadata_tags) |skip_metadata_tags|
                try props.set(c.MIX_PROP_AUDIO_LOAD_SKIP_METADATA_TAGS_BOOLEAN, .{ .boolean = skip_metadata_tags });
            if (self.ignore_loops) |ignore_loops|
                try props.set(c.MIX_PROP_AUDIO_LOAD_IGNORE_LOOPS_BOOLEAN, .{ .boolean = ignore_loops });
            if (self.audio_decoder) |audio_decoder|
                try props.set(c.MIX_PROP_AUDIO_DECODER_STRING, .{ .string = audio_decoder });
            return props;
        }
    };

    /// Audio properties.
    ///
    /// ## Version
    /// This struct is provided by zig-sdl3.
    pub const Properties = struct {
        /// The audio's title ("Smells Like Teen Spirit").
        title: ?[:0]const u8,
        /// The audio's artist name ("Nirvana").
        artist: ?[:0]const u8,
        /// The audio's album name ("Nevermind").
        album: ?[:0]const u8,
        /// The audio's copyright info ("Copyright (c) 1991").
        copyright: ?[:0]const u8,
        /// The audio's track number on the album (1).
        track: ?i64,
        /// The total tracks on the album (13).
        total_tracks: ?usize,
        /// he year the audio was released (1991).
        year: ?usize,
        /// The sample frames worth of PCM data that comprise this audio.
        /// It might be off by a little if the decoder only knows the duration as a unit of time.
        duration_frames: ?usize,
        /// If `true`, audio never runs out of sound to generate.
        /// This isn't necessarily always known to SDL mixer, though.
        duration_infinite: ?bool,

        /// Get properties from SDL.
        pub fn fromSdl(props: properties.Group) Properties {
            return .{
                .title = if (props.get(c.MIX_PROP_METADATA_TITLE_STRING)) |val| val.string else null,
                .album = if (props.get(c.MIX_PROP_METADATA_ARTIST_STRING)) |val| val.string else null,
                .artist = if (props.get(c.MIX_PROP_METADATA_ALBUM_STRING)) |val| val.string else null,
                .copyright = if (props.get(c.MIX_PROP_METADATA_COPYRIGHT_STRING)) |val| val.string else null,
                .track = if (props.get(c.MIX_PROP_METADATA_TRACK_NUMBER)) |val| val.number else null,
                .total_tracks = if (props.get(c.MIX_PROP_METADATA_TOTAL_TRACKS_NUMBER)) |val| @intCast(val.number) else null,
                .year = if (props.get(c.MIX_PROP_METADATA_YEAR_NUMBER)) |val| @intCast(val.number) else null,
                .duration_frames = if (props.get(c.MIX_PROP_METADATA_DURATION_FRAMES_NUMBER)) |val| @intCast(val.number) else null,
                .duration_infinite = if (props.get(c.MIX_PROP_METADATA_DURATION_INFINITE_BOOLEAN)) |val| val.boolean else null,
            };
        }
    };

    /// Destroy the specified audio.
    ///
    /// ## Function Parameters
    /// * `self`: The audio object to destroy.
    ///
    /// ## Remarks
    /// Audio is reference-counted internally, so this function only unrefs it.
    /// If doing so causes the reference count to drop to zero, the audio will be deallocated.
    /// This allows the system to safely operate if the audio is still assigned to a `Track` at the time of destruction.
    /// The actual destroying will happen when the track stops using it.
    ///
    /// But from the caller's perspective, once this function is called, it should assume the audio pointer has become invalid.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn deinit(
        self: Audio,
    ) void {
        c.MIX_DestroyAudio(
            self.value,
        );
    }

    /// Convert sample frames for a MIX Audio's format to milliseconds.
    ///
    /// ## Function Parameters
    /// * `self`: The audio to query.
    /// * `frames`: The audio-specific sample frames to convert to milliseconds.
    ///
    /// ## Return Value
    /// Returns converted number of milliseconds.
    ///
    /// ## Remarks
    /// This calculates time based on the audio's initial format, even if the format would change mid-stream.
    ///
    /// Sample frames are more precise than milliseconds, so out of necessity, this function will approximate by rounding down to the closest full millisecond.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn framesToMs(
        self: Audio,
        frames: usize,
    ) !usize {
        return @intCast(try errors.wrapCall(c.Sint64, c.MIX_AudioFramesToMS(self.value, @intCast(frames)), -1));
    }

    /// Get the total playback duration of an audio object in sample frames.
    ///
    /// ## Function Parameters
    /// * `self`: The audio to query.
    ///
    /// ## Return Value
    /// Returns the duration in sample frames, or if its infinite, or `null` if unknown.
    ///
    /// ## Remarks
    /// This information is also available via the `duration_frames` property, but it's common enough to provide a simple accessor function.
    ///
    /// This reports the length of the data in sample frames, so sample-perfect mixing can be possible.
    /// Sample frames are only meaningful as a measure of time if the sample rate (frequency) is also known.
    /// To convert from sample frames to milliseconds, use `Audio.framesToMs`.
    ///
    /// Not all audio file formats can report the complete length of the data they will produce through decoding: some can't calculate it, some might produce infinite audio.
    ///
    /// Also, some file formats can only report duration as a unit of time, which means SDL mixer might have to estimate sample frames from that information.
    /// With less precision, the reported duration might be off by a few sample frames in either direction.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn getDuration(
        self: Audio,
    ) ?AudioDuration {
        const ret = c.MIX_GetAudioDuration(
            self.value,
        );
        if (ret == -1)
            return null;
        if (ret == -2)
            return .{ .infinite = {} };
        return .{ .frames = @intCast(ret) };
    }

    /// Get the audio format of an audio object.
    ///
    /// ## Function Parameters
    /// * `self`: The audio object to query.
    ///
    /// ## Return Value
    /// Returns the audio specification of the audio object.
    ///
    /// ## Remarks
    /// Note that some audio files can change format in the middle; some explicitly support this, but a more common example is two MP3 files concatenated together.
    /// In many cases, SDL_mixer will correctly handle these sort of files, but this function will only report the initial format a file uses.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn getFormat(
        self: Audio,
    ) !audio.Spec {
        var spec: c.SDL_AudioSpec = undefined;
        const ret = c.MIX_GetAudioFormat(
            self.value,
            &spec,
        );
        try errors.wrapCallBool(ret);
        return audio.Spec.fromSdl(spec);
    }

    /// Get the properties associated with an audio.
    ///
    /// ## Function Parameters
    /// * `self`: The audio to query.
    ///
    /// ## Return Value
    /// Returns the audio properties.
    ///
    /// ## Remarks
    /// Note that the metadata properties are whatever SDL mixer finds in things like ID3 tags, and they often have very little standardized formatting, may be missing,
    /// and can be completely wrong if the original data is untrustworthy (like an MP3 from a P2P file sharing service).
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL mixer 3.0.0.
    pub fn getProperties(
        self: Audio,
    ) !Properties {
        const props = try errors.wrapCall(c.SDL_PropertiesID, c.MIX_GetAudioProperties(self.value), 0);
        return Properties.fromSdl(.{ .value = props });
    }

    /// Load audio for playback from a file.
    ///
    /// ## Function Parameters
    /// * `self`: The mixer the audio will be used with.
    /// * `path`: The path on the filesystem to load the audio from.
    /// * `predecode`: If `true`, data will be fully uncompressed before returning.
    ///
    /// ## Return Value
    /// Returns an audio object that can be used to make sound on a mixer.
    ///
    /// ## Remarks
    /// This is equivalent to calling:
    /// `Audio.initIo(mixer, try sdl3.io_stream.Stream.initFromFile(path, .read_binary), predecode, true);`
    /// This function loads data from a path on the filesystem.
    /// There is also a version that loads from an IO stream (`Audio.initIo`), and one that accepts properties for ultimate control (`Audio.initWithProperties`).
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn init(
        mixer: ?Mixer,
        path: [:0]const u8,
        predecode: bool,
    ) !Audio {
        const ret = c.MIX_LoadAudio(
            if (mixer) |val| val.value else null,
            path,
            predecode,
        );
        return Audio{ .value = try errors.wrapCallNull(*c.MIX_Audio, ret) };
    }

    /// Load audio data from an IO stream.
    ///
    /// ## Function Parameters
    /// * `self`: The mixer the audio will be used with.
    /// * `stream`: The stream to load the audio from.
    /// * `predecode`: If `true`, fully decode the audio now instead of decoding it during playback.
    /// * `close_when_done`: If `true`, close the stream when done with it (whether this call succeeds or not).
    ///
    /// ## Return Value
    /// Returns an audio object that can be used to make sound on a mixer.
    ///
    /// ## Remarks
    /// In normal usage, apps should load audio once, maybe at startup, then play it multiple times.
    ///
    /// When loading audio, it will be cached fully in RAM in its original data format.
    /// Each time it plays, the data will be decoded.
    /// For example, an MP3 will be stored in memory in MP3 format and be decompressed on the fly during playback.
    /// This is a tradeoff between i/o overhead and memory usage.
    ///
    /// If `predecode` is true, the data will be decompressed during load and stored as raw PCM data.
    /// This might dramatically increase loading time and memory usage, but there will be no need to decompress data during playback.
    ///
    /// One could also use `Track.setIoStream` to bypass loading the data into RAM upfront at all, but this offers still different tradeoffs.
    /// The correct approach depends on the app's needs and employing different approaches in different situations can make sense.
    ///
    /// Audio objects can be shared between mixers.
    /// This function takes a `Mixer`, to imply this is the most likely place it will be used and loading should try to match its audio format,
    /// but the resulting audio can be used elsewhere.
    /// If mixer is `null`, SDL mixer will set reasonable defaults.
    ///
    /// Once the audio is created, it can be assigned to a `Track` with `Track.setAudio`, or played without any management with `Mixer.playAudio`.
    ///
    /// When done with a mix audio, it can be freed with `Audio.deinit`.
    ///
    /// This function loads data from an IO stream.
    /// There is also a version that loads from a path on the filesystem (`Audio.load`), and one that accepts properties for ultimate control (`Audio.initWithProperties`).
    ///
    /// The IO stream provided must be able to seek, or loading will fail.
    /// If the stream can't seek (data is coming from an HTTP connection, etc), consider caching the data to memory or disk first and creating a new stream to read from there.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn initIo(
        mixer: ?Mixer,
        stream: io_stream.Stream,
        predecode: bool,
        close_when_done: bool,
    ) !Audio {
        const ret = c.MIX_LoadAudio_IO(
            if (mixer) |val| val.value else null,
            stream.value,
            predecode,
            close_when_done,
        );
        return Audio{ .value = try errors.wrapCallNull(*c.MIX_Audio, ret) };
    }

    /// Load audio for playback from a memory buffer without making a copy.
    ///
    /// ## Function Parameters
    /// * `mixer`: A mixer this audio is intended to be used with.
    /// * `data`: The buffer where the audio data lives.
    /// * `free_when_done`: If true, data will be given to `sdl3.free` when the audio is destroyed.
    ///
    /// ## Return Value
    /// Returns an audio object that can be used to make sound on a mixer.
    ///
    /// ## Remarks
    /// When loading audio through most other init functions, the data will be cached fully in RAM in its original data format, for decoding on-demand.
    /// This function does most of the same work as those functions, but instead uses a buffer of memory provided by the app that it does not make a copy of.
    ///
    /// This buffer must live for the entire time the returned audio lives, as the mixer will access the buffer whenever it needs to mix more data.
    ///
    /// This function is meant to maximize efficiency: if the data is already in memory and can remain there, don't copy it.
    /// This data can be in any supported audio file format (WAV, MP3, etc); it will be decoded on the fly while mixing.
    /// Unlike `Audio.init`, there is no predecode option offered here, as this is meant to optimize for data that's already in memory and intends to exist there for significant time;
    /// since predecoding would only need the file format data once, upfront, one could simply wrap it in `io_stream.Stream.initFromConstMem` and pass that to `Audio.initIo`.
    ///
    /// Audio objects can be shared between multiple mixers.
    /// The mixer parameter just suggests the most likely mixer to use this audio, in case some optimization might be applied, but this is not required, and a `null` mixer may be specified.
    ///
    /// If `free_when_done` is `true`, SDL mixer will call `sdl3.free(data)` when the returned audio is eventually destroyed.
    /// This can be useful when the data is not static, but rather loaded elsewhere for this specific audio and simply wants to avoid the extra copy.
    ///
    /// As audio format information is obtained from the file format metadata, this isn't useful for raw PCM data; in that case, use `Audio.initRawAudioNoCopy` instead,
    /// which offers an `audio.Spec`.
    ///
    /// Once an audio is created, it can be assigned to a `Track` with `Track.setAudio`, or played without any management with `Mixer.playAudio`.
    ///
    /// When done with an audio, it can be freed with `Audio.deinit`.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn initNoCopy(
        mixer: ?Mixer,
        data: []const u8,
        free_when_done: bool,
    ) !Audio {
        return Audio{ .value = try errors.wrapCallNull(*c.MIX_Audio, c.MIX_LoadAudioNoCopy(
            if (mixer) |val| val.value else null,
            data.ptr,
            @intCast(data.len),
            free_when_done,
        )) };
    }

    /// Load raw PCM data from a memory buffer.
    ///
    /// ## Function Parameters
    /// * `mixer`: A mixer this audio is intended to be used with.
    /// * `data`: The raw PCM data to load.
    /// * `spec`: The size, in bytes, of the raw PCM data.
    ///
    /// ## Return Value
    /// eturns an audio object that can be used to make sound on a mixer.
    ///
    /// ## Remarks
    /// There are other options for streaming raw PCM: an `audio.Stream` can be connected to a track, as can an `io_stream.Stream`,
    /// and will read from those sources on-demand when it is time to mix the audio.
    /// This function is useful for loading static audio data that is meant to be played multiple times.
    ///
    /// This function will load the raw data in its entirety and cache it in RAM, allocating a copy.
    /// If the original data will outlive the created `Audio`, you can use `Audio.initRawNoCopy` to avoid extra allocations and copies.
    ///
    /// Audio objects can be shared between multiple mixers.
    /// The mixer parameter just suggests the most likely mixer to use this audio, in case some optimization might be applied, but this is not required, and a `null` mixer may be specified.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn initRaw(
        mixer: ?Mixer,
        data: []const u8,
        spec: audio.Spec,
    ) !Audio {
        return Audio{ .value = try errors.wrapCallNull(*c.MIX_Audio, c.MIX_LoadRawAudio(
            if (mixer) |val| val.value else null,
            data.ptr,
            @intCast(data.len),
            &spec.toSdl(),
        )) };
    }

    /// Load raw PCM data from an IO steam.
    ///
    /// ## Function Parameters
    /// * `mixer`: A mixer this audio is intended to be used with.
    /// * `stream`: The stream to load data from.
    /// * `spec`: What format the raw data is in.
    /// * `close_when_done`: True if the stream should be closed before returning (regardless of success or failure).
    ///
    /// ## Return Value
    /// Returns an audio object that can be used to make sound on a mixer.
    ///
    /// Remarks
    /// There are other options for streaming raw PCM: an `audio.Stream` can be connected to a track, as can an `io_stream.Stream`,
    /// and will read from those sources on-demand when it is time to mix the audio.
    /// This function is useful for loading static audio data that is meant to be played multiple times.
    ///
    /// This function will load the raw data in its entirety and cache it in RAM.
    ///
    /// Audio objects can be shared between multiple mixers.
    /// The mixer parameter just suggests the most likely mixer to use this audio, in case some optimization might be applied, but this is not required, and a `null` mixer may be specified.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn initRawIo(
        mixer: ?Mixer,
        stream: io_stream.Stream,
        spec: audio.Spec,
        close_when_done: bool,
    ) !Audio {
        const ret = c.MIX_LoadRawAudio_IO(
            if (mixer) |val| val.value else null,
            stream.value,
            &spec.toSdl(),
            close_when_done,
        );
        return Audio{ .value = try errors.wrapCallNull(*c.MIX_Audio, ret) };
    }

    /// Load raw PCM data from a memory buffer without making a copy.
    ///
    /// ## Function Parameters
    /// * `mixer`: A mixer this audio is intended to be used with.
    /// * `data`: The buffer where the raw PCM data lives.
    /// * `spec`: What format the raw data is in.
    /// * `free_when_done`: If true, data will be given to `sdl3.free` when the audio is destroyed.
    ///
    /// ## Return Value
    /// Returns an audio object that can be used to make sound on a mixer.
    ///
    /// ## Remarks
    /// This buffer must live for the entire time the returned audio lives, as the mixer will access the buffer whenever it needs to mix more data.
    ///
    /// This function is meant to maximize efficiency: if the data is already in memory and can remain there, don't copy it.
    /// But it can also lead to some interesting tricks, like changing the buffer's contents to alter multiple playing tracks at once.
    /// (But, of course, be careful when being too clever).
    ///
    /// Audio objects can be shared between multiple mixers.
    /// The mixer parameter just suggests the most likely mixer to use this audio, in case some optimization might be applied, but this is not required, and a `null` mixer may be specified.
    ///
    /// If `free_when_done` is true, SDL mixer will call `sdl3.free(data)` when the returned audio is eventually destroyed.
    /// This can be useful when the data is not static, but rather composed dynamically for this specific audio and simply wants to avoid the extra copy.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn initRawNoCopy(
        mixer: ?Mixer,
        data: []const u8,
        spec: audio.Spec,
        free_when_done: bool,
    ) !Audio {
        const ret = c.MIX_LoadRawAudioNoCopy(
            if (mixer) |val| val.value else null,
            data.ptr,
            @intCast(data.len),
            &spec.toSdl(),
            free_when_done,
        );
        return Audio{ .value = try errors.wrapCallNull(*c.MIX_Audio, ret) };
    }

    /// Create an audio object that produces a sine wave.
    ///
    /// ## Function Parameters
    /// * `self`: The mixer the audio will be used with.
    /// * `hz`: The frequency of the sine wave, in Hz.
    /// * `amplitude`: The amplitude of the sine wave, from `0` (silence) to `1` (full volume).
    /// * `milliseconds`: The duration of the generated audio in milliseconds, or `null` to generate infinite audio that plays forever.
    ///
    /// ## Return Value
    /// Returns a new `mixer.Audio`.
    /// The caller must call `deinit()` on it when done.
    ///
    /// ## Remarks
    /// This is useful just to have something to play, perhaps for testing or debugging purposes.
    ///
    /// You specify its frequency in Hz (determines the pitch of the sinewave's audio) and amplitude (determines the volume of the sinewave: `1` is very loud, `0` is silent).
    ///
    /// A number of milliseconds of audio to generate can be specified.
    /// Specifying a value less than zero will generate infinite audio (when assigned to a `Track`, the sinewave will play forever).
    ///
    /// `Audio` objects can be shared between multiple mixers.
    /// The mixer parameter just suggests the most likely mixer to use this audio, in case some optimization might be applied, but this is not required, and a `null` mixer may be specified.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn initSineWaveAudio(
        self: ?Mixer,
        hz: u31,
        amplitude: f32,
        milliseconds: ?usize,
    ) !Audio {
        const ret = c.MIX_CreateSineWaveAudio(
            if (self) |val| val.value else null,
            @intCast(hz),
            amplitude,
            if (milliseconds) |ms| @intCast(ms) else -1,
        );
        return Audio{ .value = try errors.wrapCallNull(*c.MIX_Audio, ret) };
    }

    /// Load audio for playback through a collection of properties.
    ///
    /// ## Function Parameters
    /// * `init_properties`: A set of properties on how to load audio.
    ///
    /// ## Return Value
    /// Returns an audio object that can be used to make sound on a mixer.
    ///
    /// ## Remarks
    /// Specific decoders might accept additional custom properties, such as where to find soundfonts for MIDI playback, etc.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn initWithProperties(
        init_properties: InitProperties,
    ) !Audio {
        const props = try init_properties.toSdl();
        defer props.deinit();

        return Audio{ .value = try errors.wrapCallNull(*c.MIX_Audio, c.MIX_LoadAudioWithProperties(props.value)) };
    }

    /// Convert milliseconds to sample frames for an Audio's format.
    ///
    /// ## Function Parameters
    /// * `self`: The audio to query.
    /// * `ms`: The milliseconds to convert to audio-specific sample frames.
    ///
    /// ## Return Value
    /// Returns converted number of sample frames.
    ///
    /// ## Remarks
    /// This calculates time based on the audio's initial format, even if the format would change mid-stream.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn msToFrames(
        self: Audio,
        ms: usize,
    ) !usize {
        return @intCast(try errors.wrapCall(c.Sint64, c.MIX_AudioMSToFrames(self.value, @intCast(ms)), -1));
    }
};

/// An opaque object that represents an audio decoder.
///
/// ## Remarks
/// Most apps won't need this, as SDL mixer's usual interfaces will decode audio as needed.
/// However, if one wants to decode an audio file into a memory buffer without playing it, this interface offers that.
///
/// These objects are created with `AudioDecoder.init` or `AudioDecoder.initIo` and then can use `AudioDecoder.decode` to retrieve the raw PCM data.
///
/// ## Version
/// This struct is available since SDL_mixer 3.0.0.
pub const AudioDecoder = extern struct {
    value: *c.MIX_AudioDecoder,

    /// Decode more audio from a decoder.
    ///
    /// ## Function Parameters
    /// * `self`: The decoder from which to retrieve more data.
    /// * `buffer`: The memory buffer to store decoded audio.
    /// * `spec`: The format that audio data will be stored to `buffer`.
    ///
    /// ## Return Value
    /// Returns the bytes that have been decoded or `null` if end of file.
    ///
    /// ## Remarks
    /// Data is decoded on demand in whatever format is requested.
    /// The format is permitted to change between calls.
    ///
    /// This function will return the bytes decoded, which may be less than requested if there was an error or end-of-file.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn decode(
        self: AudioDecoder,
        buffer: []u8,
        spec: audio.Spec,
    ) !?[]u8 {
        const ret: usize = @intCast(try errors.wrapCall(c_int, c.MIX_DecodeAudio(self.value, buffer.ptr, @intCast(buffer.len), &spec.toSdl()), -1));
        if (ret == 0)
            return null;
        return buffer[0..ret];
    }

    /// Destroy the specified audio decoder.
    ///
    /// ## Function Parameters
    /// * `self`: The audio to destroy.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// Version
    /// ## This function is available since SDL_mixer 3.0.0.
    pub fn deinit(
        self: AudioDecoder,
    ) void {
        c.MIX_DestroyAudioDecoder(self.value);
    }

    /// Query the initial audio format of an audio decoder.
    ///
    /// ## Function Parameters
    /// * `self`: The audio decoder to query.
    ///
    /// ## Return Value
    /// Returns the audio format details.
    ///
    /// ## Remarks
    /// Note that some audio files can change format in the middle; some explicitly support this, but a more common example is two MP3 files concatenated together.
    /// In many cases, SDL mixer will correctly handle these sort of files, but this function will only report the initial format a file uses.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn getFormat(
        self: AudioDecoder,
    ) !audio.Spec {
        var ret: c.SDL_AudioSpec = undefined;
        try errors.wrapCallBool(c.MIX_GetAudioDecoderFormat(self.value, &ret));
        return audio.Spec.fromSdl(ret);
    }

    /// Get the properties associated with an audio decoder.
    ///
    /// ## Function Parameters
    /// * `self`: The audio decoder to query.
    ///
    /// ## Return Value
    /// Returns the properties.
    ///
    /// ## Remarks
    /// SDL mixer offers some properties of its own, but this can also be a convenient place to store app-specific data.
    ///
    /// A properties is created the first time this function is called for a given audio decoder, if necessary.
    ///
    /// The file-specific metadata exposed through this function is identical to those available through `Mixer.getAudioProperties`.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn getProperties(
        self: AudioDecoder,
    ) !properties.Group {
        return .{ .value = try errors.wrapCall(c.SDL_PropertiesID, c.MIX_GetAudioDecoderProperties(self.value), 0) };
    }

    /// Create an audio decoder from a path on the filesystem.
    ///
    /// ## Function Parameters
    /// * `path`: The path on the filesystem from which to load data.
    /// * `props`: Decoder-specific properties if desired.
    ///
    /// ## Return Value
    /// Returns an audio decoder, ready to decode.
    ///
    /// ## Remarks
    /// Most apps won't need this, as SDL mixer's usual interfaces will decode audio as needed.
    /// However, if one wants to decode an audio file into a memory buffer without playing it, this interface offers that.
    ///
    /// This function allows properties to be specified.
    /// This is intended to supply file-specific settings, such as where to find SoundFonts for a MIDI file, etc.
    /// In most cases, the caller should pass null to specify no extra properties.
    ///
    /// When done with the audio decoder, it can be destroyed with `AudioDecoder.deinit`
    ///
    /// This function requires SDL mixer to have been initialized with a successful call to `init`, but does not need an actual Mixer to have been created.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn init(
        path: [*:0]const u8,
        props: ?properties.Group,
    ) !AudioDecoder {
        return .{
            .value = try errors.wrapCallNull(*c.MIX_AudioDecoder, c.MIX_CreateAudioDecoder(path, if (props) |val| val.value else 0)),
        };
    }

    /// Create an audio decoder from an IO stream.
    ///
    /// ## Function Parameters
    /// * `io`: The i/o stream from which to load data.
    /// * `close_io`: If true, close the i/o stream when done with it.
    /// * `props`: Decoder-specific properties if desired.
    ///
    /// ## Return Value
    /// Returns an audio decoder, ready to decode.
    ///
    /// ## Remarks
    /// Most apps won't need this, as SDL mixer's usual interfaces will decode audio as needed.
    /// However, if one wants to decode an audio file into a memory buffer without playing it, this interface offers that.
    ///
    /// This function allows properties to be specified.
    /// This is intended to supply file-specific settings, such as where to find SoundFonts for a MIDI file, etc.
    /// In most cases, the caller should pass null to specify no extra properties.
    ///
    /// If `close_io` is true, then io will be closed when this decoder is done with it.
    /// If this function fails and closeio is true, then io will be closed before this function returns.
    ///
    /// When done with the audio decoder, it can be destroyed with `AudioDecoder.deinit`
    ///
    /// This function requires SDL mixer to have been initialized with a successful call to `init`, but does not need an actual Mixer to have been created.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn initIo(
        io: io_stream.Stream,
        close_io: bool,
        props: ?properties.Group,
    ) !AudioDecoder {
        return .{
            .value = try errors.wrapCallNull(*c.MIX_AudioDecoder, c.MIX_CreateAudioDecoder_IO(io.value, close_io, if (props) |val| val.value else 0)),
        };
    }
};

/// The duration of an audio track.
pub const AudioDuration = union(enum) {
    /// Audio duration in frames.
    frames: usize,
    /// The duration of the audio is infinite.
    infinite: void,
};

/// An opaque object that represents a grouping of tracks.
///
/// ## Remarks
/// SDL mixer offers callbacks at various stages of the mixing pipeline to allow apps to view and manipulate data as it is transformed.
/// Sometimes it is useful to hook in at a point where several tracks--but not all tracks-- have been mixed.
/// For example, when a game is in some options menu, perhaps adjusting game audio but not UI sounds could be useful.
///
/// SDL mixer allows you to assign several tracks to a group, and receive a callback when that group has finished mixing,
/// with a buffer of just that group's mixed audio, before it mixes into the final output.
///
/// ## Version
/// This datatype is available since SDL_mixer 3.0.0.
pub const Group = struct {
    value: *c.MIX_Group,

    /// Destroy a mixing group.
    ///
    /// ## Function Parameters
    /// * `self`: The group to destroy.
    ///
    /// ## Remarks
    /// Any tracks currently assigned to this group will be reassigned to the mixer's internal default group.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn deinit(
        self: Group,
    ) void {
        c.MIX_DestroyGroup(
            self.value,
        );
    }

    /// Get the mixer that owns a group.
    ///
    /// ## Function Parameters
    /// * `self`: The group to query.
    ///
    /// ## Return Value
    /// Returns the mixer associated with the group.
    ///
    /// ## Remarks
    /// This is the mixer pointer that was passed to `Group.init`.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn getMixer(
        self: Group,
    ) !Mixer {
        return .{ .value = try errors.wrapCallNull(*c.MIX_Mixer, c.MIX_GetGroupMixer(self.value)) };
    }

    /// Get the properties associated with a group.
    ///
    /// ## Function Parameters
    /// * `self`: The group to query.
    ///
    /// ## Return Value
    /// Returns the group properties.
    ///
    /// ## Remarks
    /// Currently SDL mixer assigns no properties of its own to a group, but this can be a convenient place to store app-specific data.
    ///
    /// The properties are created the first time this function is called for a given group.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn getProperties(
        self: Group,
    ) !properties.Group {
        return .{ .value = try errors.wrapCall(c.SDL_PropertiesID, c.MIX_GetGroupProperties(self.value), 0) };
    }

    /// Create a mixing group.
    ///
    /// ## Function Parameters
    /// * `mixer`: The mixer on which to create a mixing group.
    ///
    /// ## Return Value
    /// Returns a new `Group`. The caller must call `deinit()` on it when done.
    ///
    /// ## Remarks
    /// Tracks are assigned to a mixing group (or if unassigned, they live in a mixer's internal default group).
    /// All tracks in a group are mixed together and the app can access this mixed data before it is mixed with all other groups to produce the final output.
    ///
    /// This can be a useful feature, but is completely optional; apps can ignore mixing groups entirely and still have a full experience with SDL mixer.
    ///
    /// After creating a group, assign tracks to it with `Track.setGroup`. Use `Group.setPostMixCallback` to access the group's mixed data.
    ///
    /// A mixing group can be destroyed with `Group.deinit` when no longer needed.
    /// Destroying the mixer will also destroy all its still-existing mixing groups.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn init(
        mixer: Mixer,
    ) !Group {
        const ret = c.MIX_CreateGroup(
            mixer.value,
        );
        return Group{ .value = try errors.wrapCallNull(*c.MIX_Group, ret) };
    }

    /// Set a callback that fires when a mixer group has completed mixing.
    ///
    /// ## Function Parameters
    /// * `self`: The mixing group to assign this callback to.
    /// * `UserData`: Type of user data for the callback.
    /// * `callback`: The function to call when the group mixes or `null` to clear.
    /// * `user_data`: The user data to pass to the callback.
    ///
    /// ## Remarks
    /// After all playing tracks in a mixer group have pulled in more data from their inputs, transformed it, and mixed together into a single buffer,
    /// a callback can be fired. This lets an app view the data at the last moment that it is still a part of this group.
    /// It can also change the data in any way it pleases during this callback, and the mixer will continue as if this data came directly from the group's mix buffer.
    ///
    /// Each group has its own unique callback.
    /// Tracks that aren't in an explicit `Group` are mixed in an internal grouping that is not available to the app.
    ///
    /// Passing a `null` callback here is legal; it disables this group's callback.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn setGroupPostCallback(
        self: Group,
        comptime UserData: type,
        comptime callback: ?GroupMixCallback(UserData),
        user_data: ?*UserData,
    ) !void {
        const Cb = struct {
            pub fn run(
                user_data_c: ?*anyopaque,
                group_c: [*c]c.MIX_Group,
                spec_c: [*c]c.SDL_AudioSpec,
                pcm_c: [*c]f32,
                samples_c: c_int,
            ) callconv(.c) void {
                return callback(
                    @ptrCast(@alignCast(user_data_c)),
                    .{ .value = group_c },
                    audio.Spec.fromSdl(spec_c.*),
                    pcm_c[0..@intCast(samples_c)],
                );
            }
        };
        return errors.wrapCallBool(c.MIX_SetGroupPostMixCallback(self.value, if (callback != null) Cb.run else null, @ptrCast(@alignCast(user_data))));
    }
};

/// A number of loops.
pub const LoopCount = union(enum) {
    /// Number of loops.
    num: usize,
    /// There are infinite loops.
    infinite: void,

    /// Convert from an SDL value.
    pub fn fromSdl(
        value: c_int,
    ) LoopCount {
        if (value == -1)
            return .{ .infinite = {} };
        return .{ .num = @intCast(value) };
    }

    /// Convert to an SDL value.
    pub fn toSdl(
        self: LoopCount,
    ) c_int {
        return switch (self) {
            .num => |num| @intCast(num),
            .infinite => -1,
        };
    }
};

/// An opaque object that represents a mixer.
///
/// ## Remarks
/// The MIX mixer is the toplevel object for this library.
/// To use SDL mixer, you must have at least one, but are allowed to have several.
/// Each mixer is responsible for generating a single output stream of mixed audio, usually to an audio device for realtime playback.
///
/// Mixers are either created to feed an audio device (through `Mixer.initDevice`), or to generate audio to a buffer in memory, where it can be used for anything (through `Mixer.init`).
///
/// ## Version
/// This datatype is available since SDL_mixer 3.0.0.
pub const Mixer = struct {
    value: *c.MIX_Mixer,

    /// Mixer properties.
    ///
    /// ## Version
    /// This struct is provided by zig-sdl3.
    pub const Properties = struct {
        /// The audio device that this mixer has opened for playback.
        /// This will be `null` (no device) if the mixer was created with `Mixer.init` instead of `Mixer.initDevice`.
        device: ?audio.Device,

        /// Get properties from SDL.
        pub fn fromSdl(props: properties.Group) Properties {
            return .{
                .device = if (props.get(c.MIX_PROP_MIXER_DEVICE_NUMBER)) |val| if (val.number == 0) null else .{ .value = @intCast(val.number) } else null,
            };
        }
    };

    /// Free a mixer.
    ///
    /// ## Function Parameters
    /// * `self`: The mixer to destroy.
    ///
    /// ## Remarks
    /// If this mixer was created with `Mixer.initDevice`, this function will also close the audio device and call `sdl3.quit(.{ .audio = true })`.
    ///
    /// Any `Group` or `Track` created for this mixer will also be destroyed.
    /// Do not access them again or attempt to destroy them after the device is destroyed.
    /// `Audio` objects will not be destroyed, since they can be shared between mixers (but those will all be destroyed during `quit`).
    ///
    /// ## Thread Safety
    /// If this is used with a `Mixer` from `Mixer.initDevice`, then this function should only be called on the main thread.
    /// If this is used with a `Mixer` from `Mixer.init`, then it is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn deinit(
        self: Mixer,
    ) void {
        c.MIX_DestroyMixer(
            self.value,
        );
    }

    /// Generate mixer output when not driving an audio device.
    ///
    /// ## Function Parameters
    /// * `self`: The mixer for which to generate more audio.
    /// * `buffer`: The buffer to store audio in.
    ///
    /// ## Return Value
    /// Returns the portion of the buffer containing actually mixed audio.
    ///
    /// ## Remarks
    /// SDL mixer allows the creation of MIX Mixer objects that are not connected to an audio device, by calling `Mixer.init` instead of `Mixer.initDevice`.
    /// Such mixers will not generate output until explicitly requested through this function.
    ///
    /// The caller may request as much audio as desired, so long as buflen is a multiple of the sample frame size specified when creating the mixer
    /// (for example, if requesting stereo Sint16 audio, buflen must be a multiple of 4: 2 bytes-per-channel times 2 channels).
    ///
    /// The mixer will mix as quickly as possible; since it works in sample frames instead of time, it can potentially generate enormous amounts of audio in a small amount of time.
    ///
    /// On success, this always fills buffer with buflen bytes of audio; if all playing tracks finish mixing, it will fill the remaining buffer with silence.
    ///
    /// Each call to this function will pick up where it left off, playing tracks will continue to mix from the point the previous call completed, etc.
    /// The mixer state can be changed between each call in any way desired: tracks can be added, played, stopped, changed, removed, etc.
    /// Effectively this function does the same thing SDL mixer does internally when the audio device needs more audio to play.
    ///
    /// This function can not be used with mixers from `Mixer.initDevice` those generate audio as needed internally.
    ///
    /// This function returns the number of the buffe of real audio mixed, which might be less than `buffer.len`.
    /// While all buffer bytes of buffer will be initialized, if available tracks to mix run out, the end of the buffer will be initialized with silence;
    /// this silence will not be counted in the return value, so the caller has the option to identify how much of the buffer has legimitate contents vs appended silence.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn generate(
        self: Mixer,
        buffer: []u8,
    ) ![]u8 {
        const len: usize = @intCast(try errors.wrapCall(c_int, c.MIX_Generate(
            self.value,
            buffer.ptr,
            @intCast(buffer.len),
        ), -1));
        return buffer[0..len];
    }

    /// Get the audio format a mixer is generating.
    ///
    /// ## Function Parameters
    /// * `self`: The mixer to query.
    ///
    /// ## Return Value
    /// Returns the audio specification of the mixer.
    ///
    /// ## Remarks
    /// Generally you don't need this information, as SDL mixer will convert data as necessary between inputs you provide and its output format,
    /// but it might be useful if trying to match your inputs to reduce conversion and resampling costs.
    ///
    /// For mixers created with `Mixer.initDevice`, this is the format of the audio device (and may change later if the device itself changes;
    /// SDL_mixer will seamlessly handle this change internally, though).
    ///
    /// For mixers created with `Mixer.init`, this is the format that `Mixer.generate` will produce, as requested at create time, and does not change.
    ///
    /// Note that internally, SDL mixer will work in `floating_32_bit` format before outputting the format specified here,
    /// so it would be more efficient to match input data to that, not the final output format.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn getFormat(
        self: Mixer,
    ) !audio.Spec {
        var spec: c.SDL_AudioSpec = undefined;
        const ret = c.MIX_GetMixerFormat(
            self.value,
            &spec,
        );
        try errors.wrapCallBool(ret);
        return audio.Spec.fromSdl(spec);
    }

    /// Get a mixer's master frequency ratio.
    ///
    /// ## Function Parameters
    /// * `self`: The mixer to query.
    ///
    /// ## Return Value
    /// Returns the mixer's current master frequency ratio.
    ///
    /// ## Remarks
    /// This returns the last value set through `Mixer.setFrequencyRatio`, or `1` if no value has ever been explicitly set.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn getFrequencyRatio(
        self: Mixer,
    ) f32 {
        return c.MIX_GetMixerFrequencyRatio(self.value);
    }

    /// Get a mixer's master gain control.
    ///
    /// ## Function Parameters
    /// * `self`: The mixer to query.
    ///
    /// ## Return Value
    /// Returns the current master gain.
    ///
    /// ## Remarks
    /// This returns the last value set through `Mixer.setGain` or `1` if no value has ever been explicitly set.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn getGain(
        self: Mixer,
    ) f32 {
        return c.MIX_GetMixerGain(
            self.value,
        );
    }

    /// Get the properties associated with a mixer.
    ///
    /// ## Function Parameters
    /// * `self`: The mixer to query.
    ///
    /// ## Return Value
    /// Returns the mixer's properties.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn getProperties(
        self: Mixer,
    ) !Properties {
        const props = try errors.wrapCall(c.SDL_PropertiesID, c.MIX_GetMixerProperties(self.value), 0);
        return Properties.fromSdl(.{ .value = props });
    }

    /// Get all tracks with a specific tag.
    ///
    /// ## Function Parameters
    /// * `self`: The mixer to query.
    /// * `tag`: The tag to search.
    ///
    /// ## Return Value
    /// Returns an array of the tracks.
    /// The returned slice should be freed with `sdl3.free` when it is no longer needed.
    ///
    /// ## Remarks
    /// Tracks are not provided in any guaranteed order.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn getTaggedTracks(
        self: Mixer,
        tag: [:0]const u8,
    ) ![]Track {
        var num: c_int = undefined;
        const ret = try errors.wrapCallNull([*]Track, @ptrCast(@alignCast(c.MIX_GetTaggedTracks(self.value, tag, &num))));
        return ret[0..@intCast(num)];
    }

    /// Create a mixer that generates audio to a memory buffer.
    ///
    /// ## Function Parameters
    /// * `spec`: The audio format that mixer will generate.
    ///
    /// ## Return Value
    /// Returns a mixer that can be used to generate audio.
    ///
    /// ## Remarks
    /// Usually you want `Mixer.initDevice` instead of this function.
    /// The mixer created here can be used with `Mixer.generate` to produce more data on demand, as fast as desired.
    ///
    /// An audio format must be specified.
    /// This is the format it will output in.
    ///
    /// Once a mixer is created, next steps are usually to load audio (through `Mixer.loadAudio` and friends), create a track (`Mixer.createTrack`), and play that audio through that track.
    ///
    /// When done with the mixer, it can be destroyed with `Mixer.deinit`.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn init(
        spec: audio.Spec,
    ) !Mixer {
        return Mixer{ .value = try errors.wrapCallNull(*c.MIX_Mixer, c.MIX_CreateMixer(&spec.toSdl())) };
    }

    /// Create a mixer that plays sound directly to an audio device.
    ///
    /// ## Function Parameters
    /// * `device`: The audio device to open, such as `audio.Device.default_playback`.
    /// * `spec`: The audio format to mix in, or `null` to let SDL_mixer choose a reasonable default.
    ///
    /// ## Return Value
    /// Returns a new `mixer.Mixer`.
    ///
    /// ## Remarks
    /// This is usually the function you want, vs `Mixer.init`.
    ///
    /// You can choose a specific device ID to open, following SDL's usual rules, but often the correct choice is to specify `audio.Device.default_playback`
    /// and let SDL figure out what device to use (and seamlessly transition you to new hardware if the default changes).
    ///
    /// Only playback devices make sense here. Attempting to open a recording device will fail.
    ///
    /// This will call `sdl3.init(.{ .audio = true })` internally; it's safe to call `sdl3.init` before this call, too, if you intend to enumerate audio devices to choose one to open here.
    ///
    /// An audio format can be requested, and the system will try to set the hardware to those specifications, or as close as possible, but this is just a hint.
    /// SDL mixer will handle all data conversion behind the scenes in any case, and specifying a `null` spec is a reasonable choice.
    /// The best reason to specify a format is because you know all your data is in that format and it might save some unnecessary CPU time on conversion.
    ///
    /// The actual device format chosen is available through `Mixer.getFormat`.
    ///
    /// Once a mixer is created, next steps are usually to load audio (through `Mixer.loadAudio` and friends), create a track (`Mixer.Track.init`), and play that audio through that track.
    ///
    /// When done with the mixer, it can be destroyed with `Mixer.deinit`.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn initDevice(
        device: audio.Device,
        spec: ?audio.Spec,
    ) !Mixer {
        const spec_sdl: c.SDL_AudioSpec = if (spec) |val| val.toSdl() else undefined;
        const ret = c.MIX_CreateMixerDevice(
            device.value,
            if (spec != null) &spec_sdl else null,
        );
        return Mixer{ .value = try errors.wrapCallNull(*c.MIX_Mixer, ret) };
    }

    /// Lock a mixer by obtaining its internal mutex.
    ///
    /// ## Function Parameters
    /// * `self`: The mixer to lock.
    ///
    /// ## Remarks
    /// While locked, the mixer will not be able to mix more audio or change its internal state in another thread.
    /// Those other threads will block until the mixer is unlocked again.
    ///
    /// Under the hood, this function calls `mutex.Mutex.lock`, so all the same rules apply: the lock can be recursive,
    /// it must be unlocked the same number of times from the same thread that locked it, etc.
    ///
    /// Just about every SDL mixer API also locks the mixer while doing its work, as does the SDL audio device thread while actual mixing is in progress,
    /// so basic use of this library never requires the app to explicitly lock the device to be thread safe.
    /// There are two scenarios where this can be useful, however:
    /// * The app has a provided a callback that the mixing thread might call,
    /// and there is some app state that needs to be protected against race conditions as changes are made and mixing progresses simultaneously.
    /// Any lock can be used for this, but this is a conveniently-available lock.
    /// * The app wants to make multiple, atomic changes to the mix.
    /// For example, to start several tracks at the exact same moment, one would lock the mixer, call `Track.play` multiple times
    ///  and then unlock again; all the tracks will start mixing on the same sample frame.
    ///
    /// Each call to this function must be paired with a call to `Mixer.unlock` from the same thread.
    /// It is safe to lock a mixer multiple times; it remains locked until the final matching unlock call.
    ///
    /// Do not lock the mixer for significant amounts of time, or it can cause audio dropouts.
    /// Just do simple things quickly and unlock again.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn lock(
        self: Mixer,
    ) void {
        c.MIX_LockMixer(self.value);
    }

    /// Pause all tracks with a specific tag.
    ///
    /// ## Function Parameters
    /// * `self`: The mixer on which to pause tracks.
    /// * `tag`: The tag to use when searching for tracks.
    ///
    /// ## Remarks
    /// A paused track is not considered "stopped," so its `TrackStoppedCallback` will not fire if paused, but it won't change state by default, generate audio,
    /// or generally make progress, until it is resumed.
    ///
    /// This function makes all currently-playing tracks on the specified mixer, with a specific tag, move to a paused state.
    /// They can later be resumed.
    ///
    /// Tracks that match the specified tag that aren't currently playing are ignored.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn pauseTag(
        self: Mixer,
        tag: [:0]const u8,
    ) !void {
        return errors.wrapCallBool(c.MIX_PauseTag(self.value, tag.ptr));
    }

    /// Pause all currently-playing tracks.
    ///
    /// ## Function Parameters
    /// * `self`: The mixer on which to pause all tracks.
    ///
    /// ## Remarks
    /// A paused track is not considered "stopped," so its `TrackStoppedCallback` will not fire if paused, but it won't change state by default,
    /// generate audio, or generally make progress, until it is resumed.
    ///
    /// This function makes all tracks on the specified mixer that are currently playing move to a paused state.
    /// They can later be resumed.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn pauseAllTracks(
        self: Mixer,
    ) !void {
        return errors.wrapCallBool(c.MIX_PauseAllTracks(self.value));
    }

    /// Play audio from start to finish without any management.
    ///
    /// ## Function Parameters
    /// * `self`: The mixer on which to play this audio.
    /// * `input`: The audio input to play.
    ///
    /// ## Remarks
    /// This is what we term a "fire-and-forget" sound.
    /// Internally, SDL mixer will manage a temporary track to mix the specified audio, cleaning it up when complete.
    /// No options can be provided for how to do the mixing, like `Track.play` offers, and since the track is not available to the caller, no adjustments can be made to mixing over time.
    ///
    /// This is not the function to build an entire game of any complexity around, but it can be convenient to play simple, one-off sounds that can't be stopped early.
    /// An example would be a voice saying "GAME OVER" during an unpausable endgame sequence.
    ///
    /// There are no limits to the number of fire-and-forget sounds that can mix at once (short of running out of memory),
    /// and SDL mixer keeps an internal pool of temporary tracks it creates as needed and reuses when available.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn playAudio(
        self: Mixer,
        input: Audio,
    ) !void {
        return errors.wrapCallBool(c.MIX_PlayAudio(self.value, input.value));
    }

    /// Start (or restart) mixing all tracks with a specific tag for playback.
    ///
    /// ## Function Parameters
    /// * `self`: The mixer on which to look for tagged tracks.
    /// * `tag`: The tag to use when searching for tracks.
    /// * `options`: The set of options that will be applied to each track.
    ///
    /// ## Remarks
    /// This function follows all the same rules as `Track.play`; please refer to its documentation for the details.
    /// Unlike that function, `Mixer.playTag` operates on multiple tracks at once that have the specified tag applied, via `Track.tag`.
    ///
    /// If all of your tagged tracks have different sample rates, it would make sense to use the `*_milliseconds` properties in your options,
    /// instead of `*_frames`, and let SDL mixer figure out how to apply it to each track.
    ///
    /// This function returns true if all tagged tracks are started (or restarted).
    /// If any track fails, this function returns false, but all tracks that could start will still be started even when this function reports failure.
    ///
    /// From the point of view of the mixing process, all tracks that successfully (re)start will do so at the exact same moment.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn playTag(
        self: Mixer,
        tag: [:0]const u8,
        options: ?PlayOptions,
    ) !void {
        if (options) |val| {
            const group = try val.toProperties();
            defer group.deinit();
            const ret = c.MIX_PlayTag(
                self.value,
                tag.ptr,
                group.value,
            );
            return errors.wrapCallBool(ret);
        } else {
            return errors.wrapCallBool(c.MIX_PlayTag(
                self.value,
                tag.ptr,
                0,
            ));
        }
    }

    /// Resume all currently-paused tracks.
    ///
    /// ## Function Parameters
    /// * `self`: The mixer on which to resume all tracks.
    ///
    /// ## Remarks
    /// A paused track is not considered "stopped," so its `TrackStoppedCallback` will not fire if paused, but it won't change state by default, generate audio,
    /// or generally make progress, until it is resumed.
    ///
    /// This function makes all tracks on the specified mixer that are currently paused move to a playing state.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn resumeAllTracks(
        self: Mixer,
    ) !void {
        return errors.wrapCallBool(c.MIX_ResumeAllTracks(self.value));
    }

    /// Resume all tracks with a specific tag.
    ///
    /// ## Function Parameters
    /// * `self`: The mixer on which to resume tracks.
    /// * `tag`: The tag to use when searching for tracks.
    ///
    /// ## Remarks
    /// A paused track is not considered "stopped," so its `TrackStoppedCallback` will not fire if paused, but it won't change state by default, generate audio,
    /// or generally make progress, until it is resumed.
    ///
    /// This function makes all currently-paused tracks on the specified mixer, with a specific tag, move to a playing state.
    ///
    /// Tracks that match the specified tag that aren't currently paused are ignored.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn resumeTag(
        self: Mixer,
        tag: [:0]const u8,
    ) !void {
        return errors.wrapCallBool(c.MIX_ResumeTag(self.value, tag.ptr));
    }

    /// Set a mixer's master frequency ratio.
    ///
    /// ## Function Parameters
    /// * `self`: The mixer to adjust.
    /// * `frequency_ratio`: The frequency ratio. Must be between `0.01` and `100`.
    ///
    /// ## Remarks
    /// Each mixer has a master frequency ratio, that affects the entire mix.
    /// This can cause the final output to change speed and pitch.
    /// A value greater than `1` will play the audio faster, and at a higher pitch.
    /// A value less than `1` will play the audio slower, and at a lower pitch.
    /// `1` is normal speed.
    ///
    /// Each track also has a frequency ratio; it will be applied when mixing that track's audio regardless of the master setting.
    /// The master setting affects the final output after all mixing has been completed.
    ///
    /// A mixer's master frequency ratio defaults to `1`.
    ///
    /// This value can be changed at any time to adjust the future mix.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn setFrequencyRatio(
        self: Mixer,
        frequency_ratio: f32,
    ) !void {
        return errors.wrapCallBool(c.MIX_SetMixerFrequencyRatio(self.value, frequency_ratio));
    }

    /// Set a mixer's master gain control.
    ///
    /// ## Function Parameters
    /// * `self`: The mixer to adjust.
    /// * `gain`: The new gain value.
    ///
    /// ## Remarks
    /// Each mixer has a master gain, to adjust the volume of the entire mix.
    /// Each sample passing through the pipeline is modulated by this gain value.
    /// A gain of `0` will generate silence, `1` will not change the mixed volume, and larger than `1` will increase the volume.
    /// Negative values are illegal.
    /// There is no maximum gain specified, but this can quickly get extremely loud, so please be careful with this setting.
    ///
    /// A mixer's master gain defaults to `1`.
    ///
    /// This value can be changed at any time to adjust the future mix.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn setGain(
        self: Mixer,
        gain: f32,
    ) !void {
        const ret = c.MIX_SetMixerGain(
            self.value,
            gain,
        );
        return errors.wrapCallBool(ret);
    }

    /// Set a callback that fires when all mixing has completed.
    ///
    /// ## Function Parameters
    /// * `self`: The mixer to assign this callback to.
    /// * `UserData`: Type of user data for the callback.
    /// * `cb`: The function to call when the mixer mixes, may be `null`.
    /// * `user_data`: App provided user data.
    ///
    /// ## Remarks
    /// After all mixer groups have processed, their buffers are mixed together into a single buffer for the final output, at which point a callback can be fired.
    /// This lets an app view the data at the last moment before mixing completes.
    /// It can also change the data in any way it pleases during this callback, and the mixer will continue as if this data is the final output.
    ///
    /// Each mixer has its own unique callback.
    ///
    /// Passing a `null` callback here is legal; it disables this mixer's callback.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn setPostMixCallback(
        self: Mixer,
        comptime UserData: type,
        comptime cb: ?PostMixCallback(UserData),
        user_data: ?*anyopaque,
    ) !void {
        const Cb = struct {
            fn run(
                c_user_data: ?*anyopaque,
                c_mixer: [*c]c.MIX_Mixer,
                c_spec: [*c]const c.SDL_AudioSpec,
                c_pcm: [*c]f32,
                c_samples: c_int,
            ) callconv(.c) void {
                cb.?(@ptrCast(@alignCast(c_user_data)), .{ .value = c_mixer }, audio.Spec.fromSdl(c_spec), c_pcm[0..@intCast(c_samples)]);
            }
        };
        return errors.wrapCallBool(c.MIX_SetPostMixCallback(
            self.value,
            if (cb != null) &Cb.run else null,
            @ptrCast(@alignCast(user_data)),
        ));
    }

    /// Set the gain control of all tracks with a specific tag.
    ///
    /// ## Function Parameters
    /// * `self`: The mixer on which to look for tagged tracks.
    /// * `tag`: The tag to use when searching for tracks.
    /// * `gain`: The new gain value.
    ///
    /// ## Remarks
    /// Each track has its own gain, to adjust its overall volume.
    /// Each sample from this track is modulated by this gain value.
    /// A gain of zero will generate silence, `1` will not change the mixed volume, and larger than `1` will increase the volume.
    /// Negative values are illegal.
    /// There is no maximum gain specified, but this can quickly get extremely loud, so please be careful with this setting.
    ///
    /// A track's gain defaults to `1`.
    ///
    /// This will change the gain control on tracks on the specified mixer that have the specified tag.
    ///
    /// From the point of view of the mixing process, all tracks that successfully change gain values will do so at the exact same moment.
    ///
    /// This value can be changed at any time to adjust the future mix.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn setTagGain(
        self: Mixer,
        tag: [:0]const u8,
        gain: f32,
    ) !void {
        return errors.wrapCallBool(c.MIX_SetTagGain(self.value, tag.ptr, gain));
    }

    /// Halt all currently-playing tracks, possibly fading out over time.
    ///
    /// ## Function Parameters
    /// * `self`: The mixer on which to stop all tracks.
    /// * `fade_out_ms`: The number of milliseconds to spend fading out to silence before halting. `0` to stop immediately.
    ///
    /// ## Remarks
    /// If `fade_out_ms > 0`, the tracks do not stop mixing immediately, but rather fades to silence over that many milliseconds before stopping.
    /// Note that this is different than `Track.stop`, which wants sample frames; this function takes milliseconds because different tracks might have different sample rates.
    ///
    /// If a track ends normally while the fade-out is still in progress, the audio stops there; the fade is not adjusted to be shorter if it will last longer than the audio remaining.
    ///
    /// Once a track has completed any fadeout and come to a stop, it will call its `TrackStoppedCallback`, if any.
    /// It is legal to assign the track a new input and/or restart it during this callback.
    ///
    /// This function does not prevent new play requests from being made;
    /// it’s legal to use this function to begin fading all playing tracks but then start other tracks playing normally while those fade-outs are still in progress.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn stopAllTracks(
        self: Mixer,
        fade_out_ms: usize,
    ) !void {
        return errors.wrapCallBool(c.MIX_StopAllTracks(self.value, @intCast(fade_out_ms)));
    }

    /// Halt all tracks with a specific tag, possibly fading out over time.
    ///
    /// ## Function Parameters
    /// * `self`: The mixer on which to stop tracks.
    /// * `tag`: The tag to use when searching for tracks.
    /// * `fade_out_ms`: The number of milliseconds to spend fading out to silence before halting. `0` to stop immediately.
    ///
    /// ## Remarks
    /// If `fade_out_ms > 0`, the tracks do not stop mixing immediately, but rather fades to silence over that many milliseconds before stopping.
    /// Note that this is different than `Track.stop`, which wants sample frames; this function takes milliseconds because different tracks might have different sample rates.
    ///
    /// If a track ends normally while the fade-out is still in progress, the audio stops there; the fade is not adjusted to be shorter if it will last longer than the audio remaining.
    ///
    /// Once a track has completed any fadeout and come to a stop, it will call its `TrackStoppedCallback`, if any.
    /// It is legal to assign the track a new input and/or restart it during this callback.
    /// This function does not prevent new play requests from being made.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn stopTag(
        self: Mixer,
        tag: [:0]const u8,
        fade_out_ms: usize,
    ) !void {
        return errors.wrapCallBool(c.MIX_StopTag(self.value, tag.ptr, @intCast(fade_out_ms)));
    }

    /// Unlock a mixer previously locked by a call to `Mixer.lock`.
    ///
    /// ## Function Parameters
    /// * `self`: The mixer to unlock.
    ///
    /// ## Remarks
    /// While locked, the mixer will not be able to mix more audio or change its internal state another thread.
    /// Those other threads will block until the mixer is unlocked again.
    ///
    /// Under the hood, this function calls `mutex.Mutex.lock`, so all the same rules apply: the lock can be recursive,
    /// it must be unlocked the same number of times from the same thread that locked it, etc.
    ///
    /// ## Thread Safety
    /// This call must be paired with a previous `Mixer.lock` call on the same thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn unlock(
        self: Mixer,
    ) void {
        c.MIX_UnlockMixer(self.value);
    }
};

/// 3D coordinates for `track.set3dPosition`.
///
/// ## Remarks
/// The coordinates use a "right-handed" coordinate system, like OpenGL and OpenAL.
///
/// ## Version
/// This struct is available since SDL_mixer 3.0.0.
pub const Point3d = extern struct {
    /// X coordinate (negative left, positive right).
    x: f32,
    /// Y coordinate (negative down, positive up).
    y: f32,
    /// Z coordinate (negative forward, positive back).
    z: f32,

    comptime {
        errors.assertStructsEqual(@This(), c.MIX_Point3D);
    }

    /// Convert from an SDL value.
    pub fn fromSdl(
        value: c.MIX_Point3D,
    ) Point3d {
        return .{
            .x = value.x,
            .y = value.y,
            .z = value.z,
        };
    }

    /// Convert to an SDL value.
    pub fn toSdl(
        self: Point3d,
    ) c.MIX_Point3D {
        return .{
            .x = self.x,
            .y = self.y,
            .z = self.z,
        };
    }
};

/// Options that control how a track begins playing.
///
/// ## Remarks
/// All fields are optional; a `null` field uses SDL_mixer's default behavior for that setting.
///
/// ## Version
/// This type is provided by zig-sdl3.
pub const PlayOptions = struct {
    /// Number of times to loop the track after the first play.
    /// The default is `0`.
    loops: ?LoopCount = null,
    /// Mix at most to this sample frame position in the track.
    /// This will be treated as if the input reach EOF at this point in the audio file.
    /// If inner is `null`, mix all available audio without a limit.
    /// Default `null`.
    max_frame: ?struct { value: ?usize } = null,
    /// Mix at most to this millisecond position in the track.
    /// This will be treated as if the input reach EOF at this point in the audio file.
    /// The `max_frame` property will be preferred if specified.
    /// If inner is `null`, mix all available audio without a limit.
    /// Default `null`.
    max_milliseconds: ?struct { value: ?usize } = null,
    /// Start mixing from this sample frame position in the track's input.
    /// A `value == 0` will begin from the start of the track's input.
    /// If the input is not seekable and this `value > 0`, the function will report failure.
    /// Default `0`.
    start_frame: ?usize = null,
    /// Start mixing from this millisecond position in the track's input.
    /// The `start_frame` property will be preferred if specified.
    /// A `value == 0` will begin from the start of the track's input.
    /// If the input is not seekable and this `value > 0`, the function will report failure.
    /// Default `0`.
    start_millisecond: ?usize = null,
    /// If the track is looping, this is the sample frame position that the track will loop back to; this lets one play an intro at the start of a track on the first iteration,
    /// but have a loop point somewhere in the middle thereafter.
    /// A value of `0` will begin the loop from the start of the track's input.
    /// Default `0`.
    loop_start_frame: ?usize = null,
    /// If the track is looping, this is the millisecond position that the track will loop back to; this lets one play an intro at the start of a track on the first iteration,
    /// but have a loop point somewhere in the middle thereafter.
    /// The `loop_start_frame` property will be preferred if specified.
    /// A value of `0` will begin the loop from the start of the track's input.
    /// Default `0`.
    loop_start_millisecond: ?usize = null,
    /// The number of sample frames over which to fade in the newly-started track.
    /// The track will begin mixing silence and reach full volume smoothly over this many sample frames.
    /// If the track loops before the fade-in is complete, it will continue to fade correctly from the loop point.
    /// A value of `0` will disable fade-in, so the track starts mixing at full volume.
    /// Default `0`.
    fade_in_frames: ?usize = null,
    /// The number of milliseconds over which to fade in the newly-started track.
    /// The track will begin mixing silence and reach full volume smoothly over this many sample frames.
    /// The `fade_in_frames` property will be preferred if specified.
    /// If the track loops before the fade-in is complete, it will continue to fade correctly from the loop point.
    /// A value of `0` will disable fade-in, so the track starts mixing at full volume.
    /// Default `0`.
    fade_in_milliseconds: ?usize = null,
    /// If fading in, start fading from this volume level.
    /// `0` is silence and `1` is full volume, every in between is a linear change in gain.
    /// The specified value will be clamped between `0` and `1`.
    /// Default `0`.
    fade_in_start_gain: ?f32 = null,
    /// At the end of mixing this track, after all loops are complete, append this many sample frames of silence as if it were part of the audio file.
    /// This allows for apps to implement effects in callbacks, like reverb, that need to generate samples past the end of the stream's audio,
    /// or perhaps introduce a delay before starting a new sound on the track without having to manage it directly.
    /// A value of `0` generates no silence before stopping the track.
    /// Default `0`.
    append_silence_frames: ?usize = null,
    /// At the end of mixing this track, after all loops are complete, append this many milliseconds of silence as if it were part of the audio file.
    /// This allows for apps to implement effects in callbacks, like reverb, that need to generate samples past the end of the stream's audio,
    /// or perhaps introduce a delay before starting a new sound on the track without having to manage it directly.
    /// The `append_silence_frames` property will be preferred if specified.
    /// A value of `0` generates no silence before stopping the track.
    /// Default `0`.
    append_silence_milliseconds: ?usize = null,
    /// If `true`, when input is completely consumed for the track, the mixer will mark the track as stopped (and call any appropriate `TrackStoppedCallback`, etc); to play more,
    /// the track will need to be restarted.
    /// If false, the track will just not contribute to the mix, but it will not be marked as stopped.
    /// There may be clever logic tricks this exposes generally, but this property is specifically useful when the track's input is an `audio.Stream` assigned via `Track.setAudioStream`.
    /// Setting this property to true can be useful when pushing a complete piece of audio to the stream that has a definite ending,
    /// as the track will operate like any other audio was applied.
    /// Setting to false means as new data is added to the stream, the mixer will start using it as soon as possible,
    /// which is useful when audio should play immediately as it drips in: new VoIP packets, etc.
    /// Note that in this situation, if the audio runs out when needed, there will be gaps in the mixed output, so try to buffer enough data to avoid this when possible.
    /// Note that a track is not consider exhausted until all its loops and appended silence have been mixed (and also, that loops don't mean anything when the input is an `audio.Stream`).
    /// Default `true`.
    halt_when_exhausted: ?bool = null,
    /// This is a special-case property that most apps can ignore.
    /// For mod file formats, start mixing from a specific "order" index instead of the start of the file.
    /// A `null` value will cause this property to be ignored.
    /// If the decoder doesn't support this property, it will also be ignored.
    /// If this property is not ignored, the `start_frame` and `start_millisecond` properties will be ignored instead.
    /// Default `null`.
    start_order: ?struct { value: ?usize } = null,

    /// Convert to an SDL properties group.
    pub fn toProperties(
        self: PlayOptions,
    ) !properties.Group {
        const ret = try properties.Group.init();
        if (self.loops) |val| try ret.set(
            c.MIX_PROP_PLAY_LOOPS_NUMBER,
            .{ .number = val.toSdl() },
        );
        if (self.max_frame) |val| try ret.set(
            c.MIX_PROP_PLAY_MAX_FRAME_NUMBER,
            .{ .number = if (val.value) |v| @intCast(v) else -1 },
        );
        if (self.max_milliseconds) |val| try ret.set(
            c.MIX_PROP_PLAY_MAX_MILLISECONDS_NUMBER,
            .{ .number = if (val.value) |v| @intCast(v) else -1 },
        );
        if (self.start_frame) |val| try ret.set(
            c.MIX_PROP_PLAY_START_FRAME_NUMBER,
            .{ .number = @intCast(val) },
        );
        if (self.start_millisecond) |val| try ret.set(
            c.MIX_PROP_PLAY_START_MILLISECOND_NUMBER,
            .{ .number = @intCast(val) },
        );
        if (self.loop_start_frame) |val| try ret.set(
            c.MIX_PROP_PLAY_LOOP_START_FRAME_NUMBER,
            .{ .number = @intCast(val) },
        );
        if (self.loop_start_millisecond) |val| try ret.set(
            c.MIX_PROP_PLAY_LOOP_START_MILLISECOND_NUMBER,
            .{ .number = @intCast(val) },
        );
        if (self.fade_in_frames) |val| try ret.set(
            c.MIX_PROP_PLAY_FADE_IN_FRAMES_NUMBER,
            .{ .number = @intCast(val) },
        );
        if (self.fade_in_milliseconds) |val| try ret.set(
            c.MIX_PROP_PLAY_FADE_IN_MILLISECONDS_NUMBER,
            .{ .number = @intCast(val) },
        );
        if (self.fade_in_start_gain) |val| try ret.set(
            c.MIX_PROP_PLAY_FADE_IN_START_GAIN_FLOAT,
            .{ .float = val },
        );
        if (self.append_silence_frames) |val| try ret.set(
            c.MIX_PROP_PLAY_APPEND_SILENCE_FRAMES_NUMBER,
            .{ .number = @intCast(val) },
        );
        if (self.append_silence_milliseconds) |val| try ret.set(
            c.MIX_PROP_PLAY_APPEND_SILENCE_MILLISECONDS_NUMBER,
            .{ .number = @intCast(val) },
        );
        if (self.halt_when_exhausted) |val| try ret.set(
            c.MIX_PROP_PLAY_HALT_WHEN_EXHAUSTED_BOOLEAN,
            .{ .boolean = val },
        );
        if (self.start_order) |val| try ret.set(
            c.MIX_PROP_PLAY_START_ORDER_NUMBER,
            .{ .number = if (val.value) |v| @intCast(v) else -1 },
        );
        return ret;
    }
};

/// A set of per-channel gains for tracks using `track.setStereo`.
///
/// ## Remarks
/// When forcing a track to stereo, the app can specify a per-channel gain, to further adjust the left or right outputs.
/// When mixing audio that has been forced to stereo, each channel is modulated by these values. A value of `1` produces no change, `0` produces silence.
/// A simple panning effect would be to set left to the desired value and right to `1 - left`.
///
/// ## Version
/// This struct is available since SDL_mixer 3.0.0.
pub const StereoGains = extern struct {
    /// Left channel gain.
    left: f32,
    /// Right channel gain.
    right: f32,

    comptime {
        errors.assertStructsEqual(@This(), c.MIX_StereoGains);
    }

    /// Convert from an SDL value.
    pub fn fromSdl(
        value: c.MIX_StereoGains,
    ) StereoGains {
        return .{
            .left = value.left,
            .right = value.right,
        };
    }

    /// Convert to an SDL value.
    pub fn toSdl(
        self: StereoGains,
    ) c.MIX_StereoGains {
        return .{
            .left = self.left,
            .right = self.right,
        };
    }
};

/// An opaque object that represents a source of sound output to be mixed.
///
/// ## Remarks
/// A MIX Mixer has an arbitrary number of tracks, and each track manages its own unique audio to be mixed together.
///
/// Tracks also have other properties: gain, loop points, fading, 3D position, and other attributes that alter the produced sound; many can be altered during playback.
///
/// ## Version
/// This datatype is available since SDL_mixer 3.0.0.
pub const Track = struct {
    value: *c.MIX_Track,

    /// Destroy the specified track.
    ///
    /// ## Function Parameters
    /// * `self`: The track to destroy.
    ///
    /// ## Remarks
    /// If the track is currently playing, it will be stopped immediately, without any fadeout.
    /// If there is a callback set through `Track.setStoppedCallback`, it will not be called.
    ///
    /// If the mixer is currently mixing in another thread, this will block until it finishes.
    /// Destroying a track from the mixer thread itself (during a callback) will cause it to be destroyed as soon as this iteration of the mixer thread is not using it;
    /// in this scenario, destroying a track and then making futher changes to it is considered undefined behavior.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn deinit(
        self: Track,
    ) void {
        c.MIX_DestroyTrack(
            self.value,
        );
    }

    /// Convert sample frames for a track's current format to milliseconds.
    ///
    /// ## Function Parameters
    /// * `self`: The track to query.
    /// * `frames`: The track-specific sample frames to convert to milliseconds.
    ///
    /// ## Return Value
    /// Returns converted number of milliseconds.
    ///
    /// ## Remarks
    /// This calculates time based on the track's current input format, which can change when its input does,
    /// and also if that input changes formats mid-stream (for example, if decoding a file that is two MP3s concatenated together).
    ///
    /// Sample frames are more precise than milliseconds, so out of necessity, this function will approximate by rounding down to the closest full millisecond.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn framesToMs(
        self: Track,
        frames: usize,
    ) !usize {
        return @intCast(try errors.wrapCall(c.Sint64, c.MIX_TrackFramesToMS(self.value, @intCast(frames)), -1));
    }

    /// Get a track's current position in 3D space.
    ///
    /// ## Function Parameters
    /// * `self`: The track to query.
    ///
    /// ## Return Value
    /// The track's position.
    ///
    /// ## Remarks
    /// If 3D positioning isn't enabled for this track, through a call to `Track.set3dPosition`, this will return the zero vector.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn get3dPosition(
        self: Track,
    ) !Point3d {
        var ret: c.MIX_Point3D = undefined;
        try errors.wrapCallBool(c.MIX_GetTrack3DPosition(self.value, &ret));
        return .fromSdl(ret);
    }

    /// Query the audio assigned to a track.
    ///
    /// ## Function Parameters
    /// * `self`: The track to query.
    ///
    /// ## Return Value
    /// Returns the audio if available.
    ///
    /// ## Remarks
    /// This returns the `Audio` object currently assigned to track through a call to `Track.setAudio`.
    /// If there is none assigned, or the track has an input that isn't an `Audio` (such as an `sdl3.audio.Stream` or `sdl3.io_stream.Stream`), this will return `null`.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn getAudio(
        self: Track,
    ) ?Audio {
        const ret = c.MIX_GetTrackAudio(self.value);
        if (ret) |val|
            return .{ .value = val };
        return null;
    }

    /// Query the audio stream assigned to a track.
    ///
    /// ## Function Parameters
    /// * `self`: The track to query.
    ///
    /// ## Return Value
    /// Returns the audio stream if available.
    ///
    /// ## Remarks
    /// This returns the audio stream object currently assigned to track through a call to `Track.setAudioStream`.
    /// If there is none assigned, or the track has an input that isn't an SDL_AudioStream (such as a `Audio` or `sdl3.io_stream.Stream`), this will return `null`.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn getAudioStream(
        self: Track,
    ) ?audio.Stream {
        const ret = c.MIX_GetTrackAudioStream(self.value);
        if (ret) |val|
            return .{ .value = val };
        return null;
    }

    /// Query whether a given track is fading.
    ///
    /// ## Function Parameters
    /// * `self`: The track to query.
    ///
    /// ## Return Value
    /// Returns less than `0` if the track is fading out, greater than `0` if fading in, `0` otherwise.
    ///
    /// ## Remarks
    /// This specifically checks if the track is not stopped (paused or playing), and it is fading in or out, and returns the number of frames remaining in the fade.
    ///
    /// If fading out, the returned value will be negative.
    /// When fading in, the returned value will be positive.
    /// If not fading, this function returns zero.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn getFadeFrames(
        self: Track,
    ) isize {
        return @intCast(c.MIX_GetTrackFadeFrames(self.value));
    }

    /// Query the frequency ratio of a track.
    ///
    /// ## Function Parameters
    /// * `self`: The track on which to query the frequency ratio.
    ///
    /// ## Return Value
    /// Returns the current frequency ratio.
    ///
    /// ## Remarks
    /// The frequency ratio is used to adjust the rate at which audio data is consumed.
    /// Changing this effectively modifies the speed and pitch of the track's audio.
    /// A value greater than 1.0f will play the audio faster, and at a higher pitch.
    /// A value less than `1` will play the audio slower, and at a lower pitch.
    /// `1` is normal speed.
    ///
    /// The default value is `1`.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn getFrequencyRatio(
        self: Track,
    ) !f32 {
        return errors.wrapCall(f32, c.MIX_GetTrackFrequencyRatio(self.value), 0);
    }

    /// Get a track's gain control.
    ///
    /// ## Function Parameters
    /// * `self`: The track to query.
    ///
    /// ## Return Value
    /// Returns the current track gain.
    ///
    /// ## Remarks
    /// This returns the last value set through `Track.setGain`, or `1` if no value has ever been explicitly set.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn getGain(
        self: Track,
    ) f32 {
        return c.MIX_GetTrackGain(
            self.value,
        );
    }

    /// Query how many loops remain for a given track.
    ///
    /// ## Function Parameters
    /// * `self`: The track to query.
    ///
    /// ## Return Value
    /// Returns the number of pending loops.
    ///
    /// ## Remarks
    /// This returns the number of loops still pending; if a track will eventually complete and loop to play again one more time, this will return one loop.
    /// If a track was looping but is on its final iteration of the loop (will stop when this iteration completes), this will return zero loops.
    ///
    /// A track that is stopped (not playing and not paused) will have zero loops remaining.
    ///
    /// On various errors (`init` was not called), this returns zero loops, but there is no mechanism to distinguish errors from non-looping tracks.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn getLoops(
        self: Track,
    ) LoopCount {
        const ret = c.MIX_GetTrackLoops(self.value);
        return LoopCount.fromSdl(ret);
    }

    /// Get the `Mixer` that owns a `Track`.
    ///
    /// ## Function Parameters
    /// * `self`: The track to query.
    ///
    /// ## Return Value
    /// Returns the mixer associated with the track.
    ///
    /// ## Remarks
    /// This is the mixer pointer that was passed to `Track.init`.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn getMixer(
        self: Track,
    ) !Mixer {
        return .{ .value = try errors.wrapCallNull(*c.MIX_Mixer, c.MIX_GetTrackMixer(self.value)) };
    }

    /// Get the current input position of a playing track.
    ///
    /// ## Function Parameters
    /// * `self`: The track to change.
    ///
    /// ## Return Value
    /// Returns the track's current sample frame position.
    ///
    /// ## Remarks
    /// Not to be confused with `Track.get3dPosition`, which is positioning of the track in 3D space, not the playback position of its audio data.
    ///
    /// Position is defined in sample frames of decoded audio, not units of time, so that sample-perfect mixing can be achieved.
    /// To instead operate in units of time, use `Track.framesToMs` to convert the return value to milliseconds.
    ///
    /// Stopped and paused tracks will report the position when they halted.
    /// Playing tracks will report the current position, which will change over time.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn getPlaybackPosition(
        self: Track,
    ) !usize {
        return @intCast(try errors.wrapCall(c.Sint64, c.MIX_GetTrackPlaybackPosition(self.value), -1));
    }

    /// Get the properties associated with a track.
    ///
    /// ## Function Parameters
    /// * `self`: The track to query.
    ///
    /// ## Return Value
    /// Returns the track properties.
    ///
    /// ## Remarks
    /// Currently SDL mixer assigns no properties of its own to a track, but this can be a convenient place to store app-specific data.
    ///
    /// A properties group is created the first time this function is called for a given track.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn getProperties(
        self: Track,
    ) !properties.Group {
        return .{ .value = try errors.wrapCall(c.SDL_PropertiesID, c.MIX_GetTrackProperties(self.value), 0) };
    }

    /// Return the number of sample frames remaining to be mixed in a track.
    ///
    /// ## Function Parameters
    /// * `self`: The track to query.
    ///
    /// ## Return Value
    /// Returns the total sample frames still to be mixed, or `null` if unknown.
    ///
    /// ## Remarks
    /// If the track is playing or paused, and its total duration is known, this will report how much audio is left to mix.
    /// If the track is playing, future calls to this function will report different values.
    ///
    /// Remaining audio is defined in sample frames of decoded audio, not units of time, so that sample-perfect mixing can be achieved.
    /// To instead operate in units of time, use `Track.framesToMs` to convert the return value to milliseconds.
    ///
    /// This function does not take into account fade-outs or looping, just the current mixing position vs the duration of the track.
    ///
    /// If the duration of the track isn't known, this function returns `null`.
    /// A stopped track reports `0`.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn getRemaining(
        self: Track,
    ) ?usize {
        const ret = c.MIX_GetTrackRemaining(self.value);
        if (ret == -1)
            return null;
        return @intCast(ret);
    }

    /// Get the tags currently associated with a track.
    ///
    /// ## Function Parameters
    /// * `self`: The track to query.
    ///
    /// ## Return Value
    /// Returns an array of the tags.
    /// This is a single allocation that should be freed with `sdl3.free` when it is no longer needed.
    ///
    /// ## Remarks
    /// Tags are not provided in any guaranteed order.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn getTags(
        self: Track,
    ) ![][*:0]const u8 {
        var num: c_int = undefined;
        return @as([*][*:0]const u8, @ptrCast(@alignCast(c.MIX_GetTrackTags(self.value, &num))))[0..@intCast(num)];
    }

    /// Create a new track on a mixer.
    ///
    /// ## Function Parameters
    /// * `mixer`: The mixer to create the track on.
    ///
    /// ## Return Value
    /// Returns a new `mixer.Track`. The caller must call `deinit()` on it when done.
    ///
    /// ## Remarks
    /// A track provides a single source of audio.
    /// All currently-playing tracks will be processed and mixed together to form the final output from the mixer.
    ///
    /// There are no limits to the number of tracks one may create, beyond running out of memory,
    /// but in normal practice there are a small number of tracks that are reused between all loaded audio as appropriate.
    ///
    /// Tracks are unique to a specific `Mixer` and can't be transferred between them.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn init(
        mixer: Mixer,
    ) !Track {
        const ret = c.MIX_CreateTrack(
            mixer.value,
        );
        return Track{ .value = try errors.wrapCallNull(*c.MIX_Track, ret) };
    }

    /// Query if a track is currently playing.
    ///
    /// ## Function Parameters
    /// * `self`: The track to query.
    ///
    /// ## Return Value
    /// Returns `true` if the track is playing, `false` otherwise.
    ///
    /// ## Remarks
    /// If this returns true, the track is currently contributing to the mixer's output (it's "playing"). It is not stopped nor paused.
    ///
    /// On various errors (`init` was not called), this returns false, but there is no mechanism to distinguish errors from non-playing tracks.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn isPlaying(
        self: Track,
    ) bool {
        return c.MIX_TrackPlaying(
            self.value,
        );
    }

    /// Query if a track is currently paused.
    ///
    /// ## Function Parameters
    /// * `self`: The track to query.
    ///
    /// ## Return Value
    /// Returns `true` if the track is paused, `false` otherwise.
    ///
    /// ## Remarks
    /// If this returns true, the track is not currently contributing to the mixer's output but will when resumed (it's "paused").
    /// It is not playing nor stopped.
    ///
    /// On various errors (`init` was not called), this returns false, but there is no mechanism to distinguish errors from non-playing tracks.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn isPaused(
        self: Track,
    ) bool {
        return c.MIX_TrackPaused(
            self.value,
        );
    }

    /// Convert milliseconds to sample frames for a track's current format.
    ///
    /// ## Function Parameters
    /// * `self`: The track to query.
    /// * `ms`: The milliseconds to convert to track-specific sample frames.
    ///
    /// ## Return Value
    /// Returns converted number of sample frames.
    ///
    /// ## Remarks
    /// This calculates time based on the track's current input format, which can change when its input does,
    /// and also if that input changes formats mid-stream (for example, if decoding a file that is two MP3s concatenated together).
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn msToFrames(
        self: Track,
        ms: usize,
    ) !usize {
        return @intCast(try errors.wrapCall(c.Sint64, c.MIX_TrackMSToFrames(self.value, @intCast(ms)), -1));
    }

    /// Pause a currently-playing track.
    ///
    /// ## Function Parameters
    /// * `self`: The track to pause.
    ///
    /// ## Remarks
    /// A paused track is not considered "stopped," so its `TrackStoppedCallback` will not fire if paused, but it won't change state by default, generate audio,
    /// or generally make progress, until it is resumed.
    ///
    /// It is legal to pause a track that's in any state (playing, already paused, or stopped).
    /// Unless the track is currently playing, pausing does nothing.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn pause(
        self: Track,
    ) !void {
        const ret = c.MIX_PauseTrack(
            self.value,
        );
        return errors.wrapCallBool(ret);
    }

    /// Start (or restart) mixing a track for playback.
    ///
    /// ## Function Parameters
    /// * `self`: The track to start (or restart) mixing.
    /// * `options`: A set of properties that control playback.
    ///
    /// ## Remarks
    /// The track will use whatever input was last assigned to it when playing; an input must be assigned to this track or this function will fail.
    /// Inputs are assigned with calls to `Track.setAudio`, `Track.setAudioStream`, or `Track.setIoStream`.
    ///
    /// If the track is already playing, or paused, this will restart the track with the newly-specified parameters.
    ///
    /// As there are several parameters, and more may be added in the future, they are specified with properties.
    ///
    /// If this function fails, mixing of this track will not start (or restart, if it was already started).
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn play(
        self: Track,
        options: ?PlayOptions,
    ) !void {
        if (options) |val| {
            const group = try val.toProperties();
            defer group.deinit();
            const ret = c.MIX_PlayTrack(
                self.value,
                group.value,
            );
            return errors.wrapCallBool(ret);
        } else {
            return errors.wrapCallBool(c.MIX_PlayTrack(
                self.value,
                0,
            ));
        }
    }

    /// Resume a currently-paused track.
    ///
    /// ## Function Parameters
    /// * `self`: The track to resume.
    ///
    /// ## Remarks
    /// A paused track is not considered "stopped," so its `TrackStoppedCallback will not fire if paused, but it won't change state by default, generate audio,
    /// or generally make progress, until it is resumed.
    ///
    /// It is legal to resume a track that's in any state (playing, paused, or stopped).
    /// Unless the track is currently paused, resuming does nothing.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn resumeTrack(
        self: Track,
    ) !void {
        const ret = c.MIX_ResumeTrack(
            self.value,
        );
        return errors.wrapCallBool(ret);
    }

    /// Set a track's position in 3D space.
    ///
    /// ## Function Parameters
    /// * `self`: The track for which to set 3D position.
    /// * `position`: The new 3D position for the track.
    ///
    /// ## Remarks
    /// Please note that SDL mixer is not intended to be a extremely powerful 3D API.
    /// It lacks 3D features that other APIs like OpenAL offer: there's no doppler effect, distance models, rolloff, etc.
    /// This is meant to be Good Enough for games that can use some positional sounds and can even take advantage of surround-sound configurations.
    ///
    /// If position is not `null`, this track will be switched into 3D positional mode.
    /// If position is `null`, this will disable positional mixing (both the full 3D spatialization of this function and forced-stereo mode of `Track.setStereo`).
    ///
    /// In 3D positional mode, SDL mixer will mix this track as if it were positioned in 3D space,
    /// including distance attenuation (quieter as it gets further from the listener) and spatialization (positioned on the correct speakers to suggest direction,
    /// either with stereo outputs or full surround sound).
    ///
    /// For a mono speaker output, spatialization is effectively disabled but distance attenuation will still work, which is all you can really do with a single speaker.
    ///
    /// The coordinate system operates like OpenGL or OpenAL: a "right-handed" coordinate system.
    /// See `Point3D` for the details.
    ///
    /// The listener is always at coordinate `(0, 0, 0)` and can't be changed.
    ///
    /// The track's input will be converted to mono (1 channel) so it can be rendered across the correct speakers.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn set3dPosition(
        self: Track,
        position: ?Point3d,
    ) !void {
        return errors.wrapCallBool(c.MIX_SetTrack3DPosition(self.value, if (position) |val| &val.toSdl() else null));
    }

    /// Assign a track's input to an audio.
    ///
    /// ## Function Parameters
    /// * `self`: The track on which to set a new audio input.
    /// * `audio_data`: The audio object to assign to the track.
    ///
    /// ## Remarks
    /// An `Audio` is audio data stored in RAM (possibly still in a compressed form).
    /// One `Audio` can be assigned to multiple tracks at once.
    ///
    /// Once a track has a valid input, it can start mixing sound by calling `Track.play`, or possibly `Mixer.playTag`.
    ///
    /// Calling this function with a `null` audio input is legal, and removes any input from the track.
    /// If the track was currently playing, the next time the mixer runs, it'll notice this and mark the track as stopped, calling any assigned `TrackStoppedCallback`.
    ///
    /// It is legal to change the input of a track while it's playing, however some states, like loop points, may cease to make sense with the new audio.
    /// In such a case, one can call `Track.play` again to adjust parameters.
    ///
    /// The track will hold a reference to the provided `Audio`, so it is safe to call `Audio.deinit` on it while the track is still using it.
    /// The track will drop its reference (and possibly free the resources) once it is no longer using the `Audio`.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn setAudio(
        self: Track,
        audio_data: ?Audio,
    ) !void {
        const ret = c.MIX_SetTrackAudio(
            self.value,
            if (audio_data) |val| val.value else null,
        );
        return errors.wrapCallBool(ret);
    }

    /// Set a track's input to an `audio.Stream`.
    ///
    /// ## Function Parameters
    /// * `self`: The track on which to set a new audio input.
    /// * `stream`: The audio stream to use as the track's input.
    ///
    /// ## Remarks
    /// Using an audio stream allows the application to generate any type of audio, in any format, possibly procedurally or on-demand, and mix in with all other tracks.
    ///
    /// When a track uses an audio stream, it will call `audio.Stream.getData` as it needs more audio to mix.
    /// The app can either buffer data to the stream ahead of time, or set a callback on the stream to provide data as needed.
    /// Please refer to SDL's documentation for details.
    ///
    /// A given audio stream may only be assigned to a single track at a time; duplicate assignments won't return an error,
    /// but assigning a stream to multiple tracks will cause each track to read from the stream arbitrarily, causing confusion and incorrect mixing.
    ///
    /// Once a track has a valid input, it can start mixing sound by calling `Track.play`, or possibly `Mixer.playTag`.
    ///
    /// Calling this function with a `null` audio stream is legal, and removes any input from the track.
    /// If the track was currently playing, the next time the mixer runs, it'll notice this and mark the track as stopped, calling any assigned `TrackStoppedCallback`.
    ///
    /// It is legal to change the input of a track while it's playing, however some states, like loop points, may cease to make sense with the new audio.
    /// In such a case, one can call `Track.play` again to adjust parameters.
    ///
    /// The provided audio stream must remain valid until the track no longer needs it (either by changing the track's input or destroying the track).
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn setAudioStream(
        self: Track,
        stream: ?audio.Stream,
    ) !void {
        const ret = c.MIX_SetTrackAudioStream(
            self.value,
            if (stream) |val| val.value else null,
        );
        return errors.wrapCallBool(ret);
    }

    /// Set a callback that fires when the mixer has transformed a track's audio.
    ///
    /// ## Function Parameters
    /// * `self`: The track to assign this callback to.
    /// * `UserData`: Type of user data to pass to the callback.
    /// * `cb`: The function to call when the track mixes, may be `null`.
    /// * `user_data`: User data to pass to the callback.
    ///
    /// ## Remarks
    /// As a track needs to mix more data, it pulls from its input (an audio, an audio stream, etc).
    /// This input might be a compressed file format, like MP3, so a little more data is uncompressed from it.
    ///
    /// Once the track has PCM data to start operating on, and its raw callback has completed, it will begin to transform the audio: gain, fading, frequency ratio, 3D positioning, etc.
    ///
    /// A callback can be fired after all these transformations, but before the transformed data is mixed into other tracks.
    /// This lets an app view the data at the last moment that it is still a part of this track.
    /// It can also change the data in any way it pleases during this callback, and the mixer will continue as if this data came directly from the input.
    ///
    /// Each track has its own unique cooked callback.
    ///
    /// Passing a `null` callback here is legal; it disables this track's callback.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn setCookedCallback(
        self: Track,
        comptime UserData: type,
        comptime cb: ?TrackMixCallback(UserData),
        user_data: ?*anyopaque,
    ) !void {
        const Cb = struct {
            fn run(
                c_user_data: ?*anyopaque,
                c_track: [*c]c.MIX_Track,
                c_spec: [*c]const c.SDL_AudioSpec,
                c_pcm: [*c]f32,
                c_samples: c_int,
            ) callconv(.c) void {
                cb.?(@ptrCast(@alignCast(c_user_data)), .{ .value = c_track }, audio.Spec.fromSdl(c_spec), c_pcm[0..@intCast(c_samples)]);
            }
        };
        return errors.wrapCallBool(c.MIX_SetTrackCookedCallback(
            self.value,
            if (cb != null) &Cb.run else null,
            @ptrCast(@alignCast(user_data)),
        ));
    }

    /// Change the frequency ratio of a track.
    ///
    /// ## Function Parameters
    /// * `self`: The track on which to change the frequency ratio.
    /// * `ratio`: The frequency ratio. Must be between `0.01` and `100`.
    ///
    /// ## Remarks
    /// The frequency ratio is used to adjust the rate at which audio data is consumed.
    /// Changing this effectively modifies the speed and pitch of the track's audio.
    /// A value greater than `1` will play the audio faster, and at a higher pitch.
    /// A value less than `1` will play the audio slower, and at a lower pitch.
    /// `1` is normal speed.
    ///
    /// The default value is `1`.
    ///
    /// This value can be changed at any time to adjust the future mix.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn setFrequencyRatio(
        self: Track,
        ratio: f32,
    ) !void {
        return errors.wrapCallBool(c.MIX_SetTrackFrequencyRatio(self.value, ratio));
    }

    /// Set a track's gain control.
    ///
    /// ## Function Parameters
    /// * `self`: The track to adjust.
    /// * `gain`: The new gain value.
    ///
    /// ## Remarks
    /// Each track has its own gain, to adjust its overall volume.
    /// Each sample from this track is modulated by this gain value.
    /// A gain of zero will generate silence, `1` will not change the mixed volume, and larger than `1` will increase the volume.
    /// Negative values are illegal.
    /// There is no maximum gain specified, but this can quickly get extremely loud, so please be careful with this setting.
    ///
    /// A track's gain defaults to `1`.
    ///
    /// This value can be changed at any time to adjust the future mix.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn setGain(
        self: Track,
        gain: f32,
    ) !void {
        const ret = c.MIX_SetTrackGain(
            self.value,
            gain,
        );
        return errors.wrapCallBool(ret);
    }

    /// Assign a track to a mixing group.
    ///
    /// ## Function Parameters
    /// * `self`: The track to set mixing group assignment.
    /// * `group`: The group to assign the track to, or `null` to remove it from any group.
    ///
    /// ## Remarks
    /// All tracks in a group are mixed together, and that output is made available to the app before it is mixed into the final output.
    ///
    /// Tracks can only be in one group at a time, and the track and group must have been created on the same `Mixer`.
    ///
    /// Setting a track to a `null` group will remove it from any app-created groups, and reassign it to the mixer's internal default group.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn setGroup(
        self: Track,
        group: ?Group,
    ) !void {
        const ret = c.MIX_SetTrackGroup(
            self.value,
            if (group) |val| val.value else null,
        );
        return errors.wrapCallBool(ret);
    }

    /// Set a track's input to an IO stream.
    ///
    /// ## Function Parameters
    /// * `self`: The track on which to set a new audio input.
    /// * `io`: The new i/o stream to use as the track's input.
    /// * `close_io`: If true, close the stream when done with it.
    ///
    /// ## Remarks
    /// This is not the recommended way to set a track's input, but this can be useful for a very specific scenario: a large file,
    /// to be played once, that must be read from disk in small chunks as needed.
    /// In most cases, however, it is preferable to create an `Audio` ahead of time and use `Track.setAudio` instead.
    ///
    /// The stream supplied here should provide an audio file in a supported format.
    /// SDL_mixer will parse it during this call to make sure it's valid, and then will read file data from the stream as it needs to decode more during mixing.
    ///
    /// The stream must be able to seek through the complete set of data, or this function will fail.
    ///
    /// A given IOStream may only be assigned to a single track at a time; duplicate assignments won't return an error,
    /// but assigning a stream to multiple tracks will cause each track to read from the stream arbitrarily, causing confusion, incorrect mixing, or failure to decode.
    ///
    /// Once a track has a valid input, it can start mixing sound by calling `Track.play`, or possibly `Mixer.playTag`.
    ///
    /// Calling this function with a `null` stream is legal, and removes any input from the track.
    /// If the track was currently playing, the next time the mixer runs, it'll notice this and mark the track as stopped, calling any assigned `TrackStoppedCallback`.
    ///
    /// It is legal to change the input of a track while it's playing, however some states, like loop points, may cease to make sense with the new audio.
    /// In such a case, one can call `Track.play` again to adjust parameters.
    ///
    /// The provided stream must remain valid until the track no longer needs it (either by changing the track's input or destroying the track).
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn setIoStream(
        self: Track,
        io: ?io_stream.Stream,
        close_io: bool,
    ) !void {
        return errors.wrapCallBool(c.MIX_SetTrackIOStream(self.value, if (io) |val| val.value else null, close_io));
    }

    /// Change the number of times a currently-playing track will loop.
    ///
    /// ## Function Parameters
    /// * `self`: The track to configure.
    /// * `loops`: New number of times to loop. Zero to disable looping.
    ///
    /// ## Remarks
    /// This replaces any previously-set remaining loops.
    /// A value of `1` will loop to the start of playback one time.
    /// Zero will not loop at all.
    /// If the input is not seekable and `loop` isn't zero, this function will report success but the track will stop at the point it should loop.
    ///
    /// The new loop count replaces any previous state, even if the track has already looped.
    ///
    /// This has no effect on a track that is stopped, or rather, starting a stopped track later will set a new loop count, replacing this value.
    /// Stopped tracks can specify a loop count while starting via `play_loops`.
    /// This function is intended to alter that count in the middle of playback.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn setLoops(
        self: Track,
        loops: LoopCount,
    ) !void {
        return errors.wrapCallBool(c.MIX_SetTrackLoops(self.value, loops.toSdl()));
    }

    /// Set the current output channel map of a track.
    ///
    /// ## Function Parameters
    /// * `self`: The track to change.
    /// * `channel_map`: The new channel map, `null` to reset to default.
    ///
    /// ## Remarks
    /// Channel maps are optional; most things do not need them, instead passing data in the order that SDL expects.
    ///
    /// The output channel map reorders track data after transformations and before it is mixed into a mixer group.
    /// This can be useful for reversing stereo channels, for example.
    ///
    /// Each item in the array represents an input channel, and its value is the channel that it should be remapped to.
    /// To reverse a stereo signal's left and right values, you'd have an array of `.{ 1, 0 }`.
    /// It is legal to remap multiple channels to the same thing, so `.{ 1, 1 }` would duplicate the right channel to both channels of a stereo signal.
    /// An element in the channel map set to `-1` instead of a valid channel will mute that channel, setting it to a silence value.
    ///
    /// You cannot change the number of channels through a channel map, just reorder/mute them.
    ///
    /// Tracks default to no remapping applied.
    /// Passing a `null` channel map is legal, and turns off remapping.
    ///
    /// SDL mixer will copy the channel map; the caller does not have to save this array after this call.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    ///## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn setOutputChannelMap(
        self: Track,
        channel_map: ?[]const c_int,
    ) !void {
        return errors.wrapCallBool(c.MIX_SetTrackOutputChannelMap(self.value, if (channel_map) |val| val.ptr else null, if (channel_map) |val| @intCast(val.len) else 0));
    }

    /// Seek a playing track to a new position in its input.
    ///
    /// ## Function Parameters
    /// * `self`: The track to change.
    /// * `frames`: The sample frame position to seek to.
    ///
    /// ## Remarks
    /// Not to be confused with `Track.set3dPosition`, which is positioning of the track in 3D space, not the playback position of its audio data.
    ///
    /// On a playing track, the next time the mixer runs, it will start mixing from the new position.
    ///
    /// Position is defined in sample frames of decoded audio, not units of time, so that sample-perfect mixing can be achieved.
    /// To instead operate in units of time, use `Track.msToFrames` to get the approximate sample frames for a given tick.
    ///
    /// This function requires an input that can seek (so it can not be used if the input was set with `Track.setAudioStream`), and a audio file format that allows seeking.
    /// SDL mixer's decoders for some file formats do not offer seeking, or can only seek to times, not exact sample frames,
    /// in which case the final position may be off by some amount of sample frames.
    /// Please check your audio data and file bug reports if appropriate.
    ///
    /// It's legal to call this function on a track that is stopped, but a future call to `Track.play` will reset the start position anyhow.
    /// Paused tracks will resume at the new input position.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn setPlaybackPosition(
        self: Track,
        frames: usize,
    ) !void {
        return errors.wrapCallBool(c.MIX_SetTrackPlaybackPosition(self.value, @intCast(frames)));
    }

    /// Set a callback that fires when a track has initial decoded audio.
    ///
    /// ## Function Parameters
    /// * `self`: The track to assign this callback to.
    /// * `UserData`: The type for specifying user data.
    /// * `cb`: The function to call when the track mixes, may be `null`.
    /// * `user_data`: User data to pass to the callback.
    ///
    /// ## Remarks
    /// As a track needs to mix more data, it pulls from its input (an `Audio`, an `audio.Stream`, etc).
    /// This input might be a compressed file format, like MP3, so a little more data is uncompressed from it.
    ///
    /// Once the track has PCM data to start operating on, it can fire a callback before any changes to the raw PCM input have happened.
    /// This lets an app view the data before it has gone through transformations such as gain, 3D positioning, fading, etc.
    /// It can also change the data in any way it pleases during this callback, and the mixer will continue as if this data came directly from the input.
    ///
    /// Each track has its own unique raw callback.
    ///
    /// Passing a `null` callback here is legal; it disables this track's callback.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn setRawCallback(
        self: Track,
        comptime UserData: type,
        comptime cb: ?TrackMixCallback(UserData),
        user_data: ?*UserData,
    ) !void {
        const Cb = struct {
            fn run(
                c_user_data: ?*anyopaque,
                c_track: [*c]c.MIX_Track,
                c_spec: [*c]const c.SDL_AudioSpec,
                c_pcm: [*c]f32,
                c_samples: c_int,
            ) callconv(.c) void {
                cb.?(@ptrCast(@alignCast(c_user_data)), .{ .value = c_track }, audio.Spec.fromSdl(c_spec), c_pcm[0..@intCast(c_samples)]);
            }
        };
        return errors.wrapCallBool(c.MIX_SetTrackRawCallback(
            self.value,
            if (cb != null) &Cb.run else null,
            @ptrCast(@alignCast(user_data)),
        ));
    }

    /// Set a track's input to an SDL IO stream providing raw PCM data.
    ///
    /// ## Function Parameters
    /// * `self`: The track on which to set a new audio input.
    /// * `io`: The new i/o stream to use as the track's input.
    /// * `spec`: The format of the PCM data that the SDL IO stream will provide.
    /// * `close_io`: If true, close the stream when done with it.
    ///
    /// ## Remarks
    /// This is not the recommended way to set a track's input, but this can be useful for a very specific scenario: a large file, to be played once,
    /// that must be read from disk in small chunks as needed.
    /// In most cases, however, it is preferable to create an `Audio` ahead of time and use `Track.setAudio` instead.
    ///
    /// Also, an `Track.setAudioStream` can also provide raw PCM audio to a track, via an `audio.Stream`,
    /// which might be preferable unless the data is already coming directly from an SDL IO stream.
    ///
    /// The stream supplied here should provide an audio in raw PCM format.
    ///
    /// A given IO stream may only be assigned to a single track at a time; duplicate assignments won't return an error,
    /// but assigning a stream to multiple tracks will cause each track to read from the stream arbitrarily, causing confusion and incorrect mixing.
    ///
    /// Once a track has a valid input, it can start mixing sound by calling `Track.play` or possibly `Mixer.playTag`.
    ///
    /// Calling this function with a `null` stream is legal, and removes any input from the track.
    /// If the track was currently playing, the next time the mixer runs, it'll notice this and mark the track as stopped, calling any assigned `TrackStoppedCallback`.
    ///
    /// It is legal to change the input of a track while it's playing, however some states, like loop points, may cease to make sense with the new audio.
    /// In such a case, one can call `Track.play` again to adjust parameters.
    ///
    /// The provided stream must remain valid until the track no longer needs it (either by changing the track's input or destroying the track).
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn setRawIoStream(
        self: Track,
        io: ?io_stream.Stream,
        spec: audio.Spec,
        close_io: bool,
    ) !void {
        return errors.wrapCallBool(c.MIX_SetTrackRawIOStream(self.value, if (io) |val| val.value else null, &spec.toSdl(), close_io));
    }

    /// Force a track to stereo output, with optionally left/right panning.
    ///
    /// ## Function Parameters
    /// * `self`: The track to adjust.
    /// * `gains`: The per-channel gains, or `null` to disable spatialization.
    ///
    /// ## Remarks
    /// This will cause the output of the track to convert to stereo, and then mix it only onto the Front Left and Front Right speakers, regardless of the speaker configuration.
    /// The left and right channels are modulated by gains, which can be used to produce panning effects.
    /// This function may be called to adjust the gains at any time.
    ///
    /// If gains is not `null`, this track will be switched into forced-stereo mode.
    /// If gains is `null`, this will disable spatialization (both the forced-stereo mode of this function and full 3D spatialization of `Track.setTrack3dPosition`).
    ///
    /// Negative gains are clamped to zero; there is no clamp for maximum, so one could set the value > 1 to make a channel louder.
    ///
    /// The track's 3D position, reported by `Track.get3dPosition`, will be reset to `(0, 0, 0)`.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn setStereo(
        self: Track,
        gains: ?StereoGains,
    ) !void {
        try errors.wrapCallBool(c.MIX_SetTrackStereo(self.value, if (gains) |val| &val.toSdl() else null));
    }

    /// A callback that fires when a track is stopped.
    ///
    /// ## Function Parameters
    /// * `self`: The track to assign this callback to.
    /// * `UserData`: App provided user data type.
    /// * `cb`: The function to call when the track stops.
    /// * `user_data`: App provided user data.
    ///
    /// ## Remarks
    /// When a track completes playback, either because it ran out of data to mix (and all loops were completed as well), or it was explicitly stopped by the app,
    /// it will fire the callback specified here.
    ///
    /// Each track has its own unique callback.
    ///
    /// Passing a `null` callback here is legal; it disables this track's callback.
    ///
    /// Pausing a track will not fire the callback, nor will the callback fire on a playing track that is being destroyed.
    ///
    /// It is legal to adjust the track, including changing its input and restarting it.
    /// If this is done because it ran out of data in the middle of mixing, the mixer will start mixing the new track state in its current run without any gap in the audio.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn setStoppedCallback(
        self: Track,
        comptime UserData: type,
        comptime cb: ?TrackStoppedCallback(UserData),
        user_data: ?*UserData,
    ) !void {
        const Cb = struct {
            fn run(c_user_data: ?*anyopaque, c_track: [*c]c.MIX_Track) callconv(.c) void {
                cb.?(@ptrCast(@alignCast(c_user_data)), .{ .value = c_track });
            }
        };
        return errors.wrapCallBool(c.MIX_SetTrackStoppedCallback(self.value, if (cb == null) null else Cb.run, @ptrCast(@alignCast(user_data))));
    }

    /// Halt a currently-playing track, possibly fading out over time.
    ///
    /// ## Function Parameters
    /// * `self`: The track to halt.
    /// * `fade_out_frames`: The number of sample frames to spend fading out to silence before halting. `0` to stop immediately.
    ///
    /// ## Remarks
    /// If `fade_out_frames > 0`, the track does not stop mixing immediately, but rather fades to silence over that many sample frames before stopping.
    /// Sample frames are specific to the input assigned to the track, to allow for sample-perfect mixing.
    /// `Track.msToFrames` can be used to convert milliseconds to an appropriate value here.
    ///
    /// If the track ends normally while the fade-out is still in progress, the audio stops there; the fade is not adjusted to be shorter if it will last longer than the audio remaining.
    ///
    /// Once a track has completed any fadeout and come to a stop, it will call its `TrackStoppedCallback`, if any.
    /// It is legal to assign the track a new input and/or restart it during this callback.
    ///
    /// It is legal to halt a track that's already stopped.
    /// It does nothing, and returns true.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn stop(
        self: Track,
        fade_out_frames: usize,
    ) !void {
        const ret = c.MIX_StopTrack(
            self.value,
            @intCast(fade_out_frames),
        );
        return errors.wrapCallBool(ret);
    }

    /// Assign an arbitrary tag to a track.
    ///
    /// ## Function Parameters
    /// * `self`: The track to add a tag to.
    /// * `name`: The tag to add.
    ///
    /// ## Remarks
    /// A tag can be any valid C string in UTF-8 encoding.
    /// It can be useful to group tracks in various ways. For example, everything in-game might be marked as "game",
    /// so when the user brings up the settings menu, the app can pause all tracks involved in gameplay at once, but keep background music and menu sound effects running.
    ///
    /// A track can have as many tags as desired, until the machine runs out of memory.
    ///
    /// It's legal to add the same tag to a track more than once; the extra attempts will report success but not change anything.
    ///
    /// Tags can later be removed with `Track.untag`.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn tag(
        self: Track,
        name: [:0]const u8,
    ) !void {
        const ret = c.MIX_TagTrack(
            self.value,
            name,
        );
        return errors.wrapCallBool(ret);
    }

    /// Remove an arbitrary tag from a track.
    ///
    /// ## Function Parameters
    /// * `self`: The track from which to remove a tag.
    /// * `name`: The tag to remove from the track or `null` to remove all current tags.
    ///
    /// ## Remarks
    /// A tag can be any valid C string in UTF-8 encoding.
    /// It can be useful to group tracks in various ways.
    /// For example, everything in-game might be marked as "game", so when the user brings up the settings menu, the app can pause all tracks involved in gameplay at once,
    /// but keep background music and menu sound effects running.
    ///
    /// It's legal to remove a tag that the track doesn't have; this function doesn't report errors, so this simply does nothing.
    ///
    /// Specifying a `null` tag will remove all tags on a track.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn untag(
        self: Track,
        name: ?[:0]const u8,
    ) void {
        c.MIX_UntagTrack(
            self.value,
            if (name) |val| val.ptr else null,
        );
    }
};

/// SDL mixer version information.
pub const Version = struct {
    value: c_int,

    /// SDL mixer version compiled against.
    ///
    /// ## Version
    /// This constant is available since SDL mixer 3.0.0.
    pub const compiled_against = Version{ .value = c.SDL_MIXER_VERSION };

    /// Check if the SDL mixer version is at least greater than the given one.
    ///
    /// ## Function Parameters
    /// * `major`: The major version number.
    /// * `minor`: The minor version number.
    /// * `micro`: The micro version number.
    ///
    /// ## Return Value
    /// Returns if the mixer version is at least this given version.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn atLeast(
        major: u32,
        minor: u32,
        micro: u32,
    ) bool {
        const ret = c.SDL_MIXER_VERSION_ATLEAST(
            major,
            minor,
            micro,
        );
        return ret;
    }

    /// Create an SDL mixer version number.
    ///
    /// ## Function Parameters
    /// * `major`: The major version number.
    /// * `minor`: The minor version number.
    /// * `micro`: The micro version number.
    ///
    /// ## Return Value
    /// Returns the new version number.
    pub fn make(
        major: u32,
        minor: u32,
        micro: u32,
    ) Version {
        const ret = c.SDL_VERSIONNUM(
            @as(c_int, @intCast(major)),
            @as(c_int, @intCast(minor)),
            @as(c_int, @intCast(micro)),
        );
        return Version{ .value = ret };
    }

    /// Get the version of SDL mixer that is linked against your program.
    /// Possibly different than the compiled against version.
    ///
    /// ## Remarks
    /// If you are linking to SDL_mixer dynamically, then it is possible that the current version will be different than the version you compiled against.
    /// This function returns the current version, while `compiled_against` is the version you compiled with.
    ///
    /// This function may be called safely at any time, even before `init`.
    ///
    /// ## Return Value
    /// Returns the version compiled against mixer.
    ///
    /// ## Version
    /// This function is available since SDL_mixer 3.0.0.
    pub fn get() Version {
        const ret = c.MIX_Version();
        return Version{ .value = ret };
    }

    /// Major version number.
    ///
    /// ## Function Parameters
    /// * `self`: The version.
    ///
    /// ## Return Value
    /// The major version number.
    pub fn getMajor(
        self: Version,
    ) u32 {
        const ret = c.SDL_VERSIONNUM_MAJOR(
            self.value,
        );
        return @intCast(ret);
    }

    /// Micro version number.
    ///
    /// ## Function Parameters
    /// * `self`: The version.
    ///
    /// ## Return Value
    /// The micro version number.
    pub fn getMicro(
        self: Version,
    ) u32 {
        const ret = c.SDL_VERSIONNUM_MICRO(
            self.value,
        );
        return @intCast(ret);
    }

    /// Minor version number.
    ///
    /// ## Function Parameters
    /// * `self`: The version.
    ///
    /// ## Return Value
    /// The minor version number.
    pub fn getMinor(
        self: Version,
    ) u32 {
        const ret = c.SDL_VERSIONNUM_MINOR(
            self.value,
        );
        return @intCast(ret);
    }
};

/// Convert sample frames, at a specific sample rate, to milliseconds.
///
/// ## Function Parameters
/// * `sample_rate`: The sample rate to use for conversion.
/// * `frames`: The rate-specific sample frames to convert to milliseconds.
///
/// ## Return Value
/// Returns converted number of milliseconds.
///
/// ## Remarks
/// Sample frames are more precise than milliseconds, so out of necessity, this function will approximate by rounding down to the closest full millisecond.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// Version
/// This function is available since SDL_mixer 3.0.0.
pub fn framesToMs(
    sample_rate: u32,
    frames: usize,
) !usize {
    return @intCast(try errors.wrapCall(c.Sint64, c.MIX_FramesToMS(@intCast(sample_rate), @intCast(frames)), -1));
}

/// Report the name of a specific audio decoders.
///
/// ## Function Parameters
/// * `index`: The index of the decoder to query.
///
/// ## Return Value
/// Returns a UTF-8 (really, ASCII) string of the decoder's name.
///
/// ## Remarks
/// An audio decoder is what turns specific audio file formats into usable PCM data.
/// For example, there might be an MP3 decoder, or a WAV decoder, etc. SDL mixer probably has several decoders built in.
///
/// The names are capital English letters and numbers, low-ASCII.
/// They don't necessarily map to a specific file format; Some decoders, like "XMP" operate on multiple file types, and more than one decoder might handle the same file type,
/// like "DRMP3" vs "MPG123". Note that in that last example, neither decoder is called "MP3".
///
/// The index of a specific decoder is decided during `init` and does not change until the library is deinitialized.
/// Valid indices are between zero and the return value of `getNumAudioDecoders`.
///
/// The returned pointer is const memory owned by SDL mixer; do not free it.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL_mixer 3.0.0.
pub fn getAudioDecoder(
    index: usize,
) ![:0]const u8 {
    return errors.wrapCallCString(c.MIX_GetAudioDecoder(@intCast(index)));
}

/// Report the number of audio decoders available for use.
///
/// ## Return Value
/// Returns the number of decoders available.
///
/// ## Remarks
/// An audio decoder is what turns specific audio file formats into usable PCM data.
/// For example, there might be an MP3 decoder, or a WAV decoder, etc. SDL mixer probably has several decoders built in.
///
/// The return value can be used to call `getAudioDecoder` in a loop.
///
/// The number of decoders available is decided during `init` and does not change until the library is deinitialized.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL_mixer 3.0.0.
pub fn getNumAudioDecoders() usize {
    return @intCast(c.MIX_GetNumAudioDecoders());
}

/// Initialize the SDL mixer library.
///
/// ## Remarks
/// This must be successfully called once before (almost) any other SDL_mixer function can be used.
/// It is safe to call this multiple times; the library will only initialize once, and won't deinitialize until `mixer.quit()` has been called a matching number of times.
/// Extra attempts to init report success.
///
/// ## Thread Safety
/// This function is not thread safe.
///
/// ## Version
/// This function is available since SDL_mixer 3.0.0.
pub fn init() !void {
    const ret = c.MIX_Init();
    return errors.wrapCallBool(ret);
}

/// Deinitialize the SDL mixer library.
///
/// ## Remarks
/// This must be called when done with the library, probably at the end of your program.
///
/// It is safe to call this multiple times; the library will only deinitialize once, when this function is called the same number of times as `init` was successfully called.
///
/// Once you have successfully deinitialized the library, it is safe to call `init` to reinitialize it for further use.
///
/// On successful deinitialization, SDL mixer will destroy almost all created objects, including objects of type:
/// * Mixer
/// * Track
/// * Audio
/// * Group
/// * AudioDecoder
/// ...which is to say: it's possible a single call to this function will clean up anything it allocated, stop all audio output, close audio devices, etc.
/// Don't attempt to destroy objects after this call.
/// The app is still encouraged to manage their resources carefully and clean up first, treating this function as a safety net against memory leaks.
///
/// ## Thread Safety
/// This function is not thread safe.
///
/// ## Version
/// This function is available since SDL_mixer 3.0.0.
pub fn quit() void {
    c.MIX_Quit();
}

/// Convert milliseconds to sample frames at a specific sample rate.
///
/// ## Function Parameters
/// * `sample_rate`: The sample rate to use for conversion.
/// * `ms`: The milliseconds to convert to rate-specific sample frames.
///
/// ## Return Value
/// Returns Converted number of sample frames.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL_mixer 3.0.0.
pub fn msToFrames(
    sample_rate: u32,
    ms: usize,
) !usize {
    return @intCast(try errors.wrapCall(c.Sint64, c.MIX_MSToFrames(@intCast(sample_rate), @intCast(ms)), -1));
}

test "mixer" {
    errors.refAllDeclsRecursive(@This());
}
