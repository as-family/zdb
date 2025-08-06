#!/bin/bash

# Script to run clang-tidy on the ZDB project
# Usage: ./run-clang-tidy.sh [build_directory] [output_format]
#   output_format: html (default), yaml, both

set -e

# Show help if requested
if [[ "$1" == "--help" || "$1" == "-h" ]]; then
    cat << 'EOF'
ZDB Clang-Tidy Analysis Script

USAGE:
    ./run-clang-tidy.sh [build_directory] [output_format]

PARAMETERS:
    build_directory    Path to build directory with compile_commands.json
                      (default: out/build/gcc-14)
    
    output_format     Report format: html, yaml, both
                      (default: html)

OUTPUT FORMATS:
    html              Beautiful HTML report with interactive features
    yaml              YAML file with fix suggestions  
    both              Generate both HTML and YAML reports

EXAMPLES:
    ./run-clang-tidy.sh                           # HTML report, default build dir
    ./run-clang-tidy.sh out/build/clang-15        # HTML report, custom build dir
    ./run-clang-tidy.sh out/build/gcc-14 yaml     # YAML fixes file
    ./run-clang-tidy.sh out/build/gcc-14 both     # Both HTML and YAML

REQUIREMENTS:
    - clang-tidy installed
    - CMake project with CMAKE_EXPORT_COMPILE_COMMANDS=ON
    - Valid compile_commands.json in build directory

REPORTS:
    Reports are saved in clang-tidy-reports/ directory with timestamp.
    HTML reports automatically open in browser if available.

For more information, see clang-tidy-usage.md
EOF
    exit 0
fi

# Default build directory and output format
BUILD_DIR="${1:-out/build/gcc-14}"
OUTPUT_FORMAT="${2:-html}"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
OUTPUT_DIR="clang-tidy-reports"
REPORT_PREFIX="zdb-clang-tidy-${TIMESTAMP}"

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: Build directory '$BUILD_DIR' does not exist."
    echo "Please run cmake configure first or specify a valid build directory."
    exit 1
fi

# Check if compile_commands.json exists
if [ ! -f "$BUILD_DIR/compile_commands.json" ]; then
    echo "Error: compile_commands.json not found in '$BUILD_DIR'."
    echo "Make sure CMAKE_EXPORT_COMPILE_COMMANDS is enabled and the project is configured."
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

echo "Running clang-tidy with build directory: $BUILD_DIR"
echo "Output format: $OUTPUT_FORMAT"
echo "Reports will be saved to: $OUTPUT_DIR/"

# Find all C++ source files
CPP_FILES=$(find src/ tst/ -name "*.cpp" -type f)

if [ -z "$CPP_FILES" ]; then
    echo "No C++ source files found in src/ and tst/ directories."
    exit 1
fi

echo "Found $(echo "$CPP_FILES" | wc -l) C++ files to analyze."

# Define output files
YAML_OUTPUT="$OUTPUT_DIR/${REPORT_PREFIX}.yaml"
HTML_OUTPUT="$OUTPUT_DIR/${REPORT_PREFIX}.html"
TEXT_OUTPUT="$OUTPUT_DIR/${REPORT_PREFIX}.txt"

