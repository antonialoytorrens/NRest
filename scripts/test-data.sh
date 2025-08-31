#!/bin/bash
cd "$(dirname "$0")"
# Insert data
curl -X PUT "http://localhost:8080/templates/workflows" -H "Content-Type: application/json" --data @../mock/6270.json
echo ""
curl -X PUT "http://localhost:8080/templates/workflows" -H "Content-Type: application/json" --data @../mock/6271.json
echo ""
curl -X PUT "http://localhost:8080/templates/workflows" -H "Content-Type: application/json" --data @../mock/6272.json
echo ""
# n8n integration
curl -X GET "http://localhost:8080/health"
echo ""
curl -X GET "http://localhost:8080/templates/categories"
echo ""
curl -X GET "http://localhost:8080/templates/workflows/6270"
echo ""
curl -X GET "http://localhost:8080/templates/search?search=&page=1&limit=20&category="
echo ""
curl -X GET "http://localhost:8080/templates/collections?&search"
echo ""
curl -X GET "http://localhost:8080/templates/search?search=&page=1&limit=20&category=AI%2CAI%20Chatbot"
echo ""
curl -X OPTIONS "http://localhost:8080/templates/categories"
echo ""
curl -X OPTIONS "http://localhost:8080/templates/workflows/6270"
echo ""
curl -X OPTIONS "http://localhost:8080/templates/search?search=&page=1&limit=20&category="
echo ""
curl -X OPTIONS "http://localhost:8080/templates/search?search=&page=1&limit=20&category=AI%2CAI%20Chatbot"
echo ""
curl -X OPTIONS "http://localhost:8080/templates/collections?&search"
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

# Invalid collection
curl -X PATCH http://localhost:8080/templates/collections \
  -H "Content-Type: application/json" \
  -d '{
    "collectionId": 999,
    "templateId": 1
  }'

echo ""

# Get collection 1
curl -X GET http://localhost:8080/templates/collections/1

# n8n cloud upstream integration tests
#curl -X GET "https://api.n8n.io/templates/categories"
#echo ""
#curl -X GET "https://api.n8n.io/templates/workflows/6270"
#echo ""
#curl -X OPTIONS "https://api.n8n.io/templates/categories"
#echo ""
#curl -X OPTIONS "https://api.n8n.io/templates/collections?search=&page=1&limit=20&category="
#echo ""
#curl -X OPTIONS "https://api.n8n.io/templates/collections"
#echo ""