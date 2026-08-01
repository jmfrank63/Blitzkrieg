const c = @import("c");
const errors = @import("errors.zig");
const properties = @import("properties.zig");
const std = @import("std");

/// HID underlying bus types.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const BusType = enum(c.SDL_hid_bus_type) {
    /// An unknown bus type.
    unknown = c.SDL_HID_API_BUS_UNKNOWN,
    /// USB bus Specifications: https://usb.org/hid.
    usb = c.SDL_HID_API_BUS_USB,
    /// Bluetooth or Bluetooth LE bus Specifications:
    /// https://www.bluetooth.com/specifications/specs/human-interface-device-profile-1-1-1/
    /// https://www.bluetooth.com/specifications/specs/hid-service-1-0/
    /// https://www.bluetooth.com/specifications/specs/hid-over-gatt-profile-1-0/
    bluetooth = c.SDL_HID_API_BUS_BLUETOOTH,
    /// I2C bus Specifications: https://docs.microsoft.com/previous-versions/windows/hardware/design/dn642101(v=vs.85)
    i2c = c.SDL_HID_API_BUS_I2C,
    /// SPI bus Specifications: https://www.microsoft.com/download/details.aspx?id=103325
    spi = c.SDL_HID_API_BUS_SPI,
};

/// An opaque handle representing an open HID device.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Device = struct {
    value: *c.SDL_hid_device,

    /// Properties provided by the HID interface.
    ///
    /// ## Version
    /// This struct is provided by zig-sdl3.
    pub const Properties = struct {
        /// The `libusb_device_handle` associated with the device, if it was opened using libusb.
        libusb_device_handle: ?struct { value: ?*anyopaque },

        /// Convert from an SDL value.
        pub fn fromSdl(value: properties.Group) Properties {
            return .{
                .libusb_device_handle = if (value.get(c.SDL_PROP_HIDAPI_LIBUSB_DEVICE_HANDLE_POINTER)) |val| .{ .value = val.pointer } else null,
            };
        }
    };

    /// Close a HID device.
    ///
    /// ## Function Parameters
    /// * `self`: A device handle.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn deinit(
        self: Device,
    ) !void {
        const ret = c.SDL_hid_close(self.value);
        if (ret != 0) {
            errors.callErrorCallback();
            return error.SdlError;
        }
    }

    /// Get the device info from a HID device.
    ///
    /// ## Function Parameters
    /// * `self`: The device handle.
    ///
    /// ## Return Value
    /// Returns the device info.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getDeviceInfo(
        self: Device,
    ) !DeviceInfo {
        return @as(*DeviceInfo, @ptrCast(try errors.wrapCallNull(*c.SDL_hid_device_info, c.SDL_hid_get_device_info(self.value)))).*;
    }

    /// Get a feature report from a HID device.
    ///
    /// ## Function Parameters
    /// * `self`: A device handle.
    /// * `data`: The buffer to read data into including the Report ID. Set the first byte of data to the Report ID of the report to be read, or set it to `0` if your device does not use numbered reports.
    ///
    /// ## Return Value
    /// Returns the number of bytes read plus one for the report ID (which is still in the first byte).
    ///
    /// ## Remarks
    /// Set the first byte of data to the Report ID of the report to be read.
    /// Make sure to allow space for this extra byte in data.
    /// Upon return, the first byte will still contain the Report ID, and the report data will start in `data[1]`.
    ///
    /// Version
    /// This function is available since SDL 3.2.0.
    pub fn getFeatureReport(
        self: Device,
        data: []u8,
    ) ![]u8 {
        const ret: usize = @intCast(try errors.wrapCall(c_int, c.SDL_hid_get_feature_report(
            self.value,
            data.ptr,
            data.len,
        ), -1));
        return data.ptr[0..ret];
    }

    /// Get a string from a HID device, based on its string index.
    ///
    /// ## Function Parameters
    /// * `self`: A device handle.
    /// * `index`: String index.
    /// * `buf`: Wide string buffer to put the data into.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getIndexedString(
        self: Device,
        index: c_int,
        buf: [:0]c.wchar_t,
    ) !void {
        const ret = c.SDL_hid_get_indexed_string(self.value, index, buf.ptr, buf.len);
        if (ret != 0) {
            errors.callErrorCallback();
            return error.SdlError;
        }
    }

    /// Get an input report from a HID device.
    ///
    /// ## Function Parameters
    /// * `self`: A device handle.
    /// * `data`: The buffer to read data into including the Report ID. Set the first byte of data to the Report ID of the report to be read, or set it to `0` if your device does not use numbered reports.
    ///
    /// ## Return Value
    /// Returns the number of bytes read plus one for the report ID (which is still in the first byte).
    ///
    /// ## Remarks
    /// Set the first byte of data to the Report ID of the report to be read.
    /// Make sure to allow space for this extra byte in data.
    /// Upon return, the first byte will still contain the Report ID, and the report data will start in `data[1]`.
    ///
    /// Version
    /// This function is available since SDL 3.2.0.
    pub fn getInputReport(
        self: Device,
        data: []u8,
    ) ![]u8 {
        const ret: usize = @intCast(try errors.wrapCall(c_int, c.SDL_hid_get_input_report(
            self.value,
            data.ptr,
            data.len,
        ), -1));
        return data.ptr[0..ret];
    }

    /// Get The Manufacturer String from a HID device.
    ///
    /// ## Function Parameters
    /// * `self`: A device handle.
    /// * `buf`: Wide string buffer to put the data into.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getManufacturerString(
        self: Device,
        buf: [:0]c.wchar_t,
    ) !void {
        const ret = c.SDL_hid_get_manufacturer_string(self.value, buf.ptr, buf.len);
        if (ret != 0) {
            errors.callErrorCallback();
            return error.SdlError;
        }
    }

    /// Get The Product String from a HID device.
    ///
    /// ## Function Parameters
    /// * `self`: A device handle.
    /// * `buf`: Wide string buffer to put the data into.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getProductString(
        self: Device,
        buf: [:0]c.wchar_t,
    ) !void {
        const ret = c.SDL_hid_get_product_string(self.value, buf.ptr, buf.len);
        if (ret != 0) {
            errors.callErrorCallback();
            return error.SdlError;
        }
    }

    /// Get the properties associated with an HID device.
    ///
    /// ## Function Parameters
    /// * `self`: The device handle.
    ///
    /// ## Return Value
    /// Returns the HID device properties.
    ///
    /// ## Version
    /// This function is available since SDL 3.4.0.
    pub fn getProperties(
        self: Device,
    ) !Properties {
        const ret = c.SDL_hid_get_properties(self.value);
        return Properties.fromSdl(properties.Group{ .value = try errors.wrapCall(c.SDL_PropertiesID, ret, 0) });
    }

    /// Get a report descriptor from a HID device.
    ///
    /// ## Function Parameters
    /// * `self`: A device handle.
    /// * `data`: The buffer to copy descriptor into.
    ///
    /// ## Return Value
    /// Returns the buffer of bytes actually read, re-using the pointer from `data`.
    ///
    /// ## Remarks
    /// User has to provide a preallocated buffer where descriptor will be copied to.
    /// The recommended size for a preallocated buffer is `4096` bytes.
    ///
    /// Version
    /// This function is available since SDL 3.2.0.
    pub fn getReportDescriptor(
        self: Device,
        data: []u8,
    ) ![]u8 {
        const ret: usize = @intCast(try errors.wrapCall(c_int, c.SDL_hid_get_report_descriptor(
            self.value,
            data.ptr,
            data.len,
        ), -1));
        return data.ptr[0..ret];
    }

    /// Get The Serial Number String from a HID device.
    ///
    /// ## Function Parameters
    /// * `self`: A device handle.
    /// * `buf`: Wide string buffer to put the data into.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getSerialNumberString(
        self: Device,
        buf: [:0]c.wchar_t,
    ) !void {
        const ret = c.SDL_hid_get_serial_number_string(self.value, buf.ptr, buf.len);
        if (ret != 0) {
            errors.callErrorCallback();
            return error.SdlError;
        }
    }

    /// Open a HID device using a Vendor ID (VID), Product ID (PID) and optionally a serial number.
    ///
    /// ## Function Parameters
    /// * `vendor_id`: The Vendor ID (VID) of the device to open.
    /// * `product_id`: The Product ID (PID) of the device to open.
    /// * `serial_num`: The Serial Number of the device to open (Optionally `null`).
    ///
    /// ## Return Value
    /// Returns the device object.
    ///
    /// ## Remarks
    /// If `serial_number` is `null`, the first device with the specified VID and PID is opened.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn init(
        vendor_id: c_ushort,
        product_id: c_ushort,
        serial_num: ?[:0]c.wchar_t,
    ) !Device {
        return .{
            .value = try errors.wrapCallNull(*c.SDL_hid_device, c.SDL_hid_open(vendor_id, product_id, if (serial_num) |val| val.ptr else null)),
        };
    }

    /// Open a HID device by its path name.
    ///
    /// ## Function Parameters
    /// * `path`: The path name of the device to open.
    ///
    /// ## Return Value
    /// Returns the device object.
    ///
    /// ## Remarks
    /// The path name be determined by calling `hid_api.enumerate()`, or a platform-specific path name can be used (eg: `/dev/hidraw0` on Linux).
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn initPath(
        path: [:0]const u8,
    ) !Device {
        return .{
            .value = try errors.wrapCallNull(*c.SDL_hid_device, c.SDL_hid_open_path(path.ptr)),
        };
    }

    /// Read an Input report from a HID device.
    ///
    /// ## Function Parameters
    /// * `self`: A device handle.
    /// * `data`: The data buffer to read into. For devices with multiple reports, make sure to read an extra byte for the report number.
    ///
    /// ## Return Value
    /// Returns the buffer of bytes actually read, re-using the pointer from `data`.
    /// If no packet was available to be read and the handle is in non-blocking mode, this function returns `null`.
    ///
    /// ## Remarks
    /// Input reports are returned to the host through the INTERRUPT IN endpoint.
    /// The first byte will contain the Report number if the device uses numbered reports.
    ///
    /// Version
    /// This function is available since SDL 3.2.0.
    pub fn read(
        self: Device,
        data: []u8,
    ) !?[]u8 {
        const ret: usize = @intCast(try errors.wrapCall(c_int, c.SDL_hid_read(
            self.value,
            data.ptr,
            data.len,
        ), -1));
        if (ret == 0)
            return null;
        return data.ptr[0..ret];
    }

    /// Read an Input report from a HID device with timeout.
    ///
    /// ## Function Parameters
    /// * `self`: A device handle.
    /// * `data`: The data buffer to read into. For devices with multiple reports, make sure to read an extra byte for the report number.
    /// * `timeout_milliseconds`: Timeout in milliseconds or `null` for blocking wait.
    ///
    /// ## Return Value
    /// Returns the buffer of bytes actually read, re-using the pointer from `data`.
    /// If no packet was available to be read within the timeout period, this function returns `null`.
    ///
    /// ## Remarks
    /// Input reports are returned to the host through the INTERRUPT IN endpoint.
    /// The first byte will contain the Report number if the device uses numbered reports.
    ///
    /// Version
    /// This function is available since SDL 3.2.0.
    pub fn readTimeout(
        self: Device,
        data: []u8,
        timeout_milliseconds: ?usize,
    ) !?[]u8 {
        const ret: usize = @intCast(try errors.wrapCall(c_int, c.SDL_hid_read_timeout(
            self.value,
            data.ptr,
            data.len,
            if (timeout_milliseconds) |val| @intCast(val) else -1,
        ), -1));
        if (ret == 0)
            return null;
        return data.ptr[0..ret];
    }

    /// Send a Feature report to the device.
    ///
    /// ## Function Parameters
    /// * `self`: A device handle.
    /// * `data`: The data to send, including the report number as the first byte.
    ///
    /// ## Return Value
    /// Returns the actual number of bytes written.
    ///
    /// ## Remarks
    /// Feature reports are sent over the Control endpoint as a Set_Report transfer.
    /// The first byte of data must contain the Report ID.
    /// For devices which only support a single report, this must be set to `0x0`.
    /// The remaining bytes contain the report data.
    /// Since the Report ID is mandatory, calls to `hid_api.write()` will always contain one more byte than the report contains.
    /// For example, if a hid report is `16` bytes long, `17` bytes must be passed to `hid_api.write()`,
    /// the Report ID (or `0x0`, for devices with a single report), followed by the report data (`16` bytes).
    /// In this example, the length of passed data would be `17`.
    ///
    /// This will send the data on the first OUT endpoint, if one exists.
    /// If it does not, it will send the data through the Control Endpoint (Endpoint 0).
    ///
    /// Version
    /// This function is available since SDL 3.2.0.
    pub fn sendFeatureReport(
        self: Device,
        data: []const u8,
    ) !usize {
        return @intCast(try errors.wrapCall(c_int, c.SDL_hid_send_feature_report(
            self.value,
            data.ptr,
            data.len,
        ), -1));
    }

    /// Set the device handle to be non-blocking.
    ///
    /// ## Function Parameters
    /// * `self`: The device handle.
    /// * `non_block`: If to enable or disable non-blocking.
    ///
    /// ## Remarks
    /// In non-blocking mode calls to `hid_api.Device.read()` will return immediately with a value of `0` if there is no data to be read.
    /// In blocking mode, `hid_api.Device.read()` will wait (block) until there is data to read before returning.
    ///
    /// Nonblocking can be turned on and off at any time.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setNonblocking(
        self: Device,
        non_block: bool,
    ) !void {
        const ret = c.SDL_hid_set_nonblocking(self.value, if (non_block) 1 else 0);
        if (ret != 0) {
            errors.callErrorCallback();
            return error.SdlError;
        }
    }

    /// Write an Output report to a HID device.
    ///
    /// ## Function Parameters
    /// * `self`: A device handle.
    /// * `data`: The data to send, including the report number as the first byte.
    ///
    /// ## Return Value
    /// Returns the actual number of bytes written.
    ///
    /// ## Remarks
    /// The first byte of data must contain the Report ID.
    /// For devices which only support a single report, this must be set to `0x0`.
    /// The remaining bytes contain the report data.
    /// Since the Report ID is mandatory, calls to `hid_api.write()` will always contain one more byte than the report contains.
    /// For example, if a hid report is `16` bytes long, `17` bytes must be passed to `hid_api.write()`,
    /// the Report ID (or `0x0`, for devices with a single report), followed by the report data (`16` bytes).
    /// In this example, the length of passed data would be `17`.
    ///
    /// This will send the data on the first OUT endpoint, if one exists.
    /// If it does not, it will send the data through the Control Endpoint (Endpoint 0).
    ///
    /// Version
    /// This function is available since SDL 3.2.0.
    pub fn write(
        self: Device,
        data: []const u8,
    ) !usize {
        return @intCast(try errors.wrapCall(c_int, c.SDL_hid_write(
            self.value,
            data.ptr,
            data.len,
        ), -1));
    }
};