# Function to generate HTML report from clang-tidy output
generate_html_report() {
    local text_file="$1"
    local html_file="$2"
    local start_time="$3"
    local end_time="$4"
    
    cat > "$html_file" << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>ZDB Clang-Tidy Analysis Report</title>
    <style>
        body { 
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; 
            margin: 0; 
            padding: 20px; 
            background-color: #f5f5f5; 
        }
        .container { 
            max-width: 1200px; 
            margin: 0 auto; 
            background: white; 
            padding: 30px; 
            border-radius: 8px; 
            box-shadow: 0 2px 10px rgba(0,0,0,0.1); 
        }
        .header { 
            border-bottom: 3px solid #007acc; 
            padding-bottom: 20px; 
            margin-bottom: 30px; 
        }
        .header h1 { 
            color: #007acc; 
            margin: 0; 
            font-size: 2.5em; 
        }
        .meta-info { 
            display: grid; 
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); 
            gap: 15px; 
            margin-bottom: 30px; 
            background: #f8f9fa; 
            padding: 20px; 
            border-radius: 5px; 
        }
        .meta-item { 
            display: flex; 
            flex-direction: column; 
        }
        .meta-label { 
            font-weight: bold; 
            color: #495057; 
            font-size: 0.9em; 
            margin-bottom: 5px; 
        }
        .meta-value { 
            color: #007acc; 
            font-family: monospace; 
        }
        .summary { 
            display: grid; 
            grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); 
            gap: 15px; 
            margin-bottom: 30px; 
        }
        .summary-card { 
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); 
            color: white; 
            padding: 20px; 
            border-radius: 8px; 
            text-align: center; 
            cursor: pointer; 
            transition: all 0.3s ease; 
            user-select: none; 
        }
        .summary-card:hover { 
            transform: translateY(-2px); 
            box-shadow: 0 4px 15px rgba(0,0,0,0.2); 
        }
        .summary-card.active { 
            box-shadow: 0 0 0 3px rgba(255,255,255,0.5); 
            transform: scale(1.05); 
        }
        .summary-number { 
            font-size: 2em; 
            font-weight: bold; 
            display: block; 
        }
        .summary-label { 
            font-size: 0.9em; 
            opacity: 0.9; 
        }
        .issue { 
            background: #fff3cd; 
            border-left: 4px solid #ffc107; 
            padding: 15px; 
            margin-bottom: 15px; 
            border-radius: 0 4px 4px 0; 
        }
        .issue.error { 
            background: #f8d7da; 
            border-left-color: #dc3545; 
        }
        .issue.warning { 
            background: #fff3cd; 
            border-left-color: #ffc107; 
        }
        .issue.note { 
            background: #d1ecf1; 
            border-left-color: #17a2b8; 
        }
        .issue-header { 
            display: flex; 
            justify-content: space-between; 
            align-items: center; 
            margin-bottom: 10px; 
        }
        .issue-type { 
            background: #6c757d; 
            color: white; 
            padding: 3px 8px; 
            border-radius: 12px; 
            font-size: 0.8em; 
            font-weight: bold; 
        }
        .issue-type.error { background: #dc3545; }
        .issue-type.warning { background: #ffc107; color: #212529; }
        .issue-type.note { background: #17a2b8; }
        .file-path { 
            font-family: monospace; 
            font-size: 0.9em; 
            color: #495057; 
            background: #f8f9fa; 
            padding: 2px 6px; 
            border-radius: 3px; 
        }
        .check-name { 
            font-family: monospace; 
            font-size: 0.9em; 
            color: #007acc; 
            font-weight: bold; 
        }
        .message { 
            margin-top: 10px; 
            line-height: 1.5; 
        }
        .no-issues { 
            text-align: center; 
            padding: 40px; 
            color: #28a745; 
            font-size: 1.2em; 
        }
        .footer { 
            margin-top: 30px; 
            padding-top: 20px; 
            border-top: 1px solid #dee2e6; 
            text-align: center; 
            color: #6c757d; 
            font-size: 0.9em; 
        }
        pre { 
            background: #f8f9fa; 
            padding: 10px; 
            border-radius: 4px; 
            overflow-x: auto; 
            font-size: 0.9em; 
        }
        .hidden { 
            display: none !important; 
        }
    </style>
    <script>
        document.addEventListener('DOMContentLoaded', function() {
            const summaryCards = document.querySelectorAll('.summary-card');
            const issues = document.querySelectorAll('.issue');
            let activeFilter = 'all';

            // Add click handlers to summary cards
            summaryCards.forEach(card => {
                card.addEventListener('click', function() {
                    const filterType = this.getAttribute('data-filter');
                    
                    // Remove active class from all cards
                    summaryCards.forEach(c => c.classList.remove('active'));

                    // If clicking the same filter, toggle off (show all)
                    if (activeFilter === filterType) {
                        activeFilter = 'all';
                        filterIssues('all');
                        updateResultsTitle('all');
                    } else {
                        activeFilter = filterType;
                        this.classList.add('active');
                        filterIssues(filterType);
                        updateResultsTitle(filterType);
                    }
                });
            });

            function filterIssues(filterType) {
                issues.forEach(issue => {
                    if (filterType === 'all') {
                        issue.classList.remove('hidden');
                    } else {
                        if (issue.classList.contains(filterType)) {
                            issue.classList.remove('hidden');
                        } else {
                            issue.classList.add('hidden');
                        }
                    }
                });
            }

            function updateResultsTitle(filterType) {
                const resultsTitle = document.querySelector('h2');
                if (filterType === 'all') {
                    resultsTitle.textContent = '📋 Detailed Analysis Results';
                } else {
                    const filterName = filterType.charAt(0).toUpperCase() + filterType.slice(1) + 's';
                    resultsTitle.textContent = `📋 Filtered Results - ${filterName} Only`;
                }
            }

            // Add keyboard shortcuts (Ctrl/Cmd + 1,2,3,4)
            document.addEventListener('keydown', function(e) {
                if (e.ctrlKey || e.metaKey) {
                    let cardIndex = -1;
                    switch(e.key) {
                        case '1': cardIndex = 0; break;
                        case '2': cardIndex = 1; break;
                        case '3': cardIndex = 2; break;
                        case '4': cardIndex = 3; break;
                    }
                    if (cardIndex >= 0 && cardIndex < summaryCards.length) {
                        summaryCards[cardIndex].click();
                        e.preventDefault();
                    }
                }
            });

            // Add visual feedback for keyboard shortcuts
            const helpText = document.createElement('div');
            helpText.style.cssText = 'position: fixed; bottom: 10px; right: 10px; background: rgba(0,0,0,0.8); color: white; padding: 10px; border-radius: 4px; font-size: 12px; z-index: 1000;';
            helpText.innerHTML = 'Keyboard shortcuts:<br>Ctrl+1: All | Ctrl+2: Errors | Ctrl+3: Warnings | Ctrl+4: Notes';
            helpText.style.display = 'none';
            document.body.appendChild(helpText);

            // Show help on Ctrl key press
            document.addEventListener('keydown', function(e) {
                if (e.ctrlKey || e.metaKey) {
                    helpText.style.display = 'block';
                }
            });

            document.addEventListener('keyup', function(e) {
                if (!e.ctrlKey && !e.metaKey) {
                    helpText.style.display = 'none';
                }
            });
        });
    </script>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🔍 ZDB Clang-Tidy Analysis Report</h1>
        </div>

        <div class="meta-info">
            <div class="meta-item">
                <div class="meta-label">Generated</div>
                <div class="meta-value">
EOF
    
    echo "$(date)" >> "$html_file"
    
    cat >> "$html_file" << EOF
</div>
            </div>
            <div class="meta-item">
                <div class="meta-label">Analysis Duration</div>
                <div class="meta-value">
EOF
           echo "$((end_time - start_time)) seconds" >> "$html_file"
           cat >> "$html_file" << EOF
</div>
            </div>
            <div class="meta-item">
                <div class="meta-label">Build Directory</div>
                <div class="meta-value">
EOF
           echo "$BUILD_DIR" >> "$html_file"
           cat >> "$html_file" << EOF
           </div>
            </div>
            <div class="meta-item">
                <div class="meta-label">Files Analyzed</div>
                <div class="meta-value">
EOF
           echo "$(echo "$CPP_FILES" | wc -l)" >> "$html_file"
           cat >> "$html_file" << EOF
           files</div>
            </div>
        </div>
EOF

    # Count issues by type (filter out external library issues)
    local total_issues=$(grep "^/" "$text_file" 2>/dev/null | grep -v -E "(vcpkg|spdlog|fmt|grpc|gtest|protobuf)" | wc -l || echo "0")
    local errors=$(grep ": error:" "$text_file" 2>/dev/null | grep -v -E "(vcpkg|spdlog|fmt|grpc|gtest|protobuf)" | wc -l || echo "0")
    local warnings=$(grep ": warning:" "$text_file" 2>/dev/null | grep -v -E "(vcpkg|spdlog|fmt|grpc|gtest|protobuf)" | wc -l || echo "0")
    local notes=$(grep ": note:" "$text_file" 2>/dev/null | grep -v -E "(vcpkg|spdlog|fmt|grpc|gtest|protobuf)" | wc -l || echo "0")

    cat >> "$html_file" << EOF
        <div class="summary">
            <div class="summary-card" data-filter="all" title="Click to show all issues">
                <span class="summary-number">$total_issues</span>
                <span class="summary-label">Total Issues</span>
            </div>
            <div class="summary-card" data-filter="error" title="Click to filter errors only" style="background: linear-gradient(135deg, #ff6b6b 0%, #ee5a24 100%);">
                <span class="summary-number">$errors</span>
                <span class="summary-label">Errors</span>
            </div>
            <div class="summary-card" data-filter="warning" title="Click to filter warnings only" style="background: linear-gradient(135deg, #feca57 0%, #ff9ff3 100%);">
                <span class="summary-number">$warnings</span>
                <span class="summary-label">Warnings</span>
            </div>
            <div class="summary-card" data-filter="note" title="Click to filter notes only" style="background: linear-gradient(135deg, #48dbfb 0%, #0abde3 100%);">
                <span class="summary-number">$notes</span>
                <span class="summary-label">Notes</span>
            </div>
        </div>

        <h2>📋 Detailed Analysis Results</h2>
EOF

    if [ "$total_issues" -eq 0 ]; then
        echo '<div class="no-issues">🎉 No issues found! Your code looks great!</div>' >> "$html_file"
    else
        # Process the clang-tidy output and convert to HTML
        local current_file=""
        local current_line=""
        local current_type=""
        local current_check=""
        local current_message=""
        
        while IFS= read -r line; do
            # Match errors and warnings with check names
            if [[ $line =~ ^([^:]+):([0-9]+):([0-9]+):[[:space:]]+(error|warning):[[:space:]]+(.+)[[:space:]]+\[([^\]]+)\] ]]; then
                # New issue found with check name
                local file_path="${BASH_REMATCH[1]}"
                local line_num="${BASH_REMATCH[2]}"
                local col_num="${BASH_REMATCH[3]}"
                local issue_type="${BASH_REMATCH[4]}"
                local message="${BASH_REMATCH[5]}"
                local check_name="${BASH_REMATCH[6]}"
            # Match notes (which don't have check names in brackets)
            elif [[ $line =~ ^([^:]+):([0-9]+):([0-9]+):[[:space:]]+note:[[:space:]]+(.+)$ ]]; then
                # New note found without check name
                local file_path="${BASH_REMATCH[1]}"
                local line_num="${BASH_REMATCH[2]}"
                local col_num="${BASH_REMATCH[3]}"
                local issue_type="note"
                local message="${BASH_REMATCH[4]}"
                local check_name=""
            else
                continue
            fi
                
                # Skip issues from external libraries
                if [[ "$file_path" =~ (vcpkg|spdlog|fmt|grpc|gtest|protobuf) ]]; then
                    continue
                fi
                
                cat >> "$html_file" << EOF
        <div class="issue $issue_type">
            <div class="issue-header">
                <div>
                    <span class="file-path">$file_path:$line_num:$col_num</span>
                </div>
                <div>
                    <span class="issue-type $issue_type">$issue_type</span>
                </div>
            </div>
EOF
                if [ -n "$check_name" ]; then
                    echo "            <div class=\"check-name\">[$check_name]</div>" >> "$html_file"
                fi
                cat >> "$html_file" << EOF
            <div class="message">$message</div>
        </div>
EOF
        done < "$text_file"
    fi

    cat >> "$html_file" << EOF
        
        <div class="footer">
            <p>Generated by clang-tidy analysis script for ZDB project</p>
            <p>Report file: $html_file</p>
            <p><small>💡 Click on the summary cards above to filter results by type | Use Ctrl+1,2,3,4 for keyboard shortcuts</small></p>
        </div>
    </div>
</body>
</html>
EOF
}

# Record start time
START_TIME=$(date +%s)

# Check if run-clang-tidy is available for parallel execution
if command -v run-clang-tidy >/dev/null 2>&1; then
    echo "Using run-clang-tidy for parallel execution..."
    
    case "$OUTPUT_FORMAT" in
        "yaml")
            echo "Exporting fixes to YAML format..."
            run-clang-tidy -p "$BUILD_DIR" -export-fixes "$YAML_OUTPUT" src/ tst/ -j "$(nproc)" -extra-arg=-I"$BUILD_DIR" -extra-arg=-Wno-unknown-argument -header-filter='^(?!.*(vcpkg|\.pb\.h|spdlog|fmt|grpc|gtest|protobuf)).*' 2>&1 | tee "$TEXT_OUTPUT"
            echo "YAML report saved to: $YAML_OUTPUT"
            ;;
        "html")
            echo "Generating text output for HTML conversion..."
            run-clang-tidy -p "$BUILD_DIR" -quiet src/ tst/ -j "$(nproc)" -extra-arg=-I"$BUILD_DIR" -extra-arg=-Wno-unknown-argument -header-filter='^(?!.*(vcpkg|\.pb\.h|spdlog|fmt|grpc|gtest|protobuf)).*' 2>&1 | tee "$TEXT_OUTPUT"
            ;;
        "both")
            echo "Generating both YAML and HTML formats..."
            run-clang-tidy -p "$BUILD_DIR" -export-fixes "$YAML_OUTPUT" src/ tst/ -j "$(nproc)" -extra-arg=-I"$BUILD_DIR" -extra-arg=-Wno-unknown-argument -header-filter='^(?!.*(vcpkg|\.pb\.h|spdlog|fmt|grpc|gtest|protobuf)).*' 2>&1 | tee "$TEXT_OUTPUT"
            echo "YAML report saved to: $YAML_OUTPUT"
            ;;
        *)
            echo "Unknown output format: $OUTPUT_FORMAT. Using HTML format."
            run-clang-tidy -p "$BUILD_DIR" -quiet src/ tst/ -j "$(nproc)" -extra-arg=-I"$BUILD_DIR" -extra-arg=-Wno-unknown-argument -header-filter='^(?!.*(vcpkg|\.pb\.h|spdlog|fmt|grpc|gtest|protobuf)).*' 2>&1 | tee "$TEXT_OUTPUT"
            ;;
    esac
