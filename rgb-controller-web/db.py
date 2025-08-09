import sqlite3
import os

DB_NAME = 'sensors.db'

def init_db():
    with sqlite3.connect(DB_NAME) as conn:
        c = conn.cursor()
        # Таблица настроек
        c.execute('''CREATE TABLE IF NOT EXISTS settings (
                     id INTEGER PRIMARY KEY,
                     name TEXT UNIQUE,
                     value TEXT)''')
        
        # Таблица текста дисплея
        c.execute('''CREATE TABLE IF NOT EXISTS display_text (
                     line INTEGER PRIMARY KEY,
                     content TEXT)''')
        
        # Таблица паттернов
        c.execute('''CREATE TABLE IF NOT EXISTS patterns (
                     id INTEGER PRIMARY KEY AUTOINCREMENT,
                     name TEXT,
                     params TEXT)''')
        conn.commit()

def get_setting(name, default=None):
    with sqlite3.connect(DB_NAME) as conn:
        c = conn.cursor()
        c.execute("SELECT value FROM settings WHERE name=?", (name,))
        result = c.fetchone()
        return result[0] if result else default

def set_setting(name, value):
    with sqlite3.connect(DB_NAME) as conn:
        c = conn.cursor()
        c.execute("INSERT OR REPLACE INTO settings (name, value) VALUES (?, ?)", 
                 (name, value))
        conn.commit()

def save_display_text(line, content):
    with sqlite3.connect(DB_NAME) as conn:
        c = conn.cursor()
        c.execute("INSERT OR REPLACE INTO display_text (line, content) VALUES (?, ?)", 
                 (line, content))
        conn.commit()

def get_display_text():
    with sqlite3.connect(DB_NAME) as conn:
        c = conn.cursor()
        c.execute("SELECT content FROM display_text ORDER BY line")
        return [row[0] for row in c.fetchall()] or [''] * 4

def save_pattern(name, params):
    with sqlite3.connect(DB_NAME) as conn:
        c = conn.cursor()
        c.execute("INSERT INTO patterns (name, params) VALUES (?, ?)",
                 (name, str(params)))
        conn.commit()

def get_patterns():
    with sqlite3.connect(DB_NAME) as conn:
        c = conn.cursor()
        c.execute("SELECT id, name, params FROM patterns")
        return [{'id': row[0], 'name': row[1], 'params': eval(row[2])} for row in c.fetchall()]