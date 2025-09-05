#!/usr/bin/make -f

CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -fstack-protector-strong
LDFLAGS = -lulfius -ljansson -lsqlite3 -lpthread -ldl

# Unity library for tests
UNITY_DIR = /usr/local/include/unity
UNITY_LIB = /usr/local/lib
TEST_LIBS = -lcurl -ljansson -lunity -pthread -L$(UNITY_LIB)

# Override if it already exists
ifeq ($(wildcard $(UNITY_DIR)/unity.h),)
    UNITY_DIR = /usr/include/unity
endif

ifeq ($(wildcard $(UNITY_LIB)/libunity.a),)
    UNITY_LIB = /usr/lib
endif


SRCFILES = nrest-api.c
TESTFILES = tests/nrest-api-test.c

# Configuration
PORT ?= 5679
SQL_FILE = sql/init_database.sql
DATABASE_FILE ?= workflow_templates.db
DEFINES = -DPORT=$(PORT) -DDATABASE_FILE=\"$(DATABASE_FILE)\"

# Build directories
BUILD_DIR = build
DEBUG_DIR = $(BUILD_DIR)/debug
RELEASE_DIR = $(BUILD_DIR)/release

# Targets
DEBUG_TARGET = $(DEBUG_DIR)/nrest-api
RELEASE_TARGET = $(RELEASE_DIR)/nrest-api
LATEST_LINK = $(BUILD_DIR)/nrest-api
TEST_TARGET = $(BUILD_DIR)/test_nrest_api

# Default target
all: db debug

# Database initialization
db:
	@if [ ! -f $(DATABASE_FILE) ]; then \
		echo "Creating database schemas..."; \
		if [ -f $(SQL_FILE) ]; then \
			sqlite3 $(DATABASE_FILE) < $(SQL_FILE); \
		else \
			echo "Error: $(SQL_FILE) not found"; \
			exit 1; \
		fi; \
	else \
		echo "Database already exists: $(DATABASE_FILE)"; \
	fi

# Debug build with sanitizers
$(DEBUG_TARGET): $(SRCFILES)
	@mkdir -p $(DEBUG_DIR)
	$(CC) $(CFLAGS) -g3 -Werror -fsanitize=address,undefined,leak $(DEFINES) $< -o $@ $(LDFLAGS)

# Release build with optimizations
$(RELEASE_TARGET): $(SRCFILES)
	@mkdir -p $(RELEASE_DIR)
	$(CC) $(CFLAGS) -O2 $(DEFINES) $< -o $@ $(LDFLAGS)

# Check if latest link exists
$(LATEST_LINK):
	@if [ ! -L $(LATEST_LINK) ] && [ ! -f $(LATEST_LINK) ]; then \
		echo "No build found. Run 'make debug' or 'make release' first."; \
		exit 1; \
	fi

# Test build
$(TEST_TARGET): $(TESTFILES)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(DEFINES) -I$(UNITY_DIR) -o $@ $< $(TEST_LIBS)

debug: db $(DEBUG_TARGET)
	@ln -sf debug/nrest-api $(LATEST_LINK)
	@echo "Debug build complete: $(DEBUG_TARGET)"

release: db $(RELEASE_TARGET)
	@ln -sf release/nrest-api $(LATEST_LINK)
	@echo "Release build complete: $(RELEASE_TARGET)"

# Run the server
run: $(LATEST_LINK)
	@echo "Running NRest API on port $(PORT)..."
	./$(LATEST_LINK)

# Set up mock data
setup-mocks:
	@echo "Setting up mock data..."
	PORT=$(PORT) ./scripts/get-mocks.sh

# Run tests
test: setup-mocks $(TEST_TARGET)
	@echo "Running test suite..."
	./$(TEST_TARGET) --upstream --verbose

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)
	@echo "Build directory cleaned"

# Clean everything including database
clean-all: clean
	rm -f $(DATABASE_FILE)
	@echo "Database removed"

# Create distribution archive
dist:
	@mkdir -p $(BUILD_DIR)/dist
	tar -czf $(BUILD_DIR)/dist/nrest-api-latest.tar.gz \
		Makefile $(RELEASE_DIR) $(SRCFILES) $(TESTFILES) \
		README.md sql scripts mock TODO 2>/dev/null || \
	tar -czf $(BUILD_DIR)/dist/nrest-api-latest.tar.gz \
		Makefile $(SRCFILES) $(TESTFILES) README.md 2>/dev/null
	@echo "Distribution archive created: $(BUILD_DIR)/dist/nrest-api-latest.tar.gz"

# Help target
help:
	@echo "Available targets:"
	@echo "  make              - Build debug version (default)"
	@echo "  make debug        - Build with debug symbols and sanitizers"
	@echo "  make release      - Build optimized release version"
	@echo "  make run          - Run the server"
	@echo "  make test         - Build and run test suite"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make clean-all    - Remove build artifacts and database"
	@echo "  make dist         - Create distribution archive"
	@echo "  make help         - Show this help message"
	@echo ""
	@echo "Configuration variables:"
	@echo "  PORT=$(PORT)      - Server port"
	@echo "  DATABASE_FILE=$(DATABASE_FILE) - Database file path"

.PHONY: all db debug release run clean clean-all dist setup-mocks test help
