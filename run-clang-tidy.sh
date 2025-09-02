#!/bin/bash

# Script to run clang-tidy on the ZDB project
# Usage: ./run-clang-tidy.sh [build_directory] [output_format]
#   output_format: html (default), yaml, both

set -e

# Always run from the repository root (the script's directory)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

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
    ./run-clang-tidy.sh out/build/clang-21        # HTML report, custom build dir
    ./run-clang-tidy.sh out/build/gcc-14 yaml     # YAML fixes file
    ./run-clang-tidy.sh out/build/gcc-14 both     # Both HTML and YAML

REQUIREMENTS:
    - clang-tidy-21 installed (with run-clang-tidy-21 if available)
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

# Find all C++ source files (headers will be analyzed via HeaderFilterRegex when included)
CPP_FILES=$(find src/ tst/ -name "*.cpp" -type f)
HPP_FILES=$(find src/ tst/ -name "*.hpp" -type f)

if [ -z "$CPP_FILES" ]; then
    echo "No C++ source files found in src/ and tst/ directories."
    exit 1
fi

echo "Found $(echo "$CPP_FILES" | wc -l) C++ source files to analyze."
echo "Found $(echo "$HPP_FILES" | wc -l) C++ header files (analyzed via includes and HeaderFilterRegex)."

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
        /* Check/category filter styles */
        .filters {
            display: grid;
            grid-template-columns: 1fr;
            gap: 10px;
            margin-bottom: 20px;
            background: #f8f9fa;
            padding: 15px;
            border-radius: 6px;
            border: 1px solid #e9ecef;
        }
        .filters .title {
            font-weight: 600;
            color: #495057;
        }
        .check-search {
            display: flex;
            gap: 10px;
            align-items: center;
        }
        .check-search input {
            width: 100%;
            padding: 8px 10px;
            border: 1px solid #ced4da;
            border-radius: 4px;
            font-size: 0.95em;
        }
        .check-search button {
            padding: 8px 12px;
            background: #e9ecef;
            border: 1px solid #ced4da;
            border-radius: 4px;
            cursor: pointer;
        }
        .check-list {
            display: flex;
            flex-wrap: wrap;
            gap: 8px;
            max-height: 220px;
            overflow: auto;
            padding: 5px 2px 2px;
        }
        .check-chip {
            border: 1px solid #dee2e6;
            background: #ffffff;
            color: #343a40;
            border-radius: 999px;
            padding: 6px 10px;
            font-size: 0.85em;
            cursor: pointer;
            user-select: none;
            transition: all 0.2s ease;
        }
        .check-chip:hover { box-shadow: 0 1px 4px rgba(0,0,0,0.12); transform: translateY(-1px); }
        .check-chip.active { background: #007acc; color: white; border-color: #007acc; }
        .check-chip .chip-count { opacity: 0.8; font-weight: 600; margin-left: 6px; }
        .check-link { text-decoration: none; cursor: pointer; }
        .check-link:hover { text-decoration: underline; }
    </style>
    <script>
        document.addEventListener('DOMContentLoaded', function() {
            const summaryCards = document.querySelectorAll('.summary-card');
            const issues = document.querySelectorAll('.issue');
            let activeTypeFilter = 'all';
            let activeCheckFilter = '';

            // Add click handlers to summary cards
            summaryCards.forEach(card => {
                card.addEventListener('click', function() {
                    const filterType = this.getAttribute('data-filter');
                    
                    // Remove active class from all cards
                    summaryCards.forEach(c => c.classList.remove('active'));

                    // If clicking the same filter, toggle off (show all)
                    if (activeTypeFilter === filterType) {
                        activeTypeFilter = 'all';
                        filterIssues();
                        updateResultsTitle();
                    } else {
                        activeTypeFilter = filterType;
                        this.classList.add('active');
                        filterIssues();
                        updateResultsTitle();
                    }
                });
            });

        function filterIssues() {
                issues.forEach(issue => {
            const matchesType = (activeTypeFilter === 'all') || issue.classList.contains(activeTypeFilter);
            const issueCheck = (issue.getAttribute('data-check') || '').trim();
            const activeCheck = (activeCheckFilter || '').trim();
            const matchesCheck = (!activeCheck) || (issueCheck === activeCheck);
                    if (matchesType && matchesCheck) {
                        issue.classList.remove('hidden');
                    } else {
                        issue.classList.add('hidden');
                    }
                });
            }

            function updateResultsTitle() {
                const resultsTitle = document.querySelector('h2');
                const typeText = (activeTypeFilter === 'all') ? 'All' : (activeTypeFilter.charAt(0).toUpperCase() + activeTypeFilter.slice(1) + 's');
                const checkText = activeCheckFilter ? ` | Check: [${activeCheckFilter}]` : '';
                const filteredText = (activeTypeFilter === 'all' && !activeCheckFilter) ? '' : ` - ${typeText}${checkText}`;
                resultsTitle.textContent = `📋 Detailed Analysis Results${filteredText}`;
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

            // Check chips filtering
            const checkList = document.getElementById('check-list');
            const checkSearch = document.getElementById('check-search');
            const clearCheckBtn = document.getElementById('clear-check-filter');
        if (checkList) {
                checkList.addEventListener('click', function(e) {
                    const chip = e.target.closest('.check-chip');
                    if (!chip) return;
            const selected = (chip.getAttribute('data-check') || '').trim();
                    const previouslyActive = chip.classList.contains('active');
                    // Reset all chips
                    checkList.querySelectorAll('.check-chip').forEach(c => c.classList.remove('active'));
                    if (previouslyActive) {
                        activeCheckFilter = '';
                    } else {
                        chip.classList.add('active');
                        activeCheckFilter = selected;
                    }
                    filterIssues();
                    updateResultsTitle();
                });
            }
            if (checkSearch) {
                checkSearch.addEventListener('input', function() {
                    const q = this.value.toLowerCase();
                    checkList.querySelectorAll('.check-chip').forEach(chip => {
                        const txt = chip.textContent.toLowerCase().trim();
                        chip.style.display = txt.includes(q) ? '' : 'none';
                    });
                });
            }
            if (clearCheckBtn) {
                clearCheckBtn.addEventListener('click', function() {
                    activeCheckFilter = '';
                    if (checkList) checkList.querySelectorAll('.check-chip').forEach(c => c.classList.remove('active'));
                    filterIssues();
                    updateResultsTitle();
                });
            }

            // Clicking on a check name inside an issue also filters
            document.body.addEventListener('click', function(e) {
                const link = e.target.closest('.check-link');
                if (!link) return;
                e.preventDefault();
        const name = (link.getAttribute('data-check') || '').trim();
        activeCheckFilter = name;
                // highlight corresponding chip
                if (checkList) {
                    checkList.querySelectorAll('.check-chip').forEach(c => {
            const chipName = (c.getAttribute('data-check') || '').trim();
            if (chipName === name) c.classList.add('active'); else c.classList.remove('active');
                    });
                }
                filterIssues();
                updateResultsTitle();
                // scroll to results header for context
                const h2 = document.querySelector('h2');
                if (h2) h2.scrollIntoView({ behavior: 'smooth', block: 'start' });
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
           echo "$(echo "$CPP_FILES" | wc -l) source + $(echo "$HPP_FILES" | wc -l) header" >> "$html_file"
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

        <div class="filters">
            <div class="title">Filter by clang-tidy check</div>
            <div class="check-search">
                <input id="check-search" type="text" placeholder="Search checks, e.g. readability-identifier-naming" aria-label="Search checks" />
                <button id="clear-check-filter" title="Clear check filter">Clear</button>
            </div>
            <div id="check-list" class="check-list">
EOF

    # Build per-check counts (errors and warnings with [check]) excluding external libraries
    # Output format: count check_name
    local check_counts
    check_counts=$(grep -E "^/.*:[0-9]+:[0-9]+: +(error|warning): .*\[[^]]+\]" "$text_file" 2>/dev/null \
        | grep -v -E "(vcpkg|spdlog|fmt|grpc|gtest|protobuf)" \
        | sed -n 's/.*\[\([^]]*\)\].*/\1/p' \
        | sort | uniq -c | sort -nr || true)

    if [ -n "$check_counts" ]; then
        # Emit a chip for each check
        while IFS= read -r line; do
            # uniq -c output has leading spaces; extract count and the rest as the check name
            local count check
            count=$(echo "$line" | awk '{print $1}')
            check=$(echo "$line" | sed -E 's/^ *[0-9]+ +//')
            # Safety: trim any trailing spaces
            check=$(echo "$check" | sed -E 's/ +$//')
            [ -z "$check" ] && continue
            # Write chip with clean data-check attribute
            cat >> "$html_file" << EOF
                <span class="check-chip" data-check="$check">$check <span class="chip-count">$count</span></span>
EOF
        done <<< "$check_counts"
    else
        echo "                <em>No check-based diagnostics found.</em>" >> "$html_file"
    fi

    cat >> "$html_file" << EOF
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
    <div class="issue $issue_type" data-check="$check_name">
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
            echo "            <div class=\"check-name\"><a href=\"#\" class=\"check-link\" data-check=\"$check_name\">[$check_name]</a></div>" >> "$html_file"
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

# Check if run-clang-tidy-21 or run-clang-tidy is available for parallel execution
if command -v run-clang-tidy-21 >/dev/null 2>&1; then
    RUN_CLANG_TIDY_CMD="run-clang-tidy-21"
    echo "Using run-clang-tidy-21 for parallel execution..."
elif command -v run-clang-tidy >/dev/null 2>&1; then
    RUN_CLANG_TIDY_CMD="run-clang-tidy"
    echo "Using run-clang-tidy for parallel execution..."
else
    RUN_CLANG_TIDY_CMD=""
fi

if [ -n "$RUN_CLANG_TIDY_CMD" ]; then
    
    case "$OUTPUT_FORMAT" in
        "yaml")
            echo "Exporting fixes to YAML format..."
            "$RUN_CLANG_TIDY_CMD" -p "$BUILD_DIR" -export-fixes "$YAML_OUTPUT" src/ tst/ -j "$(nproc)" -extra-arg=-std=c++23 -extra-arg=-stdlib=libc++ -header-filter='(src|tst)/.*\.(h|hpp)$' -exclude-header-filter='.*out.*' 2>&1 | tee "$TEXT_OUTPUT"
            echo "YAML report saved to: $YAML_OUTPUT"
            ;;
        "html")
            echo "Generating text output for HTML conversion..."
            "$RUN_CLANG_TIDY_CMD" -p "$BUILD_DIR" -quiet src/ tst/ -j "$(nproc)" -extra-arg=-std=c++23 -extra-arg=-stdlib=libc++ -header-filter='(src|tst)/.*\.(h|hpp)$' -exclude-header-filter='.*out.*' 2>&1 | tee "$TEXT_OUTPUT"
            ;;
        "both")
            echo "Generating both YAML and HTML formats..."
            "$RUN_CLANG_TIDY_CMD" -p "$BUILD_DIR" -export-fixes "$YAML_OUTPUT" src/ tst/ -j "$(nproc)" -extra-arg=-std=c++23 -extra-arg=-stdlib=libc++ -header-filter='(src|tst)/.*\.(h|hpp)$' -exclude-header-filter='.*out.*' 2>&1 | tee "$TEXT_OUTPUT"
            echo "YAML report saved to: $YAML_OUTPUT"
            ;;
        *)
            echo "Unknown output format: $OUTPUT_FORMAT. Using HTML format."
            "$RUN_CLANG_TIDY_CMD" -p "$BUILD_DIR" -quiet src/ tst/ -j "$(nproc)" -extra-arg=-std=c++23 -extra-arg=-stdlib=libc++ -header-filter='(src|tst)/.*\.(h|hpp)$' -exclude-header-filter='.*out.*' 2>&1 | tee "$TEXT_OUTPUT"
            ;;
    esac
else
    echo "Using clang-tidy-21 directly..."
    # Clear the output file
    > "$TEXT_OUTPUT"
    
    # Run clang-tidy on each file
    for file in $CPP_FILES; do
        echo "Analyzing $file..."
        if [ "$OUTPUT_FORMAT" = "yaml" ] || [ "$OUTPUT_FORMAT" = "both" ]; then
            clang-tidy-21 "$file" -p "$BUILD_DIR" --format-style=file --export-fixes="${YAML_OUTPUT}.$(basename "$file")" -extra-arg=-std=c++23 -extra-arg=-stdlib=libc++ -header-filter='(src|tst)/.*\.(h|hpp)$' -exclude-header-filter='.*out.*' 2>&1 | tee -a "$TEXT_OUTPUT"
        else
            clang-tidy-21 "$file" -p "$BUILD_DIR" --format-style=file --quiet -extra-arg=-std=c++23 -extra-arg=-stdlib=libc++ -header-filter='(src|tst)/.*\.(h|hpp)$' -exclude-header-filter='.*out.*' 2>&1 | tee -a "$TEXT_OUTPUT"
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

# Exit with error count to fail CI when there are errors
if [ -f "$TEXT_OUTPUT" ]; then
    errors=$(grep ": error:" "$TEXT_OUTPUT" 2>/dev/null | grep -v -E "(vcpkg|spdlog|fmt|grpc|gtest|protobuf)" | wc -l || echo "0")
    if [ "$errors" -gt 0 ]; then
        echo ""
        echo "❌ Exiting with code $errors due to clang-tidy errors found."
        echo "   Fix the errors above and re-run the analysis."
        exit "$errors"
    else
        echo ""
        echo "✅ No errors found. Analysis completed successfully."
        exit 0
    fi
else
    echo ""
    echo "⚠️  No output file found. Analysis may have failed."
    exit 1
fi
