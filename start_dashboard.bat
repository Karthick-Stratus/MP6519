@echo off
echo Installing Python dependencies for MP6519 Dashboard...
pip install -r requirements.txt
echo.
echo Launching Dashboard...
python dashboard.py
pause
