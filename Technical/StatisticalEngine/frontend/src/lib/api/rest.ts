/*
 * REST API wrapper. The Crow server wraps every response in
 *   {success: bool, data?: any, error?: any, timestamp: string}
 * `apiFetch` unwraps that envelope and throws on success=false.
 */

import type { ApiEnvelope, PlayerView, MacroHistoryResponse, TradeBook } from '../types/server';

export class ApiError extends Error {
  constructor(message: string, public status?: number) {
    super(message);
    this.name = 'ApiError';
  }
}

async function apiFetch<T>(path: string, init?: RequestInit): Promise<T> {
  let res: Response;
  try {
    res = await fetch(path, {
      headers: { 'Content-Type': 'application/json', ...(init?.headers ?? {}) },
      ...init,
    });
  } catch (e) {
    throw new ApiError(`Network error: ${(e as Error).message}`);
  }
  let body: ApiEnvelope<T>;
  try {
    body = await res.json() as ApiEnvelope<T>;
  } catch {
    throw new ApiError(`Invalid JSON from ${path}`, res.status);
  }
  if (!res.ok || body.success === false) {
    const msg = body.error ?? `HTTP ${res.status}`;
    throw new ApiError(typeof msg === 'string' ? msg : JSON.stringify(msg), res.status);
  }
  return body.data as T;
}

export interface NewGameOptions {
  seed?: number;
  ceoId?: string;
  startingPosition?: number;
  preset?: 'sandbox' | 'easy' | 'medium' | 'hard' | 'crisis';
  startYear?: number;
}

export interface NewGameData {
  gameId: string;
  config: unknown;
  startingPosition: number;
}

export const api = {
  health: () => apiFetch<{ status: string; games: number }>('/api/health'),

  newGame: (opts: NewGameOptions = {}) =>
    apiFetch<NewGameData>('/api/game/new', {
      method: 'POST',
      body: JSON.stringify(opts),
    }),

  getState: (gameId: string) =>
    apiFetch<PlayerView>(`/api/game/${encodeURIComponent(gameId)}/state`),

  // P3: per-quarter macro snapshots for chart hydration after a load/reload.
  macroHistory: (gameId: string) =>
    apiFetch<MacroHistoryResponse>(`/api/game/${encodeURIComponent(gameId)}/macro-history`),

  submitDecision: (gameId: string, decisionId: string, optionId: string, extra?: object) =>
    apiFetch<PlayerView>(
      `/api/game/${encodeURIComponent(gameId)}/decision/${encodeURIComponent(decisionId)}`,
      { method: 'POST', body: JSON.stringify({ optionId, ...extra }) },
    ),

  respondCrisis: (gameId: string, crisisId: string, responseType: 'aggressive' | 'measured' | 'gamble') =>
    apiFetch<{ crisisId: string; responseType: string; phase: string; actionPoints: { total: number; spent: number } }>(
      `/api/game/${encodeURIComponent(gameId)}/crisis/respond`,
      { method: 'POST', body: JSON.stringify({ crisisId, responseType }) },
    ),

  saveGame: (gameId: string, slot: number | string) =>
    apiFetch<{ slot: string }>(
      `/api/game/${encodeURIComponent(gameId)}/save`,
      { method: 'POST', body: JSON.stringify({ slot }) },
    ),

  loadGame: (gameId: string, slot: number | string) =>
    apiFetch<PlayerView>(
      `/api/game/${encodeURIComponent(gameId)}/load`,
      { method: 'POST', body: JSON.stringify({ slot }) },
    ),

  acceptDeal: (gameId: string, dealId: string, investmentAmount: number) =>
    apiFetch<{ accepted: boolean; dealId: string; amount: number }>(
      `/api/game/${encodeURIComponent(gameId)}/deal/${encodeURIComponent(dealId)}/accept`,
      { method: 'POST', body: JSON.stringify({ investmentAmount }) },
    ),

  hire: (gameId: string, candidateId: string, divisionId: string) =>
    apiFetch<{ hired: boolean; candidateId: string; divisionId: string }>(
      `/api/game/${encodeURIComponent(gameId)}/hire`,
      { method: 'POST', body: JSON.stringify({ candidateId, divisionId }) },
    ),

  // STAR_02 P7: place a market order on the CEO's personal trading account.
  // amount = dollar notional; side = buy | sell. Returns the updated trade book.
  trade: (gameId: string, marketId: string, side: 'buy' | 'sell', amount: number) =>
    apiFetch<{
      traded: boolean; marketId: string; side: string; qtyDelta: number;
      realizedPnl: number; positionQtyAfter: number; tradeBook: TradeBook;
    }>(
      `/api/game/${encodeURIComponent(gameId)}/trade`,
      { method: 'POST', body: JSON.stringify({ marketId, side, amount }) },
    ),

  // STAR_02 P7: hydrate the personal trading account snapshot.
  tradeBook: (gameId: string) =>
    apiFetch<TradeBook>(`/api/game/${encodeURIComponent(gameId)}/trade-book`),

  // STAR_02 P6: accept a poach offer (buy a rival's whole division team).
  poach: (gameId: string, offerId: string) =>
    apiFetch<{ poached: boolean; offerId: string; bankCapital: number }>(
      `/api/game/${encodeURIComponent(gameId)}/poach`,
      { method: 'POST', body: JSON.stringify({ offerId }) },
    ),

  logTelemetry: (sessionId: string, events: unknown[]) =>
    fetch('/api/telemetry', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ sessionId, events }),
      keepalive: true,
    }).catch(() => { /* telemetry is best-effort */ }),

  listCeos: () => apiFetch<unknown[]>('/api/ceos'),
  listScenarios: () => apiFetch<unknown[]>('/api/scenarios'),
  listStartingPositions: () => apiFetch<unknown[]>('/api/starting-positions'),
};
