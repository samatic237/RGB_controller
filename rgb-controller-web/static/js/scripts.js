// Управление яркостью
function setBrightness() {
    const brightness = document.getElementById('brightness-slider').value;
    fetch('/set_brightness', {
        method: 'POST',
        body: new URLSearchParams({ brightness })
    })
    .then(response => response.json())
    .then(data => alert(data.message));
}

// Установка цвета
function setColor(type) {
    const color = document.getElementById('color-picker').value.slice(1);
    
    if (type === 'all') {
        fetch('/set_color', {
            method: 'POST',
            body: new URLSearchParams({ color, pixel: 'all' })
        });
    } else {
        const pixel = document.getElementById('pixel-input').value;
        fetch('/set_color', {
            method: 'POST',
            body: new URLSearchParams({ color, pixel })
        });
    }
}

// Установка режима
function setMode() {
    const mode = document.getElementById('mode-select').value;
    fetch('/set_mode', {
        method: 'POST',
        body: new URLSearchParams({ mode })
    });
}

// Обновление дисплея
function updateDisplay() {
    const text = document.getElementById('display-text').value;
    fetch('/update_display', {
        method: 'POST',
        body: new URLSearchParams({ text })
    })
    .then(() => alert('Дисплей обновлен!'));
}

// Установка режима дисплея
function setDisplayMode() {
    const mode = document.getElementById('display-mode').value;
    fetch('/set_display_mode', {
        method: 'POST',
        body: new URLSearchParams({ mode })
    });
}

// Показать часы
function showClock() {
    // Реализация отправки команды для отображения часов
    // с использованием символьной графики
}

// Добавление цветовой пары в паттерн
function addColorPair() {
    const container = document.querySelector('.color-sequence');
    const newPair = document.createElement('div');
    newPair.className = 'color-pair';
    newPair.innerHTML = `
        <input type="color" class="color-input">
        <input type="number" min="1" max="100" value="10" class="duration-input" placeholder="Длительность">
    `;
    container.appendChild(newPair);
}

// Сохранение паттерна
function savePattern() {
    const name = document.getElementById('pattern-name').value;
    const colors = [];
    
    document.querySelectorAll('.color-pair').forEach(pair => {
        const color = pair.querySelector('.color-input').value.slice(1);
        const duration = pair.querySelector('.duration-input').value;
        colors.push({ color, duration });
    });
    
    fetch('/save_pattern', {
        method: 'POST',
        body: JSON.stringify({ name, colors }),
        headers: { 'Content-Type': 'application/json' }
    });
}

// Активация паттерна
function activatePattern(id) {
    fetch('/activate_pattern', {
        method: 'POST',
        body: new URLSearchParams({ id })
    });
}