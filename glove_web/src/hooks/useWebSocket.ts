import { useEffect, useRef, useState, useCallback } from 'react';
import { useSensorStore } from '../stores/useSensorStore';
import { useGestureStore } from '../stores/useGestureStore';
import { useSettingsStore } from '../stores/useSettingsStore';
import { DEFAULT_WS_PORT, MAX_RECONNECT_RETRIES, INITIAL_RECONNECT_DELAY } from '../utils/constants';
import type { RelayMessage, ConnectionStatus } from '../types';

interface UseWebSocketReturn {
  status: ConnectionStatus;
  reconnect: () => void;
  lastMessage: RelayMessage | null;
}

export function useWebSocket(): UseWebSocketReturn {
  const wsRef = useRef<WebSocket | null>(null);
  const retryCountRef = useRef(0);
  const reconnectTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const mountedRef = useRef(true);

  const [status, setStatus] = useState<ConnectionStatus>('disconnected');
  const [lastMessage, setLastMessage] = useState<RelayMessage | null>(null);

  const relayHost = useSettingsStore((s) => s.relayHost);
  const updateFromRelay = useSensorStore((s) => s.updateFromRelay);
  const updateGesture = useGestureStore((s) => s.updateGesture);

  const getWsUrl = useCallback((): string => {
    // Relay's WS endpoint is mounted at /ws (glove_relay main.py @app.websocket("/ws")).
    // Connecting to the bare root (ws://host:port/) returns HTTP 403 — no route there.
    return `ws://${relayHost}:${DEFAULT_WS_PORT}/ws`;
  }, [relayHost]);

  const cleanup = useCallback(() => {
    if (reconnectTimerRef.current) {
      clearTimeout(reconnectTimerRef.current);
      reconnectTimerRef.current = null;
    }
    if (wsRef.current) {
      wsRef.current.onopen = null;
      wsRef.current.onclose = null;
      wsRef.current.onerror = null;
      wsRef.current.onmessage = null;
      if (wsRef.current.readyState === WebSocket.OPEN ||
          wsRef.current.readyState === WebSocket.CONNECTING) {
        wsRef.current.close();
      }
      wsRef.current = null;
    }
  }, []);

  const connect = useCallback(() => {
    cleanup();
    if (!mountedRef.current) return;

    setStatus('connecting');

    try {
      const ws = new WebSocket(getWsUrl());
      wsRef.current = ws;

      ws.onopen = () => {
        if (!mountedRef.current) return;
        setStatus('connected');
        retryCountRef.current = 0;
      };

      ws.onmessage = (event: MessageEvent) => {
        if (!mountedRef.current) return;

        try {
          const data: RelayMessage = JSON.parse(event.data as string);
          setLastMessage(data);
          updateFromRelay(data);
          updateGesture(data);
        } catch {
          console.warn('[WS] Failed to parse message:', event.data);
        }
      };

      ws.onclose = () => {
        if (!mountedRef.current) return;
        setStatus('disconnected');

        if (retryCountRef.current < MAX_RECONNECT_RETRIES) {
          const delay = INITIAL_RECONNECT_DELAY * Math.pow(2, retryCountRef.current);
          retryCountRef.current += 1;
          reconnectTimerRef.current = setTimeout(() => {
            connect();
          }, delay);
        }
      };

      ws.onerror = () => {
        if (!mountedRef.current) return;
        setStatus('error');
      };
    } catch {
      setStatus('error');
    }
  }, [getWsUrl, cleanup, updateFromRelay, updateGesture]);

  const reconnect = useCallback(() => {
    retryCountRef.current = 0;
    connect();
  }, [connect]);

  useEffect(() => {
    mountedRef.current = true;
    retryCountRef.current = 0;
    connect();

    return () => {
      mountedRef.current = false;
      cleanup();
    };
  }, [relayHost]); // eslint-disable-line react-hooks/exhaustive-deps

  return { status, reconnect, lastMessage };
}