/// Information about a connected HID device
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const DeviceInfo = extern struct {
    /// Platform-specific device path.
    path: [*:0]u8,
    /// Device Vendor ID.
    vendor_id: c_ushort,
    /// Device Product ID.
    product_id: c_ushort,
    /// Serial Number.
    serial_number: [*:0]c_int,
    /// Device Release Number in binary-coded decimal, also known as Device Version Number.
    release_number: c_ushort,
    /// Manufacturer String.
    manufacturer_string: [*:0]c_int,
    /// Product string.
    product_string: [*:0]c_int,
    /// Usage Page for this Device/Interface (Windows/Mac/hidraw only).
    usage_page: c_ushort,
    /// Usage for this Device/Interface (Windows/Mac/hidraw only).
    usage: c_ushort,
    /// The USB interface which this logical device represents.
    /// Valid only if the device is a USB HID device.
    /// Set to `-1` in all other cases.
    interface_number: c_int,
    /// Additional information about the USB interface.
    /// Valid on libusb and Android implementations.
    interface_class: c_int,
    interface_subclass: c_int,
    interface_protocol: c_int,
    /// Underlying bus type.
    bus_type: BusType,
    /// Pointer to the next device.
    next: [*c]DeviceInfo,

    // Size checks.
    comptime {
        errors.assertStructsEqual(DeviceInfo, c.SDL_hid_device_info);
    }
};

