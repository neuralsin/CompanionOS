#!/usr/bin/env python3
"""
═══════════════════════════════════════════════════════════
  STOCK MANAGER — V6 CompanionOS Desktop GUI

  Full-featured Tkinter application for:
  • Watchlist management (add/remove/reorder tickers)
  • Live price charts (1D, 1W, 1M, 3M, 1Y)
  • Portfolio tracking with P&L
  • Trade record logging
  • ESP8266 sync via UDP

  Uses yfinance for data — no API key required.
═══════════════════════════════════════════════════════════
"""

import os
import sys
import json
import time
import socket
import threading
from datetime import datetime
from typing import List, Dict, Any, Optional

import tkinter as tk
from tkinter import ttk, messagebox, simpledialog

try:
    import yfinance as yf
except ImportError:
    print("Install yfinance: pip install yfinance")
    sys.exit(1)

try:
    import matplotlib
    matplotlib.use('TkAgg')
    import matplotlib.pyplot as plt
    from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
    from matplotlib.figure import Figure
    import matplotlib.dates as mdates
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("⚠️ matplotlib not found. Charts disabled. pip install matplotlib")

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
CONFIG_FILE = os.path.join(SCRIPT_DIR, 'config.json')
RECORDS_FILE = os.path.join(SCRIPT_DIR, 'stock_records.json')
PORTFOLIO_FILE = os.path.join(SCRIPT_DIR, 'stock_portfolio.json')

# ═══════════════════════════════════════════════════════════
# Dark Theme Colors (matches CompanionOS aesthetic)
# ═══════════════════════════════════════════════════════════
DARK_BG = '#0d1117'
DARK_SURFACE = '#161b22'
DARK_CARD = '#1c2128'
DARK_BORDER = '#30363d'
DARK_TEXT = '#e6edf3'
DARK_DIM = '#7d8590'
ACCENT_GREEN = '#2ea043'
ACCENT_RED = '#da3633'
ACCENT_BLUE = '#2f81f7'
ACCENT_CYAN = '#39d2c0'
ACCENT_PURPLE = '#a371f7'


