# ESP-Emote-Expression

ESP-Emote-Expression is an ESP-IDF component for managing emote animations and UI display, designed specifically for the ESP-Brookesia project. This component provides rich emote animation management, UI element control, and event handling capabilities.

## Features

- **Emote Animation Management**: Supports emoji animation playback, including eye expressions, listening animations, etc.
- **Boot Animation**: Supports custom boot animations with configurable auto-stop and deletion
- **UI Element Management**: Supports various UI elements such as labels, images, timers, QR codes, etc.
- **Event Handling**: Supports multiple system events (speaking, listening, system messages, battery status, etc.)
- **Dialog Animation**: Supports urgent dialog animations with configurable auto-stop time
- **Resource Management**: Supports loading resources from file paths or partitions, using memory mapping for performance optimization
- **Layout Configuration**: Supports defining UI layouts through JSON configuration files

## Dependencies

- ESP-IDF >= 5.0
- `espressif/cmake_utilities` >= 0.*
- `espressif/esp_mmap_assets` >= 1.*
- `espressif2022/esp_emote_gfx` (Graphics rendering component)
- `espressif2022/esp_emote_assets` (Resource management component)

## Quick Start

### 1. Add Component Dependency

Add the following to your `idf_component.yml` file:

```yaml
dependencies:
  espressif2022/esp_emote_expression:
    version: "*"
```

### 2. Initialize Component

```c
#include "expression_emote.h"

// Configuration structure
emote_config_t config = {
    .flags = {
        .swap = true,
        .double_buffer = true,
        .buff_dma = true
    },
    .gfx_emote = {
        .h_res = 360,
        .v_res = 360,
        .fps = 30
    },
    .buffers = {
        .buf_pixels = 360 * 360
    },
    .task = {
        .task_priority = 5,
        .task_stack = 4096,
        .task_affinity = 1,
        .task_stack_in_ext = false
    },
    .flush_cb = your_flush_callback  // Optional
};

// Initialize
emote_handle_t handle = emote_init(&config);
if (handle == NULL) {
    // Initialization failed
}
```

### 3. Load Resources

```c
// Load resources from partition
emote_data_t data = {
    .type = EMOTE_SOURCE_PARTITION,
    .source.partition_label = "assets"
};

if (!emote_load_assets_from_source(handle, &data)) {
    // Load failed
}

// Load boot animation
if (!emote_load_boot_anim_from_source(handle, &data)) {
    // Load failed
}
```

### 4. Use API

```c
// Set emoji animation
emote_set_anim_emoji(handle, "happy");

// Set event message
emote_set_event_msg(handle, EMOTE_MGR_EVT_SPEAK, "Hello");

// Set QR code
emote_set_qrcode_data(handle, "https://example.com");

// Set dialog animation (auto-stop)
emote_insert_anim_dialog(handle, "warning", 3000);  // Auto-stop after 3 seconds

// Update battery status
emote_set_bat_status(handle);
```

## API Reference

### Initialization and Cleanup

- `emote_init()` - Initialize emote manager
- `emote_deinit()` - Cleanup and release resources
- `emote_is_initialized()` - Check if initialized

### Resource Loading

- `emote_load_assets_from_source()` - Load resources from source
- `emote_load_boot_anim_from_source()` - Load boot animation

### Animation Control

- `emote_set_anim_emoji()` - Set emoji animation
- `emote_set_dialog_anim()` - Set dialog animation
- `emote_insert_anim_dialog()` - Insert dialog animation (with auto-stop)
- `emote_stop_anim_dialog()` - Stop dialog animation
- `emote_wait_boot_anim_stop()` - Wait for boot animation to complete

### Events and Messages

- `emote_set_event_msg()` - Set event message
- `emote_set_qrcode_data()` - Set QR code data

### System Status

- `emote_set_bat_status()` - Update battery status
- `emote_set_label_clock()` - Update clock display
- `emote_notify_flush_finished()` - Notify flush completion

## Event Types

The component supports the following event types:

- `EMOTE_MGR_EVT_IDLE` - Idle state
- `EMOTE_MGR_EVT_SPEAK` - Speaking event
- `EMOTE_MGR_EVT_LISTEN` - Listening event
- `EMOTE_MGR_EVT_SYS` - System event
- `EMOTE_MGR_EVT_SET` - Settings event
- `EMOTE_MGR_EVT_BAT` - Battery event
- `EMOTE_MGR_EVT_QRCODE` - QR code event

## Configuration

### Graphics Configuration

- `h_res` / `v_res` - Horizontal and vertical resolution
- `fps` - Frame rate
- `buf_pixels` - Buffer pixel count

### Task Configuration

- `task_priority` - Task priority
- `task_stack` - Task stack size
- `task_affinity` - CPU affinity
- `task_stack_in_ext` - Whether to use external memory

### Display Flags

- `swap` - Whether to swap byte order
- `double_buffer` - Whether to use double buffering
- `buff_dma` - Whether to use DMA buffer

## Examples

For complete examples, please refer to the test applications in the `test_apps` directory.

## License

Apache-2.0

## Contributing

Issues and Pull Requests are welcome.

