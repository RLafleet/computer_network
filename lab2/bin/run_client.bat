@echo off
setlocal

echo Testing HTTP server connectivity...
echo.

echo Sending GET request to root path:
curl -v http://localhost:8080/
echo.

echo Sending GET request for style.css:
curl -v http://localhost:8080/style.css
echo.

echo Testing 404 error:
curl -v http://localhost:8080/nonexistent.html
echo.

endlocal