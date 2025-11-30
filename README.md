# ESP-Emote-Expression

ESP-Emote-Expression 是一个用于管理表情动画和UI显示的ESP-IDF组件，专为ESP-Brookesia项目设计。该组件提供了丰富的表情动画管理、UI元素控制和事件处理功能。

## 功能特性

- **表情动画管理**：支持emoji动画播放，包括眼睛表情、监听动画等
- **启动动画**：支持自定义启动动画，可配置自动停止和删除
- **UI元素管理**：支持标签、图像、定时器、QR码等多种UI元素
- **事件处理**：支持多种系统事件（说话、监听、系统消息、电池状态等）
- **对话框动画**：支持紧急对话框动画，可设置自动停止时间
- **资源管理**：支持从文件路径或分区加载资源，使用内存映射优化性能
- **布局配置**：支持通过JSON配置文件定义UI布局

## 依赖要求

- ESP-IDF >= 5.0
- `espressif/cmake_utilities` >= 0.*
- `espressif/esp_mmap_assets` >= 1.*
- `espressif2022/esp_emote_gfx` (图形渲染组件)
- `espressif2022/esp_emote_assets` (资源管理组件)

## 快速开始

### 1. 添加组件依赖

在你的 `idf_component.yml` 文件中添加：

```yaml
dependencies:
  espressif2022/esp_emote_expression:
    version: "*"
```

### 2. 初始化组件

```c
#include "expression_emote.h"

// 配置结构
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
    .flush_cb = your_flush_callback  // 可选
};

// 初始化
emote_handle_t handle = emote_init(&config);
if (handle == NULL) {
    // 初始化失败
}
```

### 3. 加载资源

```c
// 从分区加载资源
emote_data_t data = {
    .type = EMOTE_SOURCE_PARTITION,
    .source.partition_label = "assets"
};

if (!emote_load_assets_from_source(handle, &data)) {
    // 加载失败
}

// 加载启动动画
if (!emote_load_boot_anim_from_source(handle, &data)) {
    // 加载失败
}
```

### 4. 使用API

```c
// 设置emoji动画
emote_set_anim_emoji(handle, "happy");

// 设置事件消息
emote_set_event_msg(handle, EMOTE_MGR_EVT_SPEAK, "Hello");

// 设置QR码
emote_set_qrcode_data(handle, "https://example.com");

// 设置对话框动画（自动停止）
emote_insert_anim_dialog(handle, "warning", 3000);  // 3秒后自动停止

// 更新电池状态
emote_set_bat_status(handle);
```

## API 参考

### 初始化与清理

- `emote_init()` - 初始化表情管理器
- `emote_deinit()` - 清理并释放资源
- `emote_is_initialized()` - 检查是否已初始化

### 资源加载

- `emote_load_assets_from_source()` - 从源加载资源
- `emote_load_boot_anim_from_source()` - 加载启动动画

### 动画控制

- `emote_set_anim_emoji()` - 设置emoji动画
- `emote_set_dialog_anim()` - 设置对话框动画
- `emote_insert_anim_dialog()` - 插入对话框动画（带自动停止）
- `emote_stop_anim_dialog()` - 停止对话框动画
- `emote_wait_boot_anim_stop()` - 等待启动动画完成

### 事件与消息

- `emote_set_event_msg()` - 设置事件消息
- `emote_set_qrcode_data()` - 设置QR码数据

### 系统状态

- `emote_set_bat_status()` - 更新电池状态
- `emote_set_label_clock()` - 更新时钟显示
- `emote_notify_flush_finished()` - 通知刷新完成

## 事件类型

组件支持以下事件类型：

- `EMOTE_MGR_EVT_IDLE` - 空闲状态
- `EMOTE_MGR_EVT_SPEAK` - 说话事件
- `EMOTE_MGR_EVT_LISTEN` - 监听事件
- `EMOTE_MGR_EVT_SYS` - 系统事件
- `EMOTE_MGR_EVT_SET` - 设置事件
- `EMOTE_MGR_EVT_BAT` - 电池事件
- `EMOTE_MGR_EVT_QRCODE` - QR码事件

## 配置说明

### 图形配置

- `h_res` / `v_res` - 水平和垂直分辨率
- `fps` - 帧率
- `buf_pixels` - 缓冲区像素数

### 任务配置

- `task_priority` - 任务优先级
- `task_stack` - 任务栈大小
- `task_affinity` - CPU亲和性
- `task_stack_in_ext` - 是否使用外部内存

### 显示标志

- `swap` - 是否交换字节序
- `double_buffer` - 是否使用双缓冲
- `buff_dma` - 是否使用DMA缓冲区

## 示例

完整示例请参考 `test_apps` 目录下的测试应用。

## 许可证

Apache-2.0

## 贡献

欢迎提交Issue和Pull Request。

