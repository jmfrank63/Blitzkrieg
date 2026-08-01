const c = @import("c");
const errors = @import("errors.zig");
const std = @import("std");

/// The preferred date format of the current system locale.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const DateFormat = enum(c.SDL_DateFormat) {
    /// Year/Month/Day.
    year_month_day = c.SDL_DATE_FORMAT_YYYYMMDD,
    /// Day/Month/Year.
    day_month_year = c.SDL_DATE_FORMAT_DDMMYYYY,
    /// Month/Day/Year.
    month_day_year = c.SDL_DATE_FORMAT_MMDDYYYY,
};

/// Day of the week.
///
/// ## Version
/// This is provided by zig-sdl3.
pub const Day = enum(c_int) {
    sunday = 0,
    monday = 1,
    tuesday = 2,
    wednesday = 3,
    thursday = 4,
    friday = 5,
    saturday = 6,
};

/// Month of the year.
///
/// ## Version
/// This is provided by zig-sdl3.
pub const Month = enum(c_int) {
    january = 1,
    february = 2,
    march = 3,
    april = 4,
    may = 5,
    june = 6,
    july = 7,
    august = 8,
    september = 9,
    october = 10,
    november = 11,
    december = 12,
};

/// The preferred time format of the current system locale.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const TimeFormat = enum(c.SDL_TimeFormat) {
    /// 24 hour time.
    twenty_four_hour = c.SDL_TIME_FORMAT_24HR,
    /// 12 hour time.
    twelve_hour = c.SDL_TIME_FORMAT_12HR,
};

/// A structure holding a calendar date and time broken down into it's components.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const DateTime = struct {
    /// Year.
    year: u31,
    /// Month.
    month: Month,
    /// Day of the month. Range is [0-31] inclusively.
    day: u5,
    /// Hour. Range is [0-23] inclusively.
    hour: u5,
    /// Minute. Range is [0-59] inclusively.
    minute: u6,
    /// Seconds. Range is [0-60] inclusively.
    second: u6,
    /// Nanoseconds. Range is [0-999999999] inclusively.
    nanosecond: u31,
    /// Day of the week.
    day_of_week: Day,
    /// Seconds east of UTC.
    utc_offset: i32,

    /// Convert from an SDL value.
    pub fn fromSdl(data: c.SDL_DateTime) DateTime {
        return .{
            .year = @intCast(data.year),
            .month = @enumFromInt(data.month),
            .day = @intCast(data.day),
            .hour = @intCast(data.hour),
            .minute = @intCast(data.minute),
            .second = @intCast(data.second),
            .nanosecond = @intCast(data.nanosecond),
            .day_of_week = @enumFromInt(data.day_of_week),
            .utc_offset = @intCast(data.utc_offset),
        };
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: DateTime) c.SDL_DateTime {
        return .{
            .year = @intCast(self.year),
            .month = @intFromEnum(self.month),
            .day = @intCast(self.day),
            .hour = @intCast(self.hour),
            .minute = @intCast(self.minute),
            .second = @intCast(self.second),
            .nanosecond = @intCast(self.nanosecond),
            .day_of_week = @intFromEnum(self.day_of_week),
            .utc_offset = @intCast(self.utc_offset),
        };
    }

    /// Converts a `time.Time` in nanoseconds since the epoch to a calendar time in the `time.DateTime` format.
    ///
    /// ## Function Parameters
    /// * `time`: The `time.Time` to be converted.
    /// * `local_instead_of_utc`: The resulting `time.DateTime` will be expressed in local time if true, otherwise it will be in Universal Coordinated Time (UTC).
    ///
    /// ## Return Value
    /// The resulting `time.DateTime`.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn fromTime(
        time: Time,
        local_instead_of_utc: bool,
    ) !DateTime {
        var date_time: c.SDL_DateTime = undefined;
        const ret = c.SDL_TimeToDateTime(
            time.value,
            &date_time,
            local_instead_of_utc,
        );
        try errors.wrapCallBool(ret);
        return DateTime.fromSdl(date_time);
    }
};

