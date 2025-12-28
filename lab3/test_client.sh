#!/bin/bash
# Тестовый скрипт для улучшенного SMTP клиента

echo "=== Тестовый скрипт улучшенного SMTP клиента ==="
echo ""

echo "Проверка наличия библиотек разработки OpenSSL..."
if dpkg -l | grep -q libssl-dev; then
    echo "✓ Библиотеки разработки OpenSSL найдены"
else
    echo "⚠ Библиотеки разработки OpenSSL не найдены"
    echo "  Установите командой: sudo apt-get install libssl-dev"
fi

echo ""

echo "Компиляция SMTP клиента..."
make clean
if make; then
    echo "✓ Компиляция успешна"
else
    echo "✗ Ошибка компиляции"
    exit 1
fi

echo ""

echo "Отображение справочной информации:"
./smtp_client --help

echo ""
echo "=== Примеры команд (не выполняются) ==="
echo ""

echo "# Простое письмо"
echo "./smtp_client -s smtp.mail.ru -p 25 -f sender@mail.ru -t recipient@mail.ru -j \"Тест\" -b \"Привет\""
echo ""

echo "# Защищенное письмо с аутентификацией"
echo "./smtp_client -s smtp.gmail.com -p 587 --tls -l user@gmail.com -w \"пароль\" -f user@gmail.com -t recipient@mail.ru -j \"TLS тест\" -b \"Защищенное сообщение\""
echo ""

echo "# Письмо с несколькими вложениями"
echo "./smtp_client -s smtp.yandex.ru -p 587 --tls -l user@yandex.ru -w \"пароль\" -f user@yandex.ru -t recipient@mail.ru -j \"Файлы\" -b \"Смотрите вложения\" -a document.pdf -a photo.jpg -a data.txt"
echo ""

echo "=== Тестирование завершено ==="