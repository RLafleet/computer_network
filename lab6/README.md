# Лабораторная работа 6 — UDP Pinger

## Сборка

```bash
make
```

## Запуск сервера

```bash
./udp_pinger_server 12000 -loss 30
```

Опции сервера:

- `-loss N` — вероятность потери пакетов в процентах (0..100)
- `-hb-timeout N` — таймаут неактивности клиента в секундах (по умолчанию 5)
- `-d` — подробный вывод

## Запуск клиента

```bash
./udp_pinger_client localhost 12000
./udp_pinger_client localhost 12000 -d
./udp_pinger_client localhost 12000 -stats
./udp_pinger_client localhost 12000 -stats -hb
```

Опции клиента:

- `-d` — подробный вывод отладки
- `-stats` — вывод статистики после 10 запросов
- `-hb` — включить heartbeat после завершения ping
- `-hb-interval N` — интервал heartbeat в секундах (по умолчанию 2)
