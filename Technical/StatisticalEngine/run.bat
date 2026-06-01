@echo off
title STVG: Banking CEO Simulator
echo Starting STVG server on http://localhost:8080/
echo Press Ctrl+C to stop.
echo.
start "" http://localhost:8080/
stvg_server.exe 8080 static
