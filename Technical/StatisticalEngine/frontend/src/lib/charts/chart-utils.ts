/*
 * Shared chart helpers — formatters, time conversion, era-color mapping.
 *
 * Time model: the engine emits `totalDay` (integer trading day since
 * game start). lightweight-charts needs monotonic Time values. We map
 * totalDay -> UTCTimestamp by treating each trading day as one calendar
 * day starting on 1945-01-01, then format ticks back to "1947 Q2" for
 * display.
 *
 * Fixes:
 *   - top_20 #1: capital Y-axis shows $10.5B not 10467384616
 *   - top_20 #5: X-axis shows year+quarter not raw integers
 *   - top_20 #9: era colors keyed by canonical engine names
 *   - top_20 #13: watermark hidden
 */

import type { UTCTimestamp } from 'lightweight-charts';
import type { EraName } from '../types/server';

// 252 trading days per year (engine convention), 63 per quarter.
const TRADING_DAYS_PER_YEAR = 252;
const TRADING_DAYS_PER_QUARTER = 63;
const GAME_START_YEAR = 1945;
const EPOCH_SECONDS_1945_01_01 = Date.UTC(1945, 0, 1) / 1000;

export function totalDayToTime(totalDay: number): UTCTimestamp {
  // Each trading day = one calendar day in chart time. Linear, monotonic.
  return (EPOCH_SECONDS_1945_01_01 + totalDay * 86400) as UTCTimestamp;
}

export function timeToYearQuarter(time: number): { year: number; quarter: number } {
  const daysSinceEpoch = Math.floor((time - EPOCH_SECONDS_1945_01_01) / 86400);
  const year = GAME_START_YEAR + Math.floor(daysSinceEpoch / TRADING_DAYS_PER_YEAR);
  const quarter = Math.floor((daysSinceEpoch % TRADING_DAYS_PER_YEAR) / TRADING_DAYS_PER_QUARTER) + 1;
  return { year, quarter: Math.max(1, Math.min(4, quarter)) };
}

/** "1947 Q2" — used by timeScale.tickMarkFormatter. */
export function tickFormatYearQuarter(time: number): string {
  const yq = timeToYearQuarter(time);
  return `${yq.year} Q${yq.quarter}`;
}

/** "$10.5B" / "$500M" / "$1.2T" — kills the 10467384616 bug. */
export function formatBigDollar(price: number): string {
  const abs = Math.abs(price);
  const sign = price < 0 ? '-' : '';
  if (abs >= 1e12) return `${sign}$${(abs / 1e12).toFixed(2)}T`;
  if (abs >= 1e9)  return `${sign}$${(abs / 1e9).toFixed(2)}B`;
  if (abs >= 1e6)  return `${sign}$${(abs / 1e6).toFixed(0)}M`;
  if (abs >= 1e3)  return `${sign}$${(abs / 1e3).toFixed(0)}K`;
  return `${sign}$${abs.toFixed(0)}`;
}

/** Generic small-value formatter for market index prices (4 decimals when small). */
export function formatMarketPrice(price: number): string {
  const abs = Math.abs(price);
  if (abs >= 1000) return price.toFixed(0);
  if (abs >= 100)  return price.toFixed(2);
  if (abs >= 10)   return price.toFixed(3);
  return price.toFixed(4);
}

// ── Era-keyed color map (canonical names from EraEngine.h) ─────────────

export interface ChartEraColors {
  background: string;
  text: string;
  grid: string;
  border: string;
  upColor: string;
  downColor: string;
  lineColor: string;
}

const ERA_COLORS: Record<EraName, ChartEraColors> = {
  'Post-War Stability': {
    background: '#16120c', text: '#9a8d70', grid: '#1c1610',
    border: '#2a2218', upColor: '#6b8e4e', downColor: '#a8453a',
    lineColor: '#c4a35a',
  },
  'Expansion & Innovation': {
    background: '#102019', text: '#8aaa9c', grid: '#16291f',
    border: '#1d3a2e', upColor: '#2dd4bf', downColor: '#d97441',
    lineColor: '#2dd4bf',
  },
  'Volatility & Deregulation': {
    background: '#1f1810', text: '#b08a60', grid: '#2a2018',
    border: '#3a2818', upColor: '#f97316', downColor: '#c14040',
    lineColor: '#f97316',
  },
  'Big Bang & Excess': {
    background: '#0e1a2e', text: '#8aa0c0', grid: '#14233c',
    border: '#1f3458', upColor: '#3b82f6', downColor: '#ffd166',
    lineColor: '#3b82f6',
  },
  'Shadow Banking & Hubris': {
    background: '#0a140a', text: '#6aa66a', grid: '#102010',
    border: '#1a2a1a', upColor: '#22c55e', downColor: '#ff4040',
    lineColor: '#22c55e',
  },
  'Crisis & Reform': {
    background: '#1a0e0e', text: '#b08080', grid: '#261414',
    border: '#3a1c1c', upColor: '#22c55e', downColor: '#ef4444',
    lineColor: '#ef4444',
  },
  'Modern & Endgame': {
    background: '#120a1e', text: '#a090d0', grid: '#1c1030',
    border: '#2a1850', upColor: '#50f0ff', downColor: '#fbbf24',
    lineColor: '#a855f7',
  },
};

const DEFAULT_COLORS: ChartEraColors = ERA_COLORS['Post-War Stability'];

export function eraChartColors(eraName: string | undefined): ChartEraColors {
  if (eraName && eraName in ERA_COLORS) return ERA_COLORS[eraName as EraName];
  return DEFAULT_COLORS;
}

// ── Common chart options ───────────────────────────────────────────────

export function baseChartOptions(colors: ChartEraColors) {
  return {
    layout: {
      background: { color: colors.background },
      textColor: colors.text,
      fontFamily: 'JetBrains Mono, IBM Plex Mono, monospace',
    },
    grid: {
      vertLines: { color: colors.grid },
      horzLines: { color: colors.grid },
    },
    timeScale: {
      timeVisible: false,
      secondsVisible: false,
      borderColor: colors.border,
      tickMarkFormatter: tickFormatYearQuarter,
    },
    rightPriceScale: {
      borderColor: colors.border,
      autoScale: true,
    },
    crosshair: {
      mode: 1,
    },
    // top_20 #13 — kill TV watermark
    watermark: { visible: false },
    autoSize: true,
  } as const;
}
