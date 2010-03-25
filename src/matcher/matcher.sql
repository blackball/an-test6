CREATE TABLE tweaks (
    id               INTEGER PRIMARY KEY,
    url              TEXT,
    model            TEXT,
    wcs              TEXT,
    width            REAL,
    height           REAL);

CREATE TABLE catalog (
    id               INTEGER PRIMARY KEY,
    tweak_id         INTEGER,
    a                REAL,
    d                REAL);

CREATE INDEX idx_catalog ON catalog(tweak_id);

CREATE TABLE projected_catalog (
    catalog_id       INTEGER PRIMARY KEY ON CONFLICT REPLACE,
    tweak_id         INTEGER,
    x                REAL,
    y                REAL);

CREATE INDEX idx_projected_catalog ON projected_catalog(tweak_id);

CREATE TABLE image (
    id               INTEGER PRIMARY KEY,
    tweak_id         INTEGER,
    x                REAL,
    y                REAL);

CREATE INDEX idx_image ON image(tweak_id);

CREATE TABLE matches (
    id               INTEGER PRIMARY KEY,
    tweak_id         INTEGER,
    catalog_id       INTEGER,
    image_id         INTEGER);
