# re_render

A high-performance command-line utility designed to stream the graphical output of any active Windows window directly into the terminal using ANSI escape sequences.

---

## Key Features

* **Real-time Streaming:** Capture and project active application windows into your console.
* **Performance Optimization:** Includes smart diff-rendering, configurable refresh rates, and interlacing.
* **Color Depth Control:** Dynamically adjust bitrates from retro 12-bit up to 24-bit True Color.
* **Adaptive Sizing:** Auto-adjusts target rendering buffer to fit your terminal window layout.

---

## Preview

### Demo Showcase
<img width="1581" height="1017" alt="re_render preview" src="https://github.com/user-attachments/assets/b759e03e-90fb-41f5-9192-0cffd5c0ddd3" />

---

## Usage & CLI Reference

### Help Screen
```text
    ****     *****       ****     *****    *   *    ****     *****    ****
   *   *    *           *   *    *        **  *    *   *    *        *   *
  ****     ****        ****     ****     * * *    *   *    ****     ****
 *  *     *           *  *     *        *  **    *   *    *        *  *
*   *    *****       *   *    *****    *   *    ****     *****    *   *

Usage: re_render.exe [target_window_title] [options]
```

### Options

| Parameter | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `-h`, `--help` | — | — | Show this help message and exit. |
| `--color-threshold` | `INT` | `0` | Color tolerance for diff rendering (0-255). <br>• `0`: Exactly match (Slower)<br>• `15`: Drop-out colors (Faster) |
| `--fps` | `INT` | Max | Set maximum frames per second limit. |
| `--width` | `INT` | Auto | Set target render buffer width. |
| `--height` | `INT` | Auto | Set target render buffer height. |
| `--color-depth` | `INT` | `8` | Color depth per channel from 1 to 8.<br>• `8`: 24-bit True Color (Slower)<br>• `6`: 18-bit color (Balanced)<br>• `4`: 12-bit color (Faster) |
| `--char` | `CHAR` | — | Set a custom render block character. |
| `--auto-size` | — | — | Automatically fit frame buffer to terminal window dimensions. |
| `--scroll-disable` | — | — | Disable terminal scrolling. |
| `--interlacing` | — | — | Enable interlacing (skips every second line). |
| `--verbose` | — | — | Enable verbose output / debug stats. |

---

## Quick Start Examples

**Basic stream with performance optimization:**
```bash
.\re_render.exe "Minecraft" --width=120 --height=40 --fps=60 --interlacing
```

**Retro terminal style simulation:**
```bash
.\re_render.exe "VLC Media Player" --color-depth=4 --char="█" --auto-size
```
