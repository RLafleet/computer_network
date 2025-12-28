#!/bin/bash
# Тест длинных форм командной строки

echo "=== Тест длинных форм опций ==="
cd /mnt/c/Projects/computer_network/lab3

echo "Запуск: ./smtp_client --server localhost --port 1025 --from sender@test.com --to recipient@test.com --subject 'Тест длинных опций' --body 'Тестирование длинных опций' --verbose"
echo "Ожидается: Работать так же, как и с короткими формами опций"
echo ""

./smtp_client --server localhost --port 1025 --from sender@test.com --to recipient@test.com --subject 'Тест длинных опций' --body 'Тестирование длинных опций' --verbose

echo ""
echo "=== Тест длинных опций завершен ==="