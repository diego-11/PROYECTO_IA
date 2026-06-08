import os
import httpx
import time
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, field_validator, model_validator
from typing import Optional, List, Literal
from prometheus_client import Counter, Histogram, generate_latest
from fastapi.responses import Response
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# ── App ───────────────────────────────────────────────────────────────────────
app = FastAPI(
    title="Mancala Kalah API",
    description="Backend wrapper for C++ Mancala engine (Alpha-Beta + MCTS)",
    version="1.0.0",
)

# ── CORS ──────────────────────────────────────────────────────────────────────
ALLOWED_ORIGINS = os.getenv(
    "CORS_ORIGINS",
    "http://localhost:8080,http://127.0.0.1:8080"
).split(",")

app.add_middleware(
    CORSMiddleware,
    allow_origins=ALLOWED_ORIGINS,
    allow_methods=["GET", "POST", "OPTIONS"],
    allow_headers=["Content-Type"],
)

# ── Motor URL ─────────────────────────────────────────────────────────────────
MOTOR_URL = os.getenv("MOTOR_URL", "http://motor:8001")

# ── Metrics ───────────────────────────────────────────────────────────────────
REQUEST_COUNT = Counter(
    "mancala_requests_total", "Total /move requests", ["algo", "status"]
)
REQUEST_LATENCY = Histogram(
    "mancala_request_duration_seconds", "Latency of /move", ["algo"]
)

# ── Schemas ───────────────────────────────────────────────────────────────────
class MoveRequest(BaseModel):
    board: List[int]
    side: Literal[0, 1]
    algo: Literal["alphabeta", "mcts"] = "alphabeta"
    depth: Optional[int] = None
    simulations: Optional[int] = None
    threads: int = 1
    leaf_parallel: bool = False

    @field_validator("board")
    @classmethod
    def validate_board(cls, v):
        if len(v) != 14:
            raise ValueError("board must have exactly 14 integers")
        if any(x < 0 for x in v):
            raise ValueError("all board values must be non-negative")
        total = sum(v)
        if total == 0:
            raise ValueError("board has no seeds")
        return v

    @field_validator("threads")
    @classmethod
    def validate_threads(cls, v):
        if v < 1 or v > 64:
            raise ValueError("threads must be between 1 and 64")
        return v

    @model_validator(mode="after")
    def set_defaults(self):
        if self.algo == "alphabeta" and self.depth is None:
            self.depth = 8
        if self.algo == "mcts" and self.simulations is None:
            self.simulations = 10000
        return self


class MoveResponse(BaseModel):
    move: int
    evaluation: float
    elapsed_ms: int
    threads_used: int
    stats: dict


# ── Routes ────────────────────────────────────────────────────────────────────
@app.get("/healthz")
async def healthz():
    return {"status": "ok"}


@app.get("/readyz")
async def readyz():
    try:
        async with httpx.AsyncClient(timeout=3.0) as client:
            r = await client.get(f"{MOTOR_URL}/healthz")
            if r.status_code == 200:
                return {"status": "ready", "motor": "up"}
    except Exception as e:
        logger.warning(f"Motor not reachable: {e}")
    raise HTTPException(status_code=503, detail="Motor not available")


@app.get("/metrics")
async def metrics():
    return Response(content=generate_latest(), media_type="text/plain; charset=utf-8")


@app.post("/move", response_model=MoveResponse)
async def move(req: MoveRequest):
    payload = {
        "board":        req.board,
        "side":         req.side,
        "algo":         req.algo,
        "threads":      req.threads,
        "leaf_parallel": req.leaf_parallel,
    }
    if req.algo == "alphabeta":
        payload["depth"] = req.depth
    else:
        payload["simulations"] = req.simulations

    t0 = time.time()
    try:
        async with httpx.AsyncClient(timeout=120.0) as client:
            r = await client.post(
                f"{MOTOR_URL}/move",
                json=payload,
                headers={"Content-Type": "application/json"},
            )
    except httpx.ConnectError:
        REQUEST_COUNT.labels(algo=req.algo, status="503").inc()
        raise HTTPException(status_code=503, detail="Motor not available")
    except httpx.TimeoutException:
        REQUEST_COUNT.labels(algo=req.algo, status="504").inc()
        raise HTTPException(status_code=504, detail="Motor timed out")
    except Exception as e:
        REQUEST_COUNT.labels(algo=req.algo, status="500").inc()
        raise HTTPException(status_code=500, detail=str(e))

    elapsed = time.time() - t0
    REQUEST_LATENCY.labels(algo=req.algo).observe(elapsed)

    data = r.json()
    if "error" in data:
        REQUEST_COUNT.labels(algo=req.algo, status="500").inc()
        raise HTTPException(status_code=500, detail=data["error"])

    REQUEST_COUNT.labels(algo=req.algo, status="200").inc()
    logger.info(f"move algo={req.algo} side={req.side} → pit={data['move']} in {data['elapsed_ms']}ms")
    return MoveResponse(**data)