class StockManager:
    """Core data engine for stock tracking."""

    def __init__(self):
        self.primary_ticker = 'NVDA'
        self.watchlist: List[str] = ['AAPL', 'MSFT', 'TSLA']
        self.portfolio: List[Dict[str, Any]] = []
        self.trade_records: List[Dict[str, Any]] = []
        self.cache: Dict[str, Dict] = {}
        self.cache_expiry: Dict[str, float] = {}
        self._load_config()
        self._load_records()

    def _load_config(self):
        if os.path.exists(CONFIG_FILE):
            with open(CONFIG_FILE) as f:
                cfg = json.load(f)
            stocks = cfg.get('stocks', {})
            self.primary_ticker = stocks.get('primary_ticker', 'NVDA')
            self.watchlist = stocks.get('watchlist', ['AAPL', 'MSFT', 'TSLA'])

    def _save_config(self):
        cfg = {}
        if os.path.exists(CONFIG_FILE):
            with open(CONFIG_FILE) as f:
                cfg = json.load(f)
        cfg['stocks'] = {
            'primary_ticker': self.primary_ticker,
            'watchlist': self.watchlist,
            'portfolio_file': RECORDS_FILE,
            'update_interval_seconds': 60
        }
        with open(CONFIG_FILE, 'w') as f:
            json.dump(cfg, f, indent=2)

    def _load_records(self):
        if os.path.exists(RECORDS_FILE):
            with open(RECORDS_FILE) as f:
                self.trade_records = json.load(f)
        if os.path.exists(PORTFOLIO_FILE):
            with open(PORTFOLIO_FILE) as f:
                self.portfolio = json.load(f)

    def _save_records(self):
        with open(RECORDS_FILE, 'w') as f:
            json.dump(self.trade_records, f, indent=2)
        with open(PORTFOLIO_FILE, 'w') as f:
            json.dump(self.portfolio, f, indent=2)

    def fetch_quote(self, symbol: str) -> Optional[Dict]:
        """Fetch real-time quote via yfinance."""
        now = time.time()
        if symbol in self.cache and now - self.cache_expiry.get(symbol, 0) < 30:
            return self.cache[symbol]

        try:
            ticker = yf.Ticker(symbol)
            info = ticker.fast_info
            price = info.get('lastPrice', 0) or info.get('last_price', 0)
            prev_close = info.get('previousClose', price) or info.get('previous_close', price)
            
            if price == 0:
                # Fallback to history
                hist = ticker.history(period='1d')
                if not hist.empty:
                    price = float(hist['Close'].iloc[-1])
                    prev_close = float(hist['Open'].iloc[0])

            delta = price - prev_close
            pct = (delta / prev_close * 100) if prev_close else 0

            result = {
                'symbol': symbol,
                'price': f'{price:.2f}',
                'delta': f'{delta:+.2f}',
                'pct': f'({pct:+.1f}%)',
                'up': delta >= 0,
                'raw_price': price,
                'raw_delta': delta
            }
            self.cache[symbol] = result
            self.cache_expiry[symbol] = now
            return result
        except Exception as e:
            print(f"Quote error {symbol}: {e}")
            return None

    def fetch_history(self, symbol: str, period: str = '1d') -> list:
        """Fetch price history for sparkline/chart."""
        try:
            ticker = yf.Ticker(symbol)
            interval_map = {
                '1d': '5m', '5d': '15m', '1mo': '1d',
                '3mo': '1d', '1y': '1wk'
            }
            interval = interval_map.get(period, '1d')
            hist = ticker.history(period=period, interval=interval)
            if hist.empty:
                return []
            return [
                {'date': str(idx), 'close': float(row['Close']),
                 'open': float(row['Open']), 'high': float(row['High']),
                 'low': float(row['Low']), 'volume': int(row['Volume'])}
                for idx, row in hist.iterrows()
            ]
        except Exception as e:
            print(f"History error {symbol}: {e}")
            return []

    def get_sparkline(self, symbol: str, points: int = 40) -> list:
        """Get condensed sparkline data for ESP display."""
        history = self.fetch_history(symbol, '1d')
        if not history:
            return []
        closes = [h['close'] for h in history]
        if len(closes) <= points:
            return [int(c * 100) for c in closes]
        step = len(closes) / points
        return [int(closes[int(i * step)] * 100) for i in range(points)]

    def add_to_watchlist(self, symbol: str):
        symbol = symbol.upper().strip()
        if symbol and symbol not in self.watchlist:
            self.watchlist.append(symbol)
            self._save_config()

    def remove_from_watchlist(self, symbol: str):
        if symbol in self.watchlist:
            self.watchlist.remove(symbol)
            self._save_config()

    def set_primary(self, symbol: str):
        self.primary_ticker = symbol.upper().strip()
        self._save_config()

    def log_trade(self, symbol: str, action: str, price: float, qty: int,
                  notes: str = ''):
        record = {
            'symbol': symbol.upper(),
            'action': action,
            'price': price,
            'qty': qty,
            'total': price * qty,
            'date': datetime.now().strftime('%Y-%m-%d %H:%M'),
            'notes': notes
        }
        self.trade_records.append(record)

        # Update portfolio
        existing = next((p for p in self.portfolio if p['symbol'] == symbol.upper()), None)
        if action.upper() == 'BUY':
            if existing:
                total_qty = existing['qty'] + qty
                existing['avg_price'] = (
                    (existing['avg_price'] * existing['qty'] + price * qty) / total_qty
                )
                existing['qty'] = total_qty
            else:
                self.portfolio.append({
                    'symbol': symbol.upper(),
                    'avg_price': price,
                    'qty': qty
                })
        elif action.upper() == 'SELL':
            if existing:
                existing['qty'] = max(0, existing['qty'] - qty)
                if existing['qty'] == 0:
                    self.portfolio.remove(existing)

        self._save_records()

    def get_portfolio_summary(self) -> Dict:
        """Calculate total portfolio P&L."""
        total_invested = 0.0
        total_current = 0.0
        holdings = []

        for pos in self.portfolio:
            if pos['qty'] <= 0:
                continue
            quote = self.fetch_quote(pos['symbol'])
            current_price = float(quote['raw_price']) if quote else pos['avg_price']
            invested = pos['avg_price'] * pos['qty']
            current = current_price * pos['qty']
            pnl = current - invested
            total_invested += invested
            total_current += current

            holdings.append({
                'symbol': pos['symbol'],
                'qty': pos['qty'],
                'avg': pos['avg_price'],
                'current': current_price,
                'pnl': pnl,
                'pnl_pct': (pnl / invested * 100) if invested else 0
            })

        return {
            'holdings': holdings,
            'total_invested': total_invested,
            'total_current': total_current,
            'total_pnl': total_current - total_invested,
            'total_pnl_pct': ((total_current - total_invested) / total_invested * 100)
                             if total_invested else 0
        }

    def get_esp_payload(self) -> Dict:
        """Build compact JSON payload for ESP8266 UDP."""
        primary = self.fetch_quote(self.primary_ticker)
        sparkline = self.get_sparkline(self.primary_ticker)

        payload: Dict[str, Any] = {
            'symbol': self.primary_ticker,
            'price': f"Rs.{primary['price']}" if primary else '---',
            'delta': primary['delta'] if primary else '',
            'pct': primary['pct'] if primary else '',
            'up': primary['up'] if primary else True,
            'hist': sparkline[-40:],
            'wl': []
        }

        for sym in self.watchlist[:3]:
            q = self.fetch_quote(sym)
            if q:
                payload['wl'].append({
                    's': sym, 'p': f"Rs.{q['price']}",
                    'd': q['delta'], 'u': q['up']
                })

        return payload


