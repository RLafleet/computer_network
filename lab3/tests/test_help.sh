#!/bin/bash
# Тест команды справки

echo "=== Тест команды справки ==="
cd /mnt/c/Projects/computer_network/lab3

echo "Запуск: ./smtp_client --help"
echo "Ожидается: Отобразить информацию об использовании"
echo ""

./smtp_client --help

echo ""
echo "=== Тест справки завершен ==="