/// Start or stop a BLE scan on iOS and tvOS to pair Steam Controllers.
///
/// ## Function Parameters
/// * `active`: True to start the scan, false to stop the scan.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn bleScan(
    active: bool,
) void {
    c.SDL_hid_ble_scan(
        active,
    );
}

/// Finalize the HIDAPI library.
///
/// ## Remarks
/// This function frees all of the static data associated with HIDAPI.
/// It should be called at the end of execution to avoid memory leaks.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn deinit() !void {
    const ret = c.SDL_hid_exit();
    if (ret != 0) {
        errors.callErrorCallback();
        return error.SdlError;
    }
}

/// Check to see if devices may have been added or removed.
///
/// ## Return Value
/// Returns a change counter that is incremented with each potential device change, or `0` if device change detection isn't available.
///
/// ## Remarks
/// Enumerating the HID devices is an expensive operation, so you can call this to see if there have been any system device changes since the last call to this function.
/// A change in the counter returned doesn't necessarily mean that anything has changed, but you can call `hid_api.enumerate()` to get an updated device list.
///
/// Calling this function for the first time may cause a thread or other system resource to be allocated to track device change notifications.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn deviceChangeCount() usize {
    return @intCast(c.SDL_hid_device_change_count());
}

/// Enumerate the HID Devices.
///
/// ## Function Parameters
/// * `vendor_id`: The Vendor ID (VID) of the types of device to open, or `null` or `0` to match any vendor.
/// * `product_id`: The Product ID (PID) of the types of device to open, or `null` or `0` to match any product.
///
/// ## Return Value
/// Returns a pointer to a linked list of type `hid_api.DeviceInfo`, containing information about the HID devices attached to the system.
/// Free this with `hid_api.freeEnumeration()`.
///
/// ## Remarks
/// This function returns a linked list of all the HID devices attached to the system which match vendor_id and product_id.
/// If `vendor_id` is set to `null` or `0` then any vendor matches.
/// If `product_id` is set to `null` or `0` then any product matches.
/// If `vendor_id` and `product_id` are both set to `null` or `0`, then all HID devices will be returned.
///
/// By default SDL will only enumerate controllers, to reduce risk of hanging or crashing on bad drivers,
/// but `hints.Type.enumerate_only_controllers` can be set to `0` to enumerate all HID devices.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn enumerate(
    vendor_id: ?c_ushort,
    product_id: ?c_ushort,
) !*DeviceInfo {
    return @ptrCast(try errors.wrapCallNull(*c.SDL_hid_device_info, c.SDL_hid_enumerate(
        if (vendor_id) |val| val else 0,
        if (product_id) |val| val else 0,
    )));
}

/// Free an enumeration linked list.
///
/// ## Function Parameters
/// * `devs`: Devices returned by `hid_api.enumerate()`.
///
/// ## Remarks
/// This function frees a linked list created by `hid_api.enumerate()`.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn freeEnumeration(
    devs: *DeviceInfo,
) void {
    c.SDL_hid_free_enumeration(@ptrCast(devs));
}

/// Initialize the HIDAPI library.
///
/// ## Remarks
/// This function initializes the HIDAPI library.
/// Calling it is not strictly necessary, as it will be called automatically by `hid_api.enumerate()` and any of the `hid_api.Device.init*()` functions if it is needed.
/// This function should be called at the beginning of execution however, if there is a chance of HIDAPI handles being opened by different threads simultaneously.
///
/// Each call to this function should have a matching call to `hid_api.deinit()`.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn init() !void {
    const ret = c.SDL_hid_init();
    if (ret != 0) {
        errors.callErrorCallback();
        return error.SdlError;
    }
}

// Testing for the HID API.
test "HID API" {
    errors.refAllDeclsRecursive(@This());
}
