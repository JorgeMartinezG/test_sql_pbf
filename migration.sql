CREATE DATABASE run_tests;

\connect run_tests;

CREATE SCHEMA IF NOT EXISTS osm;
CREATE EXTENSION postgis;

CREATE TABLE IF NOT EXISTS osm.points (
    id bigint NOT NULL,
    geom public.geometry(POINT,4326)
);

CREATE TABLE IF NOT EXISTS osm.lines (
    id bigint NOT NULL,
    geom public.geometry(LINESTRING,4326)
);

CREATE TABLE IF NOT EXISTS osm.polygons (
    id bigint NOT NULL,
    geom public.geometry(POLYGON,4326)
);