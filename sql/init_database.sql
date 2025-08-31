-- Enable foreign key constraints
PRAGMA foreign_keys = ON;

-- Drop tables if they exist (in proper order to handle foreign keys)
DROP TABLE IF EXISTS collection_categories;
DROP TABLE IF EXISTS template_categories;
DROP TABLE IF EXISTS workflow_nodes;
DROP TABLE IF EXISTS collection_workflows;
DROP TABLE IF EXISTS collections;
DROP TABLE IF EXISTS templates;
DROP TABLE IF EXISTS categories;
DROP TABLE IF EXISTS users;

-- Create users table
CREATE TABLE users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    username TEXT UNIQUE NOT NULL,
    bio TEXT,
    verified INTEGER DEFAULT 0,
    links TEXT, -- JSON array as string
    avatar TEXT
);

-- Create categories table
CREATE TABLE categories (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL UNIQUE,
    icon TEXT,
    parent_id INTEGER,
    FOREIGN KEY (parent_id) REFERENCES categories(id) ON DELETE SET NULL
);

-- Create collections table with description
CREATE TABLE collections (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    rank INTEGER DEFAULT 0,
    name TEXT NOT NULL,
    description TEXT,
    total_views INTEGER DEFAULT 0,
    created_at TEXT NOT NULL
);

-- Create templates table
CREATE TABLE templates (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    total_views INTEGER DEFAULT 0,
    recent_views INTEGER DEFAULT 0,
    price REAL,
    purchase_url TEXT,
    created_at TEXT NOT NULL,
    description TEXT,
    workflow_data TEXT, -- Full workflow object
    workflow_info TEXT, -- WorkflowInfo object as JSON
    nodes_data TEXT, -- Nodes array as JSON
    image_data TEXT, -- Image array as JSON
    user_id INTEGER NOT NULL,
    last_updated_by INTEGER, -- Add lastUpdatedBy field
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE RESTRICT,
    FOREIGN KEY (last_updated_by) REFERENCES users(id) ON DELETE SET NULL
);

-- Create junction table template <-> categories (many-to-many)
CREATE TABLE template_categories (
    template_id INTEGER,
    category_id INTEGER,
    PRIMARY KEY (template_id, category_id),
    FOREIGN KEY (template_id) REFERENCES templates(id) ON DELETE CASCADE,
    FOREIGN KEY (category_id) REFERENCES categories(id) ON DELETE CASCADE
);

-- Create junction table collection <-> workflows (many-to-many)
CREATE TABLE collection_workflows (
    collection_id INTEGER,
    template_id INTEGER,
    PRIMARY KEY (collection_id, template_id),
    FOREIGN KEY (collection_id) REFERENCES collections(id) ON DELETE CASCADE,
    FOREIGN KEY (template_id) REFERENCES templates(id) ON DELETE CASCADE
);

-- Create junction table collection <-> categories (many-to-many)
CREATE TABLE collection_categories (
    collection_id INTEGER,
    category_id INTEGER,
    PRIMARY KEY (collection_id, category_id),
    FOREIGN KEY (collection_id) REFERENCES collections(id) ON DELETE CASCADE,
    FOREIGN KEY (category_id) REFERENCES categories(id) ON DELETE CASCADE
);

-- Enable foreign key constraints (again to ensure it's active)
PRAGMA foreign_keys = ON;

-- Insert default user
INSERT INTO users (id, name, username, bio, verified, links, avatar) VALUES 
(1, 'API User', 'api_user', 'Default API user', 0, '[]', 'https://gravatar.com/avatar/default');