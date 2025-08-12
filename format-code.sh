#!/bin/bash
# MoTool clang-format utility script

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default values
DRY_RUN=false
VERBOSE=false
DIRECTORY="src"

show_help() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Format C++ source files using clang-format"
    echo ""
    echo "OPTIONS:"
    echo "  -d, --directory DIR    Directory to format (default: src)"
    echo "  -n, --dry-run         Show what would be changed without modifying files"
    echo "  -v, --verbose         Verbose output"
    echo "  -h, --help            Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                    # Format all files in src/"
    echo "  $0 -n                 # Dry run - show what would change"
    echo "  $0 -d tests           # Format files in tests/"
    echo "  $0 -d src/plugins     # Format files in src/plugins/"
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--directory)
            DIRECTORY="$2"
            shift 2
            ;;
        -n|--dry-run)
            DRY_RUN=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            echo -e "${RED}Error: Unknown option $1${NC}"
            show_help
            exit 1
            ;;
    esac
done

# Check if clang-format is installed
if ! command -v clang-format &> /dev/null; then
    echo -e "${RED}Error: clang-format not found. Please install it:${NC}"
    echo "  brew install clang-format"
    exit 1
fi

# Check if directory exists
if [[ ! -d "$DIRECTORY" ]]; then
    echo -e "${RED}Error: Directory '$DIRECTORY' does not exist${NC}"
    exit 1
fi

# Check if .clang-format exists
if [[ ! -f ".clang-format" ]]; then
    echo -e "${RED}Error: .clang-format file not found in current directory${NC}"
    exit 1
fi

echo -e "${GREEN}MoTool clang-format utility${NC}"
echo "Directory: $DIRECTORY"
echo "Dry run: $DRY_RUN"
echo ""

# Find all C++ source files
FILES=$(find "$DIRECTORY" -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" -o -name "*.cc" -o -name "*.cxx" -o -name "*.mm" \) 2>/dev/null || true)

if [[ -z "$FILES" ]]; then
    echo -e "${YELLOW}No C++ source files found in '$DIRECTORY'${NC}"
    exit 0
fi

# Count files
FILE_COUNT=$(echo "$FILES" | wc -l)
echo "Found $FILE_COUNT C++ source files"
echo ""

# Process each file
CHANGED_COUNT=0
for file in $FILES; do
    if [[ $VERBOSE == true ]]; then
        echo "Processing: $file"
    fi

    if [[ $DRY_RUN == true ]]; then
        # Check if file would be changed
        if ! clang-format --dry-run --Werror "$file" 2>/dev/null; then
            echo -e "${YELLOW}Would format: $file${NC}"
            ((CHANGED_COUNT++))
        fi
    else
        # Format the file in place
        if ! clang-format -i "$file" 2>/dev/null; then
            echo -e "${RED}Error formatting: $file${NC}"
        else
            if [[ $VERBOSE == true ]]; then
                echo -e "${GREEN}Formatted: $file${NC}"
            fi
            ((CHANGED_COUNT++))
        fi
    fi
done

echo ""
if [[ $DRY_RUN == true ]]; then
    echo -e "${GREEN}Dry run complete. $CHANGED_COUNT files would be changed.${NC}"
    if [[ $CHANGED_COUNT -gt 0 ]]; then
        echo -e "${YELLOW}Run without --dry-run to apply changes.${NC}"
    fi
else
    echo -e "${GREEN}Formatting complete. Processed $CHANGED_COUNT files.${NC}"
fi
