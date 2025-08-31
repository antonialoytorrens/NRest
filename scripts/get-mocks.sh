#!/bin/bash
set -eu -o pipefail
cd $(dirname $0)

# Configuration
BASE_URL="https://api.n8n.io"
TEMPLATES_PATH="/templates/workflows"
COLLECTIONS_PATH="/templates/collections"
WORKFLOWS_PATH="/workflows/templates"
MOCK_DIR="../mock"

mkdir -p "$MOCK_DIR"

download_if_missing() {
	local url="$1"
	local output="$2"

	if [[ ! -f "$output" || ! -s "$output" ]]; then
		echo "Downloading $output from $url..."
		curl -s -X GET "$url" > "$output"
	else
		echo "File already exists and is not empty: $output"
	fi
}

download_template() {
	local workflow_id="$1"
	local url="${BASE_URL}${TEMPLATES_PATH}/${workflow_id}"
	local output="${MOCK_DIR}/${workflow_id}.json"
	download_if_missing "$url" "$output"
}

download_imported_workflow() {
	local workflow_id="$1"
	local url="${BASE_URL}${WORKFLOWS_PATH}/${workflow_id}"
	local output="${MOCK_DIR}/${workflow_id}_imported.json"
	download_if_missing "$url" "$output"
}

# Workflow templates
for workflow_id in 6270 6271 6272; do
	download_template "$workflow_id"
done

# Collections
download_if_missing "${BASE_URL}${COLLECTIONS_PATH}" "${MOCK_DIR}/n8n-api-collections.json"

# Imported workflow endpoint
download_imported_workflow "6270"

echo "Mock data setup complete!"
