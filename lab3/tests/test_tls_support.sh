#!/bin/bash
# Тест поддержки TLS

echo "=== Тест поддержки TLS ==="
cd /mnt/c/Projects/computer_network/lab3

# Создаем тестовый файл
echo "Тестовое сообщение для проверки TLS" > tls_test.txt

# Пробуем использовать флаг TLS (не должно показывать "TLS support not compiled in")
echo "Тестирование использования флага TLS..."
./smtp_client -s test.server.com -p 587 --tls -f sender@test.com -t recipient@test.com -j "TLS Test" -b tls_test.txt 2>&1 | grep -i "tls support not compiled" > /dev/null

if [ $? -eq 1 ]; then
    echo "✅ Поддержка TLS правильно скомпилирована"
else
    echo "❌ Поддержка TLS не скомпилирована"
fi

# Очистка
rm -f tls_test.txt

echo ""
echo "=== Тест поддержки TLS завершен ==="