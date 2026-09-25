/* ==========================================================================
   FPGA Negotiated Router — site behaviour
   1) Theme toggle (light / dark, persisted in localStorage)
   2) EN / 中 language switching (persisted in localStorage)
   3) Sticky-TOC active-section highlighting
   4) Interactive negotiated-routing visualization (docs/data/*.json)
   No external JavaScript dependencies.
   ========================================================================== */
(function () {
  "use strict";

  /* ---------------------------------------------------------------------- */
  /* Theme                                                                   */
  /* ---------------------------------------------------------------------- */
  var THEME_KEY = "fpga-router-theme";

  function initTheme() {
    var root = document.documentElement;
    var btn = document.getElementById("themeToggle");
    var icon = document.getElementById("themeIcon");

    function apply(theme) {
      root.setAttribute("data-theme", theme);
      if (icon) icon.textContent = theme === "dark" ? "☀️" : "🌙";
      try { localStorage.setItem(THEME_KEY, theme); } catch (e) { /* ignore */ }
    }

    try {
      var stored = localStorage.getItem(THEME_KEY);
      if (stored === "dark" || stored === "light") {
        apply(stored);
      } else if (window.matchMedia && window.matchMedia("(prefers-color-scheme: dark)").matches) {
        apply("dark");
      }
    } catch (e) { /* ignore */ }

    if (btn) {
      btn.addEventListener("click", function () {
        apply(root.getAttribute("data-theme") === "dark" ? "light" : "dark");
      });
    }
  }

  /* ---------------------------------------------------------------------- */
  /* Language switching                                                      */
  /* ---------------------------------------------------------------------- */
  var LANG_KEY = "fpga-router-lang";
  var DEFAULT_LANG = "zh";

  function currentLang() {
    return document.documentElement.getAttribute("data-lang") || DEFAULT_LANG;
  }

  function t(en, zh) {
    return currentLang() === "en" ? en : zh;
  }

  function applyLang(lang) {
    if (lang !== "en" && lang !== "zh") lang = DEFAULT_LANG;
    document.documentElement.setAttribute("data-lang", lang);
    document.documentElement.setAttribute("lang", lang === "en" ? "en" : "zh");

    var titleEl = document.body;
    var titleEn = titleEl && titleEl.getAttribute("data-title-en");
    var titleZh = titleEl && titleEl.getAttribute("data-title-zh");
    if (titleEn || titleZh) {
      document.title = lang === "en" ? (titleEn || titleZh) : (titleZh || titleEn);
    }

    document.querySelectorAll("[data-lang-btn]").forEach(function (btn) {
      btn.classList.toggle("active", btn.getAttribute("data-lang-btn") === lang);
    });
    try { localStorage.setItem(LANG_KEY, lang); } catch (e) { /* ignore */ }

    if (viz.loaded) {
      viz.renderPlayLabel();
      viz.render();
    }
    if (viz.refreshStatus) viz.refreshStatus();
  }

  function initLang() {
    var stored = DEFAULT_LANG;
    try { stored = localStorage.getItem(LANG_KEY) || DEFAULT_LANG; } catch (e) { /* ignore */ }
    applyLang(stored);
    document.querySelectorAll("[data-lang-btn]").forEach(function (btn) {
      btn.addEventListener("click", function () {
        applyLang(btn.getAttribute("data-lang-btn"));
      });
    });
  }

  /* ---------------------------------------------------------------------- */
  /* TOC active-section highlighting                                         */
  /* ---------------------------------------------------------------------- */
  function initToc() {
    var links = Array.prototype.slice.call(document.querySelectorAll(".sidebar .nav a[href^='#']"));
    if (!links.length || !("IntersectionObserver" in window)) return;

    var map = {};
    var sections = [];
    links.forEach(function (link) {
      var id = link.getAttribute("href").slice(1);
      var el = document.getElementById(id);
      if (!el) return;
      map[id] = link;
      sections.push(el);
    });
    if (!sections.length) return;

    var observer = new IntersectionObserver(function (entries) {
      entries.forEach(function (entry) {
        if (!entry.isIntersecting) return;
        links.forEach(function (l) { l.classList.remove("active"); });
        var active = map[entry.target.id];
        if (active) active.classList.add("active");
      });
    }, { rootMargin: "-30% 0px -60% 0px", threshold: 0 });

    sections.forEach(function (s) { observer.observe(s); });
  }

  /* ---------------------------------------------------------------------- */
  /* Visualization                                                           */
  /* ---------------------------------------------------------------------- */
  var SVG_NS = "http://www.w3.org/2000/svg";

  var HEAT_STOPS = [
    [0.00, [16, 32, 74]],
    [0.35, [59, 130, 246]],
    [0.65, [124, 92, 255]],
    [0.85, [192, 38, 211]],
    [1.00, [251, 113, 133]]
  ];
  var CONG_STOPS = [
    [0.00, [13, 19, 51]],
    [0.50, [245, 158, 11]],
    [1.00, [239, 68, 68]]
  ];

  function lerpColor(stops, p) {
    p = Math.max(0, Math.min(1, p));
    for (var i = 1; i < stops.length; i++) {
      if (p <= stops[i][0]) {
        var a = stops[i - 1], b = stops[i];
        var f = (p - a[0]) / (b[0] - a[0] || 1);
        var c = a[1].map(function (v, k) { return Math.round(v + (b[1][k] - v) * f); });
        return "rgb(" + c[0] + "," + c[1] + "," + c[2] + ")";
      }
    }
    var last = stops[stops.length - 1][1];
    return "rgb(" + last[0] + "," + last[1] + "," + last[2] + ")";
  }

  function el(name, attrs) {
    var node = document.createElementNS(SVG_NS, name);
    if (attrs) {
      for (var k in attrs) {
        if (Object.prototype.hasOwnProperty.call(attrs, k)) node.setAttribute(k, String(attrs[k]));
      }
    }
    return node;
  }

  function clear(node) {
    while (node.firstChild) node.removeChild(node.firstChild);
  }

  // docs/data tiles are compact arrays [x, y, occupancy, capacity, congestion_ratio];
  // accept object tiles too so the viewer stays robust.
  function normTile(t) {
    if (Array.isArray(t)) {
      return { x: t[0], y: t[1], occupancy: t[2], capacity: t[3], congestion_ratio: t[4] };
    }
    return t;
  }

  var viz = {
    data: null,
    scenario: "huge",
    step: 0,
    mode: "heat",
    playing: false,
    timer: null,
    loaded: false,
    statusKind: "info",
    statusText: { en: "", zh: "" },
    els: {},

    init: function () {
      var self = this;
      this.els = {
        select: document.getElementById("scenarioSelect"),
        range: document.getElementById("stepRange"),
        stepText: document.getElementById("stepText"),
        stepLabel: document.getElementById("stepLabel"),
        modeToggle: document.getElementById("modeToggle"),
        playBtn: document.getElementById("playBtn"),
        playIcon: document.getElementById("playIcon"),
        playLabel: document.getElementById("playLabel"),
        resetBtn: document.getElementById("resetBtn"),
        grid: document.getElementById("gridSvg"),
        statGrid: document.getElementById("statGrid"),
        statTracks: document.getElementById("statTracks"),
        statUsage: document.getElementById("statUsage"),
        status: document.getElementById("vizStatus"),
        congChart: document.getElementById("congestionChart"),
        usageChart: document.getElementById("usageChart"),
        legendLow: document.getElementById("legendLow"),
        legendHigh: document.getElementById("legendHigh")
      };

      this.els.select.addEventListener("change", function () {
        self.scenario = self.els.select.value;
        self.load();
      });
      this.els.range.addEventListener("input", function () {
        self.stop();
        self.step = Number(self.els.range.value);
        self.render();
      });
      this.els.modeToggle.addEventListener("click", function (ev) {
        var btn = ev.target.closest("[data-mode]");
        if (!btn) return;
        self.mode = btn.getAttribute("data-mode");
        self.els.modeToggle.querySelectorAll("[data-mode]").forEach(function (b) {
          b.classList.toggle("active", b === btn);
        });
        self.updateLegend();
        self.render();
      });
      this.els.playBtn.addEventListener("click", function () {
        if (self.playing) self.stop(); else self.play();
      });
      this.els.resetBtn.addEventListener("click", function () {
        self.stop();
        self.step = 0;
        self.els.range.value = "0";
        self.render();
      });

      this.renderPlayLabel();
      this.updateLegend();

      var resizeTimer = null;
      window.addEventListener("resize", function () {
        if (!self.loaded) return;
        clearTimeout(resizeTimer);
        resizeTimer = setTimeout(function () { self.renderCharts(); }, 150);
      });

      this.load();
    },

    setStatus: function (textEn, kind, textZh) {
      this.statusKind = kind || "info";
      this.statusText = { en: textEn || "", zh: textZh || textEn || "" };
      this.renderStatus();
    },

    refreshStatus: function () {
      if (!this.els.status) return;
      this.renderStatus();
    },

    renderStatus: function () {
      this.els.status.textContent = t(this.statusText.en, this.statusText.zh);
      this.els.status.classList.toggle("error", this.statusKind === "error");
    },

    updateLegend: function () {
      var stops = this.mode === "congestion" ? CONG_STOPS : HEAT_STOPS;
      this.els.legendLow.style.background = lerpColor(stops, 0.05);
      this.els.legendHigh.style.background = lerpColor(stops, 0.95);
    },

    renderPlayLabel: function () {
      var label = this.playing ? { en: "Pause", zh: "暂停" } : { en: "Play", zh: "播放" };
      this.els.playIcon.textContent = this.playing ? "⏸" : "▶";
      var span = document.createElement("span");
      span.textContent = t(label.en, label.zh);
      this.els.playLabel.innerHTML = "";
      this.els.playLabel.appendChild(span);
    },

    load: function () {
      var self = this;
      this.stop();
      this.loaded = false;
      clear(this.els.grid);
      this.els.grid.appendChild(el("text", {
        x: 50, y: 50, fill: "#96a3ce", "font-size": 5, "text-anchor": "middle"
      })).textContent = t("Loading…", "加载中…");
      this.setStatus("Loading " + this.scenario + " …", "info", "正在加载 " + this.scenario + " …");

      fetch("data/" + this.scenario + ".json", { cache: "no-cache" })
        .then(function (res) {
          if (!res.ok) throw new Error("HTTP " + res.status);
          return res.json();
        })
        .then(function (json) {
          json.steps.forEach(function (step) {
            if (step && step.tiles) step.tiles = step.tiles.map(normTile);
          });
          self.data = json;
          self.loaded = true;
          self.step = 0;
          self.els.range.min = "0";
          self.els.range.max = String(json.steps.length - 1);
          self.els.range.value = "0";
          self.setStatus(
            "Loaded " + json.scenario + " · " + json.grid_width + "×" + json.grid_height + ", W=" + json.tracks + " (" + json.steps.length + " sampled steps).",
            "info",
            "已加载 " + json.scenario + " · " + json.grid_width + "×" + json.grid_height + "，W=" + json.tracks + "（" + json.steps.length + " 个抽样步骤）。"
          );
          self.render();
        })
        .catch(function (err) {
          self.loaded = false;
          clear(self.els.grid);
          var msg = el("text", {
            x: 50, y: 47, fill: "#fb7185", "font-size": 4.4, "text-anchor": "middle"
          });
          msg.textContent = t("Failed to load visualization data.", "可视化数据加载失败。");
          self.els.grid.appendChild(msg);
          var hint = el("text", {
            x: 50, y: 55, fill: "#96a3ce", "font-size": 3.4, "text-anchor": "middle"
          });
          hint.textContent = t("Please serve over HTTP (e.g. GitHub Pages).", "请通过 HTTP 提供服务（如 GitHub Pages）。");
          self.els.grid.appendChild(hint);
          self.els.statGrid.textContent = "—";
          self.els.statTracks.textContent = "—";
          self.els.statUsage.textContent = "—";
          self.els.stepText.textContent = "0 / 0";
          self.els.stepLabel.textContent = "—";
          clear(self.els.congChart);
          clear(self.els.usageChart);
          self.setStatus(
            "Could not fetch data/" + self.scenario + ".json (" + err.message + ").",
            "error",
            "无法获取 data/" + self.scenario + ".json（" + err.message + "）。"
          );
        });
    },

    play: function () {
      var self = this;
      if (!this.loaded) return;
      if (this.step >= this.data.steps.length - 1) {
        this.step = 0;
        this.els.range.value = "0";
      }
      this.playing = true;
      this.renderPlayLabel();
      this.render();
      this.timer = setInterval(function () {
        if (self.step < self.data.steps.length - 1) {
          self.step += 1;
          self.els.range.value = String(self.step);
          self.render();
        } else {
          self.stop();
        }
      }, 800);
    },

    stop: function () {
      this.playing = false;
      if (this.timer) { clearInterval(this.timer); this.timer = null; }
      if (this.els.playIcon) this.renderPlayLabel();
    },

    tileColor: function (tile, maxCong) {
      if (this.mode === "congestion") {
        var p = maxCong > 0 ? tile.congestion_ratio / maxCong : 0;
        return lerpColor(CONG_STOPS, p);
      }
      var ratio = tile.capacity > 0 ? tile.occupancy / tile.capacity : 0;
      return lerpColor(HEAT_STOPS, ratio);
    },

    render: function () {
      if (!this.loaded) return;
      var step = this.data.steps[this.step];
      var cols = this.data.grid_width;
      var rows = this.data.grid_height;
      var margin = 1.5;
      var cell = Math.max(6, Math.floor(680 / Math.max(cols, rows)));

      var svg = this.els.grid;
      var w = margin * 2 + cols * cell;
      var h = margin * 2 + rows * cell;
      clear(svg);
      svg.setAttribute("viewBox", "0 0 " + w + " " + h);

      svg.appendChild(el("rect", { x: 0, y: 0, width: w, height: h, fill: "#0a0f28", rx: 2 }));

      var maxCong = 0;
      for (var i = 0; i < step.tiles.length; i++) {
        if (step.tiles[i].congestion_ratio > maxCong) maxCong = step.tiles[i].congestion_ratio;
      }

      var usedSum = 0, capSum = 0;
      var frag = document.createDocumentFragment();
      for (var j = 0; j < step.tiles.length; j++) {
        var tile = step.tiles[j];
        usedSum += tile.occupancy;
        capSum += tile.capacity;
        var rect = el("rect", {
          x: margin + tile.x * cell,
          y: margin + tile.y * cell,
          width: cell,
          height: cell,
          fill: this.tileColor(tile, maxCong),
          stroke: "rgba(150,170,255,0.12)",
          "stroke-width": 0.4
        });
        var title = el("title");
        title.textContent = "(" + tile.x + "," + tile.y + ")  occ " + tile.occupancy +
          " / cap " + tile.capacity + "  cong " + tile.congestion_ratio;
        rect.appendChild(title);
        frag.appendChild(rect);
      }
      svg.appendChild(frag);

      this.els.stepText.textContent = this.step + " / " + (this.data.steps.length - 1);
      this.els.stepLabel.textContent = step.label;
      this.els.statGrid.textContent = cols + " × " + rows;
      this.els.statTracks.textContent = String(this.data.tracks);
      var pct = capSum > 0 ? (usedSum * 100 / capSum) : 0;
      this.els.statUsage.textContent = pct.toFixed(1) + "%";

      this.renderCharts();
    },

    historyIndex: function () {
      if (!this.data) return this.step;
      var labels = this.data.step_labels;
      if (!labels || !labels.length) {
        return Math.round(this.step * (this.data.congestion_history.length - 1) / Math.max(1, this.data.steps.length - 1));
      }
      var label = this.data.steps[this.step].label;
      var idx = labels.indexOf(label);
      return idx >= 0 ? idx : Math.round(this.step * (this.data.congestion_history.length - 1) / Math.max(1, this.data.steps.length - 1));
    },

    renderCharts: function () {
      var total = this.data.congestion_history ? this.data.congestion_history.length : 0;
      if (!total) return;
      var idx = this.historyIndex();
      this.drawChart(this.els.congChart, this.data.congestion_history, [192, 38, 211], idx);
      this.drawChart(this.els.usageChart, this.data.resource_usage_history, [34, 211, 238], idx);
    },

    drawChart: function (svg, data, rgb, currentIdx) {
      clear(svg);
      var W = svg.clientWidth || svg.parentNode.clientWidth || 600;
      var H = 130;
      var pad = { top: 14, right: 14, bottom: 22, left: 44 };
      svg.setAttribute("viewBox", "0 0 " + W + " " + H);

      var plotW = W - pad.left - pad.right;
      var plotH = H - pad.top - pad.bottom;
      var min = Math.min.apply(null, data);
      var max = Math.max.apply(null, data);
      if (min === max) { max = min + 1; }
      var range = max - min;

      var xAt = function (i) { return pad.left + plotW * (i / Math.max(1, data.length - 1)); };
      var yAt = function (v) { return pad.top + plotH * (1 - (v - min) / range); };

      svg.appendChild(el("line", { x1: pad.left, y1: pad.top, x2: pad.left, y2: H - pad.bottom, stroke: "rgba(150,170,255,0.25)", "stroke-width": 1 }));
      svg.appendChild(el("line", { x1: pad.left, y1: H - pad.bottom, x2: W - pad.right, y2: H - pad.bottom, stroke: "rgba(150,170,255,0.25)", "stroke-width": 1 }));

      [min, max].forEach(function (v, k) {
        var txt = el("text", { x: pad.left - 6, y: yAt(v) + (k === 0 ? 3 : -2), "text-anchor": "end", "font-size": 9, fill: "#96a3ce" });
        txt.textContent = Number.isInteger(v) ? v : v.toFixed(1);
        svg.appendChild(txt);
      });

      var line = "", area = "";
      for (var i = 0; i < data.length; i++) {
        var x = xAt(i), y = yAt(data[i]);
        line += (i === 0 ? "M" : "L") + x.toFixed(1) + " " + y.toFixed(1);
        area += (i === 0 ? "M" : "L") + x.toFixed(1) + " " + y.toFixed(1);
      }
      area += "L" + xAt(data.length - 1).toFixed(1) + " " + (H - pad.bottom) +
              "L" + pad.left + " " + (H - pad.bottom) + "Z";
      svg.appendChild(el("path", { d: area, fill: "rgba(" + rgb.join(",") + ",0.12)", stroke: "none" }));
      svg.appendChild(el("path", { d: line, fill: "none", stroke: "rgb(" + rgb.join(",") + ")", "stroke-width": 1.8 }));

      if (currentIdx >= 0 && currentIdx < data.length) {
        var cx = xAt(currentIdx), cy = yAt(data[currentIdx]);
        svg.appendChild(el("line", { x1: cx, y1: pad.top, x2: cx, y2: H - pad.bottom, stroke: "rgba(255,255,255,0.35)", "stroke-width": 1, "stroke-dasharray": "3 3" }));
        svg.appendChild(el("circle", { cx: cx, cy: cy, r: 3.4, fill: "#fff", stroke: "rgb(" + rgb.join(",") + ")", "stroke-width": 2 }));
      }
    }
  };

  /* ---------------------------------------------------------------------- */
  document.addEventListener("DOMContentLoaded", function () {
    initTheme();
    initLang();
    initToc();
    if (document.getElementById("gridSvg")) viz.init();
  });
})();