else
    echo "Using clang-tidy directly..."
    # Clear the output file
    > "$TEXT_OUTPUT"
    
    # Run clang-tidy on each file
    for file in $CPP_FILES; do
        echo "Analyzing $file..."
        if [ "$OUTPUT_FORMAT" = "yaml" ] || [ "$OUTPUT_FORMAT" = "both" ]; then
            clang-tidy "$file" -p "$BUILD_DIR" --format-style=file --export-fixes="${YAML_OUTPUT}.$(basename "$file")" -extra-arg=-I"$BUILD_DIR" -extra-arg=-Wno-unknown-argument -header-filter='^(?!.*(vcpkg|\.pb\.h|spdlog|fmt|grpc|gtest|protobuf)).*' 2>&1 | tee -a "$TEXT_OUTPUT"
        else
            clang-tidy "$file" -p "$BUILD_DIR" --format-style=file --quiet -extra-arg=-I"$BUILD_DIR" -extra-arg=-Wno-unknown-argument -header-filter='^(?!.*(vcpkg|\.pb\.h|spdlog|fmt|grpc|gtest|protobuf)).*' 2>&1 | tee -a "$TEXT_OUTPUT"
        fi
    done
    
    # If YAML format was requested, merge individual YAML files
    if [ "$OUTPUT_FORMAT" = "yaml" ] || [ "$OUTPUT_FORMAT" = "both" ]; then
        echo "Merging YAML fix files..."
        echo "MainSourceFile: ''" > "$YAML_OUTPUT"
        echo "Diagnostics: []" >> "$YAML_OUTPUT"
        echo "Replacements:" >> "$YAML_OUTPUT"
        for yaml_file in "${YAML_OUTPUT}".*.cpp; do
            if [ -f "$yaml_file" ]; then
                grep -A 1000 "Replacements:" "$yaml_file" | tail -n +2 >> "$YAML_OUTPUT"
                rm "$yaml_file"
            fi
        done
        echo "YAML report saved to: $YAML_OUTPUT"
    fi
