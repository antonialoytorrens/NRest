#!/bin/sh
# set -eu -o pipefail
set -eu

# Get script directory and change to it
SCRIPT_DIR=$(dirname "$0")
cd "$SCRIPT_DIR"

# Configuration
PORT=${PORT:-8080}
API_BASE="http://localhost:${PORT}"
MOCK_DIR="../mock"

make_request() {
    method="$1"
    url="$2"
    content_type="$3"
    data="$4"
    
    if [ "$method" = "PUT" ] || [ "$method" = "PATCH" ]; then
        if [ -n "$data" ]; then
            if [ -f "$data" ]; then
                # Data is a file path
                curl -X "$method" "$url" -H "$content_type" --data "@$data"
            else
                # Data is inline JSON
                curl -X "$method" "$url" -H "$content_type" -d "$data"
            fi
        else
            curl -X "$method" "$url" -H "$content_type"
        fi
    else
        curl -X "$method" "$url" -H "$content_type"
    fi
    echo ""
}

# Main execution
main() {
    echo "Populating API with mock data..."
    
    # Insert workflows
    echo "Inserting workflow templates..."
    make_request "PUT" "${API_BASE}/templates/workflows" "Content-Type: application/json" "${MOCK_DIR}/6270.json"
    make_request "PUT" "${API_BASE}/templates/workflows" "Content-Type: application/json" "${MOCK_DIR}/6271.json"
    make_request "PUT" "${API_BASE}/templates/workflows" "Content-Type: application/json" "${MOCK_DIR}/6272.json"
    
    # Create a collection with workflows
    echo "Creating collection with workflows..."
    collection_data='{
        "name": "My Custom Collection",
        "rank": 10,
        "totalViews": 0,
        "createdAt": "2024-12-18T15:30:00.000Z",
        "workflows": [
            {"id": 6270},
            {"id": 6271}
        ]
    }'
    make_request "PUT" "${API_BASE}/templates/collections" "Content-Type: application/json" "$collection_data"
    
    # Create a collection from scratch
    echo "Creating simple collection..."
    simple_collection='{
        "name": "Simple Collection",
        "createdAt": "2024-12-18T15:35:00.000Z"
    }'
    make_request "PUT" "${API_BASE}/templates/collections" "Content-Type: application/json" "$simple_collection"
    
    # Add workflow 3 to collection 1
    echo "Adding workflow to collection..."
    patch_data='{
        "collectionId": 1,
        "templateId": 6272
    }'
    make_request "PATCH" "${API_BASE}/templates/collections" "Content-Type: application/json" "$patch_data"
    
    echo "Data population complete!"
}

# Run main function
main "$@"