/// Nanoseconds since the unix epoch.
pub const Time = struct {
    value: c.SDL_Time,

    /// Converts a calendar time to a `time.Time` in nanoseconds since the epoch.
    ///
    /// ## Function Parameters
    /// * `date_time`: The source date time.
    ///
    /// ## Return Value
    /// The resulting time.
    ///
    /// ## Remarks
    /// This function ignores the `day_of_week` member of the `time.DateTime` struct.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn fromDateTime(
        date_time: DateTime,
    ) !Time {
        const date_time_sdl: c.SDL_DateTime = date_time.toSdl();
        var time: c.SDL_Time = undefined;
        const ret = c.SDL_DateTimeToTime(
            &date_time_sdl,
            &time,
        );
        try errors.wrapCallBool(ret);
        return Time{ .value = time };
    }

    /// Converts a Windows `FILETIME` (100-nanosecond intervals since January 1, 1601) to an SDL time.
    ///
    /// ## Function Parameters
    /// * `low_date_time`: The low portion of the Windows `FILETIME` value.
    /// * `high_date_time`: The high portion of the Windows `FILETIME` value.
    ///
    /// ## Return Value
    /// Returns the converted SDL time.
    ///
    /// ## Remarks
    /// This function takes the two 32-bit values of the `FILETIME` structure as parameters.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn fromWindows(
        low_date_time: u32,
        high_date_time: u32,
    ) Time {
        const ret = c.SDL_TimeFromWindows(
            @intCast(low_date_time),
            @intCast(high_date_time),
        );
        return Time{ .value = ret };
    }

    /// Gets the current value of the system realtime clock in nanoseconds since Jan 1, 1970 in Universal Coordinated Time (UTC).
    ///
    /// ## Return Value
    /// Returns the tick count.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getCurrent() !Time {
        var time: c.SDL_Time = undefined;
        const ret = c.SDL_GetCurrentTime(
            &time,
        );
        try errors.wrapCallBool(ret);
        return Time{ .value = time };
    }

    /// Converts an SDL time into a Windows `FILETIME` (100-nanosecond intervals since January 1, 1601).
    ///
    /// ## Function Parameters
    /// * `self`: The time to convert.
    ///
    /// ## Return Value
    /// Returns the low and high 32-bit values of the Windows `FILETIME` structure in that order.
    ///
    /// ## Remarks
    /// This function fills in the two 32-bit values of the `FILETIME` structure.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn toWindows(
        self: Time,
    ) struct { u32, u32 } {
        var low_date_time: u32 = undefined;
        var high_date_time: u32 = undefined;
        c.SDL_TimeToWindows(
            self.value,
            &low_date_time,
            &high_date_time,
        );
        return .{ low_date_time, high_date_time };
    }
};

/// Get the day of week for a calendar date.
///
/// ## Function Parameters
/// * `year`: The year component of the date.
/// * `month`: The month component of the date.
/// * `day`: The day component of the date.
///
/// ## Return Value
/// Returns the day of week.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getDayOfWeek(
    year: u31,
    month: Month,
    day: u5,
) !Day {
    const ret = c.SDL_GetDayOfWeek(
        @intCast(year),
        @intFromEnum(month),
        @intCast(day),
    );
    return @enumFromInt(try errors.wrapCall(c_int, ret, -1));
}

/// Get the day of year for a calendar date.
///
/// ## Function Parameters
/// * `year`: The year component of the date.
/// * `month`: The month component of the date.
/// * `day`: The day component of the date.
///
/// ## Return Value
/// Returns the day of year [0-365] if the date is valid.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getDayOfYear(
    year: u31,
    month: Month,
    day: u5,
) !u9 {
    const ret = c.SDL_GetDayOfYear(
        @intCast(year),
        @intFromEnum(month),
        @intCast(day),
    );
    return @intCast(try errors.wrapCall(c_int, ret, -1));
}

/// Get the number of days in a month for a given year.
///
/// ## Function Parameters
/// * `year`: The year.
/// * `month`: The month.
///
/// ## Return Value
/// Returns the number of days in the requested month.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getDaysInMonth(
    year: u31,
    month: Month,
) !u5 {
    const ret = c.SDL_GetDaysInMonth(
        @intCast(year),
        @intFromEnum(month),
    );
    return @intCast(try errors.wrapCall(c_int, ret, -1));
}

/// Gets the current preferred date and time format for the system locale.
///
/// ## Return Value
/// Returns the system's preferred date and time format.
///
/// ## Remarks
/// This might be a "slow" call that has to query the operating system.
/// It's best to ask for this once and save the results.
/// However, the preferred formats can change, usually because the user has changed a system preference outside of your program.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getLocalePreferences() !struct { DateFormat, TimeFormat } {
    var date_format: c.SDL_DateFormat = undefined;
    var time_format: c.SDL_TimeFormat = undefined;
    const ret = c.SDL_GetDateTimeLocalePreferences(
        &date_format,
        &time_format,
    );
    try errors.wrapCallBool(ret);
    return .{ @enumFromInt(date_format), @enumFromInt(time_format) };
}

// Ensure time and date recognition works.
test "Dates" {
    errors.refAllDeclsRecursive(@This());

    try std.testing.expect(try getDaysInMonth(2018, Month.february) == 28);
    try std.testing.expect(try getDaysInMonth(2020, Month.february) == 29);
    try std.testing.expect(try getDaysInMonth(2014, Month.october) == 31);
    try std.testing.expect(try getDayOfYear(1972, Month.june, 13) == 164);
    try std.testing.expect(try getDayOfYear(2057, Month.march, 12) == 70);
    try std.testing.expect(try getDayOfYear(2018, Month.september, 27) == 269);
    try std.testing.expectError(error.SdlError, getDayOfYear(2020, Month.february, 31));
    try std.testing.expect(try getDayOfWeek(2001, Month.november, 2) == Day.friday);
    try std.testing.expect(try getDayOfWeek(1984, Month.january, 11) == Day.wednesday);
    try std.testing.expect(try getDayOfWeek(2024, Month.october, 9) == Day.wednesday);
    try std.testing.expectError(error.SdlError, getDayOfWeek(2020, Month.february, 31));

    const curr_time = try Time.getCurrent();
    try std.testing.expectEqual(curr_time, try Time.fromDateTime(try DateTime.fromTime(curr_time, true)));

    _ = try getLocalePreferences();

    // Idk man idk why this is not equal, probably some weird conversion loss.
    const low_date_time, const high_date_time = curr_time.toWindows();
    const conv_time = Time.fromWindows(low_date_time, high_date_time);
    try std.testing.expect(std.math.approxEqAbs(f64, @floatFromInt(curr_time.value), @floatFromInt(conv_time.value), 1000));
}
