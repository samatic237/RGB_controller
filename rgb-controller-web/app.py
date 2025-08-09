from flask import Flask, render_template, request, jsonify, redirect, url_for
import serial
import sqlite3
from threading import Lock
from datetime import datetime
import time
from config import SERIAL_PORT, SERIAL_BAUDRATE
from db import init_db, get_setting, set_setting, save_display_text, get_display_text, save_pattern, get_patterns

app = Flask(__name__)
serial_lock = Lock()

# Инициализация базы данных
init_db()

def send_command(command):
    with serial_lock:
        try:
            with serial.Serial(SERIAL_PORT, SERIAL_BAUDRATE, timeout=1) as ser:
                ser.write(f"{command}\n".encode('utf-8'))
                time.sleep(0.1)
                response = ser.readline().decode('utf-8').strip()
                return response
        except Exception as e:
            return f"Error: {str(e)}"

@app.route('/')
def index():
    brightness = get_setting('brightness', 255)
    current_mode = get_setting('current_mode', 0)
    return render_template('index.html', 
                           brightness=brightness, 
                           current_mode=current_mode)

@app.route('/set_brightness', methods=['POST'])
def set_brightness():
    brightness = request.form['brightness']
    response = send_command(f"ch_br {brightness}")
    set_setting('brightness', brightness)
    return jsonify({'status': 'success', 'message': response})

@app.route('/set_color', methods=['POST'])
def set_color():
    pixel = request.form.get('pixel', 'all')
    color = request.form['color'].lstrip('#')
    
    if pixel == 'all':
        response = send_command(f"ch_all 0x{color}")
    else:
        response = send_command(f"ch_color {pixel} 0x{color}")
    
    return jsonify({'status': 'success', 'message': response})

@app.route('/set_mode', methods=['POST'])
def set_mode():
    mode = request.form['mode']
    response = send_command(f"ch_mode {mode}")
    set_setting('current_mode', mode)
    return jsonify({'status': 'success', 'message': response})

@app.route('/display')
def display_control():
    lines = get_display_text()
    lcd_mode = get_setting('lcd_mode', 1)
    return render_template('display.html', lines=lines, lcd_mode=lcd_mode)

@app.route('/update_display', methods=['POST'])
def update_display():
    text = request.form['text']
    lines = split_text(text)
    
    for i, line in enumerate(lines[:4]):
        save_display_text(i, line[:20])
        send_command(f'lcd_print {i+1} "{line[:20]}"')
    
    return redirect(url_for('display_control'))

@app.route('/set_display_mode', methods=['POST'])
def set_display_mode():
    mode = request.form['mode']
    send_command(f"lcd_mode {mode}")
    set_setting('lcd_mode', mode)
    return jsonify({'status': 'success'})

@app.route('/sensors')
def sensors():
    return render_template('sensors.html')

@app.route('/get_sensor_data')
def get_sensor_data():
    # Для реального использования лучше добавить команду в прошивку
    # которая возвращает структурированные данные
    temp = send_command("view_dht11")
    co2 = send_command("mq135_view")
    return jsonify({'temperature': temp, 'co2': co2})

@app.route('/patterns')
def patterns():
    patterns = get_patterns()
    return render_template('patterns.html', patterns=patterns)

@app.route('/save_pattern', methods=['POST'])
def save_pattern():
    name = request.form['name']
    colors = request.form.getlist('colors[]')
    params = {
        'speed': request.form['speed'],
        'colors': ','.join(colors)
    }
    save_pattern(name, params)
    return jsonify({'status': 'success'})

@app.route('/activate_pattern', methods=['POST'])
def activate_pattern():
    pattern_id = request.form['id']
    pattern = get_pattern(pattern_id)
    # Реализация активации паттерна
    return jsonify({'status': 'success'})

def split_text(text, max_line=20):
    words = text.split()
    lines = []
    current_line = ""
    
    for word in words:
        if len(current_line) + len(word) + 1 <= max_line:
            current_line += " " + word if current_line else word
        else:
            lines.append(current_line)
            current_line = word
    
    if current_line:
        lines.append(current_line)
    
    return lines[:4]  # Максимум 4 строки

if __name__ == '__main__':
    app.run(debug=True)