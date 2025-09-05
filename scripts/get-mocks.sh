#!/bin/sh
# set -eu -o pipefail
set -eu

# Get script directory and change to it
SCRIPT_DIR=$(dirname "$0")
cd "$SCRIPT_DIR"

# Configuration
BASE_URL="https://api.n8n.io"
TEMPLATES_PATH="/templates/workflows"
COLLECTIONS_PATH="/templates/collections"
WORKFLOWS_PATH="/workflows/templates"
MOCK_DIR="../mock"

# Create mock directory if it doesn't exist
mkdir -p "$MOCK_DIR"

# Function to download file if missing or empty
download_if_missing() {
    url="$1"
    output="$2"

    if [ ! -f "$output" ] || [ ! -s "$output" ]; then
        echo "Downloading $output from $url..."
        if ! curl -sSL -X GET "$url" -o "$output"; then
            echo "Error: Failed to download $url" >&2
            return 1
        fi
    else
        echo "File already exists and is not empty: $output"
    fi
}

# Function to download workflow template
download_template() {
    workflow_id="$1"
    url="${BASE_URL}${TEMPLATES_PATH}/${workflow_id}"
    output="${MOCK_DIR}/${workflow_id}.json"
    download_if_missing "$url" "$output"
}

# Function to download imported workflow
download_imported_workflow() {
    workflow_id="$1"
    url="${BASE_URL}${WORKFLOWS_PATH}/${workflow_id}"
    output="${MOCK_DIR}/${workflow_id}_imported.json"
    download_if_missing "$url" "$output"
}

# Main execution
main() {
    echo "Setting up mock data..."
    
    # Download workflow templates
    for workflow_id in 6270 6271 6272; do
        if ! download_template "$workflow_id"; then
            echo "Warning: Failed to download template $workflow_id" >&2
        fi
    done

    # Download collections
    if ! download_if_missing "${BASE_URL}${COLLECTIONS_PATH}" "${MOCK_DIR}/n8n-api-collections.json"; then
        echo "Warning: Failed to download collections" >&2
    fi

    # Download imported workflow endpoint
    if ! download_imported_workflow "6270"; then
        echo "Warning: Failed to download imported workflow 6270" >&2
    fi

    echo "Mock data setup complete!"
}

# Run main function
main "$@"