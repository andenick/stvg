/*
 * REST API wrapper. The Crow server wraps every response in
 *   {success: bool, data?: any, error?: any, timestamp: string}
 * `apiFetch` unwraps that envelope and throws on success=false.
 */

import type { ApiEnvelope, PlayerView } from '../types/server';

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

  listCeos: () => apiFetch<unknown[]>('/api/ceos'),
  listScenarios: () => apiFetch<unknown[]>('/api/scenarios'),
  listStartingPositions: () => apiFetch<unknown[]>('/api/starting-positions'),
};
