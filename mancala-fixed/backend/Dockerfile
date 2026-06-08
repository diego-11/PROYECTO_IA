FROM python:3.12-slim

# wget needed by docker-compose healthcheck
RUN apt-get update && apt-get install -y --no-install-recommends wget \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY app/ ./app/
COPY tests/ ./tests/

RUN pip install --no-cache-dir pytest pytest-asyncio anyio \
    && python -m pytest tests/ -v --tb=short

EXPOSE 8000

ENV MOTOR_URL=http://motor:8001
ENV CORS_ORIGINS=http://localhost:8080,http://127.0.0.1:8080

CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8000"]
