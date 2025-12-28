# Лабораторная работа 7 — HTTP Proxy с кэшем

## Сборка

```bash
make
```

## Запуск

```bash
./proxy_server 8888 -cache_dir ./cache -d
```

Опции:

- `-cache_dir путь` — каталог для кэша (по умолчанию `./cache`)
- `-d` — режим отладки (подробные логи)

## Использование

### Настройка браузера

- HTTP Proxy: `localhost`
- Порт: `8888`

### Пример через curl

```bash
curl -v -x http://localhost:8888 http://www.example.com/
```