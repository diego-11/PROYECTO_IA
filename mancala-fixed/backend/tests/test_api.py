"""
Unit tests for the FastAPI backend (schema validation, endpoint logic).
Motor is mocked so these run without the C++ binary.
"""
import pytest
from fastapi.testclient import TestClient
from unittest.mock import AsyncMock, patch
import json
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from app.main import app

client = TestClient(app)

VALID_BOARD = [4,4,4,4,4,4,0,4,4,4,4,4,4,0]

MOTOR_RESPONSE_AB = {
    "move": 3,
    "evaluation": 7,
    "elapsed_ms": 124,
    "threads_used": 1,
    "stats": {"algo": "alphabeta", "nodes": 18738, "prunes": 4980},
}

MOTOR_RESPONSE_MCTS = {
    "move": 3,
    "evaluation": 0.62,
    "elapsed_ms": 118,
    "threads_used": 1,
    "stats": {
        "algo": "mcts",
        "rollouts": 10000,
        "tree_depth_avg": 5.3,
        "win_rate": 0.62,
    },
}


def make_mock_response(data):
    class FakeResponse:
        status_code = 200
        def json(self): return data
    return FakeResponse()


# ── Health endpoints ──────────────────────────────────────────────────────────

def test_healthz():
    r = client.get("/healthz")
    assert r.status_code == 200
    assert r.json()["status"] == "ok"


# ── Schema validation ─────────────────────────────────────────────────────────

def test_move_wrong_board_length():
    payload = {"board": [1,2,3], "side": 0, "algo": "alphabeta", "depth": 6}
    r = client.post("/move", json=payload)
    assert r.status_code == 422


def test_move_negative_seeds():
    board = [-1] + [4]*13
    payload = {"board": board, "side": 0, "algo": "alphabeta", "depth": 6}
    r = client.post("/move", json=payload)
    assert r.status_code == 422


def test_move_invalid_algo():
    payload = {"board": VALID_BOARD, "side": 0, "algo": "invalid"}
    r = client.post("/move", json=payload)
    assert r.status_code == 422


def test_move_invalid_side():
    payload = {"board": VALID_BOARD, "side": 2, "algo": "alphabeta", "depth": 6}
    r = client.post("/move", json=payload)
    assert r.status_code == 422


# ── Move endpoint (mocked motor) ──────────────────────────────────────────────

@patch("app.main.httpx.AsyncClient")
def test_move_alphabeta(mock_client_cls):
    mock_client = AsyncMock()
    mock_client.__aenter__ = AsyncMock(return_value=mock_client)
    mock_client.__aexit__ = AsyncMock(return_value=False)
    mock_client.post = AsyncMock(return_value=make_mock_response(MOTOR_RESPONSE_AB))
    mock_client_cls.return_value = mock_client

    payload = {"board": VALID_BOARD, "side": 0, "algo": "alphabeta", "depth": 8}
    r = client.post("/move", json=payload)
    assert r.status_code == 200
    data = r.json()
    assert data["move"] == 3
    assert data["stats"]["algo"] == "alphabeta"


@patch("app.main.httpx.AsyncClient")
def test_move_mcts(mock_client_cls):
    mock_client = AsyncMock()
    mock_client.__aenter__ = AsyncMock(return_value=mock_client)
    mock_client.__aexit__ = AsyncMock(return_value=False)
    mock_client.post = AsyncMock(return_value=make_mock_response(MOTOR_RESPONSE_MCTS))
    mock_client_cls.return_value = mock_client

    payload = {"board": VALID_BOARD, "side": 0, "algo": "mcts", "simulations": 10000}
    r = client.post("/move", json=payload)
    assert r.status_code == 200
    data = r.json()
    assert data["stats"]["algo"] == "mcts"
    assert "rollouts" in data["stats"]


@patch("app.main.httpx.AsyncClient")
def test_motor_unavailable(mock_client_cls):
    import httpx as _httpx
    mock_client = AsyncMock()
    mock_client.__aenter__ = AsyncMock(return_value=mock_client)
    mock_client.__aexit__ = AsyncMock(return_value=False)
    mock_client.post = AsyncMock(side_effect=_httpx.ConnectError("refused"))
    mock_client_cls.return_value = mock_client

    payload = {"board": VALID_BOARD, "side": 0, "algo": "alphabeta", "depth": 8}
    r = client.post("/move", json=payload)
    assert r.status_code == 503