fi

# Record end time
END_TIME=$(date +%s)

# Generate HTML report if requested
if [ "$OUTPUT_FORMAT" = "html" ] || [ "$OUTPUT_FORMAT" = "both" ]; then
    echo "Generating HTML report..."
    generate_html_report "$TEXT_OUTPUT" "$HTML_OUTPUT" "$START_TIME" "$END_TIME"
    echo "HTML report saved to: $HTML_OUTPUT"
fi

echo ""
echo "🎉 clang-tidy analysis completed!"
echo "📊 Analysis duration: $((END_TIME - START_TIME)) seconds"
echo "📁 Reports saved in: $OUTPUT_DIR/"

# Display summary
if [ -f "$TEXT_OUTPUT" ]; then
    total_issues=$(grep "^/" "$TEXT_OUTPUT" 2>/dev/null | grep -v -E "(vcpkg|spdlog|fmt|grpc|gtest|protobuf)" | wc -l || echo "0")
    errors=$(grep ": error:" "$TEXT_OUTPUT" 2>/dev/null | grep -v -E "(vcpkg|spdlog|fmt|grpc|gtest|protobuf)" | wc -l || echo "0")
    warnings=$(grep ": warning:" "$TEXT_OUTPUT" 2>/dev/null | grep -v -E "(vcpkg|spdlog|fmt|grpc|gtest|protobuf)" | wc -l || echo "0")
    notes=$(grep ": note:" "$TEXT_OUTPUT" 2>/dev/null | grep -v -E "(vcpkg|spdlog|fmt|grpc|gtest|protobuf)" | wc -l || echo "0")
    
    echo ""
    echo "📈 Summary (excluding external libraries):"
    echo "   Total issues: $total_issues"
    echo "   Errors: $errors"
    echo "   Warnings: $warnings"
    echo "   Notes: $notes"
fi

echo ""
echo "📄 Available reports:"
[ -f "$TEXT_OUTPUT" ] && echo "   📝 Text: $TEXT_OUTPUT"
[ -f "$YAML_OUTPUT" ] && echo "   📋 YAML: $YAML_OUTPUT"
[ -f "$HTML_OUTPUT" ] && echo "   🌐 HTML: $HTML_OUTPUT"
