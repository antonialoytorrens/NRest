#!/bin/bash
set -eu -o pipefail

cd "$(dirname "$0")"
# Insert workflows
curl -X PUT "http://localhost:8080/templates/workflows" -H "Content-Type: application/json" --data @../mock/6270.json
echo ""
curl -X PUT "http://localhost:8080/templates/workflows" -H "Content-Type: application/json" --data @../mock/6271.json
echo ""
curl -X PUT "http://localhost:8080/templates/workflows" -H "Content-Type: application/json" --data @../mock/6272.json
echo ""
# Create a collection with workflows
curl -X PUT http://localhost:8080/templates/collections \
  -H "Content-Type: application/json" \
  -d '{
    "name": "My Custom Collection",
    "rank": 10,
    "totalViews": 0,
    "createdAt": "2024-12-18T15:30:00.000Z",
    "workflows": [
      {"id": 6270},
      {"id": 6271}
    ]
  }'

echo ""

# Create a collection from scratch
curl -X PUT http://localhost:8080/templates/collections \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Simple Collection",
    "createdAt": "2024-12-18T15:35:00.000Z"
  }'

echo ""

# Add workflow 3 to collection 1
curl -X PATCH http://localhost:8080/templates/collections \
  -H "Content-Type: application/json" \
  -d '{
    "collectionId": 1,
    "templateId": 6272
  }'

echo ""

# Test Invalid collection
# curl -X PATCH http://localhost:8080/templates/collections \
#   -H "Content-Type: application/json" \
#   -d '{
#     "collectionId": 999,
#     "templateId": 1
#   }'
# 
# echo ""