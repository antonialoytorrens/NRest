#!/usr/bin/make -f
CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -fstack-protector-strong
LDFLAGS = -lulfius -ljansson -lsqlite3 -lpthread -ldl
SRCFILES = nrest-api.c

# Configuration
PORT ?= 8080
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

.PHONY: all debug release db run clean dist

all: db debug

db:
	@if [ ! -f $(OUTPUT_LATEST) ]; then \
		echo "Creating database schemas..."; \
		sqlite3 $(DATABASE_FILE) < $(SQL_FILE); \
	fi

debug: db $(DEBUG_TARGET)
	@ln -sf debug/nrest-api $(LATEST_LINK)

release: db $(RELEASE_TARGET)
	@ln -sf release/nrest-api $(LATEST_LINK)

$(DEBUG_TARGET): $(SRCFILES)
	@mkdir -p $(DEBUG_DIR)
	$(CC) $(CFLAGS) -g3 -Werror -fsanitize=address,undefined,leak $(DEFINES) $< -o $@ $(LDFLAGS)

$(RELEASE_TARGET): $(SRCFILES)
	@mkdir -p $(RELEASE_DIR)
	$(CC) $(CFLAGS) -O2 $(DEFINES) $< -o $@ $(LDFLAGS)

run: $(LATEST_LINK)
	@echo "Running NRest API..."
	./$(LATEST_LINK)

$(LATEST_LINK):
	@echo "No build found. Run 'make debug' or 'make release' first."
	@exit 1

clean:
	rm -rf $(BUILD_DIR)

dist:
	@mkdir -p $(BUILD_DIR)/dist
	tar -czf $(BUILD_DIR)/dist/nrest-api-latest.tar.gz Makefile nrest-api.c README.md scripts mock TODO