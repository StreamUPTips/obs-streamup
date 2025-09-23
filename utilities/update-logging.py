#!/usr/bin/env python3
"""
Script to convert remaining blog() calls to StreamUP debug logging system.
This script identifies patterns and converts them systematically.
"""

import os
import re
from pathlib import Path

def update_file(file_path):
    """Update a single file's blog calls."""
    print(f"Updating {file_path}")

    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    original_content = content

    # Add debug-logger include if not present
    if '#include "../utilities/debug-logger.hpp"' not in content and '#include "debug-logger.hpp"' not in content:
        if file_path.name.endswith('.cpp'):
            # Find first #include line and add our include
            include_match = re.search(r'(#include [^\n]+\n)', content)
            if include_match:
                first_include = include_match.group(1)
                if 'utilities/' in file_path.as_posix():
                    include_line = '#include "debug-logger.hpp"\n'
                else:
                    include_line = '#include "../utilities/debug-logger.hpp"\n'
                content = content.replace(first_include, first_include + include_line)

    # Convert remaining blog calls
    patterns = [
        # LOG_INFO patterns - convert to debug for detailed operational info
        (r'blog\(LOG_INFO,\s*"(\[StreamUP[^\]]*\])\s*([^"]*)",\s*([^)]+)\);',
         r'StreamUP::DebugLogger::LogDebugFormat("\1", "\2", \3);'),
        (r'blog\(LOG_INFO,\s*"(\[StreamUP[^\]]*\])\s*([^"]*)"([^)]*)\);',
         r'StreamUP::DebugLogger::LogDebug("\1", "\2");'),

        # LOG_INFO patterns for MultiDock - these are debug level
        (r'blog\(LOG_INFO,\s*"\[StreamUP MultiDock\]\s*([^"]*)",\s*([^)]+)\);',
         r'StreamUP::DebugLogger::LogDebugFormat("MultiDock", "\1", \2);'),
        (r'blog\(LOG_INFO,\s*"\[StreamUP MultiDock\]\s*([^"]*)"([^)]*)\);',
         r'StreamUP::DebugLogger::LogDebug("MultiDock", "\1");'),

        # LOG_WARNING patterns
        (r'blog\(LOG_WARNING,\s*"\[StreamUP[^\]]*\]\s*([^"]*)",\s*([^)]+)\);',
         r'StreamUP::DebugLogger::LogWarningFormat("General", "\1", \2);'),
        (r'blog\(LOG_WARNING,\s*"\[StreamUP MultiDock\]\s*([^"]*)",\s*([^)]+)\);',
         r'StreamUP::DebugLogger::LogWarningFormat("MultiDock", "\1", \2);'),
        (r'blog\(LOG_WARNING,\s*"\[StreamUP[^\]]*\]\s*([^"]*)"([^)]*)\);',
         r'StreamUP::DebugLogger::LogWarning("General", "\1");'),
        (r'blog\(LOG_WARNING,\s*"\[StreamUP MultiDock\]\s*([^"]*)"([^)]*)\);',
         r'StreamUP::DebugLogger::LogWarning("MultiDock", "\1");'),
    ]

    for pattern, replacement in patterns:
        content = re.sub(pattern, replacement, content)

    # Write back if changed
    if content != original_content:
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"  ✓ Updated {file_path}")
        return True
    return False

def main():
    """Main function to process all files."""
    plugin_dir = Path(__file__).parent.parent

    # Find all .cpp files in the plugin
    cpp_files = []
    for root, dirs, files in os.walk(plugin_dir):
        # Skip build directories
        if 'build' in root or 'CMakeFiles' in root:
            continue

        for file in files:
            if file.endswith('.cpp'):
                cpp_files.append(Path(root) / file)

    print(f"Found {len(cpp_files)} .cpp files to process")

    updated_count = 0
    for cpp_file in cpp_files:
        if update_file(cpp_file):
            updated_count += 1

    print(f"\nCompleted! Updated {updated_count} files.")

if __name__ == "__main__":
    main()