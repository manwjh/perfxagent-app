# Resources Directory

This directory contains all the UI resources for the PerfXAgent application.

## Directory Structure

```
resources/
├── icons/          # Application icons and UI icons
│   ├── app_icon.ico    # Main application icon
│   ├── app_icon.png    # PNG version of app icon
│   └── ui_icons/       # Other UI icons
├── images/         # Images used in the UI
│   ├── backgrounds/    # Background images
│   └── logos/         # Logo variations
├── fonts/          # Custom fonts (if any)
└── README.md       # This file
```

## Icon Guidelines

- **App Icon**: Should be available in multiple formats (ICO, PNG, ICNS for macOS)
- **UI Icons**: Use consistent style and size (recommended: 16x16, 24x24, 32x32, 48x48)
- **Format**: Prefer vector formats (SVG) when possible, with PNG fallbacks

## Usage in Code

When referencing these resources in your code, use relative paths from the project root:

```cpp
// Example usage in C++ code
std::string iconPath = "resources/icons/app_icon.ico";
```

## File Naming Convention

- Use lowercase with underscores: `app_icon.ico`
- Include size in filename for multiple sizes: `play_button_32x32.png`
- Use descriptive names: `record_active.png`, `stop_disabled.png` 