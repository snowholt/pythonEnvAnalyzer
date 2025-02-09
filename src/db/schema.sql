-- Packages table
CREATE TABLE IF NOT EXISTS packages (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    version TEXT NOT NULL,
    size INTEGER,
    has_conflicts BOOLEAN DEFAULT FALSE,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(name, version)
);

-- Dependencies table with version constraints
CREATE TABLE IF NOT EXISTS dependencies (
    package_id INTEGER,
    dependency_name TEXT NOT NULL,
    version_constraint TEXT NOT NULL DEFAULT '*',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(package_id) REFERENCES packages(id) ON DELETE CASCADE,
    PRIMARY KEY(package_id, dependency_name)
);

-- Environment settings table
CREATE TABLE IF NOT EXISTS env_settings (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL,
    description TEXT,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Default environment settings
INSERT OR IGNORE INTO env_settings (key, value, description) VALUES
    ('python_version', '', 'Python interpreter version'),
    ('venv_path', '', 'Virtual environment path'),
    ('last_scan', '', 'Timestamp of last environment scan'),
    ('scan_interval', '86400', 'Scan interval in seconds');

-- Indexes for better query performance
CREATE INDEX IF NOT EXISTS idx_packages_name ON packages(name);
CREATE INDEX IF NOT EXISTS idx_dependencies_name ON dependencies(dependency_name);