# ═══════════════════════════════════════════════════════════
# TKINTER GUI APPLICATION
# ═══════════════════════════════════════════════════════════

class StockManagerGUI:
    """Dark-themed Tkinter desktop application."""

    def __init__(self):
        self.engine = StockManager()
        self.root = tk.Tk()
        self.root.title("CompanionOS — Stock Manager")
        self.root.geometry("1100x750")
        self.root.configure(bg=DARK_BG)
        self.root.minsize(900, 600)

        # Dark theme style
        self.style = ttk.Style()
        self.style.theme_use('clam')
        self._setup_dark_theme()

        self._build_ui()
        self._start_auto_refresh()

    def _setup_dark_theme(self):
        s = self.style
        s.configure('.', background=DARK_BG, foreground=DARK_TEXT,
                     fieldbackground=DARK_SURFACE, borderwidth=0)
        s.configure('TFrame', background=DARK_BG)
        s.configure('Card.TFrame', background=DARK_CARD)
        s.configure('TLabel', background=DARK_BG, foreground=DARK_TEXT, font=('Segoe UI', 10))
        s.configure('Header.TLabel', font=('Segoe UI Semibold', 14), foreground=DARK_TEXT)
        s.configure('Price.TLabel', font=('Consolas', 28, 'bold'), foreground=ACCENT_GREEN)
        s.configure('Dim.TLabel', foreground=DARK_DIM, font=('Segoe UI', 9))
        s.configure('Green.TLabel', foreground=ACCENT_GREEN)
        s.configure('Red.TLabel', foreground=ACCENT_RED)
        s.configure('TButton', background=DARK_SURFACE, foreground=DARK_TEXT,
                     font=('Segoe UI', 10), padding=(12, 6))
        s.map('TButton', background=[('active', DARK_BORDER)])
        s.configure('Accent.TButton', background=ACCENT_BLUE, foreground='white')
        s.map('Accent.TButton', background=[('active', '#1a6ddb')])
        s.configure('TEntry', fieldbackground=DARK_SURFACE, foreground=DARK_TEXT,
                     insertcolor=DARK_TEXT)
        s.configure('TNotebook', background=DARK_BG)
        s.configure('TNotebook.Tab', background=DARK_SURFACE, foreground=DARK_DIM,
                     padding=(16, 8), font=('Segoe UI', 10))
        s.map('TNotebook.Tab',
              background=[('selected', DARK_CARD)],
              foreground=[('selected', DARK_TEXT)])
        s.configure('Treeview', background=DARK_SURFACE, foreground=DARK_TEXT,
                     fieldbackground=DARK_SURFACE, rowheight=28,
                     font=('Consolas', 10))
        s.configure('Treeview.Heading', background=DARK_CARD, foreground=DARK_DIM,
                     font=('Segoe UI Semibold', 10))
        s.map('Treeview', background=[('selected', DARK_BORDER)])

    def _build_ui(self):
        # Tab notebook
        self.notebook = ttk.Notebook(self.root)
        self.notebook.pack(fill='both', expand=True, padx=8, pady=8)

        # Tabs
        self._build_dashboard_tab()
        self._build_watchlist_tab()
        self._build_portfolio_tab()
        self._build_records_tab()
        self._build_settings_tab()

    # ── DASHBOARD TAB ─────────────────────────────────────
    def _build_dashboard_tab(self):
        tab = ttk.Frame(self.notebook)
        self.notebook.add(tab, text='  📊 Dashboard  ')

        # Top row: Primary ticker
        top = ttk.Frame(tab)
        top.pack(fill='x', padx=16, pady=(16, 8))

        self.primary_symbol_label = ttk.Label(top, text=self.engine.primary_ticker,
                                               style='Header.TLabel')
        self.primary_symbol_label.pack(side='left')

        self.primary_price_label = ttk.Label(top, text='Loading...', style='Price.TLabel')
        self.primary_price_label.pack(side='left', padx=(20, 0))

        self.primary_delta_label = ttk.Label(top, text='', style='Green.TLabel')
        self.primary_delta_label.pack(side='left', padx=(12, 0))

        # ESP sync status
        self.sync_label = ttk.Label(top, text='⏸ Not syncing', style='Dim.TLabel')
        self.sync_label.pack(side='right')

        ttk.Button(top, text='🔄 Refresh', command=self._refresh_data).pack(side='right', padx=8)
        ttk.Button(top, text='📡 Sync ESP', command=self._sync_esp,
                   style='Accent.TButton').pack(side='right', padx=4)

        # Chart area
        self.chart_frame = ttk.Frame(tab)
        self.chart_frame.pack(fill='both', expand=True, padx=16, pady=8)

        if HAS_MATPLOTLIB:
            self._setup_chart()

        # Period selector
        period_frame = ttk.Frame(tab)
        period_frame.pack(fill='x', padx=16, pady=(0, 8))
        for period, label in [('1d', '1D'), ('5d', '1W'), ('1mo', '1M'),
                               ('3mo', '3M'), ('1y', '1Y')]:
            btn = ttk.Button(period_frame, text=label,
                             command=lambda p=period: self._update_chart(p))
            btn.pack(side='left', padx=4)

        self.chart_period = '1d'

    def _setup_chart(self):
        self.fig = Figure(figsize=(8, 3.5), dpi=100, facecolor=DARK_BG)
        self.ax = self.fig.add_subplot(111)
        self.ax.set_facecolor(DARK_SURFACE)
        self.ax.tick_params(colors=DARK_DIM, labelsize=8)
        self.ax.spines['top'].set_visible(False)
        self.ax.spines['right'].set_visible(False)
        self.ax.spines['bottom'].set_color(DARK_BORDER)
        self.ax.spines['left'].set_color(DARK_BORDER)
        self.fig.subplots_adjust(left=0.08, right=0.96, top=0.92, bottom=0.15)

        self.canvas = FigureCanvasTkAgg(self.fig, self.chart_frame)
        self.canvas.get_tk_widget().pack(fill='both', expand=True)

    def _update_chart(self, period='1d'):
        if not HAS_MATPLOTLIB:
            return
        self.chart_period = period
        history = self.engine.fetch_history(self.engine.primary_ticker, period)
        if not history:
            return

        self.ax.clear()
        self.ax.set_facecolor(DARK_SURFACE)

        dates = list(range(len(history)))
        closes = [h['close'] for h in history]

        # Determine color
        color = ACCENT_GREEN if closes[-1] >= closes[0] else ACCENT_RED
        fill_color = color + '20'  # Transparent version

        self.ax.plot(dates, closes, color=color, linewidth=1.8)
        self.ax.fill_between(dates, closes, min(closes), alpha=0.1, color=color)
        self.ax.set_title(
            f'{self.engine.primary_ticker} — {period.upper()}',
            color=DARK_TEXT, fontsize=11, fontweight='semibold', loc='left'
        )
        self.ax.tick_params(colors=DARK_DIM)
        self.ax.grid(True, alpha=0.1, color=DARK_BORDER)

        self.canvas.draw()

    # ── WATCHLIST TAB ─────────────────────────────────────
    def _build_watchlist_tab(self):
        tab = ttk.Frame(self.notebook)
        self.notebook.add(tab, text='  📋 Watchlist  ')

        # Add bar
        add_frame = ttk.Frame(tab)
        add_frame.pack(fill='x', padx=16, pady=16)

        ttk.Label(add_frame, text='Add Ticker:', style='Dim.TLabel').pack(side='left')
        self.add_entry = ttk.Entry(add_frame, width=12)
        self.add_entry.pack(side='left', padx=8)
        self.add_entry.bind('<Return>', lambda e: self._add_ticker())
        ttk.Button(add_frame, text='➕ Add', command=self._add_ticker).pack(side='left')
        ttk.Button(add_frame, text='🗑️ Remove Selected',
                   command=self._remove_ticker).pack(side='right')
        ttk.Button(add_frame, text='⭐ Set as Primary',
                   command=self._set_primary).pack(side='right', padx=8)

        # Watchlist tree
        cols = ('Symbol', 'Price', 'Change', '%', 'Status')
        self.wl_tree = ttk.Treeview(tab, columns=cols, show='headings', height=15)
        for col in cols:
            self.wl_tree.heading(col, text=col)
            self.wl_tree.column(col, width=120, anchor='center')
        self.wl_tree.column('Symbol', width=100)
        self.wl_tree.pack(fill='both', expand=True, padx=16, pady=(0, 16))

    def _add_ticker(self):
        symbol = self.add_entry.get().strip().upper()
        if symbol:
            self.engine.add_to_watchlist(symbol)
            self.add_entry.delete(0, 'end')
            self._refresh_watchlist()

    def _remove_ticker(self):
        sel = self.wl_tree.selection()
        if sel:
            symbol = self.wl_tree.item(sel[0])['values'][0]
            self.engine.remove_from_watchlist(symbol)
            self._refresh_watchlist()

    def _set_primary(self):
        sel = self.wl_tree.selection()
        if sel:
            symbol = self.wl_tree.item(sel[0])['values'][0]
            self.engine.set_primary(symbol)
            self.primary_symbol_label.config(text=symbol)
            self._refresh_data()

    def _refresh_watchlist(self):
        for item in self.wl_tree.get_children():
            self.wl_tree.delete(item)

        all_tickers = [self.engine.primary_ticker] + self.engine.watchlist
        for sym in all_tickers:
            q = self.engine.fetch_quote(sym)
            if q:
                status = '⭐ Primary' if sym == self.engine.primary_ticker else ''
                tag = 'up' if q['up'] else 'down'
                self.wl_tree.insert('', 'end', values=(
                    q['symbol'], f"Rs.{q['price']}", q['delta'], q['pct'], status
                ), tags=(tag,))

        self.wl_tree.tag_configure('up', foreground=ACCENT_GREEN)
        self.wl_tree.tag_configure('down', foreground=ACCENT_RED)

    # ── PORTFOLIO TAB ─────────────────────────────────────
    def _build_portfolio_tab(self):
        tab = ttk.Frame(self.notebook)
        self.notebook.add(tab, text='  💼 Portfolio  ')

        # Summary bar
        self.portfolio_summary = ttk.Label(tab, text='Total P&L: Calculating...',
                                            style='Header.TLabel')
        self.portfolio_summary.pack(fill='x', padx=16, pady=(16, 8))

        # Add button
        btn_frame = ttk.Frame(tab)
        btn_frame.pack(fill='x', padx=16, pady=(0, 8))
        ttk.Button(btn_frame, text='📝 Log Trade', command=self._show_trade_dialog,
                   style='Accent.TButton').pack(side='left')
        ttk.Button(btn_frame, text='🔄 Refresh', command=self._refresh_portfolio).pack(
            side='left', padx=8)

        # Holdings tree
        cols = ('Symbol', 'Qty', 'Avg Price', 'Current', 'P&L', 'P&L %')
        self.pf_tree = ttk.Treeview(tab, columns=cols, show='headings', height=12)
        for col in cols:
            self.pf_tree.heading(col, text=col)
            self.pf_tree.column(col, width=110, anchor='center')
        self.pf_tree.pack(fill='both', expand=True, padx=16, pady=(0, 16))

    def _show_trade_dialog(self):
        dialog = tk.Toplevel(self.root)
        dialog.title("Log Trade")
        dialog.geometry("380x320")
        dialog.configure(bg=DARK_BG)
        dialog.transient(self.root)
        dialog.grab_set()

        fields = {}
        for i, (label, default) in enumerate([
            ('Symbol', ''), ('Action (BUY/SELL)', 'BUY'),
            ('Price', ''), ('Quantity', ''), ('Notes', '')
        ]):
            ttk.Label(dialog, text=label).grid(row=i, column=0, padx=16, pady=8, sticky='w')
            entry = ttk.Entry(dialog, width=20)
            entry.insert(0, default)
            entry.grid(row=i, column=1, padx=16, pady=8)
            fields[label] = entry

        def submit():
            try:
                self.engine.log_trade(
                    fields['Symbol'].get(),
                    fields['Action (BUY/SELL)'].get(),
                    float(fields['Price'].get()),
                    int(fields['Quantity'].get()),
                    fields['Notes'].get()
                )
                dialog.destroy()
                self._refresh_portfolio()
                self._refresh_records()
            except ValueError:
                messagebox.showerror("Error", "Invalid price or quantity")

        ttk.Button(dialog, text='✅ Save Trade', command=submit,
                   style='Accent.TButton').grid(row=5, column=0, columnspan=2, pady=16)

    def _refresh_portfolio(self):
        for item in self.pf_tree.get_children():
            self.pf_tree.delete(item)

        summary = self.engine.get_portfolio_summary()
        for h in summary['holdings']:
            tag = 'up' if h['pnl'] >= 0 else 'down'
            self.pf_tree.insert('', 'end', values=(
                h['symbol'], h['qty'], f"Rs.{h['avg']:.2f}",
                f"Rs.{h['current']:.2f}", f"Rs.{h['pnl']:+.2f}",
                f"{h['pnl_pct']:+.1f}%"
            ), tags=(tag,))

        self.pf_tree.tag_configure('up', foreground=ACCENT_GREEN)
        self.pf_tree.tag_configure('down', foreground=ACCENT_RED)

        pnl = summary['total_pnl']
        color_style = 'Green.TLabel' if pnl >= 0 else 'Red.TLabel'
        self.portfolio_summary.config(
            text=f"Total: Rs.{summary['total_current']:.2f}  |  "
                 f"P&L: Rs.{pnl:+.2f} ({summary['total_pnl_pct']:+.1f}%)",
            style=color_style
        )

    # ── RECORDS TAB ───────────────────────────────────────
    def _build_records_tab(self):
        tab = ttk.Frame(self.notebook)
        self.notebook.add(tab, text='  📒 Records  ')

        cols = ('Date', 'Symbol', 'Action', 'Price', 'Qty', 'Total', 'Notes')
        self.rec_tree = ttk.Treeview(tab, columns=cols, show='headings', height=18)
        for col in cols:
            self.rec_tree.heading(col, text=col)
            w = 140 if col in ('Date', 'Notes') else 90
            self.rec_tree.column(col, width=w, anchor='center')
        self.rec_tree.pack(fill='both', expand=True, padx=16, pady=16)

        scrollbar = ttk.Scrollbar(tab, orient='vertical', command=self.rec_tree.yview)
        self.rec_tree.configure(yscrollcommand=scrollbar.set)

    def _refresh_records(self):
        for item in self.rec_tree.get_children():
            self.rec_tree.delete(item)

        for r in reversed(self.engine.trade_records):
            tag = 'buy' if r['action'].upper() == 'BUY' else 'sell'
            self.rec_tree.insert('', 'end', values=(
                r['date'], r['symbol'], r['action'],
                f"Rs.{r['price']:.2f}", r['qty'],
                f"Rs.{r['total']:.2f}", r.get('notes', '')
            ), tags=(tag,))

        self.rec_tree.tag_configure('buy', foreground=ACCENT_GREEN)
        self.rec_tree.tag_configure('sell', foreground=ACCENT_RED)

    # ── SETTINGS TAB ──────────────────────────────────────
    def _build_settings_tab(self):
        tab = ttk.Frame(self.notebook)
        self.notebook.add(tab, text='  ⚙️ Settings  ')

        ttk.Label(tab, text='Stock Manager Settings', style='Header.TLabel').pack(
            padx=16, pady=(16, 8), anchor='w')

        # ESP sync settings
        esp_frame = ttk.Frame(tab)
        esp_frame.pack(fill='x', padx=16, pady=8)

        ttk.Label(esp_frame, text='ESP IP:').grid(row=0, column=0, sticky='w', pady=4)
        self.esp_ip_entry = ttk.Entry(esp_frame, width=20)
        self.esp_ip_entry.insert(0, '192.168.1.123')
        self.esp_ip_entry.grid(row=0, column=1, padx=8, pady=4)

        ttk.Label(esp_frame, text='ESP Port:').grid(row=1, column=0, sticky='w', pady=4)
        self.esp_port_entry = ttk.Entry(esp_frame, width=10)
        self.esp_port_entry.insert(0, '8888')
        self.esp_port_entry.grid(row=1, column=1, padx=8, pady=4, sticky='w')

        self.auto_sync_var = tk.BooleanVar(value=True)
        ttk.Checkbutton(esp_frame, text='Auto-sync to ESP every 60s',
                         variable=self.auto_sync_var).grid(row=2, column=0, columnspan=2,
                                                           pady=8, sticky='w')

        # Refresh interval
        ttk.Label(esp_frame, text='Refresh interval (s):').grid(row=3, column=0,
                                                                  sticky='w', pady=4)
        self.refresh_entry = ttk.Entry(esp_frame, width=10)
        self.refresh_entry.insert(0, '60')
        self.refresh_entry.grid(row=3, column=1, padx=8, pady=4, sticky='w')

    # ── REFRESH / SYNC ────────────────────────────────────
    def _refresh_data(self):
        """Refresh all data in background thread."""
        def _worker():
            self.engine.cache.clear()
            self.engine.cache_expiry.clear()

            q = self.engine.fetch_quote(self.engine.primary_ticker)
            if q:
                self.root.after(0, lambda: self.primary_price_label.config(
                    text=f"Rs.{q['price']}"
                ))
                style = 'Green.TLabel' if q['up'] else 'Red.TLabel'
                self.root.after(0, lambda: self.primary_delta_label.config(
                    text=f"{q['delta']}  {q['pct']}", style=style
                ))

            self.root.after(0, self._refresh_watchlist)
            self.root.after(0, self._refresh_portfolio)
            self.root.after(0, self._refresh_records)
            self.root.after(0, lambda: self._update_chart(self.chart_period))

        threading.Thread(target=_worker, daemon=True).start()

    def _sync_esp(self):
        """Send stock data to ESP8266 via UDP."""
        try:
            payload = self.engine.get_esp_payload()
            msg = f"STOCKS:{json.dumps(payload)}"

            ip = self.esp_ip_entry.get()
            port = int(self.esp_port_entry.get())

            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.sendto(msg.encode(), (ip, port))
            sock.close()

            self.sync_label.config(text=f'✅ Synced {datetime.now().strftime("%H:%M:%S")}')
        except Exception as e:
            self.sync_label.config(text=f'❌ Sync failed: {e}')

    def _start_auto_refresh(self):
        """Initial data load + periodic refresh."""
        self._refresh_data()

        def _auto_loop():
            try:
                interval = int(self.refresh_entry.get()) * 1000
            except ValueError:
                interval = 60000

            self._refresh_data()
            if self.auto_sync_var.get():
                self._sync_esp()
            self.root.after(interval, _auto_loop)

        self.root.after(60000, _auto_loop)

    def run(self):
        self.root.mainloop()


# ═══════════════════════════════════════════════════════════
# ENTRY POINT
# ═══════════════════════════════════════════════════════════

if __name__ == '__main__':
    app = StockManagerGUI()
    app.run()
