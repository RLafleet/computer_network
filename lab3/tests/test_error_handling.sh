#!/bin/bash
# Тест обработки ошибок

echo "=== Тест обработки ошибок ==="
cd /mnt/c/Projects/computer_network/lab3

echo "Запуск: ./smtp_client -s nonexistent.server.com -p 25 -f sender@test.com -t recipient@test.com -j 'Тест ошибок' -b 'Тестирование обработки ошибок'"
echo "Ожидается: Показать соответствующее сообщение об ошибке сети"
echo ""

./smtp_client -s nonexistent.server.com -p 25 -f sender@test.com -t recipient@test.com -j 'Тест ошибок' -b 'Тестирование обработки ошибок'

echo ""
echo "=== Тест обработки ошибок завершен ==="