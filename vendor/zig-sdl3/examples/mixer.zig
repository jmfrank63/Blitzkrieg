const sdl3 = @import("sdl3");
const std = @import("std");

/// Frequency of the generated tone, in Hz (440 Hz is concert A).
const tone_hz = 440;
/// Amplitude of the generated tone, from 0.0 (silence) to 1.0 (full volume).
const tone_amplitude = 0.3;
/// How long to sleep between event-loop iterations, in milliseconds.
const poll_interval_ms = 50;

pub fn main(
    init: std.process.Init,
) !void {
    _ = init;
    defer sdl3.shutdown();

    const log_app = sdl3.log.Category.application;

    try sdl3.init(.{ .audio = true, .events = true });
    defer sdl3.quit(.{ .audio = true, .events = true });

    try sdl3.mixer.init();
    defer sdl3.mixer.quit();

    const compiled_version = sdl3.mixer.Version.compiled_against;
    const linked_version = sdl3.mixer.Version.get();
    try log_app.logInfo("Using SDL_mixer {d}.{d}.{d}", .{ compiled_version.getMajor(), compiled_version.getMinor(), compiled_version.getMicro() });
    try log_app.logInfo("Linked against SDL_mixer {d}.{d}.{d}", .{ linked_version.getMajor(), linked_version.getMinor(), linked_version.getMicro() });

    // Open a mixer on the default playback device, letting SDL mixer pick the format.
    const mixer = try sdl3.mixer.Mixer.initDevice(sdl3.audio.Device.default_playback, null);
    defer mixer.deinit();

    // Generate an infinite sine wave (`null` duration).
    const tone = try sdl3.mixer.Audio.initSineWaveAudio(mixer, tone_hz, tone_amplitude, null);
    defer tone.deinit();

    const track = try sdl3.mixer.Track.init(mixer);
    defer track.deinit();

    try track.setAudio(tone);

    try log_app.logInfo("Playing a continuous {d} Hz tone. Press Ctrl+C to quit.", .{tone_hz});
    try track.play(.{});

    var quit_app = false;
    while (!quit_app) {
        while (sdl3.events.poll()) |event| {
            switch (event) {
                .quit, .terminating => quit_app = true,
                else => {},
            }
        }
        sdl3.timer.delayMilliseconds(poll_interval_ms);
    }

    try track.stop(0);
    try log_app.logInfo("Example finished.", .{});
}
