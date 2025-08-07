# Clang-Tidy Report Generator Usage

This enhanced clang-tidy script generates readable reports in multiple formats.

## Usage

```bash
./run-clang-tidy.sh [build_directory] [output_format]
```

### Parameters

- `build_directory` (optional): Path to the build directory containing `compile_commands.json`
  - Default: `out/build/gcc-14`
- `output_format` (optional): Output format for the report
  - `html` (default): Generates a beautiful HTML report
  - `yaml`: Generates YAML file with fix suggestions
  - `both`: Generates both HTML and YAML reports

### Examples

```bash
# Generate HTML report (default)
./run-clang-tidy.sh

# Generate HTML report with custom build directory
./run-clang-tidy.sh out/build/clang-15 html

# Generate YAML fixes file
./run-clang-tidy.sh out/build/gcc-14 yaml

# Generate both HTML and YAML reports
./run-clang-tidy.sh out/build/gcc-14 both
```

## Output Files

Reports are saved in the `clang-tidy-reports/` directory with timestamp:

- **HTML Report**: `zdb-clang-tidy-YYYYMMDD_HHMMSS.html`
  - Beautiful, interactive web-based report
  - Color-coded issues by severity
  - Summary statistics
  - Detailed file-by-file breakdown
  - Automatically opens in browser if available

- **YAML Report**: `zdb-clang-tidy-YYYYMMDD_HHMMSS.yaml`
  - Machine-readable format
  - Contains fix suggestions that can be applied
  - Useful for automated processing

- **Text Report**: `zdb-clang-tidy-YYYYMMDD_HHMMSS.txt`
  - Raw clang-tidy output
  - Used for generating other formats

## HTML Report Features

- 📊 **Summary Dashboard**: Quick overview with issue counts
- 🎨 **Color Coding**: 
  - Red: Errors
  - Yellow: Warnings  
  - Blue: Notes
- 📁 **File Navigation**: Issues organized by file
- 🔍 **Detailed Information**: 
  - File path and line numbers
  - Check rule names
  - Full diagnostic messages
- 📱 **Responsive Design**: Works on desktop and mobile
- ⚡ **Performance Stats**: Analysis duration and file count

## Requirements

- clang-tidy installed
- CMake project with `CMAKE_EXPORT_COMPILE_COMMANDS=ON`
- Bash shell

## Browser Compatibility

The HTML reports work in all modern browsers:
- Chrome/Chromium
- Firefox
- Safari
- Edge
