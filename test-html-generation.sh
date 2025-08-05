#!/bin/bash

# Test HTML generation function
mkdir -p test-reports

# Create some sample clang-tidy output
cat > test-reports/sample-output.txt << 'EOF'
/home/ahmed/ws/zdb/src/zdb/main.cpp:15:14: warning: parameter 'argc' is unused [misc-unused-parameters]
/home/ahmed/ws/zdb/src/zdb/main.cpp:17:17: warning: unused local variable 'peer_id' [bugprone-unused-local-non-trivial-variable]
/home/ahmed/ws/zdb/src/common/Error.cpp:6:6: error: no header providing "std::ostream" is directly included [misc-include-cleaner]
/home/ahmed/ws/zdb/src/common/Error.cpp:31:24: note: parameter name 'c' is too short [readability-identifier-length]
EOF

# Simple HTML generation function
generate_test_html() {
    local text_file="$1"
    local html_file="$2"
    
    cat > "$html_file" << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>ZDB Clang-Tidy Test Report</title>
    <style>
        body { font-family: 'Segoe UI', Arial, sans-serif; margin: 20px; background: #f5f5f5; }
        .container { max-width: 1200px; margin: 0 auto; background: white; padding: 30px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        .header { border-bottom: 3px solid #007acc; padding-bottom: 20px; margin-bottom: 30px; }
        .header h1 { color: #007acc; margin: 0; }
        .summary { display: grid; grid-template-columns: repeat(4, 1fr); gap: 15px; margin-bottom: 30px; }
        .summary-card { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; padding: 20px; border-radius: 8px; text-align: center; }
        .summary-number { font-size: 2em; font-weight: bold; display: block; }
        .issue { background: #fff3cd; border-left: 4px solid #ffc107; padding: 15px; margin: 10px 0; border-radius: 0 4px 4px 0; }
        .issue.error { background: #f8d7da; border-left-color: #dc3545; }
        .issue.warning { background: #fff3cd; border-left-color: #ffc107; }
        .issue.note { background: #d1ecf1; border-left-color: #17a2b8; }
        .issue-type { background: #6c757d; color: white; padding: 3px 8px; border-radius: 12px; font-size: 0.8em; float: right; }
        .issue-type.error { background: #dc3545; }
        .issue-type.warning { background: #ffc107; color: #212529; }
        .issue-type.note { background: #17a2b8; }
        .file-path { font-family: monospace; font-size: 0.9em; color: #495057; }
        .check-name { font-family: monospace; font-size: 0.9em; color: #007acc; font-weight: bold; }
        .message { margin-top: 10px; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🔍 ZDB Clang-Tidy Test Report</h1>
        </div>
EOF

    # Count issues
    local total_issues=$(grep -c "^/" "$text_file" 2>/dev/null || echo "0")
    local errors=$(grep -c ": error:" "$text_file" 2>/dev/null || echo "0")  
    local warnings=$(grep -c ": warning:" "$text_file" 2>/dev/null || echo "0")
    local notes=$(grep -c ": note:" "$text_file" 2>/dev/null || echo "0")

    cat >> "$html_file" << EOF
        <div class="summary">
            <div class="summary-card">
                <span class="summary-number">$total_issues</span>
                <span class="summary-label">Total Issues</span>
            </div>
            <div class="summary-card" style="background: linear-gradient(135deg, #ff6b6b 0%, #ee5a24 100%);">
                <span class="summary-number">$errors</span>
                <span class="summary-label">Errors</span>
            </div>
            <div class="summary-card" style="background: linear-gradient(135deg, #feca57 0%, #ff9ff3 100%);">
                <span class="summary-number">$warnings</span>
                <span class="summary-label">Warnings</span>
            </div>
            <div class="summary-card" style="background: linear-gradient(135deg, #48dbfb 0%, #0abde3 100%);">
                <span class="summary-number">$notes</span>
                <span class="summary-label">Notes</span>
            </div>
        </div>
        
        <h2>📋 Issues Found</h2>
EOF

    # Process each line
    while IFS= read -r line; do
        if [[ $line =~ ^([^:]+):([0-9]+):([0-9]+):[[:space:]]+(error|warning|note):[[:space:]]+(.+)[[:space:]]+\[([^\]]+)\] ]]; then
            local file_path="${BASH_REMATCH[1]}"
            local line_num="${BASH_REMATCH[2]}"
            local col_num="${BASH_REMATCH[3]}"
            local issue_type="${BASH_REMATCH[4]}"
            local message="${BASH_REMATCH[5]}"
            local check_name="${BASH_REMATCH[6]}"
            
            cat >> "$html_file" << EOF
        <div class="issue $issue_type">
            <span class="issue-type $issue_type">$issue_type</span>
            <div class="file-path">$file_path:$line_num:$col_num</div>
            <div class="check-name">[$check_name]</div>
            <div class="message">$message</div>
        </div>
EOF
        fi
    done < "$text_file"

    cat >> "$html_file" << 'EOF'
    </div>
</body>
</html>
EOF

    echo "HTML report generated: $html_file"
}

# Test the function
generate_test_html test-reports/sample-output.txt test-reports/test-report.html

echo "Test completed. Check test-reports/test-report.html"
