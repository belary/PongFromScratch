---
name: vulkan_ndc
description: Vulkan NDC 坐标系参考
type: reference
originSessionId: 3d469498-2dae-476d-be87-d3abb6f18234
---
# Vulkan NDC (Normalized Device Coordinates) 坐标系

## 坐标范围

| 轴 | 范围 | 方向 |
|---|------|------|
| **X** | [-1, 1] | 左 → 右 |
| **Y** | [-1, 1] | **下 → 上** |
| **Z** | [0, 1] | 近 → 远 |

## 与 OpenGL 的关键差异

| 特性 | OpenGL | Vulkan |
|------|--------|--------|
| Y 轴 | 上为正 | **下为正** |
| Z 范围 | [-1, 1] | **[0, 1]** |
| 前面面 | 逆时针 (CCW) | 顺时针 (CW) |

## 投影视口变换

Vulkan 投影矩阵需要 Z 映射到 [0, 1]：

```cpp
// glm::ortho 示例（右手坐标系，Y向下）
glm::mat4 proj = glm::ortho(
    -1.0f, 1.0f,   // left, right
    -1.0f, 1.0f,   // bottom, top
    0.0f, 1.0f     // near, far (Z: [0,1])
);

// 自定义投影矩阵（Z: [0,1]）
// [2/(r-l), 0,      0,      (r+l)/(r-l)]
// [0,      2/(t-b), 0,      (t+b)/(t-b)]
// [0,      0,      1/(f-n), -n/(f-n)  ]
// [0,      0,      0,      1         ]
```

## Y 轴翻转

可通过 `VkViewport` 翻转 Y 轴使其像传统坐标系：

```cpp
VkViewport viewport{};
viewport.x = 0.0f;
viewport.y = height;        // 从顶部开始
viewport.width = width;
viewport.height = -height;  // 负高度翻转 Y
viewport.minDepth = 0.0f;
viewport.maxDepth = 1.0f;